#pragma once
#include "menu/library.h"
#include "../core/features/SessionPresence.h"
#include "../core/variables/variables.h"
#include <imgui_internal.h>
#include <cmath>

namespace SessionPanels {
inline bool restoreFocus=false;
inline bool TakeRestoreFocus(){const bool requested=restoreFocus;restoreFocus=false;return requested;}
inline bool Visible(){return variables::Misc::spectatorCounter||(variables::Misc::moderatorAlerts&&!SessionPresence::state.Notices().empty());}
inline ImFont* Font(){auto font=imGuiCustom::GetFonts().InterBold;return font?font:ImGui::GetFont();}
inline float Clamp(float value,float maximum){return std::clamp(std::isfinite(value)?value:16.f,0.f,(std::max)(0.f,maximum));}
inline void Begin(const char* name,ImVec2 position,ImVec2 size){
    ImGui::SetNextWindowPos(position,ImGuiCond_Always);
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,7.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.035f,.043f,.06f,.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.22f,.25f,.32f,.9f));
    ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
}
inline void End(){ImGui::End();ImGui::PopStyleColor(2);ImGui::PopStyleVar(3);}
inline void Warning(ImDrawList* draw,ImVec2 center){
    const auto yellow=IM_COL32(255,203,72,255);
    draw->AddTriangleFilled(center+ImVec2(0,-10),center+ImVec2(-10,8),center+ImVec2(10,8),yellow);
    draw->AddLine(center+ImVec2(0,-4),center+ImVec2(0,1),IM_COL32(30,28,25,255),2);
    draw->AddCircleFilled(center+ImVec2(0,5),1.1f,IM_COL32(30,28,25,255));
}
inline void Draw(){
    auto viewport=ImGui::GetMainViewport();
    const ImVec2 origin=viewport->Pos,size=viewport->Size;
    if(size.x<80||size.y<80)return;
    auto font=Font();
    if(variables::Misc::spectatorCounter){
        const ImVec2 panel((std::min)(184.f,size.x),68);
        auto& x=variables::Misc::spectatorPanelX;auto& y=variables::Misc::spectatorPanelY;
        x=Clamp(x,size.x-panel.x);y=Clamp(y,size.y-panel.y);
        Begin("##spectator_counter",origin+ImVec2(x,y),panel);
        ImGui::SetCursorPos(ImVec2(0,0));ImGui::InvisibleButton("##spectator_drag",panel);
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
            x=Clamp(x+ImGui::GetIO().MouseDelta.x,size.x-panel.x);
            y=Clamp(y+ImGui::GetIO().MouseDelta.y,size.y-panel.y);
            ImGui::SetWindowPos(origin+ImVec2(x,y));
        }
        if(ImGui::IsItemDeactivated()&&!variables::menuOpen)restoreFocus=true;
        auto draw=ImGui::GetWindowDrawList();const auto p=ImGui::GetWindowPos();
        const auto accent=ImGui::GetColorU32(variables::Theme::accent);
        draw->AddRectFilled(p+ImVec2(0,10),p+ImVec2(3,58),accent,2);
        draw->AddText(font,12,p+ImVec2(14,11),IM_COL32(166,178,199,255),"Spectator team");
        const auto count=SessionPresence::state.Available()?std::to_string(SessionPresence::state.Spectators()):"--";
        draw->AddText(font,26,p+ImVec2(14,29),IM_COL32(244,247,252,255),count.c_str());
        draw->AddCircleFilled(p+ImVec2(panel.x-19,34),4,accent);
        End();
    }
    if(!variables::Misc::moderatorAlerts||SessionPresence::state.Notices().empty())return;
    const auto notice=SessionPresence::state.Notices().front();
    const auto count=SessionPresence::state.Notices().size();
    const float width=(std::min)(460.f,size.x-24);
    const float wrap=(std::max)(40.f,width-106);
    const auto title="Senior Mod+ Detected (\""+notice.username+"\")";
    const float titleHeight=font->CalcTextSizeA(16,FLT_MAX,wrap,title.c_str()).y;
    const float bodyY=18+titleHeight+15;
    const float height=(std::min)(size.y-24,bodyY+64+(count>1?20.f:0.f));
    Begin("##moderator_notice",origin+ImVec2(size.x-width-12,12),ImVec2(width,height));
    auto draw=ImGui::GetWindowDrawList();const auto p=ImGui::GetWindowPos();
    draw->AddRectFilled(p+ImVec2(0,8),p+ImVec2(3,height-8),IM_COL32(255,203,72,255),2);
    Warning(draw,p+ImVec2(23,27));Warning(draw,p+ImVec2(width-58,27));
    draw->AddText(font,16,p+ImVec2(44,18),IM_COL32(255,233,174,255),title.c_str(),nullptr,wrap);
    draw->AddText(font,14,p+ImVec2(20,bodyY),IM_COL32(211,218,232,255),"This individual can turn");
    draw->AddText(font,18,p+ImVec2(20,bodyY+23),IM_COL32(255,218,109,255),"hidden pveye");
    if(count>1){const auto pending=std::to_string(count)+" alerts pending - dismiss each with X";draw->AddText(font,11,p+ImVec2(20,height-22),IM_COL32(151,163,184,255),pending.c_str());}
    ImGui::SetCursorPos(ImVec2(width-36,10));const bool dismiss=ImGui::InvisibleButton("##dismiss_moderator",ImVec2(26,26));
    const auto center=p+ImVec2(width-23,23);
    if(ImGui::IsItemHovered())draw->AddRectFilled(center-ImVec2(12,12),center+ImVec2(12,12),IM_COL32(255,255,255,24),4);
    draw->AddLine(center-ImVec2(4,4),center+ImVec2(4,4),IM_COL32(235,240,250,255),1.7f);
    draw->AddLine(center+ImVec2(-4,4),center+ImVec2(4,-4),IM_COL32(235,240,250,255),1.7f);
    if(dismiss){
        SessionPresence::state.Dismiss(notice.sequence);
        if(!variables::menuOpen)restoreFocus=true;
    }
    End();
}
}
