#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "settings.h"
#include "../../variables/variables.h"
#include "../../../render/menu/library.h"
#include <algorithm>
#include "../aim/recoil.h"

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

void RenderRecoil() {
    imGuiCustom::Checkbox("Recoil dampening",&variables::Aimbot::recoilEnabled,ImVec2(12,46));
    static int profile=0;const char* weapons[]={"AR-15","Glock 19"};
    imGuiCustom::Combo("recoil_profile",&profile,weapons,2,ImVec2(12,94),272,"Weapon settings:");
    auto& strength=profile?variables::Aimbot::recoilGlockStrength:variables::Aimbot::recoilStrength;
    auto& pull=profile?variables::Aimbot::recoilGlockPull:variables::Aimbot::recoilPull;
    float percent=strength*100.f;
    imGuiCustom::SliderFloat("recoil_strength",&percent,0,100,ImVec2(12,150),272,"Strength","%.0f%%");
    strength=percent/100.f;
    imGuiCustom::SliderFloat("recoil_pull",&pull,0,profile?160.f:1200.f,ImVec2(12,206),272,profile?"Vertical pull per shot":"Vertical pull per second","%.0f");
    ImGui::SetCursorPos(ImVec2(12,265));ImGui::TextUnformatted("Switches automatically with your equipped weapon.");
    ImGui::SetCursorPos(ImVec2(12,290));ImGui::TextUnformatted("If aim moves down, lower strength or vertical pull.");
    ImGui::SetCursorPos(ImVec2(12,315));ImGui::TextUnformatted(profile?"Glock: one short pulse per click.":"AR-15: release shoot between bursts over 3.5 seconds.");
    ImGui::SetCursorPos(ImVec2(12,340));ImGui::TextUnformatted(Recoil::Status());
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
