#pragma once
#include "RuntimePaths.h"
#include "CustomModelData.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <string>
namespace CustomTextureData {
enum class Channel {Color,Material,Normal};
struct Preset {const char* name;const char* skin;int model;int groups;std::array<const wchar_t*,4> stems;const wchar_t* modelFile=nullptr;int category=0;};
inline constexpr std::array<Preset,109> Presets{{
    {"Bloodsport","X5",0,1,{L"ak-bloodsport"},nullptr,0},
    {"Asiimov","X5",0,1,{L"ak-asiimov"},nullptr,0},
    {"Crane Flight","X5",0,1,{L"ak-crane-flight"},nullptr,0},
    {"Printstream","Fade",1,3,{L"m4-printstream-receiver",L"m4-printstream-barrel",L"m4-printstream-stock"},nullptr,0},
    {"Asiimov","Fade",1,3,{L"m4-asiimov-receiver",L"m4-asiimov-barrel",L"m4-asiimov-stock"},nullptr,0},
    {"Inheritance","X5",0,1,{L"ak-inheritance"},nullptr,0},
    {"Nouveau Rouge","X5",0,1,{L"ak-nouveau-rouge"},nullptr,0},
    {"The Oligarch","X5",0,1,{L"ak-the-oligarch"},nullptr,0},
    {"Chantico's Fire","Fade",1,3,{L"m4-chanticos-fire-receiver",L"m4-chanticos-fire-barrel",L"m4-chanticos-fire-stock"},nullptr,0},
    {"Mecha Industries","Fade",1,3,{L"m4-mecha-industries-receiver",L"m4-mecha-industries-barrel",L"m4-mecha-industries-stock"},nullptr,0},
    {"Stratosphere","Fade",1,3,{L"m4-stratosphere-receiver",L"m4-stratosphere-barrel",L"m4-stratosphere-stock"},nullptr,0},
    {"Wash me plz","Fade",1,3,{L"m4-wash-me-plz-receiver",L"m4-wash-me-plz-barrel",L"m4-wash-me-plz-stock"},nullptr,0},
    {"Printstream","0416",2,4,{L"awp-printstream-body",L"awp-printstream-scope",L"awp-printstream-mag",L"awp-printstream-bolt"},nullptr,0},
    {"Lightning Strike","0416",2,4,{L"awp-lightning-strike-body",L"awp-lightning-strike-scope",L"awp-lightning-strike-mag",L"awp-lightning-strike-bolt"},nullptr,0},
    {"Desert Hydra","0416",2,4,{L"awp-desert-hydra-body",L"awp-desert-hydra-scope",L"awp-desert-hydra-mag",L"awp-desert-hydra-bolt"},nullptr,0},
    {"Gungnir","0416",2,4,{L"awp-gungnir-body",L"awp-gungnir-scope",L"awp-gungnir-mag",L"awp-gungnir-bolt"},nullptr,0},
    {"Queen's Gambit","0416",2,4,{L"awp-queens-gambit-body",L"awp-queens-gambit-scope",L"awp-queens-gambit-mag",L"awp-queens-gambit-bolt"},nullptr,0},
    {"Blaze","Gold",3,1,{L"deagle-blaze"},nullptr,0},
    {"Blue Ply","Gold",3,1,{L"deagle-blue-ply"},nullptr,0},
    {"Cobalt Disruption","Gold",3,1,{L"deagle-cobalt-disruption"},nullptr,0},
    {"Eastern Enigma","Gold",3,1,{L"deagle-eastern-enigma"},nullptr,0},
    {"Emerald J\u00f6rmungandr","Gold",3,1,{L"deagle-emerald-j-rmungandr"},nullptr,0},
    {"Firebreathing","Gold",3,1,{L"deagle-firebreathing"},nullptr,0},
    {"Oxide Blaze","Gold",3,1,{L"deagle-oxide-blaze"},nullptr,0},
    {"Printstream","Gold",3,1,{L"deagle-printstream"},nullptr,0},
    {"The Daily Deagle","Gold",3,1,{L"deagle-the-daily-deagle"},nullptr,0},
    {"Trigger Discipline","Gold",3,1,{L"deagle-trigger-discipline"},nullptr,0},
    {"Franklin","Fade",4,4,{L"glock-franklin",L"glock-franklin-slide",L"glock-franklin-mag",L"glock-franklin-barrel"},nullptr,0},
    {"Fully Tuned","Fade",4,4,{L"glock-fully-tuned",L"glock-fully-tuned-slide",L"glock-fully-tuned-mag",L"glock-fully-tuned-barrel"},nullptr,0},
    {"Gold Toof","Fade",4,4,{L"glock-gold-toof",L"glock-gold-toof-slide",L"glock-gold-toof-mag",L"glock-gold-toof-barrel"},nullptr,0},
    {"Moonrise","Fade",4,4,{L"glock-moonrise",L"glock-moonrise-slide",L"glock-moonrise-mag",L"glock-moonrise-barrel"},nullptr,0},
    {"Ramese's Reach","Fade",4,4,{L"glock-ramese-s-reach",L"glock-ramese-s-reach-slide",L"glock-ramese-s-reach-mag",L"glock-ramese-s-reach-barrel"},nullptr,0},
    {"Trace Lock","Fade",4,4,{L"glock-trace-lock",L"glock-trace-lock-slide",L"glock-trace-lock-mag",L"glock-trace-lock-barrel"},nullptr,0},
    {"Umbral Rabbit","Fade",4,4,{L"glock-umbral-rabbit",L"glock-umbral-rabbit-slide",L"glock-umbral-rabbit-mag",L"glock-umbral-rabbit-barrel"},nullptr,0},
    {"Vogue","Fade",4,4,{L"glock-vogue",L"glock-vogue-slide",L"glock-vogue-mag",L"glock-vogue-barrel"},nullptr,0},
    {"Water Elemental","Fade",4,4,{L"glock-water-elemental",L"glock-water-elemental-slide",L"glock-water-elemental-mag",L"glock-water-elemental-barrel"},nullptr,0},
    {"Consequence of the Jinn","X5",0,1,{L"ak-consequence-of-the-jinn"},nullptr,0},
    {"Frontside Misty","X5",0,1,{L"ak-frontside-misty"},nullptr,0},
    {"Gold Arabesque","X5",0,1,{L"ak-gold-arabesque"},nullptr,0},
    {"Legion of Anubis","X5",0,1,{L"ak-legion-of-anubis"},nullptr,0},
    {"Neon Revolution","X5",0,1,{L"ak-neon-revolution"},nullptr,0},
    {"Point Disarray","X5",0,1,{L"ak-point-disarray"},nullptr,0},
    {"Rat Rod","X5",0,1,{L"ak-rat-rod"},nullptr,0},
    {"Redline","X5",0,1,{L"ak-redline"},nullptr,0},
    {"Safety Net","X5",0,1,{L"ak-safety-net"},nullptr,0},
    {"Searing Rage","X5",0,1,{L"ak-searing-rage"},nullptr,0},
    {"The Empress","X5",0,1,{L"ak-the-empress"},nullptr,0},
    {"X-Ray","X5",0,1,{L"ak-x-ray"},nullptr,0},
    {"Atheris","0416",2,4,{L"awp-atheris-body",L"awp-atheris-scope",L"awp-atheris-mag",L"awp-atheris-bolt"},nullptr,0},
    {"Black Nile","0416",2,4,{L"awp-black-nile-body",L"awp-black-nile-scope",L"awp-black-nile-mag",L"awp-black-nile-bolt"},nullptr,0},
    {"Crakow!","0416",2,4,{L"awp-crakow-body",L"awp-crakow-scope",L"awp-crakow-mag",L"awp-crakow-bolt"},nullptr,0},
    {"Exothermic","0416",2,4,{L"awp-exothermic-body",L"awp-exothermic-scope",L"awp-exothermic-mag",L"awp-exothermic-bolt"},nullptr,0},
    {"LongDog","0416",2,4,{L"awp-longdog-body",L"awp-longdog-scope",L"awp-longdog-mag",L"awp-longdog-bolt"},nullptr,0},
    {"Medusa","0416",2,4,{L"awp-medusa-body",L"awp-medusa-scope",L"awp-medusa-mag",L"awp-medusa-bolt"},nullptr,0},
    {"Oni Taiji","0416",2,4,{L"awp-oni-taiji-body",L"awp-oni-taiji-scope",L"awp-oni-taiji-mag",L"awp-oni-taiji-bolt"},nullptr,0},
    {"Sovereign Flame","0416",2,4,{L"awp-sovereign-flame-body",L"awp-sovereign-flame-scope",L"awp-sovereign-flame-mag",L"awp-sovereign-flame-bolt"},nullptr,0},
    {"The End","0416",2,4,{L"awp-the-end-body",L"awp-the-end-scope",L"awp-the-end-mag",L"awp-the-end-bolt"},nullptr,0},
    {"The Prince","0416",2,4,{L"awp-the-prince-body",L"awp-the-prince-scope",L"awp-the-prince-mag",L"awp-the-prince-bolt"},nullptr,0},
    {"Wildfire","0416",2,4,{L"awp-wildfire-body",L"awp-wildfire-scope",L"awp-wildfire-mag",L"awp-wildfire-bolt"},nullptr,0},
    {"Decimator","Fade",1,3,{L"m4-decimator-receiver",L"m4-decimator-barrel",L"m4-decimator-stock"},nullptr,0},
    {"Flashback","Fade",1,3,{L"m4-flashback-receiver",L"m4-flashback-barrel",L"m4-flashback-stock"},nullptr,0},
    {"Guardian","Fade",1,3,{L"m4-guardian-receiver",L"m4-guardian-barrel",L"m4-guardian-stock"},nullptr,0},
    {"Hot Rod","Fade",1,3,{L"m4-hot-rod-receiver",L"m4-hot-rod-barrel",L"m4-hot-rod-stock"},nullptr,0},
    {"Hyper Beast","Fade",1,3,{L"m4-hyper-beast-receiver",L"m4-hyper-beast-barrel",L"m4-hyper-beast-stock"},nullptr,0},
    {"Imminent Danger","Fade",1,3,{L"m4-imminent-danger-receiver",L"m4-imminent-danger-barrel",L"m4-imminent-danger-stock"},nullptr,0},
    {"Knight","Fade",1,3,{L"m4-knight-receiver",L"m4-knight-barrel",L"m4-knight-stock"},nullptr,0},
    {"Leaded Glass","Fade",1,3,{L"m4-leaded-glass-receiver",L"m4-leaded-glass-barrel",L"m4-leaded-glass-stock"},nullptr,0},
    {"Master Piece","Fade",1,3,{L"m4-master-piece-receiver",L"m4-master-piece-barrel",L"m4-master-piece-stock"},nullptr,0},
    {"Night Terror","Fade",1,3,{L"m4-night-terror-receiver",L"m4-night-terror-barrel",L"m4-night-terror-stock"},nullptr,0},
    {"Player Two","Fade",1,3,{L"m4-player-two-receiver",L"m4-player-two-barrel",L"m4-player-two-stock"},nullptr,0},
    {"Solitude","Fade",1,3,{L"m4-solitude-receiver",L"m4-solitude-barrel",L"m4-solitude-stock"},nullptr,0},
    {"Vaporwave","Fade",1,3,{L"m4-vaporwave-receiver",L"m4-vaporwave-barrel",L"m4-vaporwave-stock"},nullptr,0},
    {"VariCamo","Fade",1,3,{L"m4-varicamo-receiver",L"m4-varicamo-barrel",L"m4-varicamo-stock"},nullptr,0},
    {"Cirno MK14","0416",2,4,{L"other-cirno-mk14-body",L"other-cirno-mk14-scope",L"other-cirno-mk14-mag",L"other-cirno-mk14-bolt"},nullptr,1},
    {"Nekoha Shizuku","X5",0,1,{L"other-nekoha-shizuku"},nullptr,1},
    {"Banshee","X5",0,1,{L"other-banshee"},nullptr,1},
    {"Vanilla NEKOPARA","0416",2,4,{L"other-vanilla-nekopara-body",L"other-vanilla-nekopara-scope",L"other-vanilla-nekopara-mag",L"other-vanilla-nekopara-bolt"},nullptr,1},
    {"Blood Splatter Camo","Fade",1,3,{L"other-blood-splatter-camo-receiver",L"other-blood-splatter-camo-barrel",L"other-blood-splatter-camo-stock"},nullptr,1},
    {"Nahida MK110","Fade",1,3,{L"other-nahida-mk110-receiver",L"other-nahida-mk110-barrel",L"other-nahida-mk110-stock"},nullptr,1},
    {"Sakuya M4A4","Fade",1,3,{L"other-sakuya-m4a4-receiver",L"other-sakuya-m4a4-barrel",L"other-sakuya-m4a4-stock"},nullptr,1},
    {"Senko","Fade",4,4,{L"other-senko-glock",L"other-senko-glock-slide",L"other-senko-glock-mag",L"other-senko-glock-barrel"},nullptr,1},
    {"Gawr Gura M4","Fade",1,3,{L"other-gawr-gura-m4-receiver",L"other-gawr-gura-m4-barrel",L"other-gawr-gura-m4-stock"},nullptr,1},
    {"Hot Blood AK74","X5",0,1,{L"other-hot-blood-ak74"},nullptr,1},
    {"Yakumo Yukari","X5",0,1,{L"other-yakumo-yukari"},nullptr,1},
    {"Skadi M4A1-S","Fade",1,3,{L"other-skadi-m4a1s-receiver",L"other-skadi-m4a1s-barrel",L"other-skadi-m4a1s-stock"},nullptr,1},
    {"Reimu M4A1-S","Fade",1,3,{L"other-reimu-m4a1s-receiver",L"other-reimu-m4a1s-barrel",L"other-reimu-m4a1s-stock"},nullptr,1},
    {"Gray Raven XXI M4A1","Fade",1,3,{L"other-gray-raven-m4a1-receiver",L"other-gray-raven-m4a1-barrel",L"other-gray-raven-m4a1-stock"},nullptr,1},
    {"Rem & Ram M4A1-S","Fade",1,3,{L"other-rem-ram-m4a1s-receiver",L"other-rem-ram-m4a1s-barrel",L"other-rem-ram-m4a1s-stock"},nullptr,1},
    {"Kokoro Amamiya M4A1-S","Fade",1,3,{L"other-kokoro-m4a1s-receiver",L"other-kokoro-m4a1s-barrel",L"other-kokoro-m4a1s-stock"},nullptr,1},
    {"Sweet Cat Ear M4A1-S","Fade",1,3,{L"other-sweet-cat-m4a1s-receiver",L"other-sweet-cat-m4a1s-barrel",L"other-sweet-cat-m4a1s-stock"},nullptr,1},
    {"Enterprise M4A1-S","Fade",1,3,{L"other-enterprise-m4a1s-receiver",L"other-enterprise-m4a1s-barrel",L"other-enterprise-m4a1s-stock"},nullptr,1},
    {"Chloe M4A1-S","Fade",1,3,{L"other-chloe-m4a1s-receiver",L"other-chloe-m4a1s-barrel",L"other-chloe-m4a1s-stock"},nullptr,1},
    {"Nekoha Shizuku M4A1","Fade",1,3,{L"other-nekoha-m4a1-receiver",L"other-nekoha-m4a1-barrel",L"other-nekoha-m4a1-stock"},nullptr,1},
    {"Enterprise White M4A1-S","Fade",1,3,{L"other-enterprise-white-m4a1s-receiver",L"other-enterprise-white-m4a1s-barrel",L"other-enterprise-white-m4a1s-stock"},nullptr,1},
    {"M4 Bloodsport","Fade",1,3,{L"cs-m4-bloodsport-receiver",L"cs-m4-bloodsport-barrel",L"cs-m4-bloodsport-stock"},nullptr,0},
    {"Nailuo M4A1-S","Fade",1,3,{L"other-nailuo-m4a1s-receiver",L"other-nailuo-m4a1s-barrel",L"other-nailuo-m4a1s-stock"},nullptr,1},
    {"Graf Zeppelin M4A1-S","Fade",1,3,{L"other-graf-zeppelin-m4a1s-receiver",L"other-graf-zeppelin-m4a1s-barrel",L"other-graf-zeppelin-m4a1s-stock"},nullptr,1},
    {"Ryo AK-47","X5",0,1,{L"other-ryo-ak47"},nullptr,1},
    {"Saratoga AK-47","X5",0,1,{L"other-saratoga-ak47"},nullptr,1},
    {"Koneko","X5",0,1,{L"other-koneko-ak"},nullptr,1},
    {"Koneko","Fade",1,3,{L"other-koneko-m4-receiver",L"other-koneko-m4-barrel",L"other-koneko-m4-stock"},nullptr,1},
    {"Karin AK-47","X5",0,1,{L"other-karin-ak47"},nullptr,1},
    {"Hibiki AK-12","X5",0,1,{L"other-hibiki-ak12"},nullptr,1},
    {"Yoimiya AK-47","X5",0,1,{L"other-yoimiya-ak47"},nullptr,1},
    {"Azhe AKM","X5",0,1,{L"other-azhe-akm"},nullptr,1},
    {"Cherry Blossom AK-47","X5",0,1,{L"other-cherry-blossom-ak47"},nullptr,1},
    {"Minato Aqua AK-47","X5",0,1,{L"other-minato-aqua-ak47"},nullptr,1},
    {"Hellfire AK-47","X5",0,1,{L"other-hellfire-ak47"},nullptr,1},
    {"Wastelander","X5",0,4,{L"wastelander-body",L"wastelander-sight",L"wastelander-lens",L"wastelander-dot"},L"wastelander-preview.gwm",1}
}};
inline bool Valid(int index){return index>=0&&index<static_cast<int>(Presets.size());}
inline bool Available(int index){return Valid(index)&&index!=73&&index!=76&&index!=79&&index!=81&&index!=86;}
inline int ActiveCount(){int count=0;for(int i=0;i<static_cast<int>(Presets.size());++i)count+=Available(i)?1:0;return count;}
inline bool Supports(int index,bool glock){return Available(index)&&(glock?(Presets[index].model>=2&&Presets[index].model<=4):Presets[index].model<=2);}
inline std::wstring Name(int index,Channel channel,int group=0){
    if(!Valid(index)||group<0||group>=Presets[index].groups)return {};
    if(channel==Channel::Normal)return L"neutral-normal.png";
    return std::wstring(Presets[index].stems[group])+(channel==Channel::Material?L"-material":channel==Channel::Normal?L"-normal":L"")+L".png";
}
inline std::filesystem::path File(int index,Channel channel,int group=0){const auto name=Name(index,channel,group);return name.empty()?std::filesystem::path{}:RuntimePaths::File(L"custom-textures")/name;}
inline std::filesystem::path File(int index,bool material=false){return File(index,material?Channel::Material:Channel::Color);}
inline std::wstring SourceName(int index,Channel channel=Channel::Color){return Name(index,channel);}
inline std::filesystem::path SourceFile(int index,Channel channel=Channel::Color){return File(index,channel);}
inline bool RequiresModel(int index){return index==108;}
inline std::wstring NativeModelName(int index){return index==108?L"wastelander-native.gwm":L"";}
inline std::wstring ModelName(int index){if(!Valid(index))return {};if(Presets[index].modelFile)return Presets[index].modelFile;const wchar_t* names[]={L"native-x5.gwm",L"native-m4a1.gwm",L"native-0416.gwm",L"native-deagle.gwm",L"native-glock.gwm"};return names[Presets[index].model];}
inline std::wstring ShadowName(int index,int group){if(!Valid(index)||index==108||group<0||group>=Presets[index].groups)return {};const wchar_t* names[]={L"x5",L"m4a1",L"0416",L"deagle",L"glock"};return L"shadow-"+std::wstring(names[Presets[index].model])+L"-"+std::to_wstring(group)+L".png";}
inline std::filesystem::path ShadowFile(int index,int group){return RuntimePaths::File(L"custom-textures")/ShadowName(index,group);}
inline std::filesystem::path ModelFile(int index){return RuntimePaths::File(L"custom-textures")/ModelName(index);}
inline std::wstring PreviewName(int index,bool=false){return Valid(index)?std::wstring(Presets[index].stems[0])+L"-preview.png":std::wstring{};}
inline std::filesystem::path PreviewFile(int index,bool=false){return RuntimePaths::File(L"custom-textures")/PreviewName(index);}
inline bool KnownFile(const std::wstring& name){for(int i=0;i<static_cast<int>(Presets.size());++i){if(name==ModelName(i)||(!NativeModelName(i).empty()&&name==NativeModelName(i))||name==PreviewName(i))return true;for(int group=0;group<Presets[i].groups;++group)for(auto channel:{Channel::Color,Channel::Material,Channel::Normal})if(name==Name(i,channel,group))return true;}return false;}

inline std::string AssetId(std::string mesh){
    const auto start=mesh.find("rbxassetid://");const auto query=mesh.find("id=");
    if(start!=mesh.npos)mesh=mesh.substr(start+13);else if(query!=mesh.npos)mesh=mesh.substr(query+3);
    const auto end=mesh.find_first_not_of("0123456789");if(end!=mesh.npos)mesh=mesh.substr(0,end);return mesh;
}
inline int Group(int index,const std::string& value){
    if(!Valid(index))return -1;const auto mesh=AssetId(value);
    if(Presets[index].model==0){for(auto id:{"105137249332183","81766375295784","93054331126203","95891610475658","118641859394860","71700100568681","108003260543980","86598522525666","110878660637855","74915530422476","86071090387346","77729921739121"})if(mesh==id)return 0;}
    if(Presets[index].model==1){if(mesh=="130533951042187"||mesh=="89088865339034")return 0;if(mesh=="92394911895072"||mesh=="140300391074568")return 1;if(mesh=="88483816698490"||mesh=="109005072716981")return 2;}
    if(Presets[index].model==2){if(mesh=="72915978896741")return 0;if(mesh=="88673729944263")return 1;if(mesh=="112855877398692")return 2;if(mesh=="121498307784399")return 3;}
    if(Presets[index].model==3)for(auto id:{"17788240085","5165152169","5165153537","5165062009","5160693646","5165147746","5160693225","5160689305","5160695315"})if(mesh==id)return 0;
    if(Presets[index].model==4){if(mesh=="139968168789097")return 0;if(mesh=="88526102437226")return 1;if(mesh=="95490897912075")return 2;if(mesh=="113543892953755"||mesh=="105223351972520")return 3;}

    return -1;
}
inline int Anchor(int index,const std::string& value){
    if(!Valid(index))return -1;const auto mesh=AssetId(value);const auto model=Presets[index].model;
    if(model==0){if(mesh=="105137249332183")return 0;if(mesh=="74915530422476")return 1;if(mesh=="86071090387346")return 2;if(mesh=="81766375295784")return 3;}
    if(model==1){if(mesh=="130533951042187")return 0;if(mesh=="89088865339034")return 1;if(mesh=="109005072716981")return 2;if(mesh=="92394911895072")return 3;}
    if(model==2)return Group(index,value);
    if(model==3){if(mesh=="5165062009")return 0;if(mesh=="17788240085")return 1;if(mesh=="5165153537")return 2;if(mesh=="5160689305")return 3;}
    if(model==4){if(mesh=="139968168789097")return 0;if(mesh=="88526102437226")return 1;if(mesh=="95490897912075")return 2;if(mesh=="113543892953755")return 3;}
    return -1;
}
inline bool ModelPart(int index,const std::string& value){
    if(!Valid(index))return false;const auto mesh=AssetId(value);
    if(Presets[index].model==3){for(const auto* id:{"17788240085","5165152169","5165153537","5165062009","5160693646","5165147746","5160693225","5160689305","5160695315"})if(mesh==id)return true;}
    if(Presets[index].model==4){for(const auto* id:{"113543892953755","139968168789097","88526102437226","105223351972520","95490897912075"})if(mesh==id)return true;}
    return Presets[index].model<=2&&Group(index,value)>=0;
}
inline bool Ornament(int index,const std::string& value){
    if(!Valid(index))return false;const auto mesh=AssetId(value);
    if(Presets[index].model==0)return mesh=="90519117045622"||mesh=="120365066408341"||mesh=="110502438098991";
    return(Presets[index].model==1||Presets[index].model==4)&&mesh=="77537260328217";
}
inline bool ImagePresent(const std::filesystem::path& file){
    try{if(!std::filesystem::is_regular_file(file))return false;std::ifstream f(file,std::ios::binary);std::array<unsigned char,24> b{};if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))return false;
        const unsigned char signature[]={137,80,78,71,13,10,26,10};if(!std::equal(std::begin(signature),std::end(signature),b.begin()))return false;
        auto u32=[&](int offset){return(unsigned(b[offset])<<24)|(unsigned(b[offset+1])<<16)|(unsigned(b[offset+2])<<8)|b[offset+3];};
        return b[12]=='I'&&b[13]=='H'&&b[14]=='D'&&b[15]=='R'&&u32(16)>0&&u32(16)<=4096&&u32(20)>0&&u32(20)<=4096;
    }catch(...){return false;}
}
inline bool Installed(int index,const std::filesystem::path& directory){if(!Valid(index))return false;for(int g=0;g<Presets[index].groups;++g)for(auto c:{Channel::Color,Channel::Material,Channel::Normal})if(!ImagePresent(directory/Name(index,c,g)))return false;if(RequiresModel(index)&&(!CustomModelData::Present(directory/ModelName(index))||!CustomModelData::Present(directory/NativeModelName(index))))return false;return true;}
inline bool Present(const std::filesystem::path& directory){for(int i=0;i<static_cast<int>(Presets.size());++i)if(Available(i)&&!Installed(i,directory))return false;return true;}
}
