#pragma once
#include <d3d11.h>
#include <string>

namespace SadblobVideo {
constexpr int ThemePreset = 15;
constexpr int CustomPreset = 16;
constexpr int StyleCount = 7;
const char* StyleName(int index);
bool HasBundledClips();
void SetCustomPath(const std::wstring& path);
void Update(ID3D11Device* device, ID3D11DeviceContext* context, bool active, int style);
ID3D11ShaderResourceView* Texture();
float AspectRatio();
bool MissingFile();
void Shutdown();
}
