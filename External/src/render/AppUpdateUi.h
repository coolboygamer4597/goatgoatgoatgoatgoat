#pragma once
#include "../core/app/AppUpdates.h"
#include "RetroUi.h"
#include "SkinDownloadsUi.h"
namespace AppUpdateUi {
inline ImVec2 Scaled(float x,float y,float scale){return ImVec2(x*scale,y*scale); }
inline bool Draw(float scale=1.f,bool permitted=true){
    if(SkinDownloadsUi::Draw(scale,permitted))return false;
    auto state=AppUpdates::Get();
    if(state.phase==AppUpdates::Phase::Ready)return AppUpdates::Launch();
    const bool offer=state.phase==AppUpdates::Phase::Available && (!state.suppressed||state.manual);
    const bool notice=state.manual && (state.phase==AppUpdates::Phase::Error || (state.phase==AppUpdates::Phase::Idle&&!state.message.empty()));
    const bool downloading=state.phase==AppUpdates::Phase::Downloading;
    if(!permitted)return false;
    if((offer||notice||downloading)&&!ImGui::IsPopupOpen("Application update"))ImGui::OpenPopup("Application update");
    const auto viewport=ImGui::GetWindowViewport();const ImVec2 center(viewport->Pos.x+viewport->Size.x*.5f,viewport->Pos.y+viewport->Size.y*.5f);
    const ImVec2 size(610*scale,300*scale);ImGui::SetNextWindowSize(size,ImGuiCond_Always);ImGui::SetNextWindowPos(center,ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,IM_COL32(18,18,23,255));
    if(ImGui::BeginPopupModal("Application update",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove)){
        auto position=ImGui::GetWindowPos();RetroUi::Frame(ImGui::GetWindowDrawList(),position,ImVec2(position.x+size.x,position.y+size.y));
        ImGui::SetCursorPos(Scaled(24,20,scale));ImGui::TextUnformatted(downloading?"Downloading update":offer?"An update is available":"Application updates");
        ImGui::SetCursorPos(Scaled(24,52,scale));ImGui::Text("Installed: %s",AppUpdates::CurrentVersion());
        if(offer||downloading){ImGui::SameLine(260*scale);ImGui::Text("Newest: %s",state.version.c_str());}
        ImGui::SetCursorPos(Scaled(24,85,scale));ImGui::BeginChild("update_details",Scaled(562,120.f,scale));
        ImGui::TextWrapped("%s",offer?state.notes.c_str():state.message.c_str());
        if(downloading)ImGui::ProgressBar(state.progress,ImVec2(-1,18*scale),nullptr);
        ImGui::EndChild();
        if(!downloading)ImGui::SetCursorPos(Scaled(24,235.f,scale));
        if(offer){if(RetroUi::Choice("UPDATE NOW",false,Scaled(270,42,scale))){AppUpdates::Download();}ImGui::SameLine(316*scale);if(RetroUi::Choice("SKIP THIS VERSION",false,Scaled(270,42,scale))){AppUpdates::Skip();ImGui::CloseCurrentPopup();}}
        else if(!downloading){if(RetroUi::Choice("CLOSE",false,Scaled(562,42,scale))){AppUpdates::Skip();ImGui::CloseCurrentPopup();}}
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();return false;
}
inline void LoaderControls(float scale,bool enabled){
    auto state=AppUpdates::Get();ImGui::BeginDisabled(!enabled||state.phase==AppUpdates::Phase::Downloading);
    ImGui::SetCursorPos(Scaled(35,352,scale));if(RetroUi::Choice(state.automatic?"Check updates on startup: ON":"Check updates on startup: OFF",state.automatic,Scaled(300,18,scale)))AppUpdates::SetAutomatic(!state.automatic);
    ImGui::SetCursorPos(Scaled(369,352,scale));if(RetroUi::Choice(state.phase==AppUpdates::Phase::Checking?"CHECKING...":"CHECK FOR UPDATES",false,Scaled(275,18,scale)))AppUpdates::Check();ImGui::EndDisabled();
}
}
