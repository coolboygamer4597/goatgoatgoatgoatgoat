#include "silent_fov_center.h"
#include "../../variables/variables.h"
#include "../../../sdk/w2s.h"
#include <algorithm>

namespace Aimbot {
RBX::Vec2 SilentAimCenter() {

    return {W2S::ScreenW() * 0.5f, W2S::ScreenH() * 0.5f};
}

float SilentAimRadius() {
    return (std::max)(5.0f, variables::Aimbot::silentFovRadius);
}
}
