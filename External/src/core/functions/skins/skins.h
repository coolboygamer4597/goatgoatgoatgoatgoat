#pragma once
#include <string>
namespace WeaponPreview {struct Snapshot;}

namespace Skins {
void Tick();
std::string SavedSelection();
std::string SavedEditorSelection();
bool RestoreEditorSelection(const std::string& value);
void RestoreSelection(const std::string& skin);
void RenderMenu();
void RenderEditor();
bool SelectCustomSkin(bool isGlock,const std::string& skin,int texture);
bool CurrentWeaponIsGlock();
std::string PreviewSelection(const std::string& weapon);
void Shutdown();
WeaponPreview::Snapshot CapturePreview(const std::string& weapon,const std::string& skin);
}
