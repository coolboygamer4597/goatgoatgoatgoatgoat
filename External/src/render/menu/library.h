#pragma once
#include "UiText.h"
#include "../UiAssets.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "../../../ext/imgui/imgui.h"
#include "../../../ext/imgui/imgui_internal.h"

#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace imGuiCustom
{
struct Theme
{
    ImVec4 WindowBg;
    ImVec4 CardBg;
    ImVec4 ControlBg;
    ImVec4 ControlInactive;
    ImVec4 Border;
    ImVec4 Accent;
    ImVec4 AccentText;
    ImVec4 Text;
    ImVec4 TextBright;
    ImVec4 KeybindBg;
};

struct Fonts
{
    ImFont* CascadiaMonoBL = nullptr;

    ImFont* InterMedium = nullptr;
    ImFont* InterSmall = nullptr;
    ImFont* InterSemiBold = nullptr;
    ImFont* InterBold = nullptr;
    ImFont* InterSemiBoldSmall = nullptr;
    ImFont* IconFont = nullptr;
};

inline Theme& GetThemeMutable()
{
    static Theme g_Theme = {
        ImVec4(0.1176f, 0.1176f, 0.1176f, 1.0f),
        ImVec4(0.1529f, 0.1529f, 0.1529f, 1.0f),
        ImVec4(0.1843f, 0.1843f, 0.1843f, 1.0f),
        ImVec4(0.2157f, 0.2157f, 0.2157f, 1.0f),
        ImVec4(0.1608f, 0.1608f, 0.1608f, 1.0f),
        ImVec4(0.3490f, 0.8118f, 0.8275f, 1.0f),
        ImVec4(0.3490f, 0.6314f, 0.6392f, 1.0f),
        ImVec4(0.7600f, 0.7600f, 0.7600f, 1.0f),
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        ImVec4(0.1800f, 0.1800f, 0.1800f, 1.0f),
    };
    return g_Theme;
}

inline const Theme& GetTheme() { return GetThemeMutable(); }

inline Fonts& GetFontsMutable()
{
    static Fonts g_Fonts = {};
    return g_Fonts;
}

inline const Fonts& GetFonts() { return GetFontsMutable(); }

inline void ApplyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const Theme& g_Theme = GetTheme();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(0.0f, 0.0f);
    style.ItemSpacing = ImVec2(0.0f, 0.0f);
    style.ItemInnerSpacing = ImVec2(0.0f, 0.0f);
    style.AntiAliasedLines = false;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = false;
    style.Colors[ImGuiCol_WindowBg] = g_Theme.WindowBg;
    style.Colors[ImGuiCol_PopupBg] = g_Theme.CardBg;
    style.Colors[ImGuiCol_Text] = g_Theme.Text;
}

inline void Initialize(ImFont* cascadiaMonoBL)
{
    GetFontsMutable().CascadiaMonoBL = cascadiaMonoBL;
    ApplyStyle();
}

inline float AnimateFloat(ImGuiID id, bool enabled, float speed = 12.0f)
{
    static std::unordered_map<ImGuiID, float> g_Animations;
    ImGuiIO& io = ImGui::GetIO();
    float& value = g_Animations[id];
    const float target = enabled ? 1.0f : 0.0f;
    value = ImLerp(value, target, ImClamp(io.DeltaTime * speed, 0.0f, 1.0f));
    return value;
}

inline ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(ImLerp(a.x, b.x, t), ImLerp(a.y, b.y, t), ImLerp(a.z, b.z, t), ImLerp(a.w, b.w, t));
}

inline ImU32 OutlineBlack() { return ImGui::GetColorU32(IM_COL32(8, 8, 8, 255)); }
inline ImU32 OutlineInner() { return ImGui::GetColorU32(IM_COL32(52, 52, 56, 255)); }

inline ImVec2 g_contentOffset = ImVec2(0.0f, 0.0f);
inline float g_fontScale = 1.25f;
inline ImGuiID& KeyCaptureId(){static ImGuiID id=0;return id;}
inline bool KeyCapturing(){return KeyCaptureId()!=0;}
inline float SliderTop() { return 12.0f * g_fontScale + 5.0f; }
inline float SliderStep() { return 12.0f * g_fontScale + 5.0f + 15.0f; }
inline float ComboTop() { return 12.0f * g_fontScale + 3.0f; }
inline float ComboStep() { return 27.0f; }
inline float CheckStep() { return 13.5f * g_fontScale + 5.0f; }

inline ImGuiID& ComboOpenId() { static ImGuiID v = 0; return v; }
inline int& ComboClosedFrame() { static int f = -100000; return f; }
inline bool PopupBlocking() {
    if (ImGui::GetFrameCount() == ComboClosedFrame())
        return true;
    return ComboOpenId() != 0;
}

inline std::vector<ImVec4>& HandDrawnPopupRects() { static std::vector<ImVec4> v; return v; }
inline void TrackHandDrawnPopupRect(float minx, float miny, float maxx, float maxy)
{
    auto& v = HandDrawnPopupRects();
    if (v.size() < 64)
        v.push_back(ImVec4(minx, miny, maxx, maxy));
}
inline void ClearHandDrawnPopupRects() { HandDrawnPopupRects().clear(); }

inline ImU32 ColorU32(const ImVec4& color, float alpha_mul = 1.0f)
{
    ImVec4 c = color;
    c.w *= alpha_mul;
    return ImGui::GetColorU32(c);
}

inline bool Checkbox(const char* label,bool* value,const ImVec2& pos){
    auto p=ImGui::GetWindowPos()+pos+g_contentOffset;
    const char* end=strstr(label,"##");std::string name=end?std::string(label,end):label;
    auto text=UiText::Tr(name.c_str());
    ImGui::SetCursorScreenPos(p);
    bool changed=ImGui::InvisibleButton(label,ImVec2(20+ImGui::CalcTextSize(text).x,15))&&!PopupBlocking();
    if(changed)*value=!*value;UiAssets::Item(changed);
    auto d=ImGui::GetWindowDrawList();auto col=ColorU32(ImGui::IsItemHovered()?GetTheme().Accent:GetTheme().TextBright);
    d->AddRect(p+ImVec2(0,1),p+ImVec2(12,13),col,0,0,1);
    if(*value)d->AddRectFilled(p+ImVec2(3,4),p+ImVec2(9,10),ColorU32(GetTheme().Accent));
    d->AddText(GetFonts().CascadiaMonoBL,16,p+ImVec2(20,0),ColorU32(GetTheme().Text),text);
    return changed;
}

inline bool ParseHexColor(const char* text, ImVec4& out) {
    if (!text)
        return false;
    while (*text == ' ' || *text == '\t')
        ++text;
    if (*text == '#')
        ++text;
    size_t len = 0;
    while (text[len] != '\0' && len < 9)
        ++len;
    if ((len != 6 && len != 8) || text[len] != '\0')
        return false;
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    unsigned int v[8];
    for (size_t i = 0; i < len; ++i) {
        int h = hexVal(text[i]);
        if (h < 0)
            return false;
        v[i] = (unsigned int)h;
    }
    out.x = (float)(v[0] * 16 + v[1]) / 255.0f;
    out.y = (float)(v[2] * 16 + v[3]) / 255.0f;
    out.z = (float)(v[4] * 16 + v[5]) / 255.0f;
    out.w = (len == 8) ? (float)(v[6] * 16 + v[7]) / 255.0f : out.w;
    return true;
}

inline bool SliderFloat(const char* label, float* value, float min_value, float max_value, const ImVec2& pos, float width, const char* text_label, const char* format);

inline bool ColorSquare(const char* id_text, ImVec4* color, const ImVec2& pos)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImVec2 base = window->Pos;
    const ImVec2 min = ImVec2(std::floor(base.x + pos.x + g_contentOffset.x), std::floor(base.y + pos.y + g_contentOffset.y));
    const ImVec2 size(14.0f, 10.0f);

    ImGui::SetCursorScreenPos(min);

    const bool pressed = ImGui::InvisibleButton(id_text, size);
    static bool colorDragging = false;
    static ImVec2 colorGrabOff{};

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(min, min + size, ColorU32(*color), 3.0f);
    draw->AddRect(min, min + size, OutlineBlack(), 3.0f, 0, 1.0f);
    if (pressed)
        ImGui::OpenPopup(id_text);

    if (ImGui::BeginPopup(id_text))
    {
        ImVec2 popPos = ImGui::GetWindowPos();
        ImVec2 popSize = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(popPos);
        ImGui::PushID("color_drag");
        ImGui::InvisibleButton("##color_drag", ImVec2(popSize.x, 10.0f));
        ImGuiID dragId = ImGui::GetItemID();
        float dragHov = AnimateFloat(dragId, ImGui::IsItemHovered(), 18.0f);
        ImDrawList* dragDraw = ImGui::GetWindowDrawList();
        for (int i = 0; i < 3; ++i) {
            float dx = popPos.x + popSize.x * 0.5f + (float)(i - 1) * 8.0f;
            dragDraw->AddCircleFilled(ImVec2(dx, popPos.y + 5.0f), 1.2f, ColorU32(LerpColor(GetTheme().Text, GetTheme().TextBright, dragHov)), 8);
        }
        if (ImGui::IsItemActivated()) {
            colorGrabOff = ImGui::GetIO().MousePos - popPos;
            colorDragging = true;
        }
        if (colorDragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                ImGui::SetWindowPos(ImGui::GetIO().MousePos - colorGrabOff, ImGuiCond_Always);
            else
                colorDragging = false;
        }
        ImGui::PopID();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GetTheme().CardBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetTheme().ControlInactive);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, GetTheme().ControlInactive);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, GetTheme().Accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, GetTheme().Accent);
        ImGui::PushStyleColor(ImGuiCol_Button, GetTheme().ControlBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetTheme().ControlInactive);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, GetTheme().ControlInactive);
        ImGui::PushStyleColor(ImGuiCol_Header, GetTheme().CardBg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetTheme().ControlInactive);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, GetTheme().ControlInactive);
        ImGui::ColorPicker4("##picker", (float*)color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopStyleColor(11);
        const Theme& ptheme = GetTheme();
        ImFont* pfont = GetFonts().InterMedium ? GetFonts().InterMedium : (GetFonts().CascadiaMonoBL ? GetFonts().CascadiaMonoBL : ImGui::GetFont());
        ImGuiWindow* pwin = ImGui::GetCurrentWindow();
        ImVec2 prel = ImGui::GetCursorScreenPos() - pwin->Pos + ImVec2(0.0f, 16.0f);
        SliderFloat("hex_opacity", &color->w, 0.0f, 1.0f, prel, 180.0f, "Opacity", "%.2f");
        if (color->w < 0.0f) color->w = 0.0f;
        if (color->w > 1.0f) color->w = 1.0f;
        char hex[16];
        ImFormatString(hex, IM_ARRAYSIZE(hex), "#%02X%02X%02X%02X",
            (int)(ImClamp(color->x, 0.0f, 1.0f) * 255.0f),
            (int)(ImClamp(color->y, 0.0f, 1.0f) * 255.0f),
            (int)(ImClamp(color->z, 0.0f, 1.0f) * 255.0f),
            (int)(ImClamp(color->w, 0.0f, 1.0f) * 255.0f));
        ImVec2 hexRel = prel + ImVec2(0.0f, 15.0f);
        ImVec2 hexMin = pwin->Pos + hexRel;
        ImVec2 hexTs = pfont->CalcTextSizeA(12.0f * g_fontScale, FLT_MAX, 0.0f, hex);
        ImGui::SetCursorScreenPos(hexMin);
        ImGui::PushID("hex_ctx_btn");
        ImGui::InvisibleButton("##hex", ImVec2(hexTs.x + 6.0f, 15.0f));
        ImGuiID hexId = ImGui::GetItemID();
        float hexHov = AnimateFloat(hexId, ImGui::IsItemHovered(), 18.0f);
        ImGui::GetWindowDrawList()->AddText(pfont, 12.0f * g_fontScale, hexMin, ColorU32(LerpColor(ptheme.Text, ptheme.TextBright, hexHov)), hex);
        static ImGuiID hexCtxOpen = 0;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            hexCtxOpen = (hexCtxOpen == hexId) ? 0 : hexId;
        bool hexCtx = (hexCtxOpen == hexId);
        float hexCtxAnim = AnimateFloat(hexId + 40, hexCtx, 18.0f);
        const float hexRowH = 16.0f;
        const float hexPad = 3.0f;
        const float hexPopW = 110.0f;
        const float hexFullH = hexPad * 2.0f + hexRowH * 2.0f;
        ImVec2 hexPopMin(hexMin.x, hexMin.y + 16.0f);
        if (hexCtx && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImRect(hexPopMin, ImVec2(hexPopMin.x + hexPopW, hexPopMin.y + hexFullH)).Contains(ImGui::GetIO().MousePos))
            hexCtxOpen = 0;
        if (hexCtxAnim > 0.01f) {
            ImDrawList* hexFg = ImGui::GetForegroundDrawList();
            hexFg->PushClipRect(hexPopMin, ImVec2(hexPopMin.x + hexPopW, hexPopMin.y + hexFullH * hexCtxAnim), true);
            ImVec2 hexBoxMax = ImVec2(hexPopMin.x + hexPopW, hexPopMin.y + hexFullH);
            hexFg->AddRectFilled(hexPopMin, hexBoxMax, ColorU32(ptheme.ControlBg, hexCtxAnim), 0.0f);
            hexFg->AddRect(hexPopMin, hexBoxMax, OutlineBlack(), 0.0f, 0, 1.0f);
            const char* hexOpts[2] = {"Copy Hex", "Paste Hex"};
            for (int i = 0; i < 2; ++i) {
                ImVec2 iMin(hexPopMin.x + 2.0f, hexPopMin.y + hexPad + hexRowH * i);
                ImVec2 iMax(hexPopMin.x + hexPopW - 2.0f, iMin.y + hexRowH);
                bool hov = ImRect(iMin, iMax).Contains(ImGui::GetIO().MousePos);
                ImGui::PushID(100 + i);
                ImGuiID iid = ImGui::GetID("hex_opt");
                float ih = AnimateFloat(iid, hov, 18.0f);
                ImVec4 parsed{};
                bool valid = (i == 1) ? ParseHexColor(ImGui::GetClipboardText(), parsed) : true;
                if (ih > 0.01f)
                    hexFg->AddRectFilled(iMin, iMax, ColorU32(LerpColor(ptheme.ControlBg, ptheme.ControlInactive, ih * 0.8f), hexCtxAnim), 0.0f);
                hexFg->AddText(pfont, 12.0f * g_fontScale, ImVec2(iMin.x + 4.0f, iMin.y + 2.0f), ColorU32(valid ? LerpColor(ptheme.Text, ptheme.TextBright, ih * 0.35f) : ImVec4(0.45f, 0.45f, 0.45f, 1.0f), hexCtxAnim), UiText::Tr(hexOpts[i]));
                if (hexCtx && hexCtxAnim > 0.70f && hov && valid && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (i == 0)
                        ImGui::SetClipboardText(hex);
                    else
                        *color = parsed;
                    hexCtxOpen = 0;
                }
                ImGui::PopID();
            }
            hexFg->PopClipRect();
        }
        ImGui::PopID();
        popPos = ImGui::GetWindowPos();
        popSize = ImGui::GetWindowSize();
        ImGui::EndPopup();
        ImDrawList* popFg = ImGui::GetForegroundDrawList();
        popFg->AddRect(popPos, popPos + popSize, OutlineBlack(), 0.0f, 0, 1.0f);
        popFg->AddRect(popPos + ImVec2(1.0f, 1.0f), popPos + popSize - ImVec2(1.0f, 1.0f), OutlineInner(), 0.0f, 0, 1.0f);
    } else {
        colorDragging = false;
    }

    return pressed;
}

inline const char* KeyName(int key)
{
    switch (key)
    {
    case 0: return "None";
    case 0x01: return "Left Mouse";
    case 0x02: return "Right Mouse";
    case 0x04: return "Middle Mouse";
    case 0x05: return "Mouse 4";
    case 0x06: return "Mouse 5";
    case 0x10: return "Shift";
    case 0x11: return "Ctrl";
    case 0x12: return "Alt";
    case 0x20: return "Space";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_ESCAPE: return "Escape";
    case VK_RETURN: return "Enter";
    case VK_TAB: return "Tab";
    case VK_BACK: return "Backspace";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    case VK_LWIN: return "Left Win";
    case VK_RWIN: return "Right Win";
    default: break;
    }
    if (key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END)
        return ImGui::GetKeyName((ImGuiKey)key);

    static char name[128];

    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        ImFormatString(name, IM_ARRAYSIZE(name), "%c", key);
    else if(key>=VK_F1 && key<=VK_F24)
        ImFormatString(name,IM_ARRAYSIZE(name),"F%d",key-VK_F1+1);
    else if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9)
        ImFormatString(name,IM_ARRAYSIZE(name),"Numpad %d",key-VK_NUMPAD0);
    else
    {
        const auto scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC_EX);
        const LONG packed=LONG((scan&255)<<16)|((scan&0xff00)?(1L<<24):0);
        wchar_t wide[64]{};
        if(!GetKeyNameTextW(packed,wide,64)||!WideCharToMultiByte(CP_UTF8,0,wide,-1,name,IM_ARRAYSIZE(name),nullptr,nullptr))
            ImFormatString(name,IM_ARRAYSIZE(name),"Unknown");
    }

    return name;
}

inline bool Keybind(const char* label, int* key, const ImVec2& pos, const ImVec2& size = ImVec2(68.0f, 13.0f), int* mode = nullptr, bool chooseMode = true)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 base = window->Pos;
    const ImVec2 min = ImVec2(std::floor(base.x + pos.x + g_contentOffset.x), std::floor(base.y + pos.y + g_contentOffset.y));
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton(label, size);
    const ImGuiID id = ImGui::GetItemID();
    const bool right_clicked = chooseMode && ImGui::IsItemClicked(ImGuiMouseButton_Right);

    static std::unordered_map<ImGuiID, int> s_keybind_modes;
    int& current_mode = mode ? *mode : s_keybind_modes[id];

    static ImGuiID context_open_id = 0;
    if (right_clicked)
    {
        context_open_id = (context_open_id == id) ? 0 : id;
    }

    ImGuiID& waiting_id = KeyCaptureId();
    static bool wait_mouse_release = false;
    static bool wait_side_release[2] = { false, false };
    if (pressed && !PopupBlocking())
    {
        context_open_id = 0;
        waiting_id = id;
        wait_mouse_release = true;
        ImGui::SetActiveID(id, window);
    }

    const bool active = waiting_id == id;
    if (active)
    {
        ImGuiIO& io = ImGui::GetIO();
        bool any_mouse_down = false;
        for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i)
            any_mouse_down |= io.MouseDown[i];
        if (!any_mouse_down)
            wait_mouse_release = false;

        for (int side = 0; side < 2; ++side)
        {
            const int idx = 3 + side;
            if (ImGui::IsMouseClicked(idx))
            {
                *key = side == 0 ? 0x05 : 0x06;
                waiting_id = 0;
                ImGui::ClearActiveID();
                break;
            }
            wait_side_release[side] = io.MouseDown[idx];
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            *key = 0;
            waiting_id = 0;
        ImGui::ClearActiveID();
        }

        if (!wait_mouse_release && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            *key = 0x01;
            waiting_id = 0;
            ImGui::ClearActiveID();
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            *key = 0x02;
            waiting_id = 0;
            ImGui::ClearActiveID();
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            *key = 0x04;
            waiting_id = 0;
            ImGui::ClearActiveID();
        }

        if (waiting_id == id && !wait_mouse_release && !wait_side_release[0] && !wait_side_release[1])
        {
            for (int key_code = ImGuiKey_NamedKey_BEGIN; key_code < ImGuiKey_NamedKey_END; ++key_code)
            {
                ImGuiKey imgui_key = (ImGuiKey)key_code;
                if (ImGui::IsKeyPressed(imgui_key) && imgui_key != ImGuiKey_Escape)
                {
                    *key = key_code;
                    waiting_id = 0;
                    ImGui::ClearActiveID();
                    break;
                }
            }
        }
    }

    const float active_anim = AnimateFloat(id, active || ImGui::IsItemHovered(), 16.0f);
    const Theme& theme = GetTheme();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, min + size, ColorU32(LerpColor(theme.KeybindBg, theme.ControlInactive, active_anim * 0.35f)), 0.0f);
    draw->AddRect(min, min + size, OutlineBlack(), 0.0f, 0, 1.0f);
    draw->AddRect(min + ImVec2(1.0f, 1.0f), min + size - ImVec2(1.0f, 1.0f), OutlineInner(), 0.0f, 0, 1.0f);

    const Fonts& fonts = GetFonts();
    ImFont* font = fonts.InterMedium ? fonts.InterMedium : (fonts.CascadiaMonoBL ? fonts.CascadiaMonoBL : ImGui::GetFont());
    const float font_size = 16.0f;
    const char* text = active ? "..." : (current_mode == 2 && *key == 0 ? "Always" : KeyName(*key));
    text=UiText::Tr(text);
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
    draw->AddText(font, font_size, ImVec2(min.x + (size.x - text_size.x) * 0.5f, min.y + (size.y - text_size.y) * 0.5f), ColorU32(ImVec4(0.839f, 0.839f, 0.839f, 1.0f)), text);

    const bool context_open = (context_open_id == id);
    const float context_anim = AnimateFloat(id + 20, context_open, 18.0f);
    const float popup_w = size.x;
    const float row_height = 16.0f;
    const float popup_padding = 3.0f;
    const float full_height = popup_padding * 2.0f + row_height * 3.0f;
    const float visible_height = full_height * context_anim;

    const ImVec2 popup_min(min.x, min.y + size.y + 2.0f);
    const ImVec2 popup_max(popup_min.x + popup_w, popup_min.y + visible_height);
    const ImRect total_rect(min, ImVec2(min.x + popup_w, popup_min.y + full_height));

    if (context_open && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !total_rect.Contains(ImGui::GetIO().MousePos))
    {
        context_open_id = 0;
    }

    if (context_anim > 0.01f)
    {

        TrackHandDrawnPopupRect(popup_min.x, popup_min.y,
            popup_min.x + popup_w, popup_min.y + full_height);
        ImDrawList* overlay = ImGui::GetForegroundDrawList();
        overlay->PushClipRect(popup_min, popup_max, true);
        const ImVec2 popup_box_max = ImVec2(popup_min.x + popup_w, popup_min.y + full_height);
        overlay->AddRectFilled(popup_min, popup_box_max, ColorU32(theme.ControlBg, context_anim), 0.0f);

        overlay->AddRect(popup_min, popup_box_max, OutlineBlack(), 0.0f, 0, 1.0f);
        overlay->AddRect(popup_min + ImVec2(1.0f, 1.0f), popup_box_max - ImVec2(1.0f, 1.0f), ImGui::GetColorU32(IM_COL32(52, 52, 56, (int)(255 * context_anim))), 0.0f, 0, 1.0f);

        struct ModeDef { const char* label; int value; };
        static const ModeDef mode_list[3] = {
            { "Toggle", 1 },
            { "Hold",   0 },
            { "Always", 2 }
        };

        for (int i = 0; i < 3; ++i)
        {
            const ImVec2 item_min(popup_min.x + 2.0f, popup_min.y + popup_padding + row_height * i);
            const ImVec2 item_max(popup_min.x + popup_w - 2.0f, item_min.y + row_height);
            const ImRect item_rect(item_min, item_max);
            const bool item_hovered = item_rect.Contains(ImGui::GetIO().MousePos);
            const bool item_pressed = context_open && context_anim > 0.70f && item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            ImGui::PushID(i);
            const ImGuiID item_id = ImGui::GetID("kb_mode");
            const float item_hover = AnimateFloat(item_id, item_hovered, 18.0f);
            const bool is_current = (current_mode == mode_list[i].value);
            const float item_selected = AnimateFloat(item_id + 1, is_current, 18.0f);
            const float item_appear = ImClamp((context_anim - i * 0.08f) / 0.45f, 0.0f, 1.0f);

            const ImVec4 row_color = LerpColor(theme.ControlBg, theme.ControlInactive, item_hover * 0.8f + item_selected * 0.4f);
            if (item_hover > 0.01f || item_selected > 0.01f)
                overlay->AddRectFilled(item_min, item_max, ColorU32(row_color, context_anim), 0.0f);

            const ImVec4 base_text_color = is_current ? theme.Accent : theme.Text;
            const ImVec4 text_color = LerpColor(base_text_color, theme.TextBright, item_hover * 0.35f);
            const ImVec2 text_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, UiText::Tr(mode_list[i].label));
            const ImVec2 text_pos(item_min.x + 4.0f, item_min.y + (row_height - text_sz.y) * 0.5f);
            overlay->AddText(font, font_size, text_pos, ColorU32(text_color, item_appear), UiText::Tr(mode_list[i].label));

            if (item_pressed)
            {
                current_mode = mode_list[i].value;
                if (mode) *mode = mode_list[i].value;
                context_open_id = 0;
                ComboClosedFrame() = ImGui::GetFrameCount();
            }
            ImGui::PopID();
        }

        overlay->PopClipRect();
    }
    if (context_open_id == id)
        ComboOpenId() = id;
    else if (ComboOpenId() == id)
        ComboOpenId() = 0;
    return pressed;
}

inline float WheelStep(const char* format){
    const char* dot=std::strchr(format,'.');int precision=dot?std::atoi(dot+1):0;
    return std::pow(10.f,-float(std::clamp(precision,0,6)));
}
inline bool WheelAdjust(float* value,float lo,float hi,const char* format){
    auto& io=ImGui::GetIO();
    if(!ImGui::IsItemHovered()||ImGui::IsAnyItemActive())return false;
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID(),ImGuiInputFlags_LockThisFrame);
    if(io.MouseWheel==0)return false;
    const float next=std::clamp(*value+io.MouseWheel*WheelStep(format)*(io.KeyShift?.1f:1.f),lo,hi);
    if(next==*value)return false;*value=next;return true;
}
inline bool SliderFloat(const char* label,float* value,float min_value,float max_value,const ImVec2& pos,float width,const char* text_label,const char* format="%.0f"){
    const auto p=ImGui::GetWindowPos()+pos+g_contentOffset;
    ImGui::PushID(label);
    ImGui::GetWindowDrawList()->AddText(GetFonts().CascadiaMonoBL,16,p-ImVec2(0,17),ColorU32(GetTheme().Text),UiText::Tr(text_label));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(2,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,GetTheme().ControlBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,GetTheme().ControlInactive);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,GetTheme().ControlInactive);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(0,0,0,0));
    ImGui::SetCursorScreenPos(p-ImVec2(0,2));ImGui::SetNextItemWidth(width-83);
    bool changed=ImGui::SliderFloat("##bar",value,min_value,max_value,"",ImGuiSliderFlags_AlwaysClamp);
    changed=WheelAdjust(value,min_value,max_value,format)||changed;
    {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();auto d=ImGui::GetWindowDrawList();float x=ImLerp(a.x+3,b.x-3,ImClamp((*value-min_value)/(max_value-min_value),0.f,1.f));
    d->AddRectFilled(ImVec2(a.x,(a.y+b.y)*.5f-1),ImVec2(x,(a.y+b.y)*.5f+1),ColorU32(GetTheme().Accent));d->AddRectFilled(ImVec2(x-1,a.y+2),ImVec2(x+1,b.y-2),ColorU32(GetTheme().TextBright));}UiAssets::Item();
    ImGui::SetCursorScreenPos(p+ImVec2(width-76,-2));ImGui::SetNextItemWidth(76);
    std::string numeric=format;const auto end=numeric.find('f');if(end!=std::string::npos)numeric.resize(end+1);else numeric="%.3f";
    if(ImGui::InputFloat("##value",value,0,0,numeric.c_str())){
        if(!std::isfinite(*value))*value=min_value;
        *value=ImClamp(*value,min_value,max_value);changed=true;
    }
    changed=WheelAdjust(value,min_value,max_value,format)||changed;
    ImGui::PopStyleColor(5);ImGui::PopStyleVar();ImGui::PopID();return changed;
}

struct ComboTextAnimation
{
    std::string Previous;
    std::string Current;
    float Blend = 1.0f;
};

struct SelectorShared
{

    static void DrawControl(const char* preview, bool open, const ImVec2& min, const ImVec2& size,
        const char* text_label, ImGuiID id, float font_size, ImFont* font)
    {
        const bool hovered = ImGui::IsItemHovered();
        const float hover = AnimateFloat(id, hovered, 18.0f);
        const float open_anim = AnimateFloat(id + 10, open, 18.0f);
        const Theme& theme = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(min, min + size, ColorU32(LerpColor(theme.ControlBg, theme.ControlInactive, hover * 0.35f)), 0.0f);
        draw->AddRect(min, min + size, ColorU32(GetTheme().TextBright), 0.0f, 0, 1.0f);
        if (text_label)
            draw->AddText(font, font_size, ImVec2(min.x, min.y - font_size - 3.0f), ColorU32(theme.Text), text_label);

        ImVec2 preview_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, preview);
        const ImVec2 preview_pos(min.x + 3.0f, min.y + (size.y - preview_sz.y) * 0.5f);
        const float dir = open ? -1.0f : 1.0f;
        const ImVec2 arrow_c(min.x + size.x - 12.0f, min.y + size.y * 0.5f);
        draw->AddTriangleFilled(arrow_c + ImVec2(-4.0f, -1.5f * dir),
            arrow_c + ImVec2(4.0f, -1.5f * dir), arrow_c + ImVec2(0.0f, 1.5f * dir),
            ColorU32(theme.Text, 0.35f + 0.65f * open_anim));
        draw->AddText(font, font_size, preview_pos, ColorU32(theme.Text), preview);
    }

    static ImVec2 PlacePopup(const ImVec2& min, const ImVec2& size, float row_height,
        int items_count, float& out_visible_h)
    {
        const float popup_padding = 3.0f;
        const float content_h = row_height * items_count;

        const float max_popup_h = (std::max)(160.0f, ImGui::GetIO().DisplaySize.y * 0.82f);
        const float visible_h = (std::min)(content_h, max_popup_h - popup_padding * 2.0f);
        ImVec2 boxMin(min.x, min.y + size.y + 2.0f);
        if (boxMin.y + visible_h + popup_padding * 2.0f > ImGui::GetIO().DisplaySize.y && min.y - 2.0f - visible_h >= 0.0f)
            boxMin.y = min.y - 2.0f - visible_h - popup_padding * 2.0f;
        out_visible_h = visible_h;
        return boxMin;
    }

    static void Rows(const char* const items[], int items_count, int* current_item,
        float row_height, float visible_h, ImFont* font, float font_size, bool& changed, bool& close_now)
    {
        const Theme& theme = GetTheme();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushFont(font, font_size);
        for (int i = 0; i < items_count; ++i)
        {
            ImGui::PushID(i);
            const bool is_current = i == *current_item;
            if (ImGui::Selectable("##row", is_current, ImGuiSelectableFlags_SelectOnRelease, ImVec2(0.0f, row_height)))
            {
                *current_item = i;
                close_now = true;
                changed = true;UiAssets::Sound(true);
            }
            UiAssets::Item();ImGui::PopID();

            const ImVec2 r_min = ImGui::GetItemRectMin();
            const ImVec2 r_max = ImGui::GetItemRectMax();
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hovered)
                dl->AddRectFilled(r_min, r_max, ColorU32(LerpColor(theme.ControlBg, theme.ControlInactive, 0.5f)), 3.0f);
            const ImVec4 text_color = is_current ? theme.Accent : (hovered ? theme.TextBright : theme.Text);
            dl->AddText(font, font_size, ImVec2(r_min.x + 3.0f, r_min.y + (row_height - font_size) * 0.5f), ColorU32(text_color), items[i]);
        }
        ImGui::Dummy(ImVec2(1,row_height));
        ImGui::PopFont();
        ImGui::PopStyleVar();
    }
};

inline bool Combo(const char* label, int* current_item, const char* const items[], int items_count, const ImVec2& pos, float width, const char* text_label)
{
    std::vector<const char*> translated;
    if(strncmp(label,"cfg_",4)!=0){for(int i=0;i<items_count;++i)translated.push_back(UiText::Tr(items[i]));items=translated.data();}
    text_label=UiText::Tr(text_label);

    const float font_size = 16.0f;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 base = window->Pos;
    const ImVec2 min = ImVec2(std::floor(base.x + pos.x + g_contentOffset.x), std::floor(base.y + pos.y + g_contentOffset.y));
    const ImVec2 size(width, 24.0f);
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton("##combo_preview", size);
    const ImGuiID id = ImGui::GetItemID();

    static ImGuiID open_id = 0;
    bool toggle_close = false;
    if (pressed)
    {
        if (open_id == id)
            toggle_close = true;
        else
            ImGui::OpenPopup("##primcombo");
        open_id = toggle_close ? 0 : id;
    }
    const bool open = open_id == id;

    const Fonts& fonts = GetFonts();
    ImFont* font = fonts.InterMedium ? fonts.InterMedium : (fonts.CascadiaMonoBL ? fonts.CascadiaMonoBL : ImGui::GetFont());
    const Theme& theme = GetTheme();

    const char* preview = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";
    static std::unordered_map<ImGuiID, ComboTextAnimation> text_animations;
    ComboTextAnimation& text_anim = text_animations[id];
    if (text_anim.Current.empty())
        text_anim.Current = preview;
    if (text_anim.Current != preview)
    {
        text_anim.Previous = text_anim.Current;
        text_anim.Current = preview;
        text_anim.Blend = 0.0f;
    }
    text_anim.Blend = ImLerp(text_anim.Blend, 1.0f, ImClamp(ImGui::GetIO().DeltaTime * 14.0f, 0.0f, 1.0f));
    SelectorShared::DrawControl(text_anim.Current.c_str(), open, min, size, text_label, id, font_size, font);
    if (!text_anim.Previous.empty() && text_anim.Blend < 0.98f)
    {
        ImVec2 preview_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text_anim.Previous.c_str());
        ImGui::GetWindowDrawList()->AddText(font, font_size,
            ImVec2(min.x + 3.0f, min.y + (size.y - preview_sz.y) * 0.5f),
            ColorU32(theme.Text, 1.0f - text_anim.Blend), text_anim.Previous.c_str());
    }

    bool changed = false;
    const float row_height = 24.0f;
    const float popup_padding = 3.0f;
    float visible_h = 0.0f;
    const ImVec2 boxMin = SelectorShared::PlacePopup(min, size, row_height, items_count, visible_h);

    if (open)
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, theme.ControlBg);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, popup_padding));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
        ImGui::SetNextWindowPos(boxMin);
        ImGui::SetNextWindowSize(ImVec2(width, visible_h + popup_padding * 2.0f));
        if (ImGui::BeginPopup("##primcombo"))
        {
            if (toggle_close)
            {
                ImGui::CloseCurrentPopup();
            }
            else
            {
                ImGui::BeginChild("##rows", ImVec2(0.0f, visible_h), ImGuiChildFlags_None);
                bool close_now = false;
                SelectorShared::Rows(items, items_count, current_item,
                    row_height, visible_h, font, font_size, changed, close_now);
                ImGui::EndChild();
                if (close_now)
                {
                    open_id = 0;
                    ComboClosedFrame() = ImGui::GetFrameCount();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        else if (open_id == id)
        {
            open_id = 0;
            ComboClosedFrame() = ImGui::GetFrameCount();
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(4);
    }
    if (open_id == id)
        ComboOpenId() = id;
    else if (ComboOpenId() == id)
    {
        ComboOpenId() = 0;
        ComboClosedFrame() = ImGui::GetFrameCount();
    }
    ImGui::PopID();
    return changed;
}

}
