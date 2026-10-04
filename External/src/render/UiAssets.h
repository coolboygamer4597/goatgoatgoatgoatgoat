#pragma once
#include <Windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "../../ext/imgui/imgui.h"
#pragma comment(lib,"winmm.lib")
namespace UiAssets {
inline bool sounds=false;
inline std::wstring Path(int id,const wchar_t* name){
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    auto dir=std::filesystem::path(temp)/L"goat-ui-assets-v2";
    std::error_code ec;std::filesystem::create_directories(dir,ec);
    auto path=dir/name;
    auto module=GetModuleHandleW(nullptr);auto res=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));
    if(!res)return {};
    auto size=SizeofResource(module,res);auto data=LockResource(LoadResource(module,res));
    if(!std::filesystem::exists(path,ec)||std::filesystem::file_size(path,ec)!=size){std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write((const char*)data,size);}
    return path.wstring();
}
inline ImFont* Font(float size){
    auto h=GetModuleHandleW(nullptr);auto r=FindResourceW(h,MAKEINTRESOURCEW(204),MAKEINTRESOURCEW(10));
    ImFontConfig cfg;cfg.OversampleH=cfg.OversampleV=1;cfg.PixelSnapH=true;cfg.FontDataOwnedByAtlas=false;
    if(r)return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(LockResource(LoadResource(h,r)),SizeofResource(h,r),size,&cfg);
    cfg.SizePixels=size;return ImGui::GetIO().Fonts->AddFontDefault(&cfg);
}
inline void Sound(bool) {}
inline void Item(bool=false) {}
inline void Shutdown() {}
}
