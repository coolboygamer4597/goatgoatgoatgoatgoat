#pragma once
#include "RetroUi.h"
#include <chrono>
#include "menu/library.h"
#include "../core/app/AppUpdates.h"
#include "../core/app/SkinDownloads.h"
#include "../core/app/CustomTextureData.h"
#include "../core/features/native/NativeChams.h"
#include "../core/functions/skins/skins.h"
namespace CustomSkinUi {
inline bool previewCustom=false;
inline int previewWeapon=0;
inline void At(float x,float y){ImGui::SetCursorPos(ImVec2(x,y)+imGuiCustom::g_contentOffset);}
inline bool Tab(const char* label,int value,int& selected,float x,float y,float width){At(x,y);if(!RetroUi::Choice(label,selected==value,ImVec2(width,28)))return false;selected=value;return true;}
inline const std::array<bool,CustomTextureData::Presets.size()>& Installed(){
    static std::array<bool,CustomTextureData::Presets.size()> installed{};static auto next=std::chrono::steady_clock::time_point{};static unsigned revision=~0u;const auto now=std::chrono::steady_clock::now();const auto current=SkinDownloads::Get().revision;
    if(now>=next||revision!=current){const auto directory=RuntimePaths::File(L"custom-textures");for(int i=0;i<static_cast<int>(installed.size());++i)installed[i]=CustomTextureData::Available(i)&&CustomTextureData::Installed(i,directory);next=now+std::chrono::seconds(1);revision=current;}return installed;
}
inline void NoSkins(){ImGui::TextWrapped("No skins detected.");if(RetroUi::Choice("Download skins?",false,ImVec2(ImGui::GetContentRegionAvail().x,28)))SkinDownloads::Open();}
inline void Draw(){
    using namespace variables::Weapons;
    static int section=0,group=0,weapon=0,customWeapon=0;
    const float left=12,width=ImGui::GetWindowWidth()-24-imGuiCustom::g_contentOffset.x*2,col=(width-16)*.5f,right=left+col+16,third=(width-16)/3;
    Tab("Native cham",0,section,left,51,third);Tab("Customs",1,section,left+third+8,51,third);Tab("Editor",2,section,left+third*2+16,51,third);
    previewCustom=section==1;previewWeapon=section==1?customWeapon:weapon;
    if(section==2){Skins::RenderEditor();return;}
    if(section==0){
        Tab("Weapon",0,group,left,94,third);Tab("Glove",1,group,left+third+8,94,third);Tab("Arm",2,group,left+third*2+16,94,third);
        bool separate=group==0?separateWeapons:group==1?separateGloves:separateArms;
        const bool previous=separate;imGuiCustom::Checkbox("Separate AR and Glock",&separate,ImVec2(right,145));
        if(separate!=previous){if(group==0)SeparateWeapons(separate);else SeparateLimbs(group==1,separate);}
        if(separate){Tab("AR",0,weapon,left,139,(col-8)*.5f);Tab("Glock",1,weapon,left+(col+8)*.5f,139,(col-8)*.5f);}
        previewWeapon=weapon;
        LimbMaterial* material=group==0?(separateWeapons?(weapon?&glockMaterial:&arMaterial):nullptr):&LimbProfile(weapon!=0,group==1);
        imGuiCustom::Checkbox("Master toggle",material?&material->enabled:&native,ImVec2(right,185));
        float* selectedColor=material?material->color:color;
        At(left,181);ImGui::TextUnformatted("Color");imGuiCustom::ColorSquare("custom_color",reinterpret_cast<ImVec4*>(selectedColor),ImVec2(left+col-20,180));
        float y=218;
        auto slider=[&](const char* id,float* value,float lo,float hi,const char* label){imGuiCustom::SliderFloat(id,value,lo,hi,ImVec2(left,y+imGuiCustom::SliderTop()),col,label,"%.2f");y+=imGuiCustom::SliderStep()+10;};
        slider("custom_opacity",material?&material->opacity:&opacity,0,1,"Opacity");
        imGuiCustom::Combo("custom_style",material?&material->style:&nativeStyle,Cheat::Visuals::NativeChams::ShaderNames(),Cheat::Visuals::NativeChams::ShaderNameCount(),ImVec2(left,y+imGuiCustom::ComboTop()),col,"Style:");y+=imGuiCustom::ComboTop()+imGuiCustom::ComboStep()+10;
        slider("custom_speed",material?&material->speed:&speed,0,15,"Animation speed");
        slider("custom_scale",material?&material->scale:&scale,group?.1f:.01f,group?8.f:100.f,"Pattern size");
        imGuiCustom::Checkbox("Hide weapon and arms",&hide,ImVec2(right,260));
        imGuiCustom::Checkbox("Show weapon preview",&preview,ImVec2(right,295));return;
    }
    Tab("AR",0,customWeapon,left,94,col);Tab("Glock",1,customWeapon,right,94,col);
    static char skinSearch[2][96]{},textureSearch[2][96]{};static int category=0;
    previewWeapon=customWeapon;const bool isGlock=customWeapon==1;const std::string selectedWeapon=isGlock?"Glock 19":"AR-15";
    int& selectedPreset=TextureForWeapon(isGlock);const auto selectedSkin=Skins::PreviewSelection(selectedWeapon);
    const auto& installed=Installed();std::vector<std::string> skins;for(int i=0;i<static_cast<int>(CustomTextureData::Presets.size());++i){const auto& preset=CustomTextureData::Presets[i];if(installed[i]&&CustomTextureData::Supports(i,isGlock)&&preset.category==category&&std::find(skins.begin(),skins.end(),preset.skin)==skins.end())skins.push_back(preset.skin);}
    At(right,139);ImGui::TextUnformatted("Supported skins");At(left,139);ImGui::TextUnformatted(category?"Other textures":"CS skins");
    At(right,165);ImGui::SetNextItemWidth(col);ImGui::InputTextWithHint("##supported_search","Search supported skins...",skinSearch[customWeapon],96);
    At(left,165);ImGui::SetNextItemWidth(col);ImGui::InputTextWithHint("##custom_texture_search","Search textures...",textureSearch[customWeapon],96);
    At(right,196);ImGui::BeginChild("supported_skin_list",ImVec2(col,124),true);ImGuiTextFilter skinFilter(skinSearch[customWeapon]);
    if(skins.empty())NoSkins();
    for(const auto& skin:skins)if(skinFilter.PassFilter(skin.c_str())&&ImGui::Selectable(skin.c_str(),selectedSkin==skin))Skins::SelectCustomSkin(isGlock,skin,-1);
    ImGui::EndChild();
    At(left,196);ImGui::BeginChild("custom_texture_list",ImVec2(col,124),true);ImGuiTextFilter textureFilter(textureSearch[customWeapon]);bool supported=false;
    for(const auto& skin:skins)if(skin==selectedSkin)supported=true;
    if(skins.empty())NoSkins();
    else if(!supported)ImGui::TextWrapped("Select a supported skin to see its textures.");
    for(int index=0;index<static_cast<int>(CustomTextureData::Presets.size());++index){const auto& preset=CustomTextureData::Presets[index];if(!installed[index]||!CustomTextureData::Supports(index,isGlock)||preset.category!=category||std::string(preset.skin)!=selectedSkin||!textureFilter.PassFilter(preset.name))continue;
        const bool selected=selectedPreset==index;
        if(ImGui::Selectable(preset.name,selected))Skins::SelectCustomSkin(isGlock,preset.skin,selected?-1:index);
        if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted(selected?"Click again to restore the original texture.":"Equip this texture and keep the selected gloves and sounds.");ImGui::EndTooltip();}
    }ImGui::EndChild();
    imGuiCustom::SliderFloat("custom_shading_intensity",&textureShadingIntensity,0,10,ImVec2(left,350),width,"Reflection strength","%.2f");
    if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted("0: matte; 1: current shading; 2: stronger reflections. Paint colors stay unchanged.");ImGui::EndTooltip();}
    imGuiCustom::SliderFloat("custom_shadow_intensity",&textureShadowIntensity,0,10,ImVec2(left,385),width,"Shadow detail","%.2f");
    if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted("0 keeps the current look. Increase to deepen the weapon's creases and surface shadows.");ImGui::EndTooltip();}
    At(left,411);if(RetroUi::Choice(category?"Categories: Other":"Categories: CS skins",false,ImVec2(col,28)))ImGui::OpenPopup("##skin_categories");
    if(ImGui::BeginPopup("##skin_categories")){if(ImGui::Selectable("CS skins",category==0))category=0;if(ImGui::Selectable("Other",category==1))category=1;ImGui::EndPopup();}
    At(right,411);if(RetroUi::Choice("Download more skins",false,ImVec2(col,28)))SkinDownloads::Open();
    imGuiCustom::Checkbox("Show weapon preview",&preview,ImVec2(left,452));imGuiCustom::Checkbox("Remove extra ornaments",&modelCleanup,ImVec2(right,452));
    At(left,478);ImGui::Text("Equipped: %s",selectedSkin.empty()?"Default":selectedSkin.c_str());
    if(selectedSkin=="X5"){imGuiCustom::Checkbox("Remove X5 outline",&removeX5Outline,ImVec2(right,478));if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted("Re-equip the weapon to apply this setting.");ImGui::EndTooltip();}}
}
}
