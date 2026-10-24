#pragma once
#include "../core/app/SkinDownloads.h"
#include "../core/app/CustomTextureData.h"
#include "RetroUi.h"
#include "../core/app/AppUpdates.h"
namespace SkinDownloadsUi {
inline bool Draw(float scale=1,bool permitted=true){
    const auto state=SkinDownloads::Get();if(!permitted||!state.visible)return false;
    static unsigned revision=~0u;static bool customize=true;static int category=-1;static char search[96]{};static std::array<bool,CustomTextureData::Presets.size()> choices{};
    if(revision!=state.revision){revision=state.revision;customize=true;category=-1;search[0]=0;choices.fill(false);}
    const bool previews=state.previewsOnly;const char* title=previews?"Skin previews":"Download skins";
    const auto viewport=ImGui::GetWindowViewport();scale=std::min(scale,std::min(viewport->Size.x/650.f,viewport->Size.y/450.f));scale=std::max(scale,.5f);const ImVec2 size(620*scale,(previews?280.f:430.f)*scale),center(viewport->Pos.x+viewport->Size.x*.5f,viewport->Pos.y+viewport->Size.y*.5f);
    if(!ImGui::IsPopupOpen(title))ImGui::OpenPopup(title);ImGui::SetNextWindowSize(size,ImGuiCond_Always);ImGui::SetNextWindowPos(center,ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,IM_COL32(18,18,23,255));
    if(ImGui::BeginPopupModal(title,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove)){
        const auto pos=ImGui::GetWindowPos();RetroUi::Frame(ImGui::GetWindowDrawList(),pos,ImVec2(pos.x+size.x,pos.y+size.y));auto at=[&](float x,float y){ImGui::SetCursorPos(ImVec2(x*scale,y*scale));};
        at(24,20);ImGui::TextUnformatted(UiText::Tr(title));at(24,53);ImGui::BeginChild("skin_download_message",ImVec2(572*scale,65*scale));ImGui::TextWrapped(UiText::Tr("%s"),UiText::Message(state.message).c_str());ImGui::EndChild();
        const bool busy=state.phase==SkinDownloads::Phase::Checking||state.phase==SkinDownloads::Phase::Downloading;std::vector<int> selected;std::size_t bytes=0;
        if(!busy&&state.phase==SkinDownloads::Phase::Offered){
            if(previews){at(24,132);if(state.previewsAvailable)ImGui::Text(UiText::Tr("Skin previews: %.1f MB"),state.previewBytes/1000000.0);else ImGui::TextUnformatted(UiText::Tr("Previews are not currently available."));if(!state.previewsInstalled&&state.previewsAvailable)bytes=state.previewBytes;}
            else if(!state.items.empty()){
                at(24,125);if(ImGui::RadioButton(UiText::Tr("Everything"),!customize))customize=false;ImGui::SameLine(190*scale);if(ImGui::RadioButton(UiText::Tr("Customize"),customize))customize=true;
                if(customize){
                    at(24,166);if(ImGui::RadioButton(UiText::Tr("All"),category==-1))category=-1;ImGui::SameLine();if(ImGui::RadioButton(UiText::Tr("CS skins"),category==0))category=0;ImGui::SameLine();if(ImGui::RadioButton(UiText::Tr("Other"),category==1))category=1;ImGui::SameLine(325*scale);ImGui::SetNextItemWidth(270*scale);ImGui::InputTextWithHint("##download_skin_search",UiText::Tr("Search skins..."),search,sizeof(search));
                    at(24,202);ImGui::BeginChild("individual_skin_downloads",ImVec2(572*scale,136*scale),true);ImGuiTextFilter filter(search);
                    for(const auto& item:state.items){const std::string label=item.skin+" / "+item.name;if((category!=-1&&item.category!=category)||!filter.PassFilter(label.c_str()))continue;ImGui::PushID(item.index);bool checked=item.installed||choices[item.index];ImGui::BeginDisabled(item.installed);if(ImGui::Checkbox(label.c_str(),&checked))choices[item.index]=checked;ImGui::EndDisabled();ImGui::SameLine(450*scale);if(item.installed)ImGui::TextUnformatted(UiText::Tr("Installed"));else ImGui::Text(UiText::Tr("%.1f MB"),item.bytes/1000000.0);ImGui::PopID();}ImGui::EndChild();
                }else{at(24,184);ImGui::TextWrapped(UiText::Tr("Download all %u available custom skins in CS skins and Other. Installed files are kept."),static_cast<unsigned>(state.items.size()));}
                for(const auto& item:state.items)if(!item.installed&&(!customize||choices[item.index])){selected.push_back(item.index);bytes+=item.bytes;}
                if(!selected.empty()&&!state.commonInstalled)bytes+=state.commonBytes;at(24,350);ImGui::Text(UiText::Tr("Download size: %.1f MB"),bytes/1000000.0);
            }
        }
        if(busy){at(24,135);ImGui::TextUnformatted(UiText::Tr(state.phase==SkinDownloads::Phase::Checking?"Checking available downloads...":"Downloading and verifying your selection..."));at(24,173);ImGui::ProgressBar(state.progress,ImVec2(572*scale,20*scale),nullptr);}
        at(24,previews?232:382);
        auto close=[&]{SkinDownloads::Dismiss();ImGui::CloseCurrentPopup();};
        if(state.phase==SkinDownloads::Phase::Downloading){if(RetroUi::Choice("CANCEL DOWNLOAD",false,ImVec2(572*scale,30*scale)))SkinDownloads::Cancel();}
        else if(state.phase==SkinDownloads::Phase::Checking||state.phase==SkinDownloads::Phase::Complete){if(RetroUi::Choice(state.phase==SkinDownloads::Phase::Checking?"CONTINUE WITHOUT":"CLOSE",false,ImVec2(572*scale,30*scale)))close();}
        else{
            const bool newer=!previews&&state.newerSkins&&std::all_of(state.items.begin(),state.items.end(),[](const auto& item){return item.installed;});const bool retry=state.phase==SkinDownloads::Phase::Error||(previews?!state.previewsAvailable:state.items.empty());ImGui::BeginDisabled(!retry&&!bytes&&!newer);
            if(RetroUi::Choice(retry?"RETRY CHECK":newer?"CHECK FOR UPDATES":previews?"DOWNLOAD PREVIEWS":"DOWNLOAD SELECTED",false,ImVec2(274*scale,30*scale))){if(newer&&!retry){close();AppUpdates::Check();}else if(retry){if(previews)SkinDownloads::OpenPreviews();else SkinDownloads::Open();}else SkinDownloads::Download(selected,previews);}ImGui::EndDisabled();ImGui::SameLine(322*scale);if(RetroUi::Choice("CONTINUE WITHOUT",false,ImVec2(274*scale,30*scale)))close();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();return true;
}
}
