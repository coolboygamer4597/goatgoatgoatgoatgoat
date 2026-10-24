#pragma once
#include "../variables/variables.h"
#include "../globals/globals.h"
#include "../variables/player_rules.h"
#include "../cache/cache.h"
#include "../app/app.h"
#include "native/WeaponTextureRuntime.h"
#include "native/WeaponOrnamentRuntime.h"
#include "../../sdk/OffsetUpdates.h"
#include "../functions/skins/skins.h"
#include "../app/CustomTextureData.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <cctype>

namespace WeaponVisuals {
struct Surface { std::uintptr_t part; int kind; bool firstPerson; bool textureTest=false,isGlock=false; int textureGroup=0; bool hidden=false; };
inline std::vector<Surface> surfaces;
inline std::vector<WeaponOrnamentRuntime::Change> ornamentChanges;
inline std::vector<WeaponOrnamentRuntime::ByteChange> ornamentEnabledChanges;
inline DWORD ornamentPid=0;
inline bool Active(){return variables::Weapons::removeX5Outline||variables::Weapons::AnyNative()||variables::Weapons::texturePreset>=0||variables::Weapons::glockTexturePreset>=0||variables::Weapons::hide||variables::Weapons::AnyLimb(false)||variables::Weapons::AnyLimb(true);}
inline bool Matches(const std::string& name){
 return (variables::Weapons::ar15&&name.find("AR-15")!=std::string::npos)||(variables::Weapons::glock&&name.find("Glock")!=std::string::npos);
}
inline std::string Lower(std::string n){for(char& c:n)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return n;}
inline bool Scope(const std::string& name){

 auto n=Lower(name);
 return n.find("glass")!=n.npos||n.find("lens")!=n.npos||n.find("reticle")!=n.npos||n.find("projector")!=n.npos;
}

inline bool AssetMesh(RBX::RbxInstance mesh){
 if(!mesh.Addr)return false;
 const auto field=mesh.Addr+Offsets::SpecialMesh::MeshId;
 auto asset=[](const std::string& id){
  if(id.empty()||id=="Unknown"||std::any_of(id.begin(),id.end(),[](unsigned char c){return c<32||c>126;}))return false;
  return id.rfind("rbxassetid://",0)==0||id.rfind("rbxasset://",0)==0||id.rfind("https://",0)==0||id.rfind("http://",0)==0||
      std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isdigit(c)!=0;});
 };

 return asset(memory->read_string(field))||asset(memory->read_string(memory->Read<std::uintptr_t>(field)));
}
inline void Refresh(bool restoreEffects=false){
 static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(!restoreEffects&&now<next)return;next=now+std::chrono::milliseconds(60);
 surfaces.clear();const auto pid=memory->get_process_id();if(pid!=ornamentPid){ornamentChanges.clear();ornamentEnabledChanges.clear();ornamentPid=pid;}
 if(!memory->IsConnected()||(!Active()&&ornamentChanges.empty()&&ornamentEnabledChanges.empty()))return;
 const auto particleLayout=WeaponOrnamentRuntime::Resolve(Offsets::ClientVersion,OffsetUpdates::OptionalField("ParticleEmitter.Rate"),OffsetUpdates::OptionalField("ParticleEmitter.Texture"),OffsetUpdates::OptionalField("ParticleEmitter.LocalTransparencyModifier"),OffsetUpdates::OptionalField("ParticleEmitter.Enabled"),OffsetUpdates::OptionalField("ParticleEmitter.Lifetime"));
 const auto outlineField=WeaponOrnamentRuntime::OutlineField(Offsets::ClientVersion,OffsetUpdates::OptionalField("Highlight.OutlineTransparency"));
 std::vector<WeaponOrnamentRuntime::Candidate> effects,enabledEffects;
 struct Node {RBX::RbxInstance item;int depth,kind;bool first;bool isGlock=false,source=false;};
 std::vector<Node> queue;

 const auto camera=Globals::workspace.FindChildByClass("Camera");
 const auto arm=camera.FindChild("ArmModel");

 if(arm.Addr)queue.push_back({arm,0,0,true,Skins::CurrentWeaponIsGlock()});
 const auto weapons=Globals::dataModel.FindChildByClass("ReplicatedStorage").FindChild("Models").FindChild("Weapons");
 for(const char* weapon:{"AR-15","Glock 19"}){
  const auto folder=weapons.FindChild(weapon);
  for(const char* skin:{"Default","X5"}){const auto model=folder.FindChild((std::string(weapon)+"_"+skin).c_str());if(model.Addr)queue.push_back({model,0,1,false,std::string(weapon)=="Glock 19",true});}
 }

 std::unordered_set<std::uintptr_t> visited;
 while(!queue.empty()&&visited.size()<2048){
  auto n=queue.back();queue.pop_back();if(!n.item.Addr||n.depth>12||!visited.insert(n.item.Addr).second)continue;
  const auto name=n.item.GetName(),lower=Lower(name),cls=n.item.GetClass();
  if(Matches(name)){n.kind=1;n.isGlock=name.find("Glock")!=std::string::npos;}
  else if(lower.find("glove")!=lower.npos)n.kind=3;
  else if(lower=="left arm"||lower=="right arm"||lower=="leftarm"||lower=="rightarm")n.kind=2;
  if(particleLayout&&cls=="ParticleEmitter"&&n.kind==1&&n.source){
   const auto parent=n.item.GetParent();const auto texture=CustomTextureData::AssetId(memory->read_string(n.item.Addr+particleLayout.texture));
   if(WeaponOrnamentRuntime::Glint(name,parent.GetName(),texture)){
    const auto preset=variables::Weapons::TextureForWeapon(n.isGlock);const bool hide=!restoreEffects&&variables::Weapons::modelCleanup&&CustomTextureData::Supports(preset,n.isGlock)&&CustomTextureData::Presets[preset].model==0;
    effects.push_back({n.item.Addr,parent.Addr,memory->Read<std::uintptr_t>(n.item.Addr+Offsets::Instance::ClassDescriptor),n.item.Addr+particleLayout.rate,hide});
    if(particleLayout.transparency)effects.push_back({n.item.Addr,parent.Addr,memory->Read<std::uintptr_t>(n.item.Addr+Offsets::Instance::ClassDescriptor),n.item.Addr+particleLayout.transparency,hide,true});
    if(particleLayout.lifetime)for(std::uintptr_t step=0;step<2;++step){const auto component=(hide?step:1-step)*4;effects.push_back({n.item.Addr,parent.Addr,memory->Read<std::uintptr_t>(n.item.Addr+Offsets::Instance::ClassDescriptor),n.item.Addr+particleLayout.lifetime+component,hide});}
    if(particleLayout.enabled)enabledEffects.push_back({n.item.Addr,parent.Addr,memory->Read<std::uintptr_t>(n.item.Addr+Offsets::Instance::ClassDescriptor),n.item.Addr+particleLayout.enabled,hide});
   }
  }
  if(outlineField&&cls=="Highlight"&&n.kind==1&&n.source){
   const auto parent=n.item.GetParent();bool x5=false;
   for(auto child:parent.GetChildList())if(child.GetClass()=="MeshPart"){
    const auto field=child.Addr+Offsets::MeshPart::MeshId;
    if(WeaponTextureRuntime::MatchesX5(memory->read_string(field))||WeaponTextureRuntime::MatchesX5(memory->read_string(memory->Read<std::uintptr_t>(field)))){x5=true;break;}
   }
   if(x5)effects.push_back({n.item.Addr,parent.GetParent().Addr,memory->Read<std::uintptr_t>(n.item.Addr+Offsets::Instance::ClassDescriptor),n.item.Addr+outlineField,!restoreEffects&&variables::Weapons::removeX5Outline,true});
  }
  const bool base=cls=="MeshPart"||cls=="Part"||cls=="UnionOperation";
  if(base&&n.kind&&!n.source){
   const bool hide=variables::Weapons::hide&&n.first;
   bool textureTest=false,hidden=false;int textureGroup=-1;
   const int preset=variables::Weapons::TextureForWeapon(n.isGlock);
   if(n.kind==1&&n.first&&!hide&&CustomTextureData::Supports(preset,n.isGlock)&&cls=="MeshPart") {
    const auto field=n.item.Addr+Offsets::MeshPart::MeshId;
    textureGroup=CustomTextureData::Group(preset,memory->read_string(field));
    if(textureGroup<0)textureGroup=CustomTextureData::Group(preset,memory->read_string(memory->Read<std::uintptr_t>(field)));
    textureTest=textureGroup>=0;
    if(variables::Weapons::modelCleanup){hidden=CustomTextureData::Ornament(preset,memory->read_string(field));if(!hidden)hidden=CustomTextureData::Ornament(preset,memory->read_string(memory->Read<std::uintptr_t>(field)));}
   }
   bool wanted=hide||hidden||(n.kind==1?(variables::Weapons::MaterialForWeapon(n.isGlock).enabled||textureTest)&&n.first&&variables::Weapons::viewmodel:variables::Weapons::MaterialForLimb(n.isGlock,n.kind==3).enabled);
   if(!hide&&n.kind==1&&Scope(name))wanted=false;

   const bool uniqueShape=cls=="MeshPart"||cls=="UnionOperation"||AssetMesh(n.item.FindChildByClass("SpecialMesh"));
   const float alpha=memory->Read<float>(n.item.Addr+Offsets::BasePart::Transparency);
   if(wanted&&uniqueShape&&std::isfinite(alpha)&&alpha>=0&&alpha<.999f)surfaces.push_back({n.item.Addr,n.kind,n.first,textureTest,n.isGlock,std::max(0,textureGroup),hidden});
  }
  if(cls=="Script"||cls=="LocalScript"||cls=="ModuleScript"||cls=="ParticleEmitter"||cls=="Beam"||cls=="Trail")continue;
  for(auto child:n.item.GetChildList())queue.push_back({child,n.depth+1,n.kind,n.first,n.isGlock,n.source});
 }
 WeaponOrnamentRuntime::UpdateEnabled(enabledEffects,ornamentEnabledChanges,[](std::uintptr_t field,std::uint8_t& value){return memory->read_raw(field,&value,sizeof(value));},[](std::uintptr_t field,std::uint8_t value){return memory->Write<std::uint8_t>(field,value);});
 WeaponOrnamentRuntime::Update(effects,ornamentChanges,[](std::uintptr_t field,float& value){return memory->read_raw(field,&value,sizeof(value));},[](std::uintptr_t field,float value){return memory->Write<float>(field,value);});
}
inline int TextureSelection(){
 for(const auto& s:surfaces)if(s.kind==1&&s.firstPerson){const auto selected=variables::Weapons::TextureForWeapon(s.isGlock);return CustomTextureData::Supports(selected,s.isGlock)?selected:-1;}
 return -2;
}
}
