#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "settings.h"
#include "../../variables/variables.h"
#include "../../../render/menu/library.h"
#include "../aim/fallen_prediction.h"
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Settings {
void RenderAimMenu() {
    RenderAimMain();
    RenderAimChecks();
}

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

void RenderVisualMenu() {

    ImVec2 vBase = ImGui::GetWindowPos();
    ImVec2 vLMin = vBase + ImVec2(6.0f,40.0f);
    ImVec2 vLMax = vBase + ImVec2(6.0f+290.0f,40.0f+340.0f);
    ImVec2 vMp = ImGui::GetIO().MousePos;
    bool vHover = (vMp.x>=vLMin.x && vMp.x<=vLMax.x && vMp.y>=vLMin.y && vMp.y<=vLMax.y) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    if (vHover) {
        float wh = ImGui::GetIO().MouseWheel;
        if (wh!=0.f && !imGuiCustom::PopupBlocking()) variables::ESP::visualScroll -= wh * 22.0f;
    }

    float vContent = 0;
    vContent += imGuiCustom::CheckStep()*2;
    if(variables::ESP::boxes){ vContent += imGuiCustom::ComboStep() + imGuiCustom::CheckStep()*2; if(variables::ESP::boxFilled) vContent += imGuiCustom::CheckStep() + (variables::ESP::boxFillGradient?imGuiCustom::CheckStep():0); }
    vContent += imGuiCustom::CheckStep()*4;
    vContent += imGuiCustom::CheckStep();
    if(variables::ESP::flags) vContent += imGuiCustom::ComboStep()+imGuiCustom::ComboTop();
    vContent += imGuiCustom::CheckStep();
    if(variables::ESP::headDot) vContent += imGuiCustom::SliderTop()+15.0f;
    vContent += imGuiCustom::CheckStep();
    if(variables::ESP::viewDirection) vContent += imGuiCustom::SliderTop()+15.0f;
    vContent += imGuiCustom::CheckStep();
    if(variables::ESP::skeleton) vContent += imGuiCustom::SliderTop()+15.0f;
    vContent += 10;
    float vMaxScroll = vContent - 340.0f + 6.0f; if(vMaxScroll<0) vMaxScroll=0;
    if(variables::ESP::visualScroll<0) variables::ESP::visualScroll=0;
    if(variables::ESP::visualScroll>vMaxScroll) variables::ESP::visualScroll=vMaxScroll;
    ImDrawList* vFg = ImGui::GetWindowDrawList();
    vFg->PushClipRect(vLMin, vLMax, true);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    float ly = 46.0f - variables::ESP::visualScroll;
    imGuiCustom::Checkbox("Enable ESP", &variables::ESP::enabled, ImVec2(12.0f, ly));
    ly += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Boxes", &variables::ESP::boxes, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("box_color", &variables::ESP::boxColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
        if (variables::ESP::boxes) {
            const char* boxModes[] = {"Static", "Dynamic"};
            imGuiCustom::Combo("box_mode", &variables::ESP::boxMode, boxModes, 2, ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 158.0f, "Box Mode:");
            ly += imGuiCustom::ComboTop() + 22.0f;
        imGuiCustom::Checkbox("Box Filled", &variables::ESP::boxFilled, ImVec2(12.0f, ly));
        imGuiCustom::ColorSquare("box_fill_color", &variables::ESP::boxFillColor, ImVec2(264.0f, ly + 1.0f));
        ly += imGuiCustom::CheckStep();
        if (variables::ESP::boxFilled) {
            imGuiCustom::Checkbox("Fill Gradient", &variables::ESP::boxFillGradient, ImVec2(12.0f, ly));
            if (variables::ESP::boxFillGradient)
                imGuiCustom::ColorSquare("box_fill_color2", &variables::ESP::boxFillColor2, ImVec2(264.0f, ly + 1.0f));
            ly += imGuiCustom::CheckStep();
        }
    }
    imGuiCustom::Checkbox("Names", &variables::ESP::names, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("name_color", &variables::ESP::nameColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Distance", &variables::ESP::distance, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("distance_color", &variables::ESP::distanceColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Health Bar", &variables::ESP::healthBar, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("health_color", &variables::ESP::healthColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Tool", &variables::ESP::tool, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("tool_color", &variables::ESP::toolColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Flags", &variables::ESP::flags, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("flags_color", &variables::ESP::flagsColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    if (variables::ESP::flags) {
        const char* flagItems[] = {"State", "Rig", "Health", "Tool", "Distance", "Velocity", "Team", "Role"};
        ly += imGuiCustom::ComboTop();
        imGuiCustom::MultiCombo("flags_sel", variables::ESP::flagSel, flagItems, 8, ImVec2(12.0f, ly), 158.0f, "Flags:");
        ly += imGuiCustom::ComboStep();
    }
    imGuiCustom::Checkbox("Head Dot", &variables::ESP::headDot, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("headdot_color", &variables::ESP::headDotColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    if (variables::ESP::headDot) {
        imGuiCustom::SliderFloat("headdot_size", &variables::ESP::headDotSize, 1.0f, 10.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Dot Size", "%.0f");
        ly += imGuiCustom::SliderTop() + 15.0f;
    }
    imGuiCustom::Checkbox("View Direction", &variables::ESP::viewDirection, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("viewdir_color", &variables::ESP::viewDirColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    if (variables::ESP::viewDirection) {
        imGuiCustom::SliderFloat("viewdir_len", &variables::ESP::viewDirLength, 1.0f, 30.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Length", "%.0f");
        ly += imGuiCustom::SliderTop() + 15.0f;
    }
    imGuiCustom::Checkbox("Skeleton", &variables::ESP::skeleton, ImVec2(12.0f, ly));
    imGuiCustom::ColorSquare("skeleton_color", &variables::ESP::skeletonColor, ImVec2(264.0f, ly + 1.0f));
    ly += imGuiCustom::CheckStep();
    if (variables::ESP::skeleton) {
        imGuiCustom::SliderFloat("skeleton_thick", &variables::ESP::skeletonThickness, 0.5f, 5.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Skeleton Thickness", "%.1f");
        ly += imGuiCustom::SliderTop() + 15.0f;
    }
    vFg->PopClipRect();
    ImGui::PopStyleVar();
    float sy = 46.0f;
    imGuiCustom::Checkbox("Dead Check", &variables::ESP::deadCheck, ImVec2(311.0f, sy));
    sy += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Local Player", &variables::ESP::localPlayer, ImVec2(311.0f, sy));
    sy += imGuiCustom::CheckStep();

    {
        ImVec2 base = ImGui::GetWindowPos();
        ImVec2 pMin = base + ImVec2(305.0f,133.0f);
        ImVec2 pMax = base + ImVec2(305.0f+290.0f,133.0f+247.0f);
        ImVec2 mp = ImGui::GetIO().MousePos;
        bool hoverPanel = (mp.x>=pMin.x && mp.x<=pMax.x && mp.y>=pMin.y && mp.y<=pMax.y) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        if (hoverPanel) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel!=0.f && !imGuiCustom::PopupBlocking()) {
                variables::World::worldScroll -= wheel * 22.0f;
            }
        }

        auto contentH = [&]()->float{
            float h=0; h+=imGuiCustom::CheckStep();
            if(variables::World::enabled){ h+=imGuiCustom::CheckStep()*2; }
            h+=imGuiCustom::CheckStep();
            if(variables::World::ores) h+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop();
            h+=imGuiCustom::CheckStep();
            if(variables::World::plants) h+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop();
            h+=imGuiCustom::CheckStep();
            if(variables::World::animals) h+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop()+imGuiCustom::CheckStep()*2;
            h+=imGuiCustom::CheckStep();
            if(variables::World::soldiers) h+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop()+imGuiCustom::CheckStep()*2;
            h+=imGuiCustom::CheckStep();
            if(variables::World::tools) h+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop()+imGuiCustom::CheckStep();
            return h + 10.0f;
        }();
        float maxScroll = contentH - 247.0f + 6.0f;
        if (maxScroll < 0) maxScroll = 0;
        if (variables::World::worldScroll < 0) variables::World::worldScroll = 0;
        if (variables::World::worldScroll > maxScroll) variables::World::worldScroll = maxScroll;
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
        ImDrawList* fg = ImGui::GetWindowDrawList();
        fg->PushClipRect(pMin, pMax, true);

        float wy = 139.0f - variables::World::worldScroll;

        ImGui::GetStyle().ScrollbarSize = 0.0f;
        imGuiCustom::Checkbox("Enable world##world_enabled", &variables::World::enabled, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::enabled) {
            imGuiCustom::Checkbox("Name##world_name", &variables::World::name, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
            imGuiCustom::Checkbox("Distance##world_distance", &variables::World::distance, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
        }
        imGuiCustom::Checkbox("Ores##world_ores", &variables::World::ores, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::ores) {
            const char* oreItems[] = {"Stone", "Phosphate", "Metal"};
            wy += imGuiCustom::ComboTop();
            imGuiCustom::MultiCombo("world_ores_sel##world_ores", variables::World::oresSel, oreItems, 3, ImVec2(311.0f, wy), 158.0f, "Ores:");
            wy += imGuiCustom::ComboStep();
        }
        imGuiCustom::Checkbox("Plants##world_plants_cb", &variables::World::plants, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::plants) {
            const char* plantItems[] = {"Wool Plant", "Blueberry Plant", "Raspberry Plant", "Lemon Plant", "Corn Plant", "Pumpkin Plant", "Tomato Plant"};
            wy += imGuiCustom::ComboTop();
            imGuiCustom::MultiCombo("world_plants_sel##world_plants", variables::World::plantsSel, plantItems, 7, ImVec2(311.0f, wy), 158.0f, "Plants:");
            wy += imGuiCustom::ComboStep();
        }
        imGuiCustom::Checkbox("Animals##world_animals", &variables::World::animals, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::animals) {
            const char* animalItems[] = {"Deer", "WildBoar", "Wolf"};
            wy += imGuiCustom::ComboTop();
            imGuiCustom::MultiCombo("world_animals_sel##world_animals", variables::World::animalsSel, animalItems, 3, ImVec2(311.0f, wy), 158.0f, "Animals:");
            wy += imGuiCustom::ComboStep();
            imGuiCustom::Checkbox("Box##world_animals_box", &variables::World::animalsBox, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
            imGuiCustom::Checkbox("Health##world_animals_health", &variables::World::animalsHealth, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
        }
        imGuiCustom::Checkbox("Soldiers##world_soldiers", &variables::World::soldiers, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::soldiers) {
            const char* soldierItems[] = {"Boris", "Bruno", "Brutus", "Soldier"};
            wy += imGuiCustom::ComboTop();
            imGuiCustom::MultiCombo("world_soldiers_sel##world_soldiers", variables::World::soldiersSel, soldierItems, 4, ImVec2(311.0f, wy), 158.0f, "Soldiers:");
            wy += imGuiCustom::ComboStep();
            imGuiCustom::Checkbox("Box##world_soldiers_box", &variables::World::soldiersBox, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
            imGuiCustom::Checkbox("Health##world_soldiers_health", &variables::World::soldiersHealth, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
        }
        imGuiCustom::Checkbox("Tools##world_tools", &variables::World::tools, ImVec2(311.0f, wy));
        wy += imGuiCustom::CheckStep();
        if (variables::World::tools) {
            const char* toolItems[] = {"Base Cabinet","Small Storage Box","Large Storage Box","Anvil","Furnace","Storage Cabinet","Sleeping Bag"};
            wy += imGuiCustom::ComboTop();
            imGuiCustom::MultiCombo("world_tools_sel##world_tools", variables::World::toolsSel, toolItems, 7, ImVec2(311.0f, wy), 158.0f, "Tools:");
            wy += imGuiCustom::ComboStep();
            imGuiCustom::Checkbox("3D Box##world_tools_box", &variables::World::toolsBox, ImVec2(311.0f, wy));
            wy += imGuiCustom::CheckStep();
        }
        fg->PopClipRect();
        ImGui::PopStyleVar();
    }
}

void RenderSettingsMenu() {
    float gy = 46.0f;
    constexpr bool showLegacySettings = false;
    if constexpr (showLegacySettings) {
        imGuiCustom::Checkbox("VSync", &variables::Misc::vsync, ImVec2(12.0f, gy));
        gy += imGuiCustom::CheckStep();
        float fps = (float)variables::Misc::fpsLimit;
        imGuiCustom::SliderFloat("fps_limit", &fps, 0.0f, 1000.0f, ImVec2(12.0f, gy + imGuiCustom::SliderTop()), 272.0f, fps < 0.5f ? "FPS Limit: Unlimited" : "FPS Limit", "%.0f");
        gy += imGuiCustom::SliderTop() + 15.0f;
        int snapped = (int)(fps + 0.5f);
        if (snapped > 0 && snapped < 60)
            snapped = 60;
        variables::Misc::fpsLimit = snapped;
        const char* priorities[] = {"Low", "Normal", "High", "Realtime"};
        gy += imGuiCustom::ComboTop();
        if (imGuiCustom::Combo("priority", &variables::Misc::priority, priorities, 4, ImVec2(12.0f, gy), 158.0f, "Priority:")) {
            DWORD cls = NORMAL_PRIORITY_CLASS;
            if (variables::Misc::priority == 0)
                cls = IDLE_PRIORITY_CLASS;
            else if (variables::Misc::priority == 2)
                cls = HIGH_PRIORITY_CLASS;
            else if (variables::Misc::priority == 3)
                cls = REALTIME_PRIORITY_CLASS;
            SetPriorityClass(GetCurrentProcess(), cls);
        }
        gy += imGuiCustom::ComboStep();
    }

    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 base = ImGui::GetWindowPos();
        ImFont* cfont = imGuiCustom::GetFonts().CascadiaMonoBL ? imGuiCustom::GetFonts().CascadiaMonoBL : ImGui::GetFont();
        float cfs = 12.0f * imGuiCustom::g_fontScale;
        cdl->AddText(cfont, cfs, ImVec2(base.x+12.0f, base.y+gy+2.0f), imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright), UiText::Tr("Config"));
        gy += 18.0f;
        static char cfgName[64] = "default";

        auto getCfgDir = []()->std::string{
            char exePath[MAX_PATH]{};
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string dir(exePath);
            const size_t s = dir.find_last_of("\\/");
            if (s != std::string::npos) dir = dir.substr(0, s);
            return dir + "\\configs";
        };
        static std::string cfgStatus;
        static double cfgStatusUntil = 0.0;
        static double lastScan = 0.0;
        auto setCfgStatus = [&](const std::string& s){ cfgStatus = s; cfgStatusUntil = ImGui::GetTime() + 3.0; };
        auto buildStr = []()->std::string{
            std::ostringstream o;
            o<<"Aimbot.enabled="<<(variables::Aimbot::enabled?1:0)<<"\n";
            o<<"Aimbot.fovRadius="<<variables::Aimbot::fovRadius<<"\n";
            o<<"Aimbot.smoothing="<<variables::Aimbot::smoothing<<"\n";
            o<<"Aimbot.aimTarget="<<variables::Aimbot::aimTarget<<"\n";
            o<<"Aimbot.aimMethod="<<variables::Aimbot::aimMethod<<"\n";
            o<<"Aimbot.showFOV="<<(variables::Aimbot::showFOV?1:0)<<"\n";
            o<<"Aimbot.visibleCheck="<<(variables::Aimbot::visibleCheck?1:0)<<"\n";
            o<<"Aimbot.prediction="<<(variables::Aimbot::prediction?1:0)<<"\n";
            o<<"Aimbot.aimbotKey="<<variables::Aimbot::aimbotKey<<"\n";
            o<<"Aimbot.silentEnabled="<<(variables::Aimbot::silentEnabled?1:0)<<"\n";
            o<<"Aimbot.silentMethod="<<variables::Aimbot::silentMethod<<"\n";
            o<<"Menu.key="<<variables::menuKey<<"\n";
            o<<"Aimbot.silentKey="<<variables::Aimbot::silentKey<<"\n";
            o<<"Aimbot.silentToggleKey="<<(variables::Aimbot::silentToggleKey?1:0)<<"\n";
            o<<"Aimbot.silentHitchanceEnabled="<<(variables::Aimbot::silentHitchanceEnabled?1:0)<<"\n";
            o<<"Aimbot.silentHitchance="<<variables::Aimbot::silentHitchance<<"\n";
            o<<"Aimbot.silentShowFOV="<<(variables::Aimbot::silentShowFOV?1:0)<<"\n";
            o<<"Aimbot.silentFovRadius="<<variables::Aimbot::silentFovRadius<<"\n";
            o<<"Aimbot.silentNearestPoint="<<(variables::Aimbot::silentNearestPoint?1:0)<<"\n";
            o<<"Aimbot.targetSelect="<<variables::Aimbot::targetSelect<<"\n";
            o<<"Aimbot.deadCheck="<<(variables::Aimbot::deadCheck?1:0)<<"\n";
            o<<"Aimbot.teamCheck="<<(variables::Aimbot::teamCheck?1:0)<<"\n";
            o<<"Aimbot.invisibleCheck="<<(variables::Aimbot::invisibleCheck?1:0)<<"\n";
            o<<"Aimbot.silentTracer="<<(variables::Aimbot::silentTracer?1:0)<<"\n";
            for(int i=0;i<4;++i) o<<"Aimbot.fovColor"<<i<<"="<<reinterpret_cast<float*>(&variables::Aimbot::fovColor)[i]<<"\n";
            for(int i=0;i<4;++i) o<<"Aimbot.silentFovColor"<<i<<"="<<reinterpret_cast<float*>(&variables::Aimbot::silentFovColor)[i]<<"\n";
            for(int i=0;i<4;++i) o<<"Aimbot.silentTracerColor"<<i<<"="<<reinterpret_cast<float*>(&variables::Aimbot::silentTracerColor)[i]<<"\n";
            o<<"Aimbot.fallen_prediction="<<(variables::Aimbot::fallen_prediction?1:0)<<"\n";
            o<<"Aimbot.selected_weapon_index="<<variables::Aimbot::selected_weapon_index<<"\n";
            o<<"Aimbot.fallen_bv_override="<<variables::Aimbot::fallen_bv_override<<"\n";
            o<<"Aimbot.includeNPC="<<(variables::Aimbot::includeNPC?1:0)<<"\n";
            o<<"Aimbot.predictionLine="<<(variables::Aimbot::predictionLine?1:0)<<"\n";
            o<<"ESP.enabled="<<(variables::ESP::enabled?1:0)<<"\n";
            o<<"ESP.boxes="<<(variables::ESP::boxes?1:0)<<"\n";
            o<<"ESP.names="<<(variables::ESP::names?1:0)<<"\n";
            o<<"ESP.distance="<<(variables::ESP::distance?1:0)<<"\n";
            o<<"ESP.healthBar="<<(variables::ESP::healthBar?1:0)<<"\n";
            o<<"ESP.skeleton="<<(variables::ESP::skeleton?1:0)<<"\n";
            o<<"ESP.meshChams="<<(variables::ESP::meshChams?1:0)<<"\n";
            o<<"ESP.meshChamsLocal="<<(variables::ESP::meshChamsLocal?1:0)<<"\n";
            o<<"ESP.meshChamsOpacity="<<variables::ESP::meshChamsOpacity<<"\n";
            o<<"ESP.meshChamsOcclusion="<<(variables::ESP::meshChamsOcclusion?1:0)<<"\n";
            o<<"ESP.meshChamsUnionWalls="<<(variables::ESP::meshChamsUnionWalls?1:0)<<"\n";
            o<<"ESP.meshTeamCheck="<<(variables::ESP::meshTeamCheck?1:0)<<"\n";
            o<<"ESP.meshDeadCheck="<<(variables::ESP::meshDeadCheck?1:0)<<"\n";
            o<<"ESP.meshInvisibleCheck="<<(variables::ESP::meshInvisibleCheck?1:0)<<"\n";
            o<<"ESP.meshTransparencyCheck="<<(variables::ESP::meshTransparencyCheck?1:0)<<"\n";
            o<<"ESP.meshTransparencyMin="<<variables::ESP::meshTransparencyMin<<"\n";
            o<<"ESP.meshTransparencyMax="<<variables::ESP::meshTransparencyMax<<"\n";
            o<<"ESP.meshVisibleMode="<<variables::ESP::meshChamsDxMode<<"\n";
            o<<"ESP.meshOccludedMode="<<variables::ESP::meshChamsOccludedDxMode<<"\n";
            o<<"ESP.meshOutline="<<(variables::ESP::meshChamsOutline?1:0)<<"\n";
            o<<"ESP.meshOutlineStyle="<<variables::ESP::meshChamsOutlineStyle<<"\n";
            o<<"ESP.meshOutlineFade="<<variables::ESP::meshChamsOutlineFade<<"\n";

            o<<"ESP.nativeChams=0\n";
            o<<"ESP.visualKeybindEnabled="<<variables::ESP::visualKeybindEnabled<<"\n";
            o<<"ESP.visualKeybindKey="<<variables::ESP::visualKeybindKey<<"\n";
            o<<"ESP.visualKeybindMode="<<variables::ESP::visualKeybindMode<<"\n";
            o<<"ESP.visualKeybindTarget="<<variables::ESP::visualKeybindTarget<<"\n";
            o<<"ESP.nativeUnlimited="<<variables::ESP::nativeUnlimited<<"\n";
            o<<"ESP.meshUnlimited="<<variables::ESP::meshUnlimited<<"\n";
            o<<"ESP.nativeDistance="<<variables::ESP::nativeDistance<<"\n";
            o<<"ESP.meshDistance="<<variables::ESP::meshDistance<<"\n";
            o<<"Weapons.mesh="<<variables::Weapons::mesh<<"\n";
            o<<"Weapons.native="<<variables::Weapons::native<<"\n";
            o<<"Weapons.viewmodel="<<variables::Weapons::viewmodel<<"\n";
            o<<"Weapons.throughWalls="<<variables::Weapons::throughWalls<<"\n";
            o<<"Weapons.world="<<variables::Weapons::world<<"\n";
            o<<"Weapons.ar15="<<variables::Weapons::ar15<<"\n";
            o<<"Weapons.glock="<<variables::Weapons::glock<<"\n";
            o<<"Weapons.occlusion="<<variables::Weapons::occlusion<<"\n";
            o<<"Weapons.glow="<<variables::Weapons::glow<<"\n";
            o<<"Weapons.unlimited="<<variables::Weapons::unlimited<<"\n";
            o<<"Weapons.distance="<<variables::Weapons::distance<<"\n";
            o<<"Weapons.opacity="<<variables::Weapons::opacity<<"\n";
            o<<"Weapons.speed="<<variables::Weapons::speed<<"\n";
            o<<"Weapons.scale="<<variables::Weapons::scale<<"\n";
            o<<"Weapons.glowStrength="<<variables::Weapons::glowStrength<<"\n";
            o<<"Weapons.meshStyle="<<variables::Weapons::meshStyle<<"\n";
            o<<"Weapons.nativeStyle="<<variables::Weapons::nativeStyle<<"\n";
            o<<"Weapons.occludedStyle="<<variables::Weapons::occludedStyle<<"\n";
            o<<"Weapons.color[0]="<<variables::Weapons::color[0]<<"\n";
            o<<"Weapons.color[1]="<<variables::Weapons::color[1]<<"\n";
            o<<"Weapons.color[2]="<<variables::Weapons::color[2]<<"\n";
            o<<"Weapons.color[3]="<<variables::Weapons::color[3]<<"\n";
            o<<"Weapons.hiddenColor[0]="<<variables::Weapons::hiddenColor[0]<<"\n";
            o<<"Weapons.hiddenColor[1]="<<variables::Weapons::hiddenColor[1]<<"\n";
            o<<"Weapons.hiddenColor[2]="<<variables::Weapons::hiddenColor[2]<<"\n";
            o<<"Weapons.hiddenColor[3]="<<variables::Weapons::hiddenColor[3]<<"\n";
            o<<"Weapons.glowColor[0]="<<variables::Weapons::glowColor[0]<<"\n";
            o<<"Weapons.glowColor[1]="<<variables::Weapons::glowColor[1]<<"\n";
            o<<"Weapons.glowColor[2]="<<variables::Weapons::glowColor[2]<<"\n";
            o<<"Weapons.glowColor[3]="<<variables::Weapons::glowColor[3]<<"\n";
            o<<"ESP.nativeChamsStyle="<<variables::ESP::nativeChamsStyle<<"\n";
            o<<"ESP.nativeChamsOpacity="<<variables::ESP::nativeChamsOpacity<<"\n";
            o<<"ESP.nativeChamsOcclusion="<<(variables::ESP::nativeChamsOcclusion?1:0)<<"\n";
            o<<"ESP.nativeChamsOccludedStyle="<<variables::ESP::nativeChamsOccludedStyle<<"\n";
            for(int i=0;i<4;++i)o<<"ESP.nativeChamsOccludedColor"<<i<<"="<<variables::ESP::nativeChamsOccludedColor[i]<<"\n";
            o<<"ESP.nativeChamsOnly="<<(variables::ESP::nativeChamsOnly?1:0)<<"\n";
            o<<"ESP.nativeChamsAnimationSpeed="<<variables::ESP::nativeChamsAnimationSpeed<<"\n";
            o<<"ESP.nativeChamsPatternSize="<<variables::ESP::nativeChamsPatternSize<<"\n";
            o<<"ESP.nativeChamsWalls="<<(variables::ESP::nativeChamsWalls?1:0)<<"\n";
            o<<"ESP.nativeChamsGlow="<<(variables::ESP::nativeChamsGlow?1:0)<<"\n";
            o<<"ESP.nativeChamsGlowStrength="<<variables::ESP::nativeChamsGlowStrength<<"\n";
            for(int i=0;i<4;++i) o<<"ESP.nativeChamsColor"<<i<<"="<<variables::ESP::nativeChamsColor[i]<<"\n";
            for(int i=0;i<4;++i) o<<"ESP.nativeChamsGlowColor"<<i<<"="<<variables::ESP::nativeChamsGlowColor[i]<<"\n";
            for(int i=0;i<4;++i) o<<"ESP.meshFillColor"<<i<<"="<<variables::ESP::chamsFillColor[i]<<"\n";
            for(int i=0;i<4;++i) o<<"ESP.meshOccludedColor"<<i<<"="<<variables::ESP::meshChamsOccludedColor[i]<<"\n";
            for(int i=0;i<4;++i) o<<"ESP.meshOutlineColor"<<i<<"="<<variables::ESP::meshChamsOutlineColor[i]<<"\n";
            o<<"World.enabled="<<(variables::World::enabled?1:0)<<"\n";
            o<<"World.name="<<(variables::World::name?1:0)<<"\n";
            o<<"World.distance="<<(variables::World::distance?1:0)<<"\n";
            o<<"World.ores="<<(variables::World::ores?1:0)<<"\n";
            for(int i=0;i<3;++i) o<<"World.oresSel"<<i<<"="<<(variables::World::oresSel[i]?1:0)<<"\n";
            o<<"World.plants="<<(variables::World::plants?1:0)<<"\n";
            for(int i=0;i<7;++i) o<<"World.plantsSel"<<i<<"="<<(variables::World::plantsSel[i]?1:0)<<"\n";
            o<<"World.animals="<<(variables::World::animals?1:0)<<"\n";
            for(int i=0;i<3;++i) o<<"World.animalsSel"<<i<<"="<<(variables::World::animalsSel[i]?1:0)<<"\n";
            o<<"World.soldiers="<<(variables::World::soldiers?1:0)<<"\n";
            for(int i=0;i<4;++i) o<<"World.soldiersSel"<<i<<"="<<(variables::World::soldiersSel[i]?1:0)<<"\n";
            o<<"World.tools="<<(variables::World::tools?1:0)<<"\n";
            for(int i=0;i<7;++i) o<<"World.toolsSel"<<i<<"="<<(variables::World::toolsSel[i]?1:0)<<"\n";
            o<<"World.toolsBox="<<(variables::World::toolsBox?1:0)<<"\n";
            o<<"World.animalsBox="<<(variables::World::animalsBox?1:0)<<"\n";
            o<<"World.soldiersBox="<<(variables::World::soldiersBox?1:0)<<"\n";
            return o.str();
        };

        auto cfg_int = [](const std::string& v)->int{ try { return std::stoi(v); } catch(...){ return 0; } };
        auto cfg_float = [](const std::string& v)->float{ try { return std::stof(v); } catch(...){ return 0.0f; } };
        auto parseStr = [](const std::string& s){
            std::istringstream iss(s); std::string line;
            auto getVal = [](const std::string& l)->std::string{ auto p=l.find('='); return p==std::string::npos?"":l.substr(p+1); };
            while(std::getline(iss,line)){
                if(line.rfind("Weapons.throughWalls=",0)==0){std::istringstream value(getVal(line));value>>variables::Weapons::throughWalls;continue;}
                if(line.rfind("ESP.nativeUnlimited=",0)==0) {std::istringstream value(getVal(line));value>>variables::ESP::nativeUnlimited;continue;}
                if(line.rfind("ESP.meshUnlimited=",0)==0) {std::istringstream value(getVal(line));value>>variables::ESP::meshUnlimited;continue;}
                if(line.rfind("ESP.nativeDistance=",0)==0) {std::istringstream value(getVal(line));value>>variables::ESP::nativeDistance;continue;}
                if(line.rfind("ESP.meshDistance=",0)==0) {std::istringstream value(getVal(line));value>>variables::ESP::meshDistance;continue;}
                if(line.rfind("Weapons.mesh=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::mesh;continue;}
                if(line.rfind("Weapons.native=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::native;continue;}
                if(line.rfind("Weapons.viewmodel=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::viewmodel;continue;}
                if(line.rfind("Weapons.world=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::world;continue;}
                if(line.rfind("Weapons.ar15=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::ar15;continue;}
                if(line.rfind("Weapons.glock=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glock;continue;}
                if(line.rfind("Weapons.occlusion=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::occlusion;continue;}
                if(line.rfind("Weapons.glow=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glow;continue;}
                if(line.rfind("Weapons.unlimited=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::unlimited;continue;}
                if(line.rfind("Weapons.distance=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::distance;continue;}
                if(line.rfind("Weapons.opacity=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::opacity;continue;}
                if(line.rfind("Weapons.speed=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::speed;continue;}
                if(line.rfind("Weapons.scale=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::scale;continue;}
                if(line.rfind("Weapons.glowStrength=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glowStrength;continue;}
                if(line.rfind("Weapons.meshStyle=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::meshStyle;continue;}
                if(line.rfind("Weapons.nativeStyle=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::nativeStyle;continue;}
                if(line.rfind("Weapons.occludedStyle=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::occludedStyle;continue;}
                if(line.rfind("Weapons.color[0]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::color[0];continue;}
                if(line.rfind("Weapons.color[1]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::color[1];continue;}
                if(line.rfind("Weapons.color[2]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::color[2];continue;}
                if(line.rfind("Weapons.color[3]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::color[3];continue;}
                if(line.rfind("Weapons.hiddenColor[0]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::hiddenColor[0];continue;}
                if(line.rfind("Weapons.hiddenColor[1]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::hiddenColor[1];continue;}
                if(line.rfind("Weapons.hiddenColor[2]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::hiddenColor[2];continue;}
                if(line.rfind("Weapons.hiddenColor[3]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::hiddenColor[3];continue;}
                if(line.rfind("Weapons.glowColor[0]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glowColor[0];continue;}
                if(line.rfind("Weapons.glowColor[1]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glowColor[1];continue;}
                if(line.rfind("Weapons.glowColor[2]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glowColor[2];continue;}
                if(line.rfind("Weapons.glowColor[3]=",0)==0) {std::istringstream value(getVal(line));value>>variables::Weapons::glowColor[3];continue;}
                if(line.rfind("Aimbot.enabled=",0)==0) variables::Aimbot::enabled = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.fovRadius=",0)==0) variables::Aimbot::fovRadius = stof(getVal(line));
                else if(line.rfind("Aimbot.smoothing=",0)==0) variables::Aimbot::smoothing = stof(getVal(line));
                else if(line.rfind("Aimbot.aimTarget=",0)==0) variables::Aimbot::aimTarget = stoi(getVal(line));
                else if(line.rfind("Aimbot.aimMethod=",0)==0) variables::Aimbot::aimMethod = (std::max)(0, (std::min)(1, stoi(getVal(line))));
                else if(line.rfind("Aimbot.showFOV=",0)==0) variables::Aimbot::showFOV = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.visibleCheck=",0)==0) variables::Aimbot::visibleCheck = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.prediction=",0)==0) variables::Aimbot::prediction = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.aimbotKey=",0)==0) variables::Aimbot::aimbotKey = stoi(getVal(line));
                else if(line.rfind("Aimbot.silentEnabled=",0)==0) variables::Aimbot::silentEnabled = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.silentMethod=",0)==0) variables::Aimbot::silentMethod = (std::max)(1, (std::min)(2, stoi(getVal(line))));
                else if(line.rfind("Menu.key=",0)==0) { int key=stoi(getVal(line)); variables::menuKey=key>0 && key<1024 ? key : VK_INSERT; }
                else if(line.rfind("Aimbot.silentKey=",0)==0) variables::Aimbot::silentKey = stoi(getVal(line));
                else if(line.rfind("Aimbot.silentToggleKey=",0)==0) variables::Aimbot::silentToggleKey = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.silentHitchanceEnabled=",0)==0) variables::Aimbot::silentHitchanceEnabled = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.silentHitchance=",0)==0) variables::Aimbot::silentHitchance = (std::max)(1.0f, (std::min)(100.0f, stof(getVal(line))));
                else if(line.rfind("Aimbot.silentShowFOV=",0)==0) variables::Aimbot::silentShowFOV = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.silentFovRadius=",0)==0) variables::Aimbot::silentFovRadius = (std::max)(5.0f, (std::min)(600.0f, stof(getVal(line))));
                else if(line.rfind("Aimbot.silentNearestPoint=",0)==0) variables::Aimbot::silentNearestPoint = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.targetSelect=",0)==0) variables::Aimbot::targetSelect = (std::max)(0, (std::min)(2, stoi(getVal(line))));
                else if(line.rfind("Aimbot.deadCheck=",0)==0) variables::Aimbot::deadCheck = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.teamCheck=",0)==0) variables::Aimbot::teamCheck = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.invisibleCheck=",0)==0) variables::Aimbot::invisibleCheck = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.silentTracer=",0)==0) variables::Aimbot::silentTracer = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.fovColor",0)==0){ int idx=line[15]-'0'; if(idx>=0&&idx<4) reinterpret_cast<float*>(&variables::Aimbot::fovColor)[idx]=stof(getVal(line)); }
                else if(line.rfind("Aimbot.silentFovColor",0)==0){ int idx=line[21]-'0'; if(idx>=0&&idx<4) reinterpret_cast<float*>(&variables::Aimbot::silentFovColor)[idx]=stof(getVal(line)); }
                else if(line.rfind("Aimbot.silentTracerColor",0)==0){ int idx=line[24]-'0'; if(idx>=0&&idx<4) reinterpret_cast<float*>(&variables::Aimbot::silentTracerColor)[idx]=stof(getVal(line)); }
                else if(line.rfind("Aimbot.fallen_prediction=",0)==0) variables::Aimbot::fallen_prediction = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.selected_weapon_index=",0)==0) variables::Aimbot::selected_weapon_index = stoi(getVal(line));
                else if(line.rfind("Aimbot.fallen_bv_override=",0)==0) variables::Aimbot::fallen_bv_override = stof(getVal(line));
                else if(line.rfind("Aimbot.includeNPC=",0)==0) variables::Aimbot::includeNPC = stoi(getVal(line))!=0;
                else if(line.rfind("Aimbot.predictionLine=",0)==0) variables::Aimbot::predictionLine = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.enabled=",0)==0) variables::ESP::enabled = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.boxes=",0)==0) variables::ESP::boxes = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.names=",0)==0) variables::ESP::names = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.distance=",0)==0) variables::ESP::distance = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.healthBar=",0)==0) variables::ESP::healthBar = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.skeleton=",0)==0) variables::ESP::skeleton = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshChams=",0)==0) variables::ESP::meshChams = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshChamsLocal=",0)==0) variables::ESP::meshChamsLocal = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshChamsOpacity=",0)==0) variables::ESP::meshChamsOpacity = (std::max)(0.0f, (std::min)(1.0f, stof(getVal(line))));
                else if(line.rfind("ESP.meshChamsOcclusion=",0)==0) variables::ESP::meshChamsOcclusion = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshChamsUnionWalls=",0)==0) variables::ESP::meshChamsUnionWalls = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshTeamCheck=",0)==0) variables::ESP::meshTeamCheck = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshDeadCheck=",0)==0) variables::ESP::meshDeadCheck = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshInvisibleCheck=",0)==0) variables::ESP::meshInvisibleCheck = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshTransparencyCheck=",0)==0) variables::ESP::meshTransparencyCheck = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshTransparencyMin=",0)==0) variables::ESP::meshTransparencyMin = stof(getVal(line));
                else if(line.rfind("ESP.meshTransparencyMax=",0)==0) variables::ESP::meshTransparencyMax = stof(getVal(line));
                else if(line.rfind("ESP.meshVisibleMode=",0)==0) variables::ESP::meshChamsDxMode = stoi(getVal(line));
                else if(line.rfind("ESP.meshOccludedMode=",0)==0) variables::ESP::meshChamsOccludedDxMode = stoi(getVal(line));
                else if(line.rfind("ESP.meshOutline=",0)==0) variables::ESP::meshChamsOutline = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.meshOutlineStyle=",0)==0) variables::ESP::meshChamsOutlineStyle = stoi(getVal(line));
                else if(line.rfind("ESP.meshOutlineFade=",0)==0) variables::ESP::meshChamsOutlineFade = stof(getVal(line));
                else if(line.rfind("ESP.visualKeybindEnabled=",0)==0) variables::ESP::visualKeybindEnabled=stoi(getVal(line))!=0;
                else if(line.rfind("ESP.visualKeybindKey=",0)==0) variables::ESP::visualKeybindKey=(std::max)(0,(std::min)(1024,stoi(getVal(line))));
                else if(line.rfind("ESP.visualKeybindMode=",0)==0) variables::ESP::visualKeybindMode=stoi(getVal(line))==1?1:0;
                else if(line.rfind("ESP.visualKeybindTarget=",0)==0) variables::ESP::visualKeybindTarget=stoi(getVal(line))==1?1:0;
                else if(line.rfind("ESP.nativeChamsStyle=",0)==0) variables::ESP::nativeChamsStyle = stoi(getVal(line));
                else if(line.rfind("ESP.nativeChamsAnimationSpeed=",0)==0) variables::ESP::nativeChamsAnimationSpeed = (std::max)(0.0f, (std::min)(15.0f, stof(getVal(line))));
                else if(line.rfind("ESP.nativeChamsPatternSize=",0)==0) variables::ESP::nativeChamsPatternSize = (std::max)(0.25f, (std::min)(15.0f, stof(getVal(line))));
                else if(line.rfind("ESP.nativeChamsWalls=",0)==0) variables::ESP::nativeChamsWalls = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.nativeChamsOpacity=",0)==0) variables::ESP::nativeChamsOpacity = (std::max)(0.0f, (std::min)(1.0f, stof(getVal(line))));
                else if(line.rfind("ESP.nativeChamsOcclusion=",0)==0) variables::ESP::nativeChamsOcclusion=stoi(getVal(line))!=0;
                else if(line.rfind("ESP.nativeChamsOccludedStyle=",0)==0) variables::ESP::nativeChamsOccludedStyle=(std::max)(0,(std::min)(29,stoi(getVal(line))));
                else if(line.rfind("ESP.nativeChamsOccludedColor",0)==0){int idx=line[28]-'0';if(idx>=0&&idx<4)variables::ESP::nativeChamsOccludedColor[idx]=(std::max)(0.0f,(std::min)(1.0f,stof(getVal(line))));}
                else if(line.rfind("ESP.nativeChamsOnly=",0)==0) variables::ESP::nativeChamsOnly = stoi(getVal(line))!=0;
                else if(line.rfind("ESP.nativeChamsGlowStrength=",0)==0) variables::ESP::nativeChamsGlowStrength = (std::max)(0.0f, (std::min)(2.0f, stof(getVal(line))));
                else if(line.rfind("ESP.nativeChamsGlow=",0)==0) variables::ESP::nativeChamsGlow = stoi(getVal(line))!=0;

                else if(line.rfind("ESP.nativeChams=",0)==0) variables::ESP::nativeChams = false;
                else if(line.rfind("ESP.nativeChamsColor",0)==0){ int idx=line[20]-'0'; if(idx>=0&&idx<4) variables::ESP::nativeChamsColor[idx]=stof(getVal(line)); }
                else if(line.rfind("ESP.nativeChamsGlowColor",0)==0){ int idx=line[25]-'0'; if(idx>=0&&idx<4) variables::ESP::nativeChamsGlowColor[idx]=stof(getVal(line)); }
                else if(line.rfind("ESP.meshFillColor",0)==0){ int idx=line[17]-'0'; if(idx>=0&&idx<4) variables::ESP::chamsFillColor[idx]=stof(getVal(line)); }
                else if(line.rfind("ESP.meshOccludedColor",0)==0){ int idx=line[21]-'0'; if(idx>=0&&idx<4) variables::ESP::meshChamsOccludedColor[idx]=stof(getVal(line)); }
                else if(line.rfind("ESP.meshOutlineColor",0)==0){ int idx=line[20]-'0'; if(idx>=0&&idx<4) variables::ESP::meshChamsOutlineColor[idx]=stof(getVal(line)); }
                else if(line.rfind("World.enabled=",0)==0) variables::World::enabled = stoi(getVal(line))!=0;
                else if(line.rfind("World.name=",0)==0) variables::World::name = stoi(getVal(line))!=0;
                else if(line.rfind("World.distance=",0)==0) variables::World::distance = stoi(getVal(line))!=0;
                else if(line.rfind("World.ores=",0)==0) variables::World::ores = stoi(getVal(line))!=0;
                else if(line.rfind("World.oresSel",0)==0){ int idx=line[12]-'0'; if(idx>=0&&idx<3) variables::World::oresSel[idx]=stoi(getVal(line))!=0; }
                else if(line.rfind("World.plants=",0)==0) variables::World::plants = stoi(getVal(line))!=0;
                else if(line.rfind("World.plantsSel",0)==0){ int idx=line[14]-'0'; if(idx>=0&&idx<7) variables::World::plantsSel[idx]=stoi(getVal(line))!=0; }
                else if(line.rfind("World.animals=",0)==0) variables::World::animals = stoi(getVal(line))!=0;
                else if(line.rfind("World.animalsSel",0)==0){ int idx=line[15]-'0'; if(idx>=0&&idx<3) variables::World::animalsSel[idx]=stoi(getVal(line))!=0; }
                else if(line.rfind("World.soldiers=",0)==0) variables::World::soldiers = stoi(getVal(line))!=0;
                else if(line.rfind("World.soldiersSel",0)==0){ int idx=line[16]-'0'; if(idx>=0&&idx<4) variables::World::soldiersSel[idx]=stoi(getVal(line))!=0; }
                else if(line.rfind("World.tools=",0)==0) variables::World::tools = stoi(getVal(line))!=0;
                else if(line.rfind("World.toolsSel",0)==0){ int idx=line[12]-'0'; if(idx>=0&&idx<7) variables::World::toolsSel[idx]=stoi(getVal(line))!=0; }
                else if(line.rfind("World.toolsBox=",0)==0) variables::World::toolsBox = stoi(getVal(line))!=0;
                else if(line.rfind("World.animalsBox=",0)==0) variables::World::animalsBox = stoi(getVal(line))!=0;
                else if(line.rfind("World.soldiersBox=",0)==0) variables::World::soldiersBox = stoi(getVal(line))!=0;
            }
        };
        ImVec2 inpPos = base + ImVec2(12.0f, gy + imGuiCustom::g_contentOffset.y);
        ImGui::SetCursorScreenPos(inpPos);
        ImGui::PushItemWidth(158.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4,2));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlBg));
        ImGui::PushStyleColor(ImGuiCol_Text, imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright));
        ImGui::PushStyleColor(ImGuiCol_Border, imGuiCustom::ColorU32(ImVec4(0,0,0,0)));
        ImGui::InputText("##cfgName", cfgName, sizeof(cfgName));
        ImGui::PopStyleColor(3); ImGui::PopStyleVar(); ImGui::PopItemWidth();
        {
            ImDrawList* idl = ImGui::GetWindowDrawList();
            ImVec2 bmin = ImGui::GetItemRectMin(), bmax = ImGui::GetItemRectMax();
            idl->AddRect(bmin, bmax, imGuiCustom::OutlineBlack(), 0.0f, 0, 1.0f);
            idl->AddRect(bmin+ImVec2(1,1), bmax-ImVec2(1,1), imGuiCustom::OutlineInner(), 0.0f, 0, 1.0f);
        }
        gy += 24.0f;

        {
            static std::vector<std::string> cfgFiles;
            static std::vector<const char*> cfgPtrs;
            static int cfgSel = 0;
            double now = ImGui::GetTime();
            if (now - lastScan > 1.0) {
                lastScan = now;
                cfgFiles.clear();
                namespace fs = std::filesystem;
                try {
                    const fs::path dir = fs::u8path(getCfgDir());
                    if (fs::exists(dir)) {
                        for (auto &p : fs::directory_iterator(dir)) {
                            if (p.is_regular_file() && p.path().extension()==".config") {
                                cfgFiles.push_back(p.path().stem().string());
                            }
                        }
                    }
                } catch(...) {}
                cfgPtrs.clear(); for(auto &s: cfgFiles) cfgPtrs.push_back(s.c_str());
                if(cfgSel >= (int)cfgPtrs.size()) cfgSel = 0;
            }
            if (!cfgPtrs.empty()) {
                gy += imGuiCustom::ComboTop();
                if (imGuiCustom::Combo("cfg_select##cfg_select", &cfgSel, cfgPtrs.data(), (int)cfgPtrs.size(), ImVec2(12.0f, gy), 158.0f, "Configs:")) {
                    strncpy_s(cfgName, sizeof(cfgName), cfgPtrs[cfgSel], _TRUNCATE);
                }
                gy += imGuiCustom::ComboStep();
            } else {
                gy += 2.0f;
            }
        }
        auto cfgBtn = [&](const char* label, ImVec2 off){
            ImVec2 p = base + off + imGuiCustom::g_contentOffset;
            ImGui::SetCursorScreenPos(p);
            ImGui::PushStyleColor(ImGuiCol_Button, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_Text, imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright));
            bool r = ImGui::Button(UiText::Tr(label), ImVec2(75.0f,18.0f));
            ImDrawList* bdl = ImGui::GetWindowDrawList(); ImVec2 bmin=ImGui::GetItemRectMin(), bmax=ImGui::GetItemRectMax();
            bdl->AddRect(bmin,bmax,imGuiCustom::OutlineBlack(),0.0f,0,1.0f); bdl->AddRect(bmin+ImVec2(1,1),bmax-ImVec2(1,1),imGuiCustom::OutlineInner(),0.0f,0,1.0f);
            ImGui::PopStyleColor(4);
            return r;
        };
        const std::string cfgDir = getCfgDir();
        std::string path = cfgDir + "\\" + std::string(cfgName) + ".config";
        if(cfgBtn("Save", ImVec2(12.0f, gy))){
            bool ok = false;
            try {
                std::filesystem::create_directories(std::filesystem::u8path(cfgDir));
                std::ofstream f(std::filesystem::u8path(path));
                ok = (bool)f;
                if (ok) { f<<buildStr(); ok = (bool)f; }
            } catch(...) { ok = false; }
            setCfgStatus(ok ? (std::string(UiText::Tr("saved: ")) + std::string(cfgName)) : UiText::Tr("save FAILED (folder?)"));
            lastScan = 0.0;
        }
        if(cfgBtn("Load", ImVec2(95.0f, gy))){
            std::ifstream f(std::filesystem::u8path(path));
            if(f){ std::stringstream ss; ss<<f.rdbuf(); parseStr(ss.str()); setCfgStatus(UiText::Tr("loaded: ") + std::string(cfgName)); }
            else setCfgStatus(UiText::Tr("not found: ") + std::string(cfgName));
        }
        gy += 22.0f;
        if(cfgBtn("Export", ImVec2(12.0f, gy))){
            ImGui::SetClipboardText(buildStr().c_str()); setCfgStatus(UiText::Tr("copied to clipboard"));
        }
        if(cfgBtn("Import", ImVec2(95.0f, gy))){
            const char* cb = ImGui::GetClipboardText();
            if(cb && *cb){ parseStr(std::string(cb)); setCfgStatus(UiText::Tr("imported from clipboard")); }
            else setCfgStatus(UiText::Tr("clipboard empty"));
        }
        gy += 22.0f;
        if(cfgBtn("Delete", ImVec2(12.0f, gy))){
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(path), ec);
            setCfgStatus(ec ? (UiText::Tr("delete FAILED: ") + std::string(cfgName))
                            : (UiText::Tr("deleted: ") + std::string(cfgName)));
            lastScan = 0.0;
        }

        {
            const double nowT = ImGui::GetTime();
            if (!cfgStatus.empty() && nowT < cfgStatusUntil) {
                cdl->AddText(cfont, cfs, ImVec2(base.x+95.0f, base.y+gy+4.0f) + imGuiCustom::g_contentOffset,
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), cfgStatus.c_str());
            } else if (nowT >= cfgStatusUntil) {
                cfgStatus.clear();
            }
        }
        gy += 26.0f;
    }
    if constexpr (showLegacySettings) {
        float hy = 46.0f;
        imGuiCustom::SliderFloat("menu_font", &variables::Misc::menuFontSize, 0.8f, 1.5f, ImVec2(311.0f, hy + imGuiCustom::SliderTop()), 272.0f, "Menu Font Size", "%.2f");
    if (variables::Misc::menuFontSize < 0.8f)
        variables::Misc::menuFontSize = 0.8f;
    if (variables::Misc::menuFontSize > 1.5f)
        variables::Misc::menuFontSize = 1.5f;
    hy += imGuiCustom::SliderTop() + 15.0f;
    imGuiCustom::SliderFloat("esp_font", &variables::Misc::espFontSize, 10.0f, 20.0f, ImVec2(311.0f, hy + imGuiCustom::SliderTop()), 272.0f, "ESP Font Size", "%.0f");
    if (variables::Misc::espFontSize < 10.0f)
        variables::Misc::espFontSize = 10.0f;
    if (variables::Misc::espFontSize > 20.0f)
        variables::Misc::espFontSize = 20.0f;
    hy += imGuiCustom::SliderTop() + 15.0f;
    float ty = hy + 6.0f;
    auto themeRow = [&](const char* id, const char* label, ImVec4* col) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 base = ImGui::GetWindowPos();
        ImFont* font = imGuiCustom::GetFonts().CascadiaMonoBL ? imGuiCustom::GetFonts().CascadiaMonoBL : ImGui::GetFont();
        const float fs = 12.0f * imGuiCustom::g_fontScale;
        dl->AddText(font, fs, ImVec2(base.x + 311.0f, base.y + ty), imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), UiText::Tr(label));
        imGuiCustom::ColorSquare(id, col, ImVec2(563.0f, ty + 1.0f));
        ty += imGuiCustom::CheckStep();
    };
    themeRow("theme_bg", "Background", &variables::Theme::background);
    themeRow("theme_panels", "Panels", &variables::Theme::panels);
    themeRow("theme_controls", "Controls", &variables::Theme::controls);
    themeRow("theme_accent", "Accent", &variables::Theme::accent);
    themeRow("theme_text", "Text", &variables::Theme::text);
    themeRow("theme_textbright", "Text Bright", &variables::Theme::textBright);
    ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImVec2(311.0f, ty));
    ImGui::PushStyleColor(ImGuiCol_Button, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
    ImGui::PushStyleColor(ImGuiCol_Text, imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright));
    if (ImGui::Button(UiText::Tr("Reset Theme"), ImVec2(158.0f, 18.0f))) {
        variables::Theme::background = ImVec4(0.1176f, 0.1176f, 0.1176f, 1.0f);
        variables::Theme::panels = ImVec4(0.1529f, 0.1529f, 0.1529f, 1.0f);
        variables::Theme::controls = ImVec4(0.1843f, 0.1843f, 0.1843f, 1.0f);
        variables::Theme::accent = ImVec4(0.3490f, 0.8118f, 0.8275f, 1.0f);
        variables::Theme::text = ImVec4(0.7600f, 0.7600f, 0.7600f, 1.0f);
        variables::Theme::textBright = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    {
        ImDrawList* bdl = ImGui::GetWindowDrawList();
        const ImVec2 bmin = ImGui::GetItemRectMin();
        const ImVec2 bmax = ImGui::GetItemRectMax();
        bdl->AddRect(bmin, bmax, imGuiCustom::OutlineBlack(), 0.0f, 0, 1.0f);
        bdl->AddRect(bmin + ImVec2(1.0f, 1.0f), bmax - ImVec2(1.0f, 1.0f), imGuiCustom::OutlineInner(), 0.0f, 0, 1.0f);
    }
        ImGui::PopStyleColor(4);
    }
}
}
