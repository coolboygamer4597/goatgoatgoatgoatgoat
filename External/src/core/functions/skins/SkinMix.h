#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <Windows.h>
namespace SkinMix {
struct Choices {
    std::string ar="Default",glock="Default",arGloves="Default",glockGloves="Default",arSounds,glockSounds;
    std::string arSoundWeapon="AR-15",glockSoundWeapon="Glock 19";
    bool mute=false;
};
inline std::string GlovesForWeapon(const Choices& c,const std::string& weapon){
    return weapon=="AR-15"?c.arGloves:weapon=="Glock 19"?c.glockGloves:"Default";
}
inline bool Valid(const Choices& c){
    for(auto p:{&c.ar,&c.glock,&c.arGloves,&c.glockGloves,&c.arSounds,&c.glockSounds})
        if(p->size()>128||p->find_first_of("\r\n\0",0,3)!=std::string::npos)return false;
    return !c.ar.empty()&&!c.glock.empty()&&!c.arGloves.empty()&&!c.glockGloves.empty()
        &&(c.arSoundWeapon=="AR-15"||c.arSoundWeapon=="Glock 19")&&(c.glockSoundWeapon=="AR-15"||c.glockSoundWeapon=="Glock 19");
}
inline std::string Serialize(const Choices& c){std::ostringstream s;s<<"SKINMIX 2\n"<<std::quoted(c.ar)<<' '<<std::quoted(c.glock)<<' '<<std::quoted(c.arGloves)<<' '<<std::quoted(c.glockGloves)<<' '<<std::quoted(c.arSounds)<<' '<<std::quoted(c.glockSounds)<<' '<<std::quoted(c.arSoundWeapon)<<' '<<std::quoted(c.glockSoundWeapon)<<' '<<c.mute<<'\n';return s.str();}
inline bool Parse(const std::string& text,Choices& output){
    std::istringstream s(text);std::string tag;int version,mute;Choices c;
    if(!(s>>tag>>version)||tag!="SKINMIX"||(version!=1&&version!=2))return false;
    if(!(s>>std::quoted(c.ar)>>std::quoted(c.glock)>>std::quoted(c.arGloves)))return false;
    if(version==1)c.glockGloves=c.arGloves;
    else if(!(s>>std::quoted(c.glockGloves)))return false;
    if(!(s>>std::quoted(c.arSounds)>>std::quoted(c.glockSounds)))return false;
    if(version==2&&!(s>>std::quoted(c.arSoundWeapon)>>std::quoted(c.glockSoundWeapon)))return false;
    if(!(s>>mute)||mute<0||mute>1)return false;
    s>>std::ws;if(!s.eof()||!Valid(c))return false;c.mute=mute;output=std::move(c);return true;
}
inline bool ValidName(const std::string& n){return !n.empty()&&n.size()<=48&&std::all_of(n.begin(),n.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-';});}
inline std::filesystem::path Directory(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);return std::filesystem::path(p).parent_path()/L"skin-presets";}
inline bool Save(const std::string& name,const Choices& c,std::string& error){
    if(!ValidName(name)||!Valid(c)){error="Use a preset name with letters, numbers, spaces, - or _.";return false;}
    std::error_code ec;auto dir=Directory();std::filesystem::create_directories(dir,ec);if(ec){error="Could not create preset folder";return false;}
    auto file=dir/("preset-"+name+".skinmix"),temp=file;temp+=L".tmp";
    {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<Serialize(c);f.flush();if(!f){error="Could not save preset";return false;}}
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Could not finish saving preset";return false;}
    error="Preset saved";return true;
}
inline bool Load(const std::string& name,Choices& c,std::string& error){
    if(!ValidName(name)){error="Choose a valid preset name";return false;}
    std::ifstream f(Directory()/("preset-"+name+".skinmix"),std::ios::binary);std::string text((std::istreambuf_iterator<char>(f)),{});
    if(text.size()>4096||!Parse(text,c)){error="Preset is missing or invalid";return false;}error="Preset loaded";return true;
}
inline std::vector<std::string> Presets(){
    std::vector<std::string> names;std::error_code ec;auto dir=Directory();
    for(std::filesystem::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){
        auto name=it->path().stem().string();if(it->path().extension()==".skinmix"&&name.rfind("preset-",0)==0&&ValidName(name.substr(7)))names.push_back(name.substr(7));
    }std::sort(names.begin(),names.end());return names;
}
}
