#pragma once
#include <chrono>
#include "PreviewCache.h"
#include "../core/app/CustomTextureData.h"
#include "../core/app/AppUpdates.h"
#include "../core/app/SkinDownloads.h"
#include "RetroUi.h"
#include "CustomTextureViewer.h"
#include <future>
namespace CustomSkinPreview {
inline std::future<PreviewCache::Image> pending;
inline std::filesystem::path requested,pendingPath;
inline std::filesystem::file_time_type revision{};
inline std::chrono::steady_clock::time_point nextCheck{};
inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;
inline UINT width=0,height=0;
inline bool finished=false;
inline void Poll(ID3D11Device* device,const std::filesystem::path& path){
    const auto now=std::chrono::steady_clock::now();
    bool changed=requested!=path;
    if(changed||now>=nextCheck){std::error_code ec;const auto current=std::filesystem::last_write_time(path,ec);changed|=!ec&&current!=revision;if(!ec)revision=current;nextCheck=now+std::chrono::seconds(1);}
    if(changed){requested=path;image.Reset();width=height=0;finished=false;}
    if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        PreviewCache::Image decoded;try{decoded=pending.get();}catch(...){}
        if(pendingPath==requested){finished=true;if(decoded.valid()){
            D3D11_TEXTURE2D_DESC d{};d.Width=decoded.width;d.Height=decoded.height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA pixels{decoded.rgba.data(),decoded.width*4,0};Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            if(SUCCEEDED(device->CreateTexture2D(&d,&pixels,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&image))){width=decoded.width;height=decoded.height;}
        }}
    }
    if(!pending.valid()&&!finished){pendingPath=requested;pending=std::async(std::launch::async,[file=requested]{try{return PreviewCache::DecodeImage(file);}catch(...){return PreviewCache::Image{};}});}
}
inline CustomTextureViewer::Viewer viewer;
inline int mode=1;
inline void Suspend(){viewer.Suspend();image.Reset();requested.clear();finished=false;}
inline void Draw(ID3D11Device* device,ID3D11DeviceContext* context,bool isGlock){
    const int selected=variables::Weapons::TextureForWeapon(isGlock);
    if(!CustomTextureData::Supports(selected,isGlock)){ImGui::TextUnformatted("Select a custom texture to preview it.");return;}
    const auto& preset=CustomTextureData::Presets[selected];ImGui::Text("%s / %s",preset.skin,preset.name);
    if(ImGui::RadioButton("Side profile",mode==0))mode=0;ImGui::SameLine();if(ImGui::RadioButton("3D",mode==1))mode=1;
    if(mode==1){
        ImGui::SameLine();if(ImGui::SmallButton("Reset view"))viewer.Reset();
        viewer.Shading(variables::Weapons::textureShading);viewer.Intensity(variables::Weapons::textureShadingIntensity);viewer.Shadow(variables::Weapons::textureShadowIntensity);viewer.Poll(device,selected);viewer.Render(context);auto texture=viewer.Image();const auto room=ImGui::GetContentRegionAvail();
        if(texture){const float scale=(std::min)(room.x/1024.f,(std::max)(1.f,room.y-20.f)/600.f);const ImVec2 size(1024*scale,600*scale),position=ImGui::GetCursorScreenPos();ImGui::GetWindowDrawList()->AddImage((ImTextureID)texture,position,ImVec2(position.x+size.x,position.y+size.y));ImGui::InvisibleButton("##custom_model_orbit",size);if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left))viewer.Orbit(ImGui::GetIO().MouseDelta.x,ImGui::GetIO().MouseDelta.y);if(ImGui::IsItemHovered()&&ImGui::GetIO().MouseWheel!=0)viewer.Zoom(ImGui::GetIO().MouseWheel);ImGui::TextUnformatted("Drag to rotate; scroll to zoom.");}
        else{ImGui::TextUnformatted(viewer.Failed?"3D preview could not be prepared.":viewer.Missing?"Preview files are missing.":"Preparing 3D preview...");if(viewer.Missing&&RetroUi::Choice("Download more skin",false,ImVec2(210,28)))SkinDownloads::Open();}
        return;
    }
    viewer.Suspend();Poll(device,CustomTextureData::PreviewFile(selected));
    const auto available=ImGui::GetContentRegionAvail();
    if(image&&width&&height){const float scale=(std::min)(available.x/width,(std::max)(1.f,available.y-24)/height);const ImVec2 size(width*scale,height*scale);ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(available.x-size.x)*.5f);ImGui::Image((ImTextureID)image.Get(),size);}
    else{ImGui::TextUnformatted(finished?"Custom preview files are missing.":"Preparing shaded preview...");if(finished&&RetroUi::Choice("Download more skin",false,ImVec2(210,28)))SkinDownloads::Open();}
}
inline void Shutdown(){viewer.Suspend();if(pending.valid())try{pending.get();}catch(...){}image.Reset();requested.clear();pendingPath.clear();finished=false;}
}
