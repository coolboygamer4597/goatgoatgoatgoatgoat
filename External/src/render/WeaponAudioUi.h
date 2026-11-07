#pragma once
#include "../core/features/game/WeaponAudio.h"
#include "FeatureUi.h"
namespace WeaponAudioUi {
inline void Draw(bool glock){
    auto config=WeaponAudio::Snapshot();bool changed=false;static int event[2]{};auto& selected=event[glock?1:0];
    const auto origin=ImGui::GetCursorPos();FeatureUi::Column column{origin.x+4,origin.y+4,ImGui::GetContentRegionAvail().x-8};
    changed|=column.Check("Use local weapon sounds",config.enabled);column.Combo("local_audio_event","Sound event",selected,WeaponAudio::Events,4);
    auto& pick=config.clips[glock?1:0][selected];auto library=WeaponAudio::Library();std::vector<std::string> names{"Original game sound"};std::vector<const char*> choices;int choice=0;
    for(const auto& file:library){names.push_back(WeaponAudio::Display(file));if(file==pick.file)choice=static_cast<int>(names.size())-1;}for(const auto& name:names)choices.push_back(name.c_str());
    if(column.Combo("local_audio_clip","Sound file",choice,choices.data(),static_cast<int>(choices.size()))){pick.file=choice?library[choice-1]:std::string{};changed=true;}
    if(column.Button("Import WAV / MP3")){auto file=WeaponAudio::Pick();if(!file.empty()){pick.file=file;config.enabled=true;changed=true;}}
    changed|=column.Slider("local_audio_volume","Volume",pick.volume,0,2,"%.2fx");changed|=column.Slider("local_audio_pitch","Pitch",pick.pitch,.25f,2,"%.2fx");
    if(column.Button("Preview sound")&&!WeaponAudio::Preview(pick)){std::lock_guard<std::mutex> guard(WeaponAudio::lock);WeaponAudio::status="Could not play this file";}if(column.Button("Stop preview"))WeaponAudio::StopPreview();
    if(changed)WeaponAudio::Update(config);std::string status;{std::lock_guard<std::mutex> guard(WeaponAudio::lock);status=WeaponAudio::status;}column.Text(status);ImGui::SetCursorPos(ImVec2(origin.x,column.y));ImGui::Dummy(ImVec2(1,1));
}
}
