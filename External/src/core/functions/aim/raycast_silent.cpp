#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "raycast_silent.h"
#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include "../../globals/globals.h"
#include <Windows.h>
#include <vector>
#include <cstring>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#ifndef CFG_CALL_TARGET_VALID
#define CFG_CALL_TARGET_VALID 0x00000001
#endif

#define g_Memory (*memory)

namespace Console {
enum class Color { Orange, Yellow };
inline void Clear() {}
inline void DumpWorld() {}
inline void DumpSilent(bool ok, std::uintptr_t orig, std::uintptr_t stub,
                       std::uintptr_t state, std::uintptr_t slot, const char* detail) {
    std::printf("[Silent] %s handler=0x%llx stub=0x%llx state=0x%llx slot=0x%llx%s%s\n",
        ok ? "ready" : "failed",
        static_cast<unsigned long long>(orig),
        static_cast<unsigned long long>(stub),
        static_cast<unsigned long long>(state),
        static_cast<unsigned long long>(slot),
        detail ? " detail=" : "", detail ? detail : "");
}
inline void Log(Color, const char* message) {
    std::printf("[Silent] %s\n", message ? message : "");
}
}

namespace Cheat {
    namespace Features {
        namespace RaycastSilent {
            namespace {

                constexpr std::uintptr_t desc_rva_z = Offsets::WorldRoot::RaycastBoundDesc;
                constexpr std::uintptr_t bound_fn_offset = Offsets::WorldRoot::RaycastBoundFn;

#pragma pack(push, 4)
                struct RaycastState {
                    std::uint32_t active = 0;
                    std::uint32_t reserved = 0;
                    float target_x = 0.f;
                    float target_y = 0.f;
                    float target_z = 0.f;
                    float scale = 1.15f;
                    std::uint64_t calls = 0;
                    float cam_x = 0.f;
                    float cam_y = 0.f;
                    float cam_z = 0.f;
                    std::uint32_t counter_pad = 0;
                    std::uint64_t handler_calls = 0;
                };
#pragma pack(pop)

                static_assert(offsetof(RaycastState, active) == 0x00, "active");
                static_assert(offsetof(RaycastState, target_x) == 0x08, "target");
                static_assert(offsetof(RaycastState, calls) == 0x18, "calls");
                static_assert(offsetof(RaycastState, cam_x) == 0x20, "cam");
                static_assert(offsetof(RaycastState, handler_calls) == 0x30, "handler calls");

                struct Hook {
                    std::uintptr_t thunk = 0;
                    std::uintptr_t state = 0;
                    std::uintptr_t originalFunction = 0;
                    std::uintptr_t module_base = 0;
                    std::uintptr_t descriptor_rva = 0;
                    bool thunk_owned = false;
                    bool installed = false;
                    bool active = false;
                };

                Hook g_hook{};
                Hook g_legacy{};
                bool g_wallbang = false;
                auto g_lastFail = std::chrono::steady_clock::time_point{};
                auto g_lastLegacyFail = std::chrono::steady_clock::time_point{};

                bool addr_ok(std::uintptr_t a)
                {
                    return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
                }

                bool w_mem(std::uintptr_t a, const void* d, std::size_t s)
                {
                    if (!addr_ok(a) || !d || !s || !g_Memory.GetHandle())
                    {
                        return false;
                    }
                    return g_Memory.WriteRaw(a, d, s) == s;
                }

                std::size_t page_sz()
                {
                    static std::size_t p = 0;
                    if (!p)
                    {
                        SYSTEM_INFO i{};
                        GetSystemInfo(&i);
                        p = (std::size_t)i.dwPageSize;
                        if (!p) p = 0x1000u;
                    }
                    return p;
                }

                DWORD query_protect(std::uintptr_t a)
                {
                    MEMORY_BASIC_INFORMATION mbi{};
                    if (!VirtualQueryEx(g_Memory.GetHandle(),
                            reinterpret_cast<void*>(a), &mbi, sizeof(mbi)))
                    {
                        return 0;
                    }
                    return mbi.Protect;
                }

                bool is_executable_protect(DWORD p)
                {
                    DWORD x = p & 0xFF;
                    if (x == PAGE_EXECUTE)
                    {
                        return true;
                    }

                    if (x == PAGE_EXECUTE_READ)
                    {
                        return true;
                    }

                    if (x == PAGE_EXECUTE_READWRITE)
                    {
                        return true;
                    }

                    if (x == PAGE_EXECUTE_WRITECOPY)
                    {
                        return true;
                    }

                    return false;
                }

                bool protect_remote(std::uintptr_t address, std::size_t size, DWORD protection,
                                    DWORD* old_protect = nullptr)
                {
                    if (!addr_ok(address) || !size || !g_Memory.GetHandle())
                    {
                        return false;
                    }

                    std::uintptr_t page_mask = ~((std::uintptr_t)page_sz() - 1);
                    std::uintptr_t base = address & page_mask;
                    std::uintptr_t end =
                        (address + size + page_sz() - 1) & page_mask;
                    std::size_t span = (std::size_t)(end - base);

                    using NtProtectFn = LONG(WINAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
                    static NtProtectFn nt_protect = nullptr;
                    if (!nt_protect)
                    {
                        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
                        if (ntdll)
                        {
                            nt_protect = (NtProtectFn)GetProcAddress(ntdll, "NtProtectVirtualMemory");
                        }
                    }

                    auto try_one = [&](DWORD req) -> bool
                    {
                        DWORD old = 0;
                        if (VirtualProtectEx(g_Memory.GetHandle(),
                                (void*)base, span, req, &old))
                        {
                            if (old_protect)
                            {
                                *old_protect = old;
                            }
                            return true;
                        }

                        if (!nt_protect)
                        {
                            return false;
                        }

                        PVOID  nt_base = (void*)base;
                        SIZE_T nt_size = span;
                        ULONG  nt_old  = 0;
                        LONG st = nt_protect(
                            g_Memory.GetHandle(), &nt_base, &nt_size, req, &nt_old);
                        if (st >= 0)
                        {
                            if (old_protect)
                            {
                                *old_protect = (DWORD)nt_old;
                            }
                            return true;
                        }

                        return false;
                    };

                    if (try_one(protection))
                    {
                        return true;
                    }

                    if (protection == PAGE_EXECUTE_READWRITE &&
                        try_one(PAGE_EXECUTE_WRITECOPY))
                    {
                        return true;
                    }

                    if (protection == PAGE_READWRITE && try_one(PAGE_WRITECOPY))
                    {
                        return true;
                    }

                    return false;
                }

                bool write_protected(std::uintptr_t address, const void* data, std::size_t size)
                {
                    if (!addr_ok(address) || !data || !size)
                    {
                        return false;
                    }

                    DWORD old = 0;
                    bool changed =
                        protect_remote(address, size, PAGE_EXECUTE_READWRITE, &old);
                    bool wrote = w_mem(address, data, size);
                    if (changed)
                    {
                        protect_remote(address, size, old, nullptr);
                    }
                    return wrote;
                }

                bool mark_cfg(std::uintptr_t t)
                {
                    auto resolve = []() -> FARPROC
                    {
                        const char* mods[] = {
                            "kernelbase.dll", "kernel32.dll",
                            "api-ms-win-core-memory-l1-1-3.dll"
                        };
                        for (auto* m : mods)
                        {
                            HMODULE h = GetModuleHandleA(m);
                            if (!h)
                            {
                                h = LoadLibraryA(m);
                            }

                            if (!h)
                            {
                                continue;
                            }

                            FARPROC p = GetProcAddress(h, "SetProcessValidCallTargets");
                            if (p)
                            {
                                return p;
                            }
                        }
                        return nullptr;
                    };

                    FARPROC proc = resolve();
                    if (!proc)
                    {
                        return false;
                    }

                    struct Info {
                        ULONG_PTR Offset;
                        ULONG     Flags;
                    } info{};
                    info.Offset = t & (page_sz() - 1);
                    info.Flags  = CFG_CALL_TARGET_VALID;

                    using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
                    SetLastError(0);
                    BOOL ok = ((Fn)proc)(
                        g_Memory.GetHandle(),
                        (void*)(t & ~((std::uintptr_t)page_sz() - 1)),
                        page_sz(), 1, &info);
                    return ok != 0;
                }

                void append_u64(std::vector<std::uint8_t>& c, std::uint64_t v)
                {
                    const auto* b = (const std::uint8_t*)&v;
                    c.insert(c.end(), b, b + 8);
                }

                void patch_rel32(std::vector<std::uint8_t>& c, std::size_t o, std::size_t t)
                {
                    std::int32_t v = (std::int32_t)((std::ptrdiff_t)t - (std::ptrdiff_t)(o + 4));
                    std::memcpy(c.data() + o, &v, 4);
                }

                std::vector<std::uint8_t> make_jmp_thunk(std::uintptr_t orig)
                {
                    std::vector<std::uint8_t> c;
                    c.insert(c.end(), { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });
                    append_u64(c, orig);
                    return c;
                }

                std::vector<std::uint8_t> make_legacy_thunk(std::uintptr_t stateAddress, std::uintptr_t originalFunction) {
        std::vector<std::uint8_t> code;
        code.reserve(768);
        std::vector<std::size_t> inactiveJumpOffsets;

        auto emitInactiveJump = [&]() {
            code.insert(code.end(), { 0x0F, 0x84 });
            inactiveJumpOffsets.push_back(code.size());
            code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });
            };

        code.insert(code.end(), { 0x48, 0x81, 0xEC, 0xA0, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x4C, 0x89, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x49, 0xBA });
        append_u64(code, stateAddress);
        code.insert(code.end(), { 0x41, 0x83, 0x3A, 0x00 });
        emitInactiveJump();
        code.insert(code.end(), { 0x4D, 0x85, 0xC0 });
        emitInactiveJump();

        code.insert(code.end(), { 0x41, 0x0F, 0x10, 0x00 });
        code.insert(code.end(), { 0x0F, 0x11, 0x44, 0x24, 0x40 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x40, 0x10 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x58 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x40, 0x14 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x5C });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x40, 0x18 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x60 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x42, 0x08 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x00 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x4A, 0x0C });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x48, 0x04 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x52, 0x10 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x50, 0x08 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x42, 0x14 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x4C });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x4A, 0x14 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x50 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x52, 0x14 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x54 });

        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x5C, 0x24, 0x58 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xDB });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x64, 0x24, 0x5C });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x64, 0x24, 0x60 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x51, 0xDB });
        code.insert(code.end(), { 0x0F, 0x57, 0xE4 });
        code.insert(code.end(), { 0x0F, 0x2E, 0xDC });
        code.insert(code.end(), { 0x0F, 0x86 });
        const std::size_t origMagZeroJump = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x64, 0x24, 0x4C });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x6C, 0x24, 0x50 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xED });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x6C, 0x24, 0x54 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xED });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x51, 0xE4 });

        code.insert(code.end(), { 0x0F, 0x28, 0xF4 });

        code.insert(code.end(), { 0x0F, 0x2E, 0xE4 });
        code.insert(code.end(), { 0x7A, 0x02 });
        code.insert(code.end(), { 0x0F, 0x57, 0xED });
        code.insert(code.end(), { 0x0F, 0x2E, 0xEC });
        code.insert(code.end(), { 0x0F, 0x83, 0x00, 0x00, 0x00, 0x00 });
        const std::size_t nearZeroJump = code.size() - 4;
        code.insert(code.end(), { 0xF3, 0x0F, 0x5F, 0xDC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xDE });

        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x4C });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xC3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x4C });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x50 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xCB });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x50 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x10, 0x54, 0x24, 0x54 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xD3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x54 });

        const std::size_t skipOffset = code.size();
        patch_rel32(code, origMagZeroJump, skipOffset);
        patch_rel32(code, nearZeroJump, skipOffset);

        code.insert(code.end(), { 0x4C, 0x8D, 0x44, 0x24, 0x40 });
        code.push_back(0xE9);
        const std::size_t activeJumpOffset = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        const std::size_t inactiveOffset = code.size();
        for (const std::size_t jumpOffset : inactiveJumpOffsets)
            patch_rel32(code, jumpOffset, inactiveOffset);

        code.insert(code.end(), { 0x4C, 0x8B, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00 });

        const std::size_t callOriginalOffset = code.size();
        patch_rel32(code, activeJumpOffset, callOriginalOffset);

        code.insert(code.end(), { 0x48, 0x8B, 0x84, 0x24, 0xD0, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x48, 0x89, 0x44, 0x24, 0x20 });
        code.insert(code.end(), { 0x48, 0x8B, 0x84, 0x24, 0xCC, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x48, 0x89, 0x44, 0x24, 0x28 });
        code.insert(code.end(), { 0x48, 0xB8 });
        append_u64(code, originalFunction);
        code.insert(code.end(), { 0xFF, 0xD0 });
        code.insert(code.end(), { 0x48, 0x81, 0xC4, 0xA0, 0x00, 0x00, 0x00 });
        code.push_back(0xC3);
        return code;
    }

    std::vector<std::uint8_t> make_hook_thunk(std::uintptr_t stateAddress, std::uintptr_t originalFunction) {
        std::vector<std::uint8_t> code;
        code.reserve(384);
        std::vector<std::size_t> inactiveJumpOffsets;

        auto emitInactiveJump = [&]() {
            code.insert(code.end(), { 0x0F, 0x84 });
            inactiveJumpOffsets.push_back(code.size());
            code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });
            };

        code.insert(code.end(), { 0x48, 0x83, 0xEC, 0x68 });
        code.insert(code.end(), { 0x49, 0xBA });
        append_u64(code, stateAddress);
        code.insert(code.end(), { 0x41, 0x83, 0x3A, 0x00 });
        emitInactiveJump();
        code.insert(code.end(), { 0x4D, 0x85, 0xC0 });
        emitInactiveJump();
        code.insert(code.end(), { 0x4D, 0x85, 0xC9 });
        emitInactiveJump();

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x42, 0x08 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x00 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x40 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x4A, 0x0C });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x48, 0x04 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x44 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x52, 0x10 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x50, 0x08 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x48 });

        code.insert(code.end(), { 0x0F, 0x28, 0xD8 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xDB });
        code.insert(code.end(), { 0x0F, 0x28, 0xE1 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0x0F, 0x28, 0xE2 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x51, 0xDB });
        code.insert(code.end(), { 0x0F, 0x57, 0xED });
        code.insert(code.end(), { 0x0F, 0x2E, 0xDD });
        code.insert(code.end(), { 0x0F, 0x86 });
        inactiveJumpOffsets.push_back(code.size());
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x21 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x04 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xED });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x08 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xED });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x51, 0xE4 });
        code.insert(code.end(), { 0x0F, 0x57, 0xED });
        code.insert(code.end(), { 0x0F, 0x2E, 0xE5 });
        code.insert(code.end(), { 0x0F, 0x86 });
        inactiveJumpOffsets.push_back(code.size());
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        code.insert(code.end(), { 0x41, 0x8B, 0x42, 0x04 });
        code.insert(code.end(), { 0xA8, 0x04 });
        code.insert(code.end(), { 0x0F, 0x84 });
        const std::size_t skipShortGuardJumpOffset = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0xB8, 0x00, 0x00, 0x80, 0x7F });
        code.insert(code.end(), { 0x66, 0x0F, 0x6E, 0xE8 });
        code.insert(code.end(), { 0x0F, 0x2E, 0xE5 });
        code.insert(code.end(), { 0x0F, 0x82 });
        inactiveJumpOffsets.push_back(code.size());
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });
        const std::size_t skipShortGuardOffset = code.size();
        patch_rel32(code, skipShortGuardJumpOffset, skipShortGuardOffset);

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x29 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE8 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x71, 0x04 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xF1 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xEE });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x71, 0x08 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xF2 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xEE });
        code.insert(code.end(), { 0x0F, 0x28, 0xF4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xF3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xEE });
        code.insert(code.end(), { 0xB8, 0x00, 0x00, 0x80, 0xBF });
        code.insert(code.end(), { 0x66, 0x0F, 0x6E, 0xF0 });
        code.insert(code.end(), { 0x0F, 0x2E, 0xEE });
        code.insert(code.end(), { 0x0F, 0x82 });
        inactiveJumpOffsets.push_back(code.size());
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        code.insert(code.end(), { 0x41, 0x8B, 0x42, 0x04 });
        code.insert(code.end(), { 0xA8, 0x02 });
        code.insert(code.end(), { 0x0F, 0x84 });
        const std::size_t noTerrainGuardJumpOffset = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x0F, 0x28, 0xEC });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x6A, 0x14 });
        code.insert(code.end(), { 0x0F, 0x2E, 0xEB });
        code.insert(code.end(), { 0x0F, 0x82 });
        inactiveJumpOffsets.push_back(code.size());
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        const std::size_t noTerrainGuardOffset = code.size();
        patch_rel32(code, noTerrainGuardJumpOffset, noTerrainGuardOffset);

        code.insert(code.end(), { 0xA8, 0x01 });
        code.insert(code.end(), { 0x0F, 0x85 });
        const std::size_t wallbangJumpOffset = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        code.insert(code.end(), { 0x0F, 0x28, 0xEB });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x6A, 0x14 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5F, 0xE5 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xE3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xC4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x40 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xCC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x44 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xD4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x48 });
        code.push_back(0xE9);
        const std::size_t applyDirectionJumpOffset = code.size();
        code.insert(code.end(), { 0x00, 0x00, 0x00, 0x00 });

        const std::size_t wallbangOffset = code.size();
        patch_rel32(code, wallbangJumpOffset, wallbangOffset);
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xC3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xCB });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xD3 });

        code.insert(code.end(), { 0x0F, 0x28, 0xE0 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x08 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x50 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x40 });

        code.insert(code.end(), { 0x0F, 0x28, 0xE1 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x0C });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x54 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x44 });

        code.insert(code.end(), { 0x0F, 0x28, 0xE2 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x10 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x58 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x48 });
        code.insert(code.end(), { 0x4C, 0x8D, 0x44, 0x24, 0x50 });

        const std::size_t applyDirectionOffset = code.size();
        patch_rel32(code, applyDirectionJumpOffset, applyDirectionOffset);
        code.insert(code.end(), { 0x49, 0xFF, 0x42, 0x18 });
        code.insert(code.end(), { 0x4C, 0x8D, 0x4C, 0x24, 0x40 });

        const std::size_t inactiveOffset = code.size();
        for (const std::size_t jumpOffset : inactiveJumpOffsets)
            patch_rel32(code, jumpOffset, inactiveOffset);

        code.insert(code.end(), { 0x48, 0x8B, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00 });
        code.insert(code.end(), { 0x48, 0x89, 0x44, 0x24, 0x20 });
        code.insert(code.end(), { 0x48, 0xB8 });
        append_u64(code, originalFunction);
        code.insert(code.end(), { 0xFF, 0xD0 });
        code.insert(code.end(), { 0x48, 0x83, 0xC4, 0x68 });
        code.push_back(0xC3);
        return code;
    }

    std::vector<std::uint8_t> make_direct_hook_thunk(
        std::uintptr_t stateAddress, std::uintptr_t originalFunction) {
        std::vector<std::uint8_t> code;
        code.reserve(256);
        std::vector<std::size_t> inactiveJumps;

        auto emitInactive = [&]() {
            code.insert(code.end(), { 0x0F, 0x84 });
            inactiveJumps.push_back(code.size());
            code.insert(code.end(), { 0, 0, 0, 0 });
        };

        code.insert(code.end(), { 0x49, 0xBA });
        append_u64(code, stateAddress);
        code.insert(code.end(), { 0x49, 0xFF, 0x42, 0x30 });
        code.insert(code.end(), { 0x41, 0x83, 0x3A, 0x00 });
        emitInactive();
        code.insert(code.end(), { 0x4D, 0x85, 0xC0 });
        emitInactive();
        code.insert(code.end(), { 0x4D, 0x85, 0xC9 });
        emitInactive();

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x42, 0x08 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x00 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x4A, 0x0C });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x48, 0x04 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x52, 0x10 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x50, 0x08 });

        code.insert(code.end(), { 0x41, 0x8B, 0x42, 0x04 });
        code.insert(code.end(), { 0xA8, 0x01 });
        code.insert(code.end(), { 0x0F, 0x85 });
        const std::size_t magicJump = code.size();
        code.insert(code.end(), { 0, 0, 0, 0 });

        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x42, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x01 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x4A, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x49, 0x04 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x52, 0x14 });
        code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x51, 0x08 });
        code.push_back(0xE9);
        const std::size_t appliedJump = code.size();
        code.insert(code.end(), { 0, 0, 0, 0 });

        const std::size_t magicOffset = code.size();
        patch_rel32(code, magicJump, magicOffset);

        code.insert(code.end(), { 0x0F, 0x28, 0xD8 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xDB });
        code.insert(code.end(), { 0x0F, 0x28, 0xE1 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0x0F, 0x28, 0xE2 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xDC });
        code.insert(code.end(), { 0xF3, 0x0F, 0x51, 0xDB });
        code.insert(code.end(), { 0x0F, 0x57, 0xE4 });
        code.insert(code.end(), { 0x0F, 0x2E, 0xDC });
        code.insert(code.end(), { 0x0F, 0x86 });
        inactiveJumps.push_back(code.size());
        code.insert(code.end(), { 0, 0, 0, 0 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xC3 });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xCB });
        code.insert(code.end(), { 0xF3, 0x0F, 0x5E, 0xD3 });

        auto emitMagicAxis = [&](std::uint8_t targetOffset,
                                 std::initializer_list<std::uint8_t> unitToXmm4,
                                 std::initializer_list<std::uint8_t> originStore,
                                 std::initializer_list<std::uint8_t> directionStore) {
            code.insert(code.end(), unitToXmm4);
            code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
            code.insert(code.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, targetOffset });
            code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xEC });
            code.insert(code.end(), originStore);
            code.insert(code.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
            code.insert(code.end(), { 0x0F, 0x57, 0xED });
            code.insert(code.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
            code.insert(code.end(), directionStore);
        };
        emitMagicAxis(0x08, { 0x0F, 0x28, 0xE0 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x28 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x29 });
        emitMagicAxis(0x0C, { 0x0F, 0x28, 0xE1 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x68, 0x04 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x69, 0x04 });
        emitMagicAxis(0x10, { 0x0F, 0x28, 0xE2 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x68, 0x08 },
            { 0xF3, 0x41, 0x0F, 0x11, 0x69, 0x08 });

        const std::size_t appliedOffset = code.size();
        patch_rel32(code, appliedJump, appliedOffset);
        code.insert(code.end(), { 0x49, 0xFF, 0x42, 0x18 });

        const std::size_t inactiveOffset = code.size();
        for (const std::size_t jump : inactiveJumps)
            patch_rel32(code, jump, inactiveOffset);

        code.insert(code.end(), { 0xFF, 0x25, 0, 0, 0, 0 });
        append_u64(code, originalFunction);
        return code;
    }

    void RecoverPreviousDirect(std::uintptr_t slot) {
        const auto shape=make_direct_hook_thunk(0,0);
        for(int layer=0;layer<16;++layer){
            const auto current=g_Memory.Read<std::uintptr_t>(slot);
            if(!addr_ok(current))return;
            std::vector<std::uint8_t> bytes(shape.size());
            SIZE_T read=0;
            if(!ReadProcessMemory(memory->GetHandle(),reinterpret_cast<void*>(current),bytes.data(),bytes.size(),&read)||read!=bytes.size())return;
            std::uintptr_t state=0,original=0;
            std::memcpy(&state,bytes.data()+2,8);
            std::memcpy(&original,bytes.data()+bytes.size()-8,8);
            if(!addr_ok(state)||!addr_ok(original)||original==current)return;
            if(bytes!=make_direct_hook_thunk(state,original))return;
            const std::uint32_t off=0;
            if(!w_mem(state,&off,sizeof(off)))return;
            if(!write_protected(slot,&original,sizeof(original)))return;
        }
    }

    bool region_is_padding(std::uintptr_t a, std::size_t n)
                {
                    std::vector<std::uint8_t> buf(n);
                    if (g_Memory.ReadRaw(a, buf.data(), n) != n)
                    {
                        return false;
                    }

                    for (auto b : buf)
                    {
                        if (b != 0xCC && b != 0x00 && b != 0x90)
                        {
                            return false;
                        }
                    }
                    return true;
                }

                bool read_val(std::uintptr_t a, void* d, std::size_t s) {
                    return g_Memory.ReadRaw(a, d, s) == s;
                }

                std::uintptr_t find_cave_in_module(std::uintptr_t module_base, std::size_t need,
                                                  std::uintptr_t min_offset,
                                                  std::uintptr_t ignore)
                {
                    if (!addr_ok(module_base) || !need)
                    {
                        return 0;
                    }

                    IMAGE_DOS_HEADER dos{};
                    if (!read_val(module_base, &dos, sizeof(dos)) ||
                        dos.e_magic != IMAGE_DOS_SIGNATURE)
                    {
                        return 0;
                    }

                    IMAGE_NT_HEADERS64 nt{};
                    const std::uintptr_t nt_addr =
                        module_base + static_cast<std::uintptr_t>(dos.e_lfanew);
                    if (!read_val(nt_addr, &nt, sizeof(nt)) ||
                        nt.Signature != IMAGE_NT_SIGNATURE)
                    {
                        return 0;
                    }

                    const std::uintptr_t section_base =
                        nt_addr + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                        nt.FileHeader.SizeOfOptionalHeader;

                    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                    {
                        IMAGE_SECTION_HEADER section{};
                        if (!read_val(section_base +
                                          static_cast<std::uintptr_t>(i) * sizeof(section),
                                      &section, sizeof(section)))
                        {
                            break;
                        }

                        if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                        {
                            continue;
                        }

                        const std::uintptr_t section_off = section.VirtualAddress;
                        std::size_t section_size = section.Misc.VirtualSize
                            ? section.Misc.VirtualSize
                            : section.SizeOfRawData;
                        if (!section_off || section_size < need)
                        {
                            continue;
                        }

                        std::uintptr_t scan_off = section_off;
                        if (scan_off < min_offset)
                        {
                            scan_off = min_offset;
                        }

                        if (scan_off >= section_off + section_size)
                        {
                            continue;
                        }

                        const std::uintptr_t scan_start = module_base + scan_off;
                        const std::size_t scan_size = static_cast<std::size_t>(
                            section_off + section_size - scan_off);

                        std::vector<std::uint8_t> buf(scan_size);
                        if (!read_val(scan_start, buf.data(), buf.size()))
                        {
                            continue;
                        }

                        std::size_t run_start = 0;
                        std::size_t run_len = 0;
                        for (std::size_t j = 0; j < buf.size(); ++j)
                        {
                            const std::uint8_t b = buf[j];
                            if (b != 0x00 && b != 0xCC && b != 0x90)
                            {
                                run_len = 0;
                                run_start = j + 1;
                                continue;
                            }
                            ++run_len;
                            if (run_len < need)
                            {
                                continue;
                            }

                            std::uintptr_t cand = scan_start + run_start;
                            const std::uintptr_t aligned =
                                (cand + 0x0F) & ~static_cast<std::uintptr_t>(0x0F);
                            const std::size_t loss =
                                static_cast<std::size_t>(aligned - cand);
                            if (run_len < need + loss)
                            {
                                continue;
                            }

                            if (ignore && aligned == ignore)
                            {
                                continue;
                            }

                            if (g_hook.thunk && aligned == g_hook.thunk)
                            {
                                continue;
                            }

                            return aligned;
                        }
                    }
                    return 0;
                }

                std::uintptr_t find_exec_cave(std::size_t need, std::uintptr_t,
                                             std::uintptr_t ignore = 0)
                {
                    static const wchar_t* pref[] = {
                        L"winsta.dll",
                        L"win32u.dll",
                        L"uxtheme.dll",
                        L"dwmapi.dll",
                        L"msctf.dll",
                        L"TextInputFramework.dll",
                        L"CoreMessaging.dll",
                        L"user32.dll",
                    };

                    for (std::size_t mi = 0; mi < sizeof(pref) / sizeof(pref[0]); ++mi)
                    {
                        const wchar_t* name = pref[mi];
                        const std::uintptr_t mod = g_Memory.GetModuleBase(name);
                        if (!mod)
                        {
                            continue;
                        }

                        const std::uintptr_t min_off = (mi == 0) ? 0x2000u : 0x1000u;
                        const std::uintptr_t cave =
                            find_cave_in_module(mod, need, min_off, ignore);
                        if (!cave)
                        {
                            continue;
                        }

                        (void)name;
                        return cave;
                    }

                    MEMORY_BASIC_INFORMATION mbi{};
                    std::uintptr_t addr = 0;
                    std::uintptr_t fallback_rwx = 0;

                    while (VirtualQueryEx(g_Memory.GetHandle(),
                        reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)))
                    {
                        const auto base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
                        const auto size = static_cast<std::size_t>(mbi.RegionSize);
                        addr = base + size;
                        if (addr < base)
                        {
                            break;
                        }

                        if (mbi.State != MEM_COMMIT)
                        {
                            continue;
                        }

                        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                        {
                            continue;
                        }

                        if (!is_executable_protect(mbi.Protect))
                        {
                            continue;
                        }

                        if (size < need)
                        {
                            continue;
                        }

                        if (g_hook.thunk && base <= g_hook.thunk && g_hook.thunk < addr)
                        {
                            continue;
                        }

                        for (std::size_t off = 0; off + need <= size; off += 0x10)
                        {
                            const std::uintptr_t cand = base + off;
                            if (ignore && cand == ignore)
                            {
                                continue;
                            }

                            if (region_is_padding(cand, need))
                            {
                                return cand;
                            }
                        }

                        if (!fallback_rwx && mbi.Type == MEM_PRIVATE &&
                            (mbi.Protect & 0xFF) == PAGE_EXECUTE_READWRITE &&
                            size >= need + 0x40)
                        {
                            const std::uintptr_t cand = base + size - need;
                            if (!ignore || cand != ignore)
                            {
                                fallback_rwx = cand;
                            }
                        }
                    }

                    return fallback_rwx;
                }

                std::uintptr_t alloc_exec_page() {
                    const std::uintptr_t p = g_Memory.Alloc(page_sz(), PAGE_EXECUTE_READWRITE);
                    if (!p) return 0;

                    const DWORD prot = query_protect(p);
                    if (!is_executable_protect(prot)) {
                        g_Memory.Free(p);
                        return 0;
                    }
                    return p;
                }

                bool descriptor_has_name(std::uintptr_t base, std::uintptr_t rva,
                                         const char* expected) {
                    if (!base || !rva || !expected)
                        return false;
                    const std::uintptr_t name_ptr = g_Memory.Read<std::uintptr_t>(base + rva + 0x8);
                    if (!addr_ok(name_ptr))
                        return false;
                    char name[64]{};
                    if (g_Memory.ReadRaw(name_ptr, name, sizeof(name) - 1) == 0)
                        return false;
                    return std::strcmp(name, expected) == 0;
                }

                std::uintptr_t find_descriptor(std::uintptr_t base, const char* name,
                                               std::uintptr_t preferred) {
                    if (descriptor_has_name(base, preferred, name))
                        return preferred;
                    constexpr std::uintptr_t radius = 0x20000;
                    const std::uintptr_t lo = desc_rva_z > radius ? desc_rva_z - radius : 0;
                    const std::uintptr_t hi = desc_rva_z + radius;
                    for (std::uintptr_t rva = (lo + 0xF) & ~std::uintptr_t(0xF);
                         rva < hi; rva += 0x10) {
                        if (!descriptor_has_name(base, rva, name))
                            continue;
                        const std::uintptr_t fn = g_Memory.Read<std::uintptr_t>(
                            base + rva + bound_fn_offset);
                        if (addr_ok(fn))
                            return rva;
                    }
                    return 0;
                }

                bool install_legacy() {
                    if (g_legacy.installed)
                        return true;
                    const auto now = std::chrono::steady_clock::now();
                    if (g_lastLegacyFail.time_since_epoch().count() != 0 &&
                        now - g_lastLegacyFail < std::chrono::milliseconds(1500))
                        return false;

                    const std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                    if (!base)
                        return false;
                    const std::uintptr_t rva = find_descriptor(
                        base, "FindPartOnRay", Offsets::WorldRoot::FindPartOnRayBoundDesc);
                    if (!rva) {
                        g_lastLegacyFail = now;
                        return false;
                    }
                    const std::uintptr_t slot = base + rva + bound_fn_offset;
                    const std::uintptr_t fn = g_Memory.Read<std::uintptr_t>(slot);
                    if (!addr_ok(fn)) {
                        g_lastLegacyFail = now;
                        return false;
                    }
                    g_legacy.state = g_Memory.Alloc(page_sz(), PAGE_READWRITE);
                    if (!g_legacy.state) {
                        g_lastLegacyFail = now;
                        return false;
                    }
                    const auto thunk = make_legacy_thunk(g_legacy.state, fn);
                    if (thunk.empty() || thunk.size() > 0x300) {
                        g_Memory.Free(g_legacy.state);
                        g_legacy.state = 0;
                        g_lastLegacyFail = now;
                        return false;
                    }

                    bool owned = false;
                    std::uintptr_t stub = find_exec_cave(0x300, base, g_hook.thunk);
                    if (!stub) {
                        stub = alloc_exec_page();
                        owned = stub != 0;
                    }
                    RaycastState empty{};
                    if (!stub || !write_protected(stub, thunk.data(), thunk.size()) ||
                        !w_mem(g_legacy.state, &empty, sizeof(empty))) {
                        if (owned && stub)
                            g_Memory.Free(stub);
                        g_Memory.Free(g_legacy.state);
                        g_legacy = {};
                        g_lastLegacyFail = now;
                        return false;
                    }
                    FlushInstructionCache(g_Memory.GetHandle(), reinterpret_cast<void*>(stub), thunk.size());
                    mark_cfg(stub);
                    if (!is_executable_protect(query_protect(stub)) ||
                        !write_protected(slot, &stub, sizeof(stub)) ||
                        g_Memory.Read<std::uintptr_t>(slot) != stub) {
                        if (owned)
                            g_Memory.Free(stub);
                        g_Memory.Free(g_legacy.state);
                        g_legacy = {};
                        g_lastLegacyFail = now;
                        return false;
                    }
                    g_legacy.thunk = stub;
                    g_legacy.originalFunction = fn;
                    g_legacy.module_base = base;
                    g_legacy.descriptor_rva = rva;
                    g_legacy.thunk_owned = owned;
                    g_legacy.installed = true;
                    g_lastLegacyFail = {};
                    std::printf("[Silent] legacy ready handler=0x%llx stub=0x%llx descriptor=0x%llx\n",
                        static_cast<unsigned long long>(fn),
                        static_cast<unsigned long long>(stub),
                        static_cast<unsigned long long>(rva));
                    return true;
                }

                void remove_hook(Hook& hook) {
                    if (hook.state) {
                        const std::uint32_t inactive = 0;
                        w_mem(hook.state, &inactive, sizeof(inactive));
                    }
                    if (hook.installed && addr_ok(hook.originalFunction) &&
                        hook.module_base && hook.descriptor_rva) {
                        const std::uintptr_t slot = hook.module_base +
                            hook.descriptor_rva + bound_fn_offset;
                        if (g_Memory.Read<std::uintptr_t>(slot) == hook.thunk)
                            write_protected(slot, &hook.originalFunction, sizeof(hook.originalFunction));
                    }

                    hook.installed = false;
                    hook.active = false;
                    hook.thunk_owned = false;
                }

                void report_inject(bool ok, std::uintptr_t orig = 0, std::uintptr_t stub = 0,
                                   const char* detail = nullptr) {
                    const std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                    const std::uintptr_t slot = base ? (base + desc_rva_z + bound_fn_offset) : 0;
                    Console::Clear();
                    Console::DumpWorld();
                    Console::DumpSilent(ok, orig, stub, g_hook.state, slot, detail);
                }

            }

            bool Ready() { return g_hook.installed; }
            bool Aiming() { return g_hook.active; }
            bool WallbangMode() { return g_wallbang; }
            std::uintptr_t OriginalHandler() { return g_hook.originalFunction; }
            std::uint64_t WorldCalls() {
                return g_hook.state ? g_Memory.Read<std::uint64_t>(
                    g_hook.state + offsetof(RaycastState, calls)) : 0;
            }
            std::uint64_t LegacyCalls() {
                return g_legacy.state ? g_Memory.Read<std::uint64_t>(
                    g_legacy.state + offsetof(RaycastState, calls)) : 0;
            }
            std::uint64_t HandlerCalls() {
                return g_hook.state ? g_Memory.Read<std::uint64_t>(
                    g_hook.state + offsetof(RaycastState, handler_calls)) : 0;
            }

            bool Install()
            {
                if (g_hook.installed)
                {
                    return true;
                }

                std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                if (!base)
                {
                    return false;
                }

                auto now = std::chrono::steady_clock::now();
                if (g_lastFail.time_since_epoch().count() != 0 &&
                    now - g_lastFail < std::chrono::milliseconds(1500))
                {
                    return false;
                }

                std::uintptr_t slot = base + desc_rva_z + bound_fn_offset;
                RecoverPreviousDirect(slot);
                std::uintptr_t fn   = g_Memory.Read<std::uintptr_t>(slot);

                if (!addr_ok(fn))
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "bad handler");
                    return false;
                }

                if (!g_hook.state)
                {
                    g_hook.state = g_Memory.Alloc(page_sz(), PAGE_READWRITE);
                }

                if (!g_hook.state)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "state alloc");
                    return false;
                }

                if (g_hook.thunk && g_hook.originalFunction == fn &&
                    addr_ok(g_hook.thunk))
                {
                    protect_remote(slot, 8, PAGE_READWRITE, nullptr);
                    if (write_protected(slot, &g_hook.thunk, sizeof(g_hook.thunk)) &&
                        g_Memory.Read<std::uintptr_t>(slot) == g_hook.thunk)
                    {
                        g_hook.module_base      = base;
                        g_hook.descriptor_rva   = desc_rva_z;
                        g_hook.installed        = true;
                        g_hook.active           = false;
                        g_lastFail              = {};
                        return true;
                    }
                }

                auto thunk = make_direct_hook_thunk(g_hook.state, fn);

                if (thunk.size() > 0x300)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "stub too large");
                    return false;
                }

                bool owned = false;
                std::uintptr_t stub = 0;
                std::uintptr_t ignore_cave = 0;

                for (int attempt = 0; attempt < 8 && !stub; ++attempt)
                {
                    std::uintptr_t cand = find_exec_cave(0x300, base, ignore_cave);
                    if (!cand)
                    {
                        break;
                    }

                    SetLastError(0);
                    DWORD old_prot = 0;
                    bool prot_ok =
                        protect_remote(cand, thunk.size(), PAGE_EXECUTE_READWRITE, &old_prot);

                    SetLastError(0);
                    if (!write_protected(cand, thunk.data(), thunk.size()))
                    {
                        if (prot_ok)
                        {
                            protect_remote(cand, thunk.size(), old_prot, nullptr);
                        }
                        ignore_cave = cand;
                        continue;
                    }

                    stub = cand;
                    owned = false;
                }

                if (!stub)
                {
                    stub = alloc_exec_page();
                    owned = stub != 0;
                    if (stub)
                    {
                        SetLastError(0);
                        if (!write_protected(stub, thunk.data(), thunk.size()))
                        {
                            g_Memory.Free(stub);
                            stub = 0;
                            owned = false;
                        }
                    }
                }

                if (!stub)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "no host");
                    return false;
                }

                RaycastState empty{};
                SetLastError(0);
                if (!w_mem(g_hook.state, &empty, sizeof(empty)))
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "state write");
                    return false;
                }

                FlushInstructionCache(g_Memory.GetHandle(), (void*)stub, thunk.size());
                mark_cfg(stub);

                DWORD prot = query_protect(stub);
                if (!is_executable_protect(prot))
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "stub not executable");
                    return false;
                }

                protect_remote(slot, 8, PAGE_READWRITE, nullptr);
                if (!write_protected(slot, &stub, sizeof(stub)) ||
                    g_Memory.Read<std::uintptr_t>(slot) != stub)
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "slot write");
                    return false;
                }

                g_hook.module_base      = base;
                g_hook.descriptor_rva   = desc_rva_z;
                g_hook.originalFunction = fn;
                g_hook.thunk            = stub;
                g_hook.thunk_owned      = owned;
                g_hook.installed        = true;
                g_hook.active           = false;
                report_inject(true, fn, stub);
                return true;
            }

            void Remove()
            {
                remove_hook(g_legacy);
                remove_hook(g_hook);
                g_wallbang = false;
            }

            void Ensure(bool want)
            {
                std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                static std::uintptr_t s_last_base = 0;
                static DWORD recoveredPid=0;
                if(base && recoveredPid!=memory->get_process_id()){
                    RecoverPreviousDirect(base+desc_rva_z+bound_fn_offset);
                    recoveredPid=memory->get_process_id();
                }

                if (g_hook.installed && base && s_last_base && base != s_last_base)
                {
                    Remove();

                    g_hook.thunk = 0;
                    g_hook.state = 0;
                    g_legacy.thunk = 0;
                    g_legacy.state = 0;
                    Console::Clear();
                    Console::DumpWorld();
                    Console::Log(Console::Color::Orange, "Silent rescan  module");
                }

                if (base)
                {
                    s_last_base = base;
                }

                if (want)
                {
                    if (!base)
                    {
                        return;
                    }

                    if (g_hook.installed && g_hook.thunk)
                    {
                        std::uintptr_t slot = base + desc_rva_z + bound_fn_offset;
                        std::uintptr_t cur = g_Memory.Read<std::uintptr_t>(slot);
                        if (cur != g_hook.thunk)
                        {
                            g_hook.installed = false;
                        }
                    }

                    if (!g_hook.installed)
                    {
                        Install();
                    }
                }

                else if (g_hook.installed || g_legacy.installed)
                {
                    Remove();
                    Console::Clear();
                    Console::DumpWorld();
                    Console::Log(Console::Color::Yellow, "Silent removed");
                }
            }

            void SetActive(bool on, const Vector3& world_target, bool wallbang)
            {
                if (!on)
                {
                    const std::uint32_t off=0;
                    for(Hook* hook : {&g_hook,&g_legacy}){
                        if(hook->state)w_mem(hook->state,&off,sizeof(off));
                        hook->active=false;
                    }
                    g_wallbang = false;
                    return;
                }

                if (!g_hook.installed)
                {
                    return;
                }

                float pos[3]{ world_target.x, world_target.y, world_target.z };
                std::uint32_t flags = 0x4u;
                if (wallbang)
                {
                    flags |= 1u;
                }
                float scale = 1.15f;
                std::uint32_t one = 1;

                float cam[3]{ 0.f, 0.f, 0.f };
                if (::Globals::camera.Addr)
                {
                    const RBX::Vec3 cp = ::Globals::camera.GetCameraCFrame().GetPosition();
                    cam[0] = cp.X;
                    cam[1] = cp.Y;
                    cam[2] = cp.Z;
                }

                auto activate = [&](Hook& hook) {
                    if (!hook.installed || !hook.state)
                        return;
                    w_mem(hook.state + offsetof(RaycastState, reserved), &flags, sizeof(flags));
                    w_mem(hook.state + offsetof(RaycastState, target_x), pos, sizeof(pos));
                    w_mem(hook.state + offsetof(RaycastState, scale), &scale, sizeof(scale));
                    w_mem(hook.state + offsetof(RaycastState, cam_x), cam, sizeof(cam));
                    w_mem(hook.state + offsetof(RaycastState, active), &one, sizeof(one));
                    hook.active = true;
                };
                activate(g_hook);
                g_wallbang = wallbang;
            }

        }
    }
}

#undef g_Memory
