#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Core {
	namespace Features {
		namespace ShadowPlayPatch {
			namespace ManualMap {

				inline bool RemoteWrite(HANDLE process, void* remote, const void* local, size_t size)
				{
					SIZE_T written = 0;
					return WriteProcessMemory(process, remote, local, size, &written) && written == size;
				}

				inline uintptr_t RemoteLoadLibrary(HANDLE process, const wchar_t* moduleName)
				{
					if (!process || !moduleName || !*moduleName)
						return 0;

					const size_t bytes = (wcslen(moduleName) + 1) * sizeof(wchar_t);
					void* remoteName = VirtualAllocEx(
						process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
					if (!remoteName)
						return 0;

					if (!RemoteWrite(process, remoteName, moduleName, bytes)) {
						VirtualFreeEx(process, remoteName, 0, MEM_RELEASE);
						return 0;
					}

					HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
					auto* loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
						GetProcAddress(k32, "LoadLibraryW"));
					if (!loadLib) {
						VirtualFreeEx(process, remoteName, 0, MEM_RELEASE);
						return 0;
					}

					HANDLE thread = CreateRemoteThread(
						process, nullptr, 0, loadLib, remoteName, 0, nullptr);
					if (!thread) {
						VirtualFreeEx(process, remoteName, 0, MEM_RELEASE);
						return 0;
					}

					WaitForSingleObject(thread, 15000);
					DWORD code = 0;
					GetExitCodeThread(thread, &code);
					CloseHandle(thread);
					VirtualFreeEx(process, remoteName, 0, MEM_RELEASE);
					return static_cast<uintptr_t>(code);
				}

				inline bool ResolveImports(
					HANDLE process,
					uint8_t* localImage,
					const IMAGE_NT_HEADERS64* nt,
					uintptr_t (*remoteModuleBase)(HANDLE, const wchar_t*))
				{
					const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
					if (!dir.VirtualAddress || !dir.Size)
						return true;

					auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
						localImage + dir.VirtualAddress);
					for (; importDesc->Name; ++importDesc) {
						const char* modNameA = reinterpret_cast<const char*>(
							localImage + importDesc->Name);
						std::wstring modNameW(modNameA, modNameA + strlen(modNameA));

						uintptr_t remoteMod = remoteModuleBase(process, modNameW.c_str());
						if (!remoteMod)
							remoteMod = RemoteLoadLibrary(process, modNameW.c_str());
						if (!remoteMod)
							return false;


						HMODULE localMod = GetModuleHandleW(modNameW.c_str());
						if (!localMod)
							localMod = LoadLibraryW(modNameW.c_str());
						if (!localMod)
							return false;

						auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(
							localImage + (importDesc->OriginalFirstThunk
								? importDesc->OriginalFirstThunk
								: importDesc->FirstThunk));
						auto* iat = reinterpret_cast<IMAGE_THUNK_DATA64*>(
							localImage + importDesc->FirstThunk);

						for (; thunk->u1.AddressOfData; ++thunk, ++iat) {
							FARPROC localFn = nullptr;
							if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
								localFn = GetProcAddress(
									localMod,
									MAKEINTRESOURCEA(IMAGE_ORDINAL64(thunk->u1.Ordinal)));
							} else {
								auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
									localImage + thunk->u1.AddressOfData);
								localFn = GetProcAddress(
									localMod, reinterpret_cast<const char*>(ibn->Name));
							}
							if (!localFn)
								return false;

							iat->u1.Function = remoteMod
								+ (reinterpret_cast<uintptr_t>(localFn)
									- reinterpret_cast<uintptr_t>(localMod));
						}
					}
					return true;
				}

				inline void Relocate(uint8_t* localImage, uintptr_t remoteBase, const IMAGE_NT_HEADERS64* nt)
				{
					const intptr_t delta = static_cast<intptr_t>(
						remoteBase - nt->OptionalHeader.ImageBase);
					if (delta == 0)
						return;

					const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
					if (!dir.VirtualAddress || !dir.Size)
						return;

					auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
						localImage + dir.VirtualAddress);
					const auto* relocEnd = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
						reinterpret_cast<uint8_t*>(reloc) + dir.Size);

					while (reloc < relocEnd && reloc->SizeOfBlock) {
						const DWORD count =
							(reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
						auto* entries = reinterpret_cast<WORD*>(reloc + 1);
						for (DWORD i = 0; i < count; ++i) {
							const WORD type = entries[i] >> 12;
							const WORD offset = entries[i] & 0x0FFF;
							if (type == IMAGE_REL_BASED_DIR64) {
								auto* patch = reinterpret_cast<uint64_t*>(
									localImage + reloc->VirtualAddress + offset);
								*patch += static_cast<uint64_t>(delta);
							} else if (type == IMAGE_REL_BASED_HIGHLOW) {
								auto* patch = reinterpret_cast<uint32_t*>(
									localImage + reloc->VirtualAddress + offset);
								*patch += static_cast<uint32_t>(delta);
							}
						}
						reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
							reinterpret_cast<uint8_t*>(reloc) + reloc->SizeOfBlock);
					}
				}


				inline bool MapAndRun(
					HANDLE process,
					const unsigned char* peBytes,
					size_t peSize,
					uintptr_t (*remoteModuleBase)(HANDLE, const wchar_t*))
				{
					if (!process || !peBytes || peSize < sizeof(IMAGE_DOS_HEADER))
						return false;

					const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(peBytes);
					if (dos->e_magic != IMAGE_DOS_SIGNATURE)
						return false;
					if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > peSize)
						return false;

					const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
						peBytes + dos->e_lfanew);
					if (nt->Signature != IMAGE_NT_SIGNATURE
						|| nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
						return false;

					const SIZE_T imageSize = nt->OptionalHeader.SizeOfImage;
					void* remoteImage = VirtualAllocEx(
						process,
						nullptr,
						imageSize,
						MEM_COMMIT | MEM_RESERVE,
						PAGE_EXECUTE_READWRITE);
					if (!remoteImage)
						return false;

					std::vector<uint8_t> localImage(imageSize, 0);
					std::memcpy(localImage.data(), peBytes, nt->OptionalHeader.SizeOfHeaders);

					const auto* section = IMAGE_FIRST_SECTION(nt);
					for (UINT i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
						if (!section->SizeOfRawData)
							continue;
						if (section->PointerToRawData + section->SizeOfRawData > peSize)
							continue;
						if (section->VirtualAddress + section->SizeOfRawData > imageSize)
							continue;
						std::memcpy(
							localImage.data() + section->VirtualAddress,
							peBytes + section->PointerToRawData,
							section->SizeOfRawData);
					}

					auto* localNt = reinterpret_cast<IMAGE_NT_HEADERS64*>(
						localImage.data() + dos->e_lfanew);
					const uintptr_t remoteBase = reinterpret_cast<uintptr_t>(remoteImage);
					Relocate(localImage.data(), remoteBase, localNt);

					if (!ResolveImports(process, localImage.data(), localNt, remoteModuleBase)) {
						VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
						return false;
					}

					if (!RemoteWrite(process, remoteImage, localImage.data(), imageSize)) {
						VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
						return false;
					}

					const uintptr_t entry = remoteBase + localNt->OptionalHeader.AddressOfEntryPoint;


					uint8_t stub[] = {
						0x48, 0x83, 0xEC, 0x28,
						0x48, 0xB9, 0, 0, 0, 0, 0, 0, 0, 0,
						0xBA, 0x01, 0x00, 0x00, 0x00,
						0x4D, 0x31, 0xC0,
						0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,
						0xFF, 0xD0,
						0x48, 0x83, 0xC4, 0x28,
						0xC3
					};
					*reinterpret_cast<uint64_t*>(stub + 6) = remoteBase;
					*reinterpret_cast<uint64_t*>(stub + 24) = entry;

					void* remoteStub = VirtualAllocEx(
						process, nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
					if (!remoteStub) {
						VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
						return false;
					}
					if (!RemoteWrite(process, remoteStub, stub, sizeof(stub))) {
						VirtualFreeEx(process, remoteStub, 0, MEM_RELEASE);
						VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
						return false;
					}

					HANDLE thread = CreateRemoteThread(
						process,
						nullptr,
						0,
						reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteStub),
						nullptr,
						0,
						nullptr);
					if (!thread) {
						VirtualFreeEx(process, remoteStub, 0, MEM_RELEASE);
						VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
						return false;
					}

					WaitForSingleObject(thread, 30000);
					DWORD code = 0;
					GetExitCodeThread(thread, &code);
					CloseHandle(thread);
					VirtualFreeEx(process, remoteStub, 0, MEM_RELEASE);


					return code != 0;
				}

			}
		}
	}
}
