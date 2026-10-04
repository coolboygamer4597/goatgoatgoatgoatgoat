#pragma once
#include <Windows.h>
#include <filesystem>

namespace RuntimePaths {
inline std::filesystem::path File(const wchar_t* name) {
    static const auto directory = [] {
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        return std::filesystem::path(executable).parent_path();
    }();
    return directory / name;
}
}
