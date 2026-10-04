#pragma once
#include "../variables/variables.h"
#include "../globals/globals.h"
#include "../variables/player_rules.h"
#include "../cache/cache.h"
#include "../app/app.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <cctype>

namespace WeaponVisuals {
struct Surface { std::uintptr_t part; int kind; bool firstPerson; };
inline std::vector<Surface> surfaces;
inline bool Active(){return variables::Weapons::native||variables::Weapons::hide||variables::Weapons::arms.enabled||variables::Weapons::gloves.enabled;}
inline bool Matches(const std::string& name){
 return (variables::Weapons::ar15&&name.find("AR-15")!=std::string::npos)||(variables::Weapons::glock&&name.find("Glock")!=std::string::npos);
}
inline std::string Lower(std::string n){for(char& c:n)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return n;}
inline bool Scope(const std::string& name){

 auto n=Lower(name);
 return n.find("glass")!=n.npos||n.find("lens")!=n.npos||n.find("reticle")!=n.npos||n.find("projector")!=n.npos;
}
inline void RestoreHidden(){}
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
inline void Refresh(){
 static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::milliseconds(60);
 surfaces.clear();if(!Active())return;
 struct Node {RBX::RbxInstance item;int depth,kind;bool first;};
 std::vector<Node> queue;

 const auto camera=Globals::workspace.FindChildByClass("Camera");
 const auto arm=camera.FindChild("ArmModel");

 if(arm.Addr)queue.push_back({arm,0,0,true});
 std::unordered_set<std::uintptr_t> visited;
 while(!queue.empty()&&visited.size()<2048){
  auto n=queue.back();queue.pop_back();if(!n.item.Addr||n.depth>12||!visited.insert(n.item.Addr).second)continue;
  const auto name=n.item.GetName(),lower=Lower(name),cls=n.item.GetClass();
  if(Matches(name))n.kind=1;
  else if(lower.find("glove")!=lower.npos)n.kind=3;
  else if(lower=="left arm"||lower=="right arm"||lower=="leftarm"||lower=="rightarm")n.kind=2;
  const bool base=cls=="MeshPart"||cls=="Part"||cls=="UnionOperation";
  if(base&&n.kind){
   const bool hide=variables::Weapons::hide&&n.first;
   bool wanted=hide||(n.kind==1?variables::Weapons::native&&n.first&&variables::Weapons::viewmodel:n.kind==2?variables::Weapons::arms.enabled:variables::Weapons::gloves.enabled);
   if(!hide&&n.kind==1&&Scope(name))wanted=false;

   const bool uniqueShape=cls=="MeshPart"||cls=="UnionOperation"||AssetMesh(n.item.FindChildByClass("SpecialMesh"));
   const float alpha=memory->Read<float>(n.item.Addr+Offsets::BasePart::Transparency);
   if(wanted&&uniqueShape&&std::isfinite(alpha)&&alpha>=0&&alpha<.999f)surfaces.push_back({n.item.Addr,n.kind,n.first});
  }
  if(cls=="Script"||cls=="LocalScript"||cls=="ModuleScript"||cls=="ParticleEmitter"||cls=="Beam"||cls=="Trail")continue;
  for(auto child:n.item.GetChildList())queue.push_back({child,n.depth+1,n.kind,n.first});
 }
}
}
