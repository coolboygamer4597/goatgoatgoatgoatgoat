#pragma once
#include "../../app/RuntimePaths.h"
#include "../../../memory/memory.h"
#include <Windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"winmm.lib")

namespace WeaponAudio {
inline constexpr const char* Events[]={"Shot","Reload","Cocking","Empty"};
struct Clip {std::string file;float volume=1,pitch=1;};
struct Config {bool enabled=false;Clip clips[2][4];};
inline std::mutex lock;
inline Config settings;
inline unsigned revision=1;
inline std::string status;
inline std::filesystem::path Folder(){return RuntimePaths::File(L"custom-sounds");}
inline std::string Utf8(const std::filesystem::path& p){return p.u8string();}
inline std::string Hash(const std::vector<unsigned char>& bytes){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::array<unsigned char,32> digest{};DWORD size=0,used=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    const auto close=[&]{if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);};
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),4,&used,0)<0){close();return {};}
    std::vector<unsigned char> object(size);if(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)<0||BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)<0||BCryptFinishHash(hash,digest.data(),32,0)<0){close();return {};}
    close();static const char hex[]="0123456789abcdef";std::string out;for(auto b:digest){out+=hex[b>>4];out+=hex[b&15];}return out;
}
inline bool Load(const std::filesystem::path& path,std::vector<unsigned char>& bytes){
    std::error_code error;auto size=std::filesystem::file_size(path,error);if(error||size<12||size>32*1024*1024)return false;
    bytes.resize(static_cast<std::size_t>(size));std::ifstream in(path,std::ios::binary);return bool(in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()));
}
inline bool ValidName(const std::string& name){
    if(name.size()!=16||(name.substr(12)!=".wav"&&name.substr(12)!=".mp3"))return false;return std::all_of(name.begin(),name.begin()+12,[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
inline Config Snapshot(unsigned* stamp=nullptr){std::lock_guard<std::mutex> guard(lock);if(stamp)*stamp=revision;return settings;}
inline void Update(const Config& value){std::lock_guard<std::mutex> guard(lock);settings=value;for(auto& weapon:settings.clips)for(auto& c:weapon){if(!c.file.empty()&&!ValidName(c.file))c.file.clear();c.volume=std::clamp(c.volume,0.f,2.f);c.pitch=std::clamp(c.pitch,.25f,2.f);}++revision;}
inline std::string Serialize(){auto c=Snapshot();std::ostringstream out;out<<c.enabled;for(auto& w:c.clips)for(auto& clip:w)out<<' '<<std::quoted(clip.file)<<' '<<clip.volume<<' '<<clip.pitch;return out.str();}
inline bool Restore(const std::string& text){if(text.size()>2048)return false;std::istringstream in(text);Config c;if(!(in>>c.enabled))return false;for(auto& w:c.clips)for(auto& clip:w)if(!(in>>std::quoted(clip.file)>>clip.volume>>clip.pitch)||(!clip.file.empty()&&!ValidName(clip.file))||!std::isfinite(clip.volume)||!std::isfinite(clip.pitch))return false;in>>std::ws;if(!in.eof())return false;Update(c);return true;}
inline bool Import(const std::filesystem::path& source,std::string& file,std::string& message){
    auto extension=source.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
    std::vector<unsigned char> bytes;if((extension!=L".wav"&&extension!=L".mp3")||!Load(source,bytes)){message="Choose a WAV or MP3 file smaller than 32 MB";return false;}
    const bool wave=bytes.size()>44&&!std::memcmp(bytes.data(),"RIFF",4)&&!std::memcmp(bytes.data()+8,"WAVE",4);
    const bool mp3=bytes.size()>12&&(!std::memcmp(bytes.data(),"ID3",3)||(bytes[0]==0xff&&(bytes[1]&0xe0)==0xe0));
    if((extension==L".wav"&&!wave)||(extension==L".mp3"&&!mp3)){message="The file does not contain the selected audio format";return false;}
    const auto hash=Hash(bytes);if(hash.empty()){message="Could not check the sound file";return false;}
    file=hash.substr(0,12)+(extension==L".wav"?".wav":".mp3");std::error_code error;std::filesystem::create_directories(Folder(),error);if(error){message="Could not create the sound library";return false;}
    auto target=Folder()/std::filesystem::u8path(file);if(std::filesystem::exists(target,error)){std::vector<unsigned char> prior;if(!Load(target,prior)||Hash(prior)!=hash){message="The sound library has a filename conflict";return false;}}
    else{std::ofstream out(target,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<char*>(bytes.data()),bytes.size())){message="Could not copy the sound";return false;}}
    auto label=target;label+=L".name";{std::ofstream out(label,std::ios::binary);out<<source.filename().u8string();}
    message="Imported "+source.filename().u8string();return true;
}
inline std::string Display(const std::string& file){if(file.empty())return "Original game sound";std::ifstream in((Folder()/std::filesystem::u8path(file)).wstring()+L".name",std::ios::binary);std::string name;std::getline(in,name);return name.empty()?file:name.substr(0,160);}
inline std::vector<std::string> Library(){std::vector<std::string> files;std::error_code error;for(std::filesystem::directory_iterator i(Folder(),error),end;!error&&i!=end;i.increment(error))if(i->is_regular_file(error)&&ValidName(i->path().filename().u8string()))files.push_back(i->path().filename().u8string());std::sort(files.begin(),files.end());return files;}
inline std::string Pick(){wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetForegroundWindow();dialog.lpstrFilter=L"Audio files (*.wav;*.mp3)\0*.wav;*.mp3\0\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;dialog.lpstrTitle=L"Import a local weapon sound";if(!GetOpenFileNameW(&dialog))return {};std::string file,message;const bool ok=Import(path,file,message);{std::lock_guard<std::mutex> guard(lock);status=message;}return ok?file:std::string{};}
inline void StopPreview(){mciSendStringW(L"close goat_weapon_preview",nullptr,0,nullptr);}
inline bool Preview(const Clip& clip){StopPreview();if(!ValidName(clip.file))return false;auto path=Folder()/std::filesystem::u8path(clip.file);auto command=L"open \""+path.wstring()+L"\" alias goat_weapon_preview";if(mciSendStringW(command.c_str(),nullptr,0,nullptr))return false;mciSendStringW((L"setaudio goat_weapon_preview volume to "+std::to_wstring(int(std::clamp(clip.volume,0.f,1.f)*1000))).c_str(),nullptr,0,nullptr);return mciSendStringW(L"play goat_weapon_preview",nullptr,0,nullptr)==0;}
inline int Event(std::string path){
    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});auto p=path.rfind('/');const auto name=path.substr(p==std::string::npos?0:p+1);
    for(const char* s:{"empty","dryfire","click"})if(name.find(s)!=std::string::npos)return 3;
    for(const char* s:{"bolt","cock","rack","slide","chamber"})if(name.find(s)!=std::string::npos)return 2;
    for(const char* s:{"reload","magin","magout","magazine","clipin","clipout"})if(name.find(s)!=std::string::npos)return 1;
    for(const char* s:{"fire","shoot","shot","bang"})if(name.find(s)!=std::string::npos)return 0;
    return -1;
}
inline std::string Install(const Clip& clip){
    if(!ValidName(clip.file)||!memory||!memory->IsConnected())return {};
    wchar_t exe[32768]{};DWORD n=32768;if(!QueryFullProcessImageNameW(memory->GetHandle(),0,exe,&n))return {};
    auto client=std::filesystem::path(exe);if(client.filename()!=L"RobloxPlayerBeta.exe")return {};
    auto folder=client.parent_path()/L"content"/L"gg";std::error_code error;std::filesystem::create_directories(folder,error);if(error)return {};
    const auto source=Folder()/std::filesystem::u8path(clip.file),target=folder/std::filesystem::u8path(clip.file);
    std::vector<unsigned char> bytes;if(!Load(source,bytes)||Hash(bytes).substr(0,12)!=clip.file.substr(0,12))return {};
    if(std::filesystem::exists(target,error)){std::vector<unsigned char> prior;if(!Load(target,prior)||Hash(prior)!=Hash(bytes))return {};}
    else if(!std::filesystem::copy_file(source,target,std::filesystem::copy_options::none,error)||error)return {};
    return "rbxasset://gg/"+clip.file;
}
}
