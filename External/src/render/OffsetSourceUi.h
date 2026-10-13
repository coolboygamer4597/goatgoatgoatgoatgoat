#pragma once
#include "../sdk/OffsetUpdates.h"
#include "CaptureProtection.h"
#include "RetroUi.h"
#include <commdlg.h>
#pragma comment(lib,"comdlg32.lib")

namespace OffsetSourceUi {
inline const char* const names[]={"Automatic updates (recommended)","Bundled offsets","Local offsets file"};
inline int chosen=0;
inline std::filesystem::path file;
inline std::string version,error;
inline std::string LoaderLabel(){
    const auto source=OffsetUpdates::GetSelection().source;
    return std::string("OFFSETS: ")+(source==OffsetUpdates::Source::Bundled?"Bundled":source==OffsetUpdates::Source::Local?"Local file":"Automatic");
}
inline UINT_PTR CALLBACK DialogHook(HWND window,UINT msg,WPARAM,LPARAM l){
    if(msg==WM_INITDIALOG){auto ofn=reinterpret_cast<OPENFILENAMEW*>(l);CaptureProtection::Apply(GetAncestor(window,GA_ROOT),ofn->lCustData!=0);}return 0;
}
inline std::filesystem::path Browse(HWND owner,bool streamproof,const std::filesystem::path& previous={}){
    wchar_t selected[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"Offsets files (*.h;*.json)\0*.h;*.json\0Header files (*.h)\0*.h\0JSON files (*.json)\0*.json\0";
    dialog.lpstrFile=selected;dialog.nMaxFile=32768;dialog.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_ENABLEHOOK;dialog.lpfnHook=DialogHook;dialog.lCustData=streamproof?1:0;
    return GetOpenFileNameW(&dialog)?std::filesystem::path(selected):previous;
}
inline bool LoadClick(bool click,float scale,bool streamproof,bool force=false){
    if(click||force){version=OffsetUpdates::RunningVersion();if(!force&&!OffsetUpdates::NeedsPrompt(version))return true;auto selection=OffsetUpdates::GetSelection();chosen=static_cast<int>(selection.source);file=selection.file;error.clear();ImGui::OpenPopup("Choose offsets source");}
    bool start=false;auto main=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main->Pos.x+main->Size.x*.5f,main->Pos.y+main->Size.y*.5f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(550*scale,344*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if(ImGui::BeginPopupModal("Choose offsets source",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground)){
        const auto p=ImGui::GetWindowPos();auto at=[&](float x,float y){return ImVec2(p.x+x*scale,p.y+y*scale);};
        auto draw=ImGui::GetWindowDrawList();RetroUi::Frame(draw,p,at(550,344));
        RetroUi::Stars(draw,at(12,10),ImVec2(478*scale,27*scale),static_cast<float>(ImGui::GetTime()));
        draw->AddText(ImGui::GetFont(),22*scale,at(24,14),RetroUi::White,"OFFSETS");
        if(RetroUi::CaptionButton("##offset_close",at(514,9),true))ImGui::CloseCurrentPopup();
        ImGui::SetCursorScreenPos(at(24,51));ImGui::Text("Roblox: %s",version.empty()?"Could not identify version":version.c_str());
        for(int i=0;i<3;++i){
            ImGui::SetCursorScreenPos(at(24,84+i*43.f));
            if(RetroUi::Choice(names[i],chosen==i,ImVec2(502*scale,34*scale)))chosen=i;
        }
        ImGui::SetCursorScreenPos(at(24,218));
        if(chosen==0)ImGui::TextWrapped("Downloads matching offsets and keeps a verified offline copy.");
        if(chosen==1)ImGui::TextWrapped("Bundled version: %s",BundledOffsets::ClientVersion.c_str());
        if(chosen==2){
            if(RetroUi::Choice("Select offsets file...",false,ImVec2(240*scale,26*scale)))file=Browse(static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw),streamproof,file);
            ImGui::SetCursorScreenPos(at(24,250));ImGui::TextUnformatted(file.empty()?"No file selected":file.filename().u8string().c_str());
        }
        if(!error.empty()){ImGui::SetCursorScreenPos(at(24,270));ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.45f,.4f,1));ImGui::PushTextWrapPos(526*scale);ImGui::TextWrapped("%s",error.c_str());ImGui::PopTextWrapPos();ImGui::PopStyleColor();}
        ImGui::SetCursorScreenPos(at(24,305));auto size=ImVec2(270*scale,28*scale);
        if(RetroUi::Choice("Use source and load",false,size)){
            try{
                if(!OffsetData::Version(version))throw std::runtime_error("Open the desktop Roblox client before loading.");
                if(chosen==1&&BundledOffsets::ClientVersion!=version)throw std::runtime_error("Bundled offsets do not match this Roblox version.");
                if(chosen==2){if(file.empty())throw std::runtime_error("Select an offsets file first.");OffsetUpdates::LocalPackage(file,version);}
                OffsetUpdates::SetSelection(static_cast<OffsetUpdates::Source>(chosen),file);start=true;ImGui::CloseCurrentPopup();
            }catch(const std::exception& e){error=e.what();}
        }
        ImGui::SetCursorScreenPos(at(306,305));if(RetroUi::Choice("Cancel",false,ImVec2(220*scale,28*scale)))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    return start;
}
}
