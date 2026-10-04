#pragma once
#include <atomic>
#include "../../../src/sdk/sdk.h"

namespace Globals {
inline RBX::RbxInstance dataModel;
inline RBX::RenderEngine renderEngine{0};
inline RBX::RbxInstance workspace;
inline RBX::RbxInstance players;
inline RBX::RbxInstance camera;
inline RBX::RbxInstance localPlayer;
inline std::atomic<bool> running{true};
}
