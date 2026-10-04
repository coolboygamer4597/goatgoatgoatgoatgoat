#include "music_host_bind.h"
#include <map>

namespace {

struct SpringState { float value = 0.0f; float velocity = 0.0f; };
std::unordered_map<ImGuiID, float> g_anims;
std::unordered_map<ImGuiID, SpringState> g_springs;

HWND        g_window = nullptr;
ID3D11Device* g_device = nullptr;
ImFont*     g_regular = nullptr;
ImFont*     g_bold = nullptr;
bool        g_fullscreen = false;
WINDOWPLACEMENT g_prevPlacement{ sizeof(WINDOWPLACEMENT) };
LONG_PTR    g_prevStyle = 0;

}

namespace MusicHost {
void Bind(HWND window, ID3D11Device* device, ImFont* regular, ImFont* bold) {
    g_window = window;
    g_device = device;
    g_regular = regular;
    g_bold = bold;
}
}

namespace music_host {

ImVec2 Measure(ImFont* font, float size, const char* text) {
    ImFont* selected = font ? font : ImGui::GetFont();
    return selected->CalcTextSizeA(size, FLT_MAX, 0.0f, text ? text : "");
}

void DrawText(ImDrawList* drawList, ImFont* font, float size,
              ImVec2 position, ImU32 color, const char* text) {
    drawList->AddText(font ? font : ImGui::GetFont(), size, position, color,
                      text ? text : "");
}

void DrawShadow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding,
                int layers, float spread, float strength) {

    (void)drawList; (void)min; (void)max; (void)rounding;
    (void)layers; (void)spread; (void)strength;
}

namespace animation {

float Anim(ImGuiID id, bool enabled, float speed) {
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    const float target = enabled ? 1.0f : 0.0f;
    auto [entry, inserted] = g_anims.try_emplace(id, target);
    entry->second += (target - entry->second) * (1.0f - std::exp(-speed * delta));
    return entry->second;
}

float SpringF(ImGuiID id, float target, float speed, float damping) {
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    speed = std::max(speed, 0.01f);
    damping = std::max(damping, 0.0f);

    auto [entry, inserted] = g_springs.try_emplace(id, SpringState{ target, 0.0f });
    SpringState& state = entry->second;
    if (inserted) return state.value;

    const float previous = state.value;
    const float omegaSquared = speed * speed;
    const float dampingTerm = 1.0f + 2.0f * delta * damping * speed;
    const float targetTerm = delta * delta * omegaSquared;
    const float inverse = 1.0f / (dampingTerm + targetTerm);
    state.value = (dampingTerm * previous + delta * state.velocity +
                   targetTerm * target) * inverse;
    state.velocity = (state.velocity + delta * omegaSquared *
                      (target - previous)) * inverse;
    if (std::abs(target - state.value) < 0.0005f &&
        std::abs(state.velocity) < 0.0005f) {
        state = { target, 0.0f };
    }
    return state.value;
}

void SetSpring(ImGuiID id, float value) {
    g_springs[id] = { value, 0.0f };
}

float ClickBounce(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return 1.0f;

    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.28f;
    if (entry->second >= duration) {
        entry->second = -1.0f;
        return 1.0f;
    }
    const float progress = entry->second / duration;
    const float amplitude = 0.16f * std::exp(-5.0f * progress);
    return 1.0f - amplitude * std::cos(9.0f * progress);
}

float ClickGlow(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return -1.0f;
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.42f;
    if (entry->second >= duration) { entry->second = -1.0f; return -1.0f; }
    return entry->second / duration;
}

float PressPulse(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return 0.0f;
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.22f;
    if (entry->second >= duration) { entry->second = -1.0f; return 0.0f; }
    const float p = entry->second / duration;
    return (1.0f - p) * (1.0f - p);
}

}

namespace overlay {

HWND GetOverlayWindow() { return g_window; }
void* GetD3DDevice() { return g_device; }

void ToggleFullscreenWindow() {
    HWND h = g_window;
    if (!h) return;

    g_fullscreen = !g_fullscreen;
}

bool IsFullscreenWindow() { return g_fullscreen; }
ImFont* GetFont(int index) {
    if (index == 0) return g_regular;
    if (index == 1) return g_bold;
    return nullptr;
}
ImFont* GetMusicRegularFont() { return g_regular; }
ImFont* GetMusicBoldFont() { return g_bold; }

}
}
