#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <Windows.h>
#include "SkinLook.h"
namespace SkinMix {
struct Choices {
    std::string ar="Default",glock="Default",arGloves="Default",glockGloves="Default",arSounds,glockSounds;
    std::string arSoundWeapon="AR-15",glockSoundWeapon="Glock 19";
    bool mute=false,customAr0416=false;
    SkinLook::Look look;
};
inline std::string GlovesForWeapon(const Choices& c,const std::string& weapon){
    return weapon=="AR-15"?c.arGloves:weapon=="Glock 19"?c.glockGloves:"Default";
}
inline bool Valid(const Choices& c){
    for(auto p:{&c.ar,&c.glock,&c.arGloves,&c.glockGloves,&c.arSounds,&c.glockSounds})
        if(p->size()>128||p->find_first_of("\r\n\0",0,3)!=std::string::npos)return false;
    return (!c.customAr0416||c.ar=="0416")&&!c.ar.empty()&&!c.glock.empty()&&!c.arGloves.empty()&&!c.glockGloves.empty()
        &&SkinLook::Valid(c.look)&&(c.arSoundWeapon=="AR-15"||c.arSoundWeapon=="Glock 19")&&(c.glockSoundWeapon=="AR-15"||c.glockSoundWeapon=="Glock 19");
}
inline std::string Serialize(const Choices& c){std::ostringstream s;s<<"SKINMIX 4\n"<<std::quoted(c.ar)<<' '<<std::quoted(c.glock)<<' '<<std::quoted(c.arGloves)<<' '<<std::quoted(c.glockGloves)<<' '<<std::quoted(c.arSounds)<<' '<<std::quoted(c.glockSounds)<<' '<<std::quoted(c.arSoundWeapon)<<' '<<std::quoted(c.glockSoundWeapon)<<' '<<c.mute<<' '<<c.customAr0416<<'\n';SkinLook::Write(s,c.look);return s.str();}
inline bool Parse(const std::string& text,Choices& output){
    std::istringstream s(text);std::string tag;int version,mute;Choices c;
    if(!(s>>tag>>version)||tag!="SKINMIX"||(version<1||version>4))return false;
    if(!(s>>std::quoted(c.ar)>>std::quoted(c.glock)>>std::quoted(c.arGloves)))return false;
    if(version==1)c.glockGloves=c.arGloves;
    else if(!(s>>std::quoted(c.glockGloves)))return false;
    if(!(s>>std::quoted(c.arSounds)>>std::quoted(c.glockSounds)))return false;
    if(version>=2&&!(s>>std::quoted(c.arSoundWeapon)>>std::quoted(c.glockSoundWeapon)))return false;
    if(!(s>>mute)||mute<0||mute>1)return false;
    if(version>=4){int cross;if(!(s>>cross)||cross<0||cross>1)return false;c.customAr0416=cross!=0;}
    if(version>=3&&!SkinLook::Read(s,c.look))return false;
    s>>std::ws;if(!s.eof()||!Valid(c))return false;c.mute=mute;output=std::move(c);return true;
}
inline bool ValidName(const std::string& n){return !n.empty()&&n.size()<=48&&std::all_of(n.begin(),n.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-';});}
inline std::string Encode(const Choices& choices){const auto text=Serialize(choices);std::string value;value.reserve(text.size()*2);for(unsigned char c:text){value+="0123456789abcdef"[c>>4];value+="0123456789abcdef"[c&15];}return value;}
inline bool Decode(const std::string& value,Choices& choices){if(value.empty()||value.size()>32768||value.size()%2)return false;auto digit=[](unsigned char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};std::string text;text.reserve(value.size()/2);for(std::size_t i=0;i<value.size();i+=2){const int a=digit(value[i]),b=digit(value[i+1]);if(a<0||b<0)return false;text.push_back(static_cast<char>((a<<4)|b));}return Parse(text,choices);}
inline std::filesystem::path Directory(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);return std::filesystem::path(p).parent_path()/L"skin-presets";}
inline bool Save(const std::string& name,const Choices& c,std::string& error){
    if(!ValidName(name)||!Valid(c)){error="Use a preset name with letters, numbers, spaces, - or _.";return false;}
    std::error_code ec;auto dir=Directory();std::filesystem::create_directories(dir,ec);if(ec){error="Could not create preset folder";return false;}
    auto file=dir/("preset-"+name+".skinmix"),temp=file;temp+=L".tmp";
    {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<Serialize(c);f.flush();if(!f){error="Could not save preset";return false;}}
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Could not finish saving preset";return false;}
    error="Preset saved";return true;
}
inline bool Builtin(const std::string& name,Choices& c){
    c={};c.look.present=true;
    if(name=="BloodsportCombo"){c.ar="X5";c.arGloves="X4";c.arSounds="B5";c.glock="AKNY";c.glockGloves="Champ";c.glockSounds="AKNY";c.look.texture[0]=0;}
    else if(name=="cranefligh combot"){c.ar="X5";c.arGloves="Champ";c.arSounds="Coordinator";c.glock="AKNY";c.glockGloves="Champ";c.glockSounds="Shogun";c.look.texture[0]=2;}
    else if(name=="pink lumber"){
        c.ar=c.glock=c.arGloves=c.glockGloves=c.arSounds=c.glockSounds="Lumberjack";
        c.look.separate[0]=true;
        for(int w=0;w<2;++w){auto& m=c.look.materials[w][0];m.enabled=true;m.style=26;m.opacity=1;m.speed=10.86f;m.scale=2.67f;m.color[0]=1;m.color[1]=.35f;m.color[2]=.7f;m.color[3]=1;}
    }else return false;
    return true;
}
inline bool Load(const std::string& name,Choices& c,std::string& error){
    if(!ValidName(name)){error="Choose a valid preset name";return false;}
    const auto file=Directory()/("preset-"+name+".skinmix");
    if(!std::filesystem::exists(file)&&Builtin(name,c)){error="Preset loaded";return true;}
    std::ifstream f(file,std::ios::binary);std::string text((std::istreambuf_iterator<char>(f)),{});
    if(text.size()>16384||!Parse(text,c)){error="Preset is missing or invalid";return false;}error="Preset loaded";return true;
}
inline std::vector<std::string> Presets(){
    std::vector<std::string> names{"BloodsportCombo","cranefligh combot","pink lumber"};std::error_code ec;auto dir=Directory();
    for(std::filesystem::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){
        auto name=it->path().stem().string();if(it->path().extension()==".skinmix"&&name.rfind("preset-",0)==0&&ValidName(name.substr(7)))names.push_back(name.substr(7));
    }std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());return names;
}
}
