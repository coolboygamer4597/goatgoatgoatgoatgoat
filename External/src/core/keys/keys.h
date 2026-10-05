#pragma once
#include <windows.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "../../../ext/imgui/imgui.h"
#include "../variables/variables.h"

namespace Keys {
inline bool IsKeyPressed(int key) {
    if (key <= 0)
        return false;
    if (key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END) {

        int vk = 0;
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z) vk = 'A' + (key - ImGuiKey_A);
        else if (key >= ImGuiKey_0 && key <= ImGuiKey_9) vk = '0' + (key - ImGuiKey_0);
        else if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) vk = VK_F1 + (key - ImGuiKey_F1);
        else if (key == ImGuiKey_MouseLeft) vk=VK_LBUTTON;
        else if (key == ImGuiKey_MouseRight) vk=VK_RBUTTON;
        else if (key == ImGuiKey_MouseMiddle) vk=VK_MBUTTON;
        else if (key == ImGuiKey_MouseX1) vk=VK_XBUTTON1;
        else if (key == ImGuiKey_MouseX2) vk=VK_XBUTTON2;
        else if (key == ImGuiKey_PrintScreen) vk=VK_SNAPSHOT;
        else if (key == ImGuiKey_Pause) vk=VK_PAUSE;
        else if (key == ImGuiKey_NumLock) vk=VK_NUMLOCK;
        else if (key == ImGuiKey_ScrollLock) vk=VK_SCROLL;
        else if (key == ImGuiKey_Insert) vk = VK_INSERT;
        else if (key == ImGuiKey_Delete) vk = VK_DELETE;
        else if (key == ImGuiKey_Home) vk = VK_HOME;
        else if (key == ImGuiKey_End) vk = VK_END;
        else if (key == ImGuiKey_PageUp) vk = VK_PRIOR;
        else if (key == ImGuiKey_PageDown) vk = VK_NEXT;
        else if (key == ImGuiKey_Space) vk = VK_SPACE;
        else if (key == ImGuiKey_Tab) vk = VK_TAB;
        else if (key == ImGuiKey_Escape) vk = VK_ESCAPE;
        else if (key == ImGuiKey_Enter) vk = VK_RETURN;
        else if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) vk = VK_SHIFT;
        else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl) vk = VK_CONTROL;
        else if (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt) vk = VK_MENU;
        else if (key == ImGuiKey_CapsLock) vk = VK_CAPITAL;
        else if (key == ImGuiKey_Backspace) vk = VK_BACK;
        else if (key == ImGuiKey_LeftArrow) vk = VK_LEFT;
        else if (key == ImGuiKey_RightArrow) vk = VK_RIGHT;
        else if (key == ImGuiKey_UpArrow) vk = VK_UP;
        else if (key == ImGuiKey_DownArrow) vk = VK_DOWN;
        else if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) vk=VK_NUMPAD0+(key-ImGuiKey_Keypad0);
        else if (key == ImGuiKey_KeypadEnter) vk=VK_RETURN;
        else if (key == ImGuiKey_KeypadAdd) vk=VK_ADD;
        else if (key == ImGuiKey_KeypadSubtract) vk=VK_SUBTRACT;
        else if (key == ImGuiKey_KeypadMultiply) vk=VK_MULTIPLY;
        else if (key == ImGuiKey_KeypadDivide) vk=VK_DIVIDE;
        else if (key == ImGuiKey_KeypadDecimal) vk=VK_DECIMAL;
        else if (key == ImGuiKey_GraveAccent) vk=VK_OEM_3;
        else if (key == ImGuiKey_Minus) vk=VK_OEM_MINUS;
        else if (key == ImGuiKey_Equal) vk=VK_OEM_PLUS;
        else if (key == ImGuiKey_LeftBracket) vk=VK_OEM_4;
        else if (key == ImGuiKey_RightBracket) vk=VK_OEM_6;
        else if (key == ImGuiKey_Backslash) vk=VK_OEM_5;
        else if (key == ImGuiKey_Semicolon) vk=VK_OEM_1;
        else if (key == ImGuiKey_Apostrophe) vk=VK_OEM_7;
        else if (key == ImGuiKey_Comma) vk=VK_OEM_COMMA;
        else if (key == ImGuiKey_Period) vk=VK_OEM_PERIOD;
        else if (key == ImGuiKey_Slash) vk=VK_OEM_2;
        if (vk != 0) return (GetAsyncKeyState(vk) & 0x8000) != 0;
        return false;
    }
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

inline bool Gate(int key, int mode, bool& tog, bool& was) {
    if (mode == 2) return true;
    if (key == 0) return true;

    static bool toggle_states[512]{};
    static bool last_states[512]{};
    int idx = key;
    if (key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END) {
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z) idx = 'A' + (key - ImGuiKey_A);
        else if (key >= ImGuiKey_0 && key <= ImGuiKey_9) idx = '0' + (key - ImGuiKey_0);
        else if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) idx = VK_F1 + (key - ImGuiKey_F1);
        else if (key == ImGuiKey_Space) idx = VK_SPACE;
        else if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) idx = VK_SHIFT;
        else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl) idx = VK_CONTROL;
        else if (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt) idx = VK_MENU;
        else idx = key % 512;
    }
    if (idx < 0 || idx >= 512) idx = key % 512;
    bool down = IsKeyPressed(key);
    if (mode == 1) {
        if (down && !last_states[idx]) toggle_states[idx] = !toggle_states[idx];
        last_states[idx] = down;
        tog = toggle_states[idx];
        was = last_states[idx];
        return toggle_states[idx];
    }
    last_states[idx] = down;
    was = down;
    tog = down;
    return down;
}

inline bool StreamProofOn() {
    static bool tog = false;
    static bool was = false;
    if (!variables::Misc::streamProof) {
        tog = false;
        was = false;
        return false;
    }
    return Gate(variables::Misc::streamKey, variables::Misc::streamKeyMode, tog, was);
}

inline bool SpotifyOn() {
    static bool tog = false;
    static bool was = false;
    if (!variables::Misc::spotifyPlayer) {
        tog = false;
        was = false;
        return false;
    }
    return Gate(variables::Misc::spotifyKey, variables::Misc::spotifyKeyMode, tog, was);
}

inline bool KeybindsOn() { return variables::Misc::keybinds; }
}
