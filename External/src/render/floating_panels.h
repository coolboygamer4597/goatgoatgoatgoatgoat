#pragma once
#include "menu/library.h"
#include "../core/variables/variables.h"
#include "../core/keys/keys.h"
#include "../core/net/performance.h"
#include <mutex>
#include <vector>
#include <cstdio>
namespace FloatingPanels {
struct Rect {float x=0,y=0,w=0,h=0;};
inline std::mutex rectMutex;
inline Rect rects[2];
inline bool WantsMouse(float x,float y){std::lock_guard<std::mutex> lock(rectMutex);for(auto r:rects)if(r.w>0 && x>=r.x && y>=r.y && x<=r.x+r.w && y<=r.y+r.h)return true;return false;}
inline void Begin(int index,const char* id,const char* title,float& x,float& y,float width,float height){
 ImGui::PushFont(imGuiCustom::GetFonts().InterSmall?imGuiCustom::GetFonts().InterSmall:ImGui::GetFont(),13.0f);
 const auto screen=ImGui::GetIO().DisplaySize;
 x=(std::max)(0.f,(std::min)(x,(std::max)(0.f,screen.x-width)));
 y=(std::max)(0.f,(std::min)(y,(std::max)(0.f,screen.y-height)));
 ImGui::SetNextWindowPos(ImVec2(x,y),ImGuiCond_Always);
 ImGui::SetNextWindowSize(ImVec2(width,height));
 ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
 ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.055f,.05f,.045f,.94f));
 ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings);
 ImGui::SetCursorPos(ImVec2(0,0));ImGui::InvisibleButton("##drag",ImVec2(width,29));
 if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)){
  x=(std::max)(0.f,(std::min)(x+ImGui::GetIO().MouseDelta.x,(std::max)(0.f,screen.x-width)));
  y=(std::max)(0.f,(std::min)(y+ImGui::GetIO().MouseDelta.y,(std::max)(0.f,screen.y-height)));
  ImGui::SetWindowPos(ImVec2(x,y));
 }
 auto dl=ImGui::GetWindowDrawList();auto p=ImGui::GetWindowPos();
 dl->AddRectFilledMultiColor(p,ImVec2(p.x+width,p.y+2),ImGui::GetColorU32(variables::Theme::accent),IM_COL32(100,65,40,15),IM_COL32(100,65,40,15),ImGui::GetColorU32(variables::Theme::accent));
 dl->AddText(p+ImVec2(12,9),IM_COL32(230,226,222,255),UiText::Tr(title));
 {std::lock_guard<std::mutex> lock(rectMutex);rects[index]={p.x,p.y,width,height};}
}
inline void End(){ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar(2);ImGui::PopFont();}
inline void Draw(){
 {std::lock_guard<std::mutex> lock(rectMutex);rects[0]={};rects[1]={};}
 if(Keys::KeybindsOn()){
  struct Row {const char* name;int key;bool active;};std::vector<Row> rows;
  if(variables::Aimbot::silentEnabled)rows.push_back({variables::Aimbot::silentMethod==2?"Magic Bullet":"Silent Aim",variables::Aimbot::silentKey,variables::Aimbot::silentActive});
  for(int target=0;target<2;++target){
   bool bound=variables::ESP::visualKeybindEnabled && variables::ESP::visualKeybindTarget==target;
   bool active=target==0?variables::ESP::nativeChams:variables::ESP::meshChams;
   if(bound||active)rows.push_back({target==0?"Native":"Mesh Chams",bound?variables::ESP::visualKeybindKey:0,active});
  }
  Begin(0,"##floating_keybinds","Keybind list",variables::Misc::keybindPanelX,variables::Misc::keybindPanelY,285,42+20.f*(std::max)(1,int(rows.size())));
  float y=34;
  for(auto row:rows){
   const auto color=row.active?IM_COL32(255,255,255,255):IM_COL32(128,128,128,255);
   auto dl=ImGui::GetWindowDrawList();auto p=ImGui::GetWindowPos();
   dl->AddText(p+ImVec2(12,y),color,UiText::Tr(row.name));
   const char* key=row.key?UiText::Tr(imGuiCustom::KeyName(row.key)):UiText::Tr("Always");
   float width=ImGui::CalcTextSize(key).x;
   dl->AddText(p+ImVec2(273-width,y),color,key);y+=20;
  }
  if(rows.empty()){ImGui::SetCursorPos(ImVec2(12,34));ImGui::TextDisabled("%s",UiText::Tr("No enabled keybinds"));}
  End();
 }
 if(variables::Misc::performancePanel){
  Begin(1,"##performance","Performance",variables::Misc::performancePanelX,variables::Misc::performancePanelY,335,300);
  ImGui::SetCursorPos(ImVec2(12,34));
  ImGui::Text("%s %.0f | CPU %.2f%% | %.0f MB",UiText::Tr("Overlay FPS"),Performance::fps,Performance::cpu,Performance::memoryMb);
  ImGui::SetCursorPos(ImVec2(12,61));ImGui::TextDisabled("%s",UiText::Tr("CPU ms / frame"));
  const char* labels[]={"Menu","Player cache","Silent Aim","Native","Mesh Chams","Overlay","Present"};
  auto dl=ImGui::GetWindowDrawList();auto p=ImGui::GetWindowPos();
  for(int i=0;i<Performance::Count;++i){
   const float y=85+22.f*i;dl->AddText(p+ImVec2(12,y),IM_COL32(200,200,200,255),UiText::Tr(labels[i]));
   char value[32];std::snprintf(value,sizeof(value),"%.3f",Performance::average[i]);
   dl->AddText(p+ImVec2(323-ImGui::CalcTextSize(value).x,y),IM_COL32(230,230,230,255),value);
  }
  ImGui::SetCursorPos(ImVec2(12,249));ImGui::Text("%s: %.2f%%",UiText::Tr("Skin worker CPU"),Performance::skinCpu);
  ImGui::SetCursorPos(ImVec2(12,277));ImGui::TextDisabled("%s",UiText::Tr("Elapsed timings; GPU not sampled"));
  End();
 }
}
}
