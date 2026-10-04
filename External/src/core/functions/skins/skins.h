#pragma once
#include <string>
namespace WeaponPreview {struct Snapshot;}

namespace Skins {
void Tick();
std::string SavedSelection();
void RestoreSelection(const std::string& skin);
void RenderMenu();
void RenderEditor();
std::string PreviewSelection(const std::string& weapon);
void Shutdown();
WeaponPreview::Snapshot CapturePreview(const std::string& weapon,const std::string& skin);
}
