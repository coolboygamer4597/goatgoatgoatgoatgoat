#pragma once
#include "menu/library.h"
#include "RetroUi.h"
namespace FeatureUi {
inline void Sections(const char* const* labels,int count,int& selected,float y=43){
    for(int i=0;i<count;++i){ImGui::SetCursorPos(ImVec2(12+i*136.f,y)+imGuiCustom::g_contentOffset);if(RetroUi::Choice(labels[i],selected==i,ImVec2(132,29))&&!imGuiCustom::PopupBlocking())selected=i;}
}
struct Column {
    float x,y,width=310;
    bool Check(const char* label,bool& value){bool changed=imGuiCustom::Checkbox(label,&value,ImVec2(x,y));y+=imGuiCustom::CheckStep()+4;return changed;}
    bool Slider(const char* id,const char* label,float& value,float low,float high,const char* format="%.2f"){bool changed=imGuiCustom::SliderFloat(id,&value,low,high,ImVec2(x,y+imGuiCustom::SliderTop()),width,label,format);y+=imGuiCustom::SliderStep()+10;return changed;}
    bool Combo(const char* id,const char* label,int& value,const char* const* choices,int count){bool changed=imGuiCustom::Combo(id,&value,choices,count,ImVec2(x,y+imGuiCustom::ComboTop()),width,label);y+=imGuiCustom::ComboTop()+imGuiCustom::ComboStep()+8;return changed;}
    bool Colour(const char* id,const char* label,float* values,bool alpha=true){
        Label(label);ImVec4 colour(values[0],values[1],values[2],alpha?values[3]:1);imGuiCustom::ColorSquare(id,&colour,ImVec2(x+width-18,y+2));bool changed=colour.x!=values[0]||colour.y!=values[1]||colour.z!=values[2]||(alpha&&colour.w!=values[3]);{values[0]=colour.x;values[1]=colour.y;values[2]=colour.z;if(alpha)values[3]=colour.w;}y+=29;return changed;
    }
    void Label(const char* text){auto& fonts=imGuiCustom::GetFonts();auto* font=fonts.CascadiaMonoBL?fonts.CascadiaMonoBL:ImGui::GetFont();ImGui::GetWindowDrawList()->AddText(font,16,ImGui::GetWindowPos()+ImVec2(x,y)+imGuiCustom::g_contentOffset,imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),UiText::Tr(text));}
    void Text(const std::string& text){auto* font=ImGui::GetFont();auto rendered=UiText::Message(text);auto size=font->CalcTextSizeA(15,width,width,rendered.c_str());ImGui::GetWindowDrawList()->AddText(font,15,ImGui::GetWindowPos()+ImVec2(x,y)+imGuiCustom::g_contentOffset,imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),rendered.c_str(),nullptr,width);y+=size.y+10;}
    bool Button(const char* label,float buttonWidth=0){ImGui::SetCursorPos(ImVec2(x,y)+imGuiCustom::g_contentOffset);bool changed=RetroUi::Choice(label,false,ImVec2(buttonWidth>0?buttonWidth:width,28))&&!imGuiCustom::PopupBlocking();y+=36;return changed;}
    bool Input(const char* id,const char* label,char* value,std::size_t size,ImGuiInputTextFlags flags=0){
        Label(label);ImGui::SetCursorPos(ImVec2(x,y+20)+imGuiCustom::g_contentOffset);ImGui::SetNextItemWidth(width);auto& theme=imGuiCustom::GetTheme();ImGui::PushStyleColor(ImGuiCol_FrameBg,theme.ControlBg);ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,theme.ControlInactive);ImGui::PushStyleColor(ImGuiCol_FrameBgActive,theme.ControlInactive);ImGui::PushStyleColor(ImGuiCol_Border,theme.TextBright);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(5,4));ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1);ImGui::PushID(id);bool changed=ImGui::InputText("##value",value,size,flags);ImGui::PopID();UiAssets::Item();ImGui::PopStyleVar(2);ImGui::PopStyleColor(4);y+=58;return changed;
    }
};
}
