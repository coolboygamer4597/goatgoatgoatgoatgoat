#pragma once
#include <Windows.h>

namespace WindowIdentity {
const char* BuildId() noexcept;
void BeginSession() noexcept;
void Generate(wchar_t (&name)[64]) noexcept;
void RecordClass(const char* event, const char* meaning, const char* original,
    const wchar_t* name, DWORD error = ERROR_SUCCESS) noexcept;
}
