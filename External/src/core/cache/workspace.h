#pragma once
#include "../../sdk/sdk.h"
#include <vector>
#include <thread>
#include <atomic>
#include "../globals/globals.h"
namespace WorkspaceCache {
inline bool IsVisible(const RBX::Vec3& camPos, const RBX::Vec3& targetPos) { (void)camPos; (void)targetPos; return true; }
inline void Loop() {
    while (Globals::running.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
}
