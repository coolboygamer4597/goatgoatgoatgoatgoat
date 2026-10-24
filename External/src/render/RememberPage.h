#pragma once
#include "menu/library.h"
namespace MenuNavigation {
inline bool RememberPage(bool& remember) {
 ImGui::SetCursorPos({700,606});
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4,4});
 ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing,{8,0});
 ImGui::PushStyleColor(ImGuiCol_CheckMark,variables::Theme::accent);
 ImGui::PushStyleColor(ImGuiCol_Text,variables::Theme::textBright);
 const bool changed=ImGui::Checkbox(UiText::Tr("Remember this page"),&remember);
 ImGui::PopStyleColor(2);ImGui::PopStyleVar(2);
 return changed;
}
}
