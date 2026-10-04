#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "mics.h"
#include "../../keys/keys.h"
#include "../../../render/menu/library.h"
#include "../../../render/SadblobVideo.h"
#include <thread>
#include <chrono>
#include "../../cache/cache.h"
#include <cctype>
#include <commdlg.h>
#include <filesystem>

#pragma comment(lib, "comdlg32.lib")

namespace Mics {
void RenderPlayersMenu() {
    struct Entry { std::uint64_t id; std::string name; };
    static std::vector<Entry> roster;
    static double nextRefresh=0;
    static std::uint64_t selected=0;
    static char search[96]{};
    if(ImGui::GetTime()>=nextRefresh) {
        nextRefresh=ImGui::GetTime()+1.0;
        roster.clear();
        for(const auto& p:Globals::players.GetChildList()) {
            if(p.GetClass()!="Player") continue;
            auto id=memory->read<std::uint64_t>(p.Addr+Offsets::Player::UserId);
            if(id && p.Addr!=Globals::localPlayer.Addr) roster.push_back({id,p.GetName()});
        }
        std::sort(roster.begin(),roster.end(),[](const Entry& a,const Entry& b){return a.name<b.name;});
    }
    const auto offset=imGuiCustom::g_contentOffset;
    ImGui::SetCursorPos(ImVec2(12+offset.x,46+offset.y));
    ImGui::SetNextItemWidth(272);
    ImGui::InputTextWithHint("##player_search",UiText::Tr("Search players..."),search,sizeof(search));
    auto lower=[](std::string s){for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;};
    const auto query=lower(search);
    ImGui::SetCursorPos(ImVec2(12+offset.x,76+offset.y));
    ImGui::BeginChild("player_list",ImVec2(272,290),true);
    bool any=false;
    for(const auto& p:roster) {
        if(lower(p.name).find(query)==std::string::npos)continue;
        any=true;
        ImGui::PushID(p.name.c_str());
        if(ImGui::Selectable(p.name.c_str(),selected==p.id))selected=p.id;
        ImGui::PopID();
    }
    if(!any)ImGui::TextUnformatted(UiText::Tr("No players found"));
    ImGui::EndChild();
    auto it=std::find_if(roster.begin(),roster.end(),[](const Entry& p){return p.id==selected;});
    if(it!=roster.end()) {
        ImGui::SetCursorPos(ImVec2(317+offset.x,46+offset.y));
        ImGui::TextUnformatted(it->name.c_str());
        auto& rule=PlayerRules::rules[selected];
        imGuiCustom::Checkbox("Silent Aim whitelist",&rule.aim,ImVec2(317,80));
        imGuiCustom::Checkbox("ESP whitelist",&rule.esp,ImVec2(317,80+imGuiCustom::CheckStep()));
    }
}
void RenderThemeMenu() {
    float ry = 46.0f;
    auto chooseCustomMp4 = [] {
        wchar_t selected[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetForegroundWindow();
        dialog.lpstrFilter = L"MP4 video (*.mp4)\0*.mp4\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = selected;
        dialog.nMaxFile = static_cast<DWORD>(std::size(selected));
        dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&dialog))
            variables::Theme::customMp4Path = std::filesystem::path(selected).u8string();
    };

        {
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImVec2 base = ImGui::GetWindowPos();
            ImFont* cfont = imGuiCustom::GetFonts().CascadiaMonoBL ? imGuiCustom::GetFonts().CascadiaMonoBL : ImGui::GetFont();
            float cfs = 12.0f * imGuiCustom::g_fontScale;
            cdl->AddText(cfont, cfs, ImVec2(base.x + 12.0f, base.y + ry + 2.0f), imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright), UiText::Tr("Theme"));
            ry += 18.0f;

            struct ThemePreset { const char* name; ImVec4 bg, panels, controls, accent, text, textBright; };
            static const ThemePreset kPresets[] = {
                { "Retro", ImVec4(.015f,.015f,.02f,1), ImVec4(.025f,.025f,.03f,1), ImVec4(.06f,.06f,.07f,1), ImVec4(1,.9f,.15f,1), ImVec4(.9f,.9f,.9f,1), ImVec4(1,1,1,1) },
                { "Midnight",   ImVec4(0.043f,0.055f,0.078f,1), ImVec4(0.071f,0.086f,0.122f,1), ImVec4(0.102f,0.125f,0.188f,1), ImVec4(0.357f,0.549f,1.000f,1), ImVec4(0.700f,0.730f,0.780f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Ocean",      ImVec4(0.047f,0.075f,0.098f,1), ImVec4(0.071f,0.110f,0.141f,1), ImVec4(0.094f,0.145f,0.188f,1), ImVec4(0.000f,0.780f,0.850f,1), ImVec4(0.700f,0.780f,0.820f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Emerald",    ImVec4(0.055f,0.078f,0.063f,1), ImVec4(0.078f,0.110f,0.090f,1), ImVec4(0.106f,0.149f,0.122f,1), ImVec4(0.200f,0.850f,0.450f,1), ImVec4(0.720f,0.790f,0.750f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Blood",      ImVec4(0.078f,0.047f,0.047f,1), ImVec4(0.110f,0.067f,0.067f,1), ImVec4(0.149f,0.090f,0.090f,1), ImVec4(0.900f,0.200f,0.200f,1), ImVec4(0.800f,0.720f,0.720f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Sunset",     ImVec4(0.078f,0.059f,0.047f,1), ImVec4(0.110f,0.086f,0.067f,1), ImVec4(0.149f,0.118f,0.090f,1), ImVec4(1.000f,0.510f,0.200f,1), ImVec4(0.810f,0.750f,0.700f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Violet",     ImVec4(0.063f,0.051f,0.094f,1), ImVec4(0.090f,0.075f,0.133f,1), ImVec4(0.122f,0.102f,0.180f,1), ImVec4(0.640f,0.400f,1.000f,1), ImVec4(0.750f,0.720f,0.820f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Rose",       ImVec4(0.082f,0.051f,0.067f,1), ImVec4(0.114f,0.075f,0.094f,1), ImVec4(0.153f,0.102f,0.125f,1), ImVec4(1.000f,0.400f,0.620f,1), ImVec4(0.810f,0.720f,0.760f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Gold",       ImVec4(0.075f,0.067f,0.047f,1), ImVec4(0.106f,0.094f,0.067f,1), ImVec4(0.145f,0.129f,0.090f,1), ImVec4(1.000f,0.780f,0.250f,1), ImVec4(0.800f,0.770f,0.690f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Toxic",      ImVec4(0.051f,0.059f,0.043f,1), ImVec4(0.075f,0.086f,0.059f,1), ImVec4(0.102f,0.118f,0.078f,1), ImVec4(0.600f,1.000f,0.150f,1), ImVec4(0.740f,0.790f,0.690f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Frost",      ImVec4(0.075f,0.082f,0.090f,1), ImVec4(0.106f,0.114f,0.125f,1), ImVec4(0.145f,0.157f,0.173f,1), ImVec4(0.780f,0.890f,1.000f,1), ImVec4(0.740f,0.770f,0.800f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Carbon",     ImVec4(0.024f,0.024f,0.024f,1), ImVec4(0.047f,0.047f,0.047f,1), ImVec4(0.078f,0.078f,0.078f,1), ImVec4(0.950f,0.950f,0.950f,1), ImVec4(0.650f,0.650f,0.650f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Slate",      ImVec4(0.090f,0.098f,0.110f,1), ImVec4(0.125f,0.137f,0.153f,1), ImVec4(0.161f,0.176f,0.196f,1), ImVec4(0.450f,0.620f,0.850f,1), ImVec4(0.740f,0.760f,0.790f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Coral",      ImVec4(0.071f,0.055f,0.063f,1), ImVec4(0.102f,0.078f,0.090f,1), ImVec4(0.137f,0.106f,0.122f,1), ImVec4(1.000f,0.450f,0.380f,1), ImVec4(0.800f,0.740f,0.750f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Primordial", ImVec4(0.067f,0.067f,0.071f,1), ImVec4(0.090f,0.090f,0.094f,1), ImVec4(0.118f,0.118f,0.125f,1), ImVec4(0.950f,0.750f,0.300f,1), ImVec4(0.700f,0.700f,0.720f,1), ImVec4(0.960f,0.960f,0.970f,1) },
                { "Sadblob theme", ImVec4(0.039f,0.047f,0.078f,1), ImVec4(0.067f,0.075f,0.110f,1), ImVec4(0.102f,0.114f,0.157f,1), ImVec4(0.663f,0.741f,1.000f,1), ImVec4(0.780f,0.800f,0.890f,1), ImVec4(1.000f,1.000f,1.000f,1) },
                { "Custom Video", ImVec4(0.039f,0.047f,0.078f,1), ImVec4(0.067f,0.075f,0.110f,1), ImVec4(0.102f,0.114f,0.157f,1), ImVec4(0.663f,0.741f,1.000f,1), ImVec4(0.780f,0.800f,0.890f,1), ImVec4(1.000f,1.000f,1.000f,1) },
            };
            int& s_presetIdx = variables::Theme::preset;
            auto applyPreset = [&](const ThemePreset& p) {
                variables::Theme::background = p.bg;
                variables::Theme::panels = p.panels;
                variables::Theme::controls = p.controls;
                variables::Theme::accent = p.accent;
                variables::Theme::text = p.text;
                variables::Theme::textBright = p.textBright;
            };
            constexpr int kPresetCount = (int)(sizeof(kPresets)/sizeof(kPresets[0]));
            if (s_presetIdx == SadblobVideo::ThemePreset && !SadblobVideo::HasBundledClips()) {
                s_presetIdx = SadblobVideo::CustomPreset;
                applyPreset(kPresets[s_presetIdx]);
            }
            auto movePreset = [&](int direction) {
                do {
                    s_presetIdx = (s_presetIdx + direction + kPresetCount) % kPresetCount;
                } while (s_presetIdx == SadblobVideo::ThemePreset && !SadblobVideo::HasBundledClips());
                applyPreset(kPresets[s_presetIdx]);
                if (s_presetIdx == SadblobVideo::CustomPreset && variables::Theme::customMp4Path.empty())
                    chooseCustomMp4();
            };
            ImGui::SetCursorScreenPos(base + ImVec2(12.0f, ry));
            ImGui::PushStyleColor(ImGuiCol_Button, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_Text, imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright));

            constexpr float kPresetRowW = 206.0f;
            if (ImGui::Button("<##theme_prev", ImVec2(22.0f, 20.0f))) {
                movePreset(-1);
            }
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(base + ImVec2(12.0f + kPresetRowW - 22.0f, ry));
            if (ImGui::Button(">##theme_next", ImVec2(22.0f, 20.0f))) {
                movePreset(1);
            }
            {
                const char* name = UiText::Tr(kPresets[s_presetIdx].name);
                const ImVec2 ns = cfont->CalcTextSizeA(cfs, FLT_MAX, 0.0f, name);
                cdl->AddText(cfont, cfs,
                    ImVec2(base.x + 12.0f + 22.0f + (kPresetRowW - 44.0f - ns.x) * 0.5f,
                           base.y + ry + (20.0f - ns.y) * 0.5f),
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Accent), name);
            }
            ry += 24.0f;
            ImGui::PopStyleColor(4);
            if (s_presetIdx == SadblobVideo::ThemePreset) {
                ry += 3.0f;
                cdl->AddText(cfont, cfs, base + ImVec2(12.0f, ry + 2.0f),
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), "Animated style");
                ry += 19.0f;
                constexpr float styleWidth = 290.0f;
                ImGui::SetCursorScreenPos(base + ImVec2(12.0f, ry));
                if (ImGui::Button("<##sadblob_prev", ImVec2(22.0f, 22.0f)))
                    variables::Theme::sadblobStyle = (variables::Theme::sadblobStyle + SadblobVideo::StyleCount - 1) % SadblobVideo::StyleCount;
                ImGui::SetCursorScreenPos(base + ImVec2(12.0f + styleWidth - 22.0f, ry));
                if (ImGui::Button(">##sadblob_next", ImVec2(22.0f, 22.0f)))
                    variables::Theme::sadblobStyle = (variables::Theme::sadblobStyle + 1) % SadblobVideo::StyleCount;
                const char* styleName = SadblobVideo::StyleName(variables::Theme::sadblobStyle);
                const ImVec2 styleSize = cfont->CalcTextSizeA(cfs, FLT_MAX, 0.0f, styleName);
                cdl->AddText(cfont, cfs,
                    base + ImVec2(12.0f + (styleWidth - styleSize.x) * 0.5f, ry + (22.0f - styleSize.y) * 0.5f),
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Accent), styleName);
                ry += 26.0f;
                if (SadblobVideo::MissingFile()) {
                    cdl->AddText(cfont, cfs, base + ImVec2(12.0f, ry),
                        imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),
                        "Clip unavailable (see Sadblob folder)");
                    ry += 18.0f;
                }
            } else if (s_presetIdx == SadblobVideo::CustomPreset) {
                ry += 3.0f;
                cdl->AddText(cfont, cfs, base + ImVec2(12.0f, ry + 2.0f),
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), "Custom MP4 video");
                ry += 20.0f;
                ImGui::SetCursorScreenPos(base + ImVec2(12.0f, ry));
                if (ImGui::Button("Choose MP4...##custom_theme_mp4", ImVec2(140.0f, 22.0f)))
                    chooseCustomMp4();
                ry += 27.0f;
                std::string filename = variables::Theme::customMp4Path.empty()
                    ? "No MP4 selected"
                    : std::filesystem::u8path(variables::Theme::customMp4Path).filename().u8string();
                cdl->AddText(cfont, cfs, base + ImVec2(12.0f, ry),
                    imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), filename.c_str());
                ry += 19.0f;
                if (SadblobVideo::MissingFile() && !variables::Theme::customMp4Path.empty()) {
                    cdl->AddText(cfont, cfs, base + ImVec2(12.0f, ry),
                        imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),
                        "Video unavailable - choose another MP4");
                    ry += 18.0f;
                }
            }

            auto themeRow = [&](const char* id, const char* label, ImVec4* col) {
                cdl->AddText(cfont, cfs, ImVec2(base.x + 12.0f, base.y + ry + 2.0f), imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text), UiText::Tr(label));
                imGuiCustom::ColorSquare(id, col, ImVec2(206.0f, ry + 1.0f));
                ry += imGuiCustom::CheckStep();
            };
            themeRow("theme_bg", "Background", &variables::Theme::background);
            themeRow("theme_panels", "Panels", &variables::Theme::panels);
            themeRow("theme_controls", "Controls", &variables::Theme::controls);
            themeRow("theme_accent", "Accent", &variables::Theme::accent);
            themeRow("theme_text", "Text", &variables::Theme::text);
            themeRow("theme_textbright", "Text Bright", &variables::Theme::textBright);
            ImGui::SetCursorScreenPos(base + ImVec2(12.0f, ry));
            ImGui::PushStyleColor(ImGuiCol_Button, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, imGuiCustom::ColorU32(imGuiCustom::GetTheme().ControlInactive));
            ImGui::PushStyleColor(ImGuiCol_Text, imGuiCustom::ColorU32(imGuiCustom::GetTheme().TextBright));
            if (ImGui::Button(UiText::Tr("Reset Theme"), ImVec2(120.0f, 20.0f))) {
                s_presetIdx = 0;
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
                bdl->AddRect(bmin + ImVec2(1, 1), bmax - ImVec2(1, 1), imGuiCustom::OutlineInner(), 0.0f, 0, 1.0f);
            }
            ImGui::PopStyleColor(4);
            ry += 26.0f;
        }
}

void Tick() {
    auto character = Globals::localPlayer.GetModelRef();
    auto humanoid = character.Addr ? character.FindChildByClass("Humanoid") : RBX::RbxInstance{};
    if (!humanoid.Addr)
        return;
    static bool jumpWas = false;
    static float jumpBackup = 50.0f;
    static std::uintptr_t jumpHum = 0;
    static bool jumpTog = false;
    static bool jumpKeyWas = false;
    if (variables::Local::jumpEnabled && Keys::Gate(variables::Local::jumpKey, variables::Local::jumpKeyMode, jumpTog, jumpKeyWas)) {
        if (!jumpWas || jumpHum != humanoid.Addr) {
            jumpBackup = memory->read<float>(humanoid.Addr + Offsets::Humanoid::JumpPower);
            if (!std::isfinite(jumpBackup) || jumpBackup <= 0.0f)
                jumpBackup = 50.0f;
            jumpHum = humanoid.Addr;
        }
        RBX::ModifyJumpPower(humanoid, variables::Local::jumpPower);
        jumpWas = true;
    } else if (jumpWas) {
        if (jumpHum)
            RBX::ModifyJumpPower(RBX::RbxInstance{jumpHum}, jumpBackup);
        jumpWas = false;
    }
}

void TickFov() {
    static bool wasOn = false;
    static bool tog = false;
    static bool was = false;
    const bool on = variables::Movement::fov && Keys::Gate(variables::Movement::fovKey, variables::Movement::fovKeyMode, tog, was);
    if (on && Globals::camera.Addr) {
        wasOn = true;
        memory->write<float>(Globals::camera.Addr + Offsets::Camera::FieldOfView, variables::Movement::fovValue * (3.14159265f / 180.0f));
    } else if (wasOn) {
        wasOn = false;
        if (Globals::camera.Addr)
            memory->write<float>(Globals::camera.Addr + Offsets::Camera::FieldOfView, 70.0f * (3.14159265f / 180.0f));
    }
}

void Loop() {
    using namespace std::chrono_literals;
    while (Globals::running) {
        Tick();
        TickFov();
        std::this_thread::sleep_for(5ms);
    }
}

void RenderLocalMenu() {
    float ly = 46.0f;
    imGuiCustom::Checkbox("JumpPower", &variables::Local::jumpEnabled, ImVec2(12.0f, ly));
    imGuiCustom::Keybind("jump_key", &variables::Local::jumpKey, ImVec2(222.0f, ly - 1.0f), ImVec2(68.0f, 13.0f), &variables::Local::jumpKeyMode);
    ly += imGuiCustom::CheckStep();
    if (variables::Local::jumpEnabled) {
        imGuiCustom::SliderFloat("jumppower", &variables::Local::jumpPower, 50.0f, 200.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "JumpPower", "%.0f");
        ly += imGuiCustom::SliderStep();
    }
    imGuiCustom::Checkbox("BunnyHop", &variables::Movement::bunnyHop, ImVec2(12.0f, ly));
    imGuiCustom::Keybind("bhop_key", &variables::Movement::bunnyHopKey, ImVec2(222.0f, ly - 1.0f), ImVec2(68.0f, 13.0f), &variables::Movement::bunnyHopKeyMode);
    ly += imGuiCustom::CheckStep();
    if (variables::Movement::bunnyHop) {
        imGuiCustom::SliderFloat("bhop_speed", &variables::Movement::bunnyHopSpeed, 16.0f, 250.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "BunnyHop Speed", "%.0f");
        ly += imGuiCustom::SliderStep();
    }
    imGuiCustom::Checkbox("HipHeight", &variables::Movement::hipHeight, ImVec2(12.0f, ly));
    ly += imGuiCustom::CheckStep();
    if (variables::Movement::hipHeight) {
        imGuiCustom::SliderFloat("hip_height", &variables::Movement::hipHeightValue, 0.0f, 10.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "HipHeight", "%.1f");
        ly += imGuiCustom::SliderStep();
    }

}

void RenderMiscMenu() {
    float ry = 46.0f;
    constexpr bool showLegacyMicsSettings = false;
    if constexpr (!showLegacyMicsSettings) {
        imGuiCustom::Checkbox("Stream Proof", &variables::Misc::streamProof, ImVec2(12.0f, ry));
        imGuiCustom::Keybind("stream_key", &variables::Misc::streamKey, ImVec2(206.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Misc::streamKeyMode);
        ry += imGuiCustom::CheckStep();
        imGuiCustom::Checkbox("Spotify Player", &variables::Misc::spotifyPlayer, ImVec2(12.0f, ry));
        imGuiCustom::Keybind("spotify_key", &variables::Misc::spotifyKey, ImVec2(206.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Misc::spotifyKeyMode);
        ry += imGuiCustom::CheckStep();
        imGuiCustom::Checkbox("Keybind list", &variables::Misc::keybinds, ImVec2(12.0f,ry));
        ry += imGuiCustom::CheckStep();
        imGuiCustom::Checkbox("Performance", &variables::Misc::performancePanel, ImVec2(12.0f,ry));
        ry += imGuiCustom::CheckStep();

        {
            static float fpsSlider = (float)variables::Misc::fpsLimit;
            imGuiCustom::SliderFloat("fps_cap", &fpsSlider, 60.0f, 2000.0f,
                ImVec2(12.0f, ry + imGuiCustom::SliderTop()), 272.0f, "FPS Cap", "%.0f");
            int snapped = (int)(fpsSlider + 0.5f);
            if (variables::Misc::fpsLimit != snapped) {
                variables::Misc::fpsLimit = snapped;

                if (variables::Misc::fpsLimit < 60)
                    variables::Misc::fpsLimit = 60;
            }
            ry += imGuiCustom::SliderTop() + 15.0f;
        }
        ry += 16.0f;
        const char* languages[]={"Norsk", "English"};
        if(imGuiCustom::Combo("ui_language", &UiText::language, languages, 2,
            ImVec2(12.0f,ry+imGuiCustom::ComboTop()),158.0f,"Language:")) UiText::Save();
        ry += imGuiCustom::ComboTop()+32.0f;
        ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos()+ImVec2(12.0f,ry),
            ImGui::GetColorU32(ImGuiCol_Text),UiText::Tr("Menu key:"));
        imGuiCustom::Keybind("menu_key", &variables::menuKey, ImVec2(206.0f,ry), ImVec2(85,15),nullptr,false);
        if(variables::menuKey<=0) variables::menuKey=VK_INSERT;
        return;
    }

    imGuiCustom::Checkbox("Team Check", &variables::teamCheck, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("team_key", &variables::teamCheckKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::teamCheckKeyMode);
    ry += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Stream Proof", &variables::Misc::streamProof, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("stream_key", &variables::Misc::streamKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Misc::streamKeyMode);
    ry += imGuiCustom::CheckStep();
    ry += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("Keybinds", &variables::Misc::keybinds, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("keybinds_key", &variables::Misc::keybindsKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Misc::keybindsKeyMode);
    ry += imGuiCustom::CheckStep();
    imGuiCustom::Checkbox("FOV", &variables::Movement::fov, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("fov_key", &variables::Movement::fovKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Movement::fovKeyMode);
    ry += imGuiCustom::CheckStep();
    if (variables::Movement::fov) {
        imGuiCustom::SliderFloat("fov_value", &variables::Movement::fovValue, 30.0f, 150.0f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "FOV Value", "%.0f");
        ry += imGuiCustom::SliderStep();
    }
    imGuiCustom::Checkbox("Fly", &variables::Movement::fly, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("fly_key", &variables::Movement::flyKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Movement::flyKeyMode);
    ry += imGuiCustom::CheckStep();
    if (variables::Movement::fly) {
        imGuiCustom::Checkbox("Check Typing", &variables::Movement::flyCheckTyping, ImVec2(311.0f, ry));
        ry += imGuiCustom::CheckStep();
        imGuiCustom::SliderFloat("fly_speed", &variables::Movement::flySpeed, 5.0f, 1000.0f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Fly Speed", "%.0f");
        ry += imGuiCustom::SliderStep();
        imGuiCustom::SliderFloat("fly_vert", &variables::Movement::flyVerticalBoost, 0.1f, 3.0f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Vertical Boost", "%.2f");
        ry += imGuiCustom::SliderStep();
        imGuiCustom::SliderFloat("fly_damping", &variables::Movement::flyDamping, 0.0f, 50.0f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Damping", "%.1f");
        ry += imGuiCustom::SliderStep();
    }

    imGuiCustom::Checkbox("Noclip", &variables::Movement::noclip, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("noclip_key", &variables::Movement::noclipKey, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Movement::noclipKeyMode);
    ry += imGuiCustom::CheckStep();
    if (variables::Movement::noclip) {
        ry += imGuiCustom::ComboTop();
        const char* noclipModes[] = {"All Parts", "Root Only"};
        imGuiCustom::Combo("noclip_mode", &variables::Movement::noclipMode, noclipModes, 2, ImVec2(311.0f, ry), 158.0f, "Noclip Mode:");
        ry += imGuiCustom::ComboStep();
    }
    imGuiCustom::Checkbox("Freecam", &variables::Freecam::enabled, ImVec2(311.0f, ry));
    imGuiCustom::Keybind("freecam_key", &variables::Freecam::key, ImVec2(505.0f, ry - 1.0f), ImVec2(68.0f, 13.0f), &variables::Freecam::keyMode);
    ry += imGuiCustom::CheckStep();
    if (variables::Freecam::enabled) {
        imGuiCustom::SliderFloat("fcam_speed", &variables::Freecam::speed, 5.0f, 300.0f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Speed", "%.0f");
        ry += imGuiCustom::SliderTop() + 15.0f;
        imGuiCustom::SliderFloat("fcam_sens", &variables::Freecam::sensitivity, 0.001f, 0.02f, ImVec2(311.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Sensitivity", "%.4f");
        ry += imGuiCustom::SliderTop() + 15.0f;
    }
}
}
