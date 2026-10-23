#include "ShadowPlayPatch.h"
#include "NvidiaProtectBytes.h"
#include "ManualMap.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>

namespace Core {
	namespace Features {
		namespace ShadowPlayPatch {
			namespace {

				static std::vector<DWORD> g_InjectedPids;
				static DWORD g_LastAttemptMs = 0;
				static constexpr DWORD kRetryMs = 2000;
				static HANDLE g_ConfigMap = nullptr;


				static constexpr wchar_t kConfigMapName[] = L"Local\\CapPipeCfg";
				static constexpr DWORD kConfigMagic = 0x5053564Eu;

				struct SharedConfig {
					DWORD skipPid;
					DWORD magic;
				};

				static void NvLog(const char* fmt, ...)
				{
					char line[1024];
					va_list args;
					va_start(args, fmt);
					const int n = _vsnprintf_s(line, _TRUNCATE, fmt, args);
					va_end(args);
					if (n <= 0)
						return;
					OutputDebugStringA("[nvsp] ");
					OutputDebugStringA(line);
					OutputDebugStringA("\n");
				}

				static std::wstring ToLower(std::wstring s)
				{
					std::transform(s.begin(), s.end(), s.begin(), ::towlower);
					return s;
				}

				static std::vector<DWORD> ProcessesByName(const wchar_t* name)
				{
					std::vector<DWORD> ids;
					HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
					if (snap == INVALID_HANDLE_VALUE)
						return ids;

					PROCESSENTRY32W pe{};
					pe.dwSize = sizeof(pe);
					const std::wstring target = ToLower(name);
					if (Process32FirstW(snap, &pe)) {
						do {
							if (ToLower(pe.szExeFile) == target)
								ids.push_back(pe.th32ProcessID);
						} while (Process32NextW(snap, &pe));
					}
					CloseHandle(snap);
					return ids;
				}

				static bool ModuleLoaded(DWORD pid, const wchar_t* moduleName)
				{
					HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
					if (snap == INVALID_HANDLE_VALUE)
						return false;

					MODULEENTRY32W me{};
					me.dwSize = sizeof(me);
					const std::wstring target = ToLower(moduleName);
					bool found = false;
					if (Module32FirstW(snap, &me)) {
						do {
							if (ToLower(me.szModule) == target || ToLower(me.szExePath) == target) {
								found = true;
								break;
							}
						} while (Module32NextW(snap, &me));
					}
					CloseHandle(snap);
					return found;
				}

				static bool IsInjected(DWORD pid)
				{
					return std::find(g_InjectedPids.begin(), g_InjectedPids.end(), pid) != g_InjectedPids.end();
				}

				static void MarkInjected(DWORD pid)
				{
					if (!IsInjected(pid))
						g_InjectedPids.push_back(pid);
				}

				static void PruneDeadInjected()
				{
					std::vector<DWORD> alive;
					alive.reserve(g_InjectedPids.size());
					for (DWORD pid : g_InjectedPids) {
						HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
						if (!h)
							continue;
						DWORD code = 0;
						const bool ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
						CloseHandle(h);
						if (ok)
							alive.push_back(pid);
						else
							NvLog("Prune: pid=%lu exited", pid);
					}
					g_InjectedPids.swap(alive);
				}

				static uintptr_t RemoteModuleBase(HANDLE hProcess, const wchar_t* moduleName)
				{
					HANDLE snap = CreateToolhelp32Snapshot(
						TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProcess));
					if (snap == INVALID_HANDLE_VALUE)
						return 0;

					MODULEENTRY32W me{};
					me.dwSize = sizeof(me);
					uintptr_t base = 0;
					if (Module32FirstW(snap, &me)) {
						do {
							if (_wcsicmp(me.szModule, moduleName) == 0) {
								base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
								break;
							}
						} while (Module32NextW(snap, &me));
					}
					CloseHandle(snap);
					return base;
				}

				static void PublishSkipPid(DWORD pid)
				{
					if (!g_ConfigMap) {
						g_ConfigMap = CreateFileMappingW(
							INVALID_HANDLE_VALUE,
							nullptr,
							PAGE_READWRITE,
							0,
							sizeof(SharedConfig),
							kConfigMapName);
						if (!g_ConfigMap) {
							NvLog("PublishSkipPid CreateFileMapping FAIL gle=%lu", GetLastError());
							return;
						}
					}

					auto* cfg = static_cast<SharedConfig*>(
						MapViewOfFile(g_ConfigMap, FILE_MAP_WRITE, 0, 0, sizeof(SharedConfig)));
					if (!cfg) {
						NvLog("PublishSkipPid MapView FAIL gle=%lu", GetLastError());
						return;
					}
					cfg->skipPid = pid;
					cfg->magic = kConfigMagic;
					FlushViewOfFile(cfg, sizeof(SharedConfig));
					UnmapViewOfFile(cfg);
					NvLog("PublishSkipPid %lu (shared memory)", pid);
				}

				static bool MapImage(
					HANDLE hProcess,
					DWORD pid,
					const char* tag,
					const unsigned char* bytes,
					size_t size)
				{
					NvLog("Map[%s]: %zu bytes -> pid=%lu", tag, size, pid);
					const bool ok = ManualMap::MapAndRun(
						hProcess, bytes, size, &RemoteModuleBase);
					NvLog("Map[%s]: %s pid=%lu gle=%lu",
						tag,
						ok ? "SUCCESS" : "FAIL",
						pid,
						ok ? 0ul : GetLastError());
					return ok;
				}


				static bool RemoteAlreadyPresent(DWORD pid)
				{
					wchar_t name[64]{};
					_snwprintf_s(name, _TRUNCATE, L"Local\\CapPipe.%lu", pid);
					HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, name);
					if (!h)
						return false;
					CloseHandle(h);
					return true;
				}

				static bool InjectProtect(DWORD pid)
				{
					if (IsInjected(pid)) {
						NvLog("Inject: already done (local) pid=%lu", pid);
						return true;
					}


					if (RemoteAlreadyPresent(pid)) {
						MarkInjected(pid);
						NvLog("Inject: already present in pid=%lu — skip remap", pid);
						return true;
					}

					HANDLE hProcess = OpenProcess(
						PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE
							| PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
						FALSE,
						pid);
					if (!hProcess) {
						NvLog("Inject: OpenProcess FAIL pid=%lu gle=%lu", pid, GetLastError());
						return false;
					}

					const bool ok = MapImage(
						hProcess,
						pid,
						"nvsp",
						Embedded::kNvidiaProtectBytes,
						Embedded::kNvidiaProtectSize);

					CloseHandle(hProcess);

					if (ok) {
						MarkInjected(pid);
						NvLog("Inject: OK pid=%lu", pid);
					}
					return ok;
				}

			}

			bool EnsurePatched()
			{
				NvLog("EnsurePatched begin (affinity+Process32 + FBC->DDA, idempotent)");
				PublishSkipPid(GetCurrentProcessId());
				PruneDeadInjected();

				const auto pids = ProcessesByName(L"nvcontainer.exe");
				NvLog("Found %zu nvcontainer (already=%zu)", pids.size(), g_InjectedPids.size());
				if (pids.empty())
					return false;

				bool anyOk = !g_InjectedPids.empty();
				for (DWORD pid : pids) {
					if (InjectProtect(pid))
						anyOk = true;
				}

				NvLog("EnsurePatched end ok=%d", anyOk ? 1 : 0);
				return anyOk;
			}

			void Tick(bool streamproofEnabled)
			{
				if (!streamproofEnabled) {
					if (!g_InjectedPids.empty())
						NvLog("Tick: streamproof off, clear state");
					g_InjectedPids.clear();
					return;
				}

				const DWORD now = GetTickCount();
				if ((now - g_LastAttemptMs) < kRetryMs && g_LastAttemptMs != 0)
					return;
				g_LastAttemptMs = now;
				EnsurePatched();
			}
		}
	}
}
