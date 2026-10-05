#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "settings.h"
#include "../../variables/variables.h"
#include "../../../render/menu/library.h"
#include <algorithm>

namespace Settings {
void RenderAimMain() {
    float ay = 46.0f;
    imGuiCustom::Checkbox("Silent Aim", &variables::Aimbot::silentEnabled, ImVec2(12.0f, ay));
    imGuiCustom::Keybind("silent_key", &variables::Aimbot::silentKey,
        ImVec2(222.0f, ay - 1.0f), ImVec2(68.0f, 13.0f), nullptr, false);
    ay += imGuiCustom::CheckStep();
    int keyMode = variables::Aimbot::silentToggleKey ? 1 : 0;
    const char* keyModes[] = {"Hold key", "Toggle key"};
    imGuiCustom::Combo("silent_key_mode", &keyMode, keyModes, 2,
        ImVec2(12.0f, ay + imGuiCustom::ComboTop()), 158.0f, "Key Mode:");
    variables::Aimbot::silentToggleKey = keyMode == 1;
    ay += imGuiCustom::ComboTop() + 22.0f;
    const char* silentMethods[] = {"Silent Aim", "Magic Bullet"};
    int silentMethodUi = (std::max)(0, (std::min)(1, variables::Aimbot::silentMethod - 1));
    imGuiCustom::Combo("silent_method", &silentMethodUi, silentMethods, 2, ImVec2(12.0f, ay + imGuiCustom::ComboTop()), 158.0f, "Method:");
    variables::Aimbot::silentMethod = silentMethodUi + 1;
    ay += imGuiCustom::ComboTop() + 22.0f;
    const char* targets[] = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg", "HumanoidRootPart", "Closest"};
    imGuiCustom::Combo("aim_target", &variables::Aimbot::aimTarget, targets, 8, ImVec2(12.0f, ay + imGuiCustom::ComboTop()), 158.0f, "Target Part:");
    ay += imGuiCustom::ComboTop() + 22.0f;
    imGuiCustom::Checkbox("FOV Circle", &variables::Aimbot::silentShowFOV, ImVec2(12.0f, ay));
    imGuiCustom::ColorSquare("silent_fov_color", &variables::Aimbot::silentFovColor, ImVec2(264.0f, ay + 1.0f));
    ay += imGuiCustom::CheckStep();
    imGuiCustom::SliderFloat("silent_fov", &variables::Aimbot::silentFovRadius, 5.0f, 600.0f, ImVec2(12.0f, ay + imGuiCustom::SliderTop()), 272.0f, "FOV Radius", "%.0f");
    ay += imGuiCustom::SliderStep();
    imGuiCustom::Checkbox("Nearest Point", &variables::Aimbot::silentNearestPoint, ImVec2(12.0f, ay));
}

void RenderAimChecks() {
    float by = 46.0f;
    imGuiCustom::Checkbox("Dead Check", &variables::Aimbot::deadCheck, ImVec2(12.0f, by));
    by += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Team Check", &variables::Aimbot::teamCheck, ImVec2(12.0f, by));
    by += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Invisible Check", &variables::Aimbot::invisibleCheck, ImVec2(12.0f, by));
}
}
