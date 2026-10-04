#pragma once
#include <Windows.h>

namespace CaptureProtection {
inline bool Apply(HWND window, bool enabled) {
    if (!window) return false;
    if (SetWindowDisplayAffinity(window, enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE))
        return true;

    return enabled && SetWindowDisplayAffinity(window, WDA_MONITOR);
}
}
