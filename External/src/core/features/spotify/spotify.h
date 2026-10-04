#pragma once
#include "music_host_bind.h"
#include "music_player_ui.h"
#include "media.h"

namespace SpotifyPlayer {
inline void Initialize(ID3D11Device*, ID3D11DeviceContext*) {}
inline void Tick(bool) {}
inline bool WantsPointerInput(void*, bool) { return false; }
inline bool PointerInputActive() { return false; }
inline void Render(bool, bool) {}
inline void Shutdown() {}
}
