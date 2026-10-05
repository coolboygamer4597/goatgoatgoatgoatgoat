#pragma once
#include "../../ext/imgui/imgui.h"
#include <cmath>
#include "UiAssets.h"
#include "../core/variables/variables.h"

namespace RetroUi {
inline ImU32 White = IM_COL32(242,242,235,255);
inline ImU32 Gold = IM_COL32(255,222,50,255);

inline void Frame(ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 color=IM_COL32(242,242,235,255)) {
    d->AddRectFilled(p,q,ImGui::GetColorU32(variables::Theme::background));
    d->AddRect(p,q,ImGui::GetColorU32(variables::Theme::accent),0,0,1);
    d->AddRect(ImVec2(p.x+5,p.y+5),ImVec2(q.x-5,q.y-5),IM_COL32(70,70,80,255),0,0,1);
}
inline void Stars(ImDrawList* d, ImVec2 p, ImVec2 size, float time) {
    d->PushClipRect(p,ImVec2(p.x+size.x,p.y+size.y),true);
    for(int i=0;i<26;++i){
        float x=std::floor(p.x+std::fmod(i*97.0f+time*(2+i%3),size.x));
        float y=std::floor(p.y+std::fmod(i*43.0f,size.y));
        int a=35+int(35*(.5f+.5f*std::sin(time*1.4f+i)));
        d->AddRectFilled(ImVec2(x,y),ImVec2(x+2,y+2),IM_COL32(220,220,255,a));
    }
    d->PopClipRect();
}

inline bool CaptionButton(const char* id, ImVec2 p, bool close) {
    ImGui::SetCursorScreenPos(p);
    bool click=ImGui::InvisibleButton(id,ImVec2(26,23));
    auto d=ImGui::GetWindowDrawList();
    const bool held=ImGui::IsItemActive(),hover=ImGui::IsItemHovered();
    ImVec2 q(p.x+26,p.y+23);
    d->AddRectFilled(p,q,close?IM_COL32(180+(hover?30:0),55,51,255):IM_COL32(57,83+(hover?20:0),151,255));
    d->AddRect(p,q,ImGui::GetColorU32(variables::Theme::background));
    d->AddLine(ImVec2(p.x+1,p.y+1),ImVec2(q.x-2,p.y+1),held?IM_COL32(40,40,55,255):White);
    d->AddLine(ImVec2(p.x+1,p.y+1),ImVec2(p.x+1,q.y-2),held?IM_COL32(40,40,55,255):White);
    if(close){d->AddLine(ImVec2(p.x+8,p.y+6),ImVec2(p.x+18,p.y+16),White,2);d->AddLine(ImVec2(p.x+18,p.y+6),ImVec2(p.x+8,p.y+16),White,2);}
    else d->AddRectFilled(ImVec2(p.x+7,p.y+15),ImVec2(p.x+19,p.y+18),White);
    return click;
}
inline void Icon(ImDrawList* d,ImVec2 p,int kind,bool selected,bool hover){
    static const char* rows[4][13]={
      {"0000001000000","0000111110000","0001000001000","0010000000100","0100000000010","0100001000010","1100011100011","0100001000010","0100000000010","0010000000100","0001000001000","0000111110000","0000001000000"},
      {"0000011100000","0000011100000","0000011100000","0000000000000","0011111111100","0011011101100","0011011101100","0011011101100","0000011100000","0000110110000","0000110110000","0000110110000","0000110110000"},
      {"0000011100000","0010011100100","0111111111110","0011100011100","0011000001100","1110000000111","1110001000111","1110000000111","0011000001100","0011100011100","0111111111110","0010011100100","0000011100000"},
      {"0001100011000","0011110111100","0111111111110","1111111111111","1111111111111","1101111111011","0001111111000","0001111111000","0001111111000","0001111111000","0001111111000","0001111111000","0001111111000"}
    };
    p.x=std::floor(p.x);p.y=std::floor(p.y);
    const ImU32 col=IM_COL32(245,245,245,255);
    for(int y=0;y<13;++y)for(int x=0;x<13;++x)if(rows[kind%4][y][x]=='1')
        d->AddRectFilled(ImVec2(p.x+x,p.y+y),ImVec2(p.x+x+1,p.y+y+1),col);
    if(hover&&!selected)d->AddRect(ImVec2(p.x-3,p.y-3),ImVec2(p.x+16,p.y+16),col);
    if(selected)d->AddRectFilled(ImVec2(p.x,p.y+16),ImVec2(p.x+13,p.y+18),ImGui::GetColorU32(variables::Theme::accent));
}
inline bool Choice(const char* label,bool selected,ImVec2 size,int icon=-1){
    const ImVec2 p=ImGui::GetCursorScreenPos();bool clicked=ImGui::InvisibleButton(label,size);UiAssets::Item(clicked);
    auto d=ImGui::GetWindowDrawList();bool hover=ImGui::IsItemHovered();
    auto col=ImGui::GetColorU32(selected||hover?variables::Theme::accent:variables::Theme::text);
    auto bg=variables::Theme::panels;bg.w=.8f;d->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),ImGui::GetColorU32(bg));
    d->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),col,0,0,selected?2.f:1.f);
    if(icon>=0)Icon(d,ImVec2(p.x+12,p.y+(size.y-12)/2),icon,selected,hover);
    else if(selected||hover)d->AddText(ImVec2(p.x+10,p.y+(size.y-ImGui::GetFontSize())/2),col,selected?">":"-");
    d->AddText(ImVec2(p.x+34,p.y+(size.y-ImGui::CalcTextSize(label).y)/2),col,label);return clicked;
}
}
