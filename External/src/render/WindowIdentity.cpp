#include "WindowIdentity.h"
#include <objbase.h>
#include <atomic>
#include <cstdio>
#pragma comment(lib, "ole32.lib")

namespace WindowIdentity {
void Generate(wchar_t (&name)[64]) noexcept {
    GUID id{};
    if (SUCCEEDED(CoCreateGuid(&id))) {
        swprintf_s(name, L"Wnd_%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
            static_cast<unsigned>(id.Data1), id.Data2, id.Data3,
            id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
            id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    } else {
        static std::atomic<unsigned long> sequence{0};
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        swprintf_s(name, L"Wnd_%08lx%016llx%08lx", GetCurrentProcessId(),
            static_cast<unsigned long long>(counter.QuadPart), ++sequence);
    }
}

}
