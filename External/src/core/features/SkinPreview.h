#pragma once
#include "mesh/parser/MeshParser.h"
#include "mesh/cache/MeshCache.h"
#include "../../sdk/MeshBridge.h"
#include "../../render/menu/library.h"
#include <mutex>
namespace SkinPreview {
struct Triangle{Mesh::Vector3 p[3];ImU32 color;};
struct Model{std::vector<Triangle> triangles;std::string name;};
inline std::mutex mutex;
inline Model models[2];
inline void Capture(std::uint64_t root,int index,const std::string& label){
    using namespace Cheat::Visuals;Model model;model.name=label;
    for(const auto& part:MeshParser::CollectWeapon(root)){
        Cheat::BasePart bp(part.part);auto pos=bp.GetPosition(),size=bp.GetSize();auto rotation=bp.GetRotation();auto tint=bp.GetColor();
        const auto mesh=MeshCache::Get().FindShared(part.mesh_id);
        if(!mesh||mesh->vertices.empty()||mesh->faces.empty())continue;
        Mesh::Vector3 low{FLT_MAX,FLT_MAX,FLT_MAX},high{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(auto& v:mesh->vertices){low.x=(std::min)(low.x,v.pos[0]);low.y=(std::min)(low.y,v.pos[1]);low.z=(std::min)(low.z,v.pos[2]);high.x=(std::max)(high.x,v.pos[0]);high.y=(std::max)(high.y,v.pos[1]);high.z=(std::max)(high.z,v.pos[2]);}
        Mesh::Vector3 scale{size.x/(std::max)(.001f,high.x-low.x),size.y/(std::max)(.001f,high.y-low.y),size.z/(std::max)(.001f,high.z-low.z)};
        if(part.special_mesh)scale=g_Memory.Read<Mesh::Vector3>(part.special_mesh+Offsets::SpecialMesh::Scale);
        const auto stride=(std::max)(std::size_t(1),mesh->faces.size()/750);
        for(std::size_t f=0;f<mesh->faces.size()&&model.triangles.size()<18000;f+=stride){Triangle t{};bool valid=true;
            for(int j=0;j<3;++j){auto id=mesh->faces[f].indices[j];if(id>=mesh->vertices.size()){valid=false;break;}auto v=mesh->vertices[id];float x=v.pos[0]*scale.x,y=v.pos[1]*scale.y,z=v.pos[2]*scale.z;
                t.p[j]={pos.x+rotation.m[0][0]*x+rotation.m[0][1]*y+rotation.m[0][2]*z,pos.y+rotation.m[1][0]*x+rotation.m[1][1]*y+rotation.m[1][2]*z,pos.z+rotation.m[2][0]*x+rotation.m[2][1]*y+rotation.m[2][2]*z};
                if(!std::isfinite(t.p[j].x)||!std::isfinite(t.p[j].y)||!std::isfinite(t.p[j].z))valid=false;
            }
            if(valid){t.color=ImGui::ColorConvertFloat4ToU32(ImVec4(tint.r,tint.g,tint.b,1));model.triangles.push_back(t);}
        }
    }
    std::lock_guard<std::mutex> lock(mutex);models[index]=std::move(model);
}
inline void Draw(){
    std::lock_guard<std::mutex> lock(mutex);auto dl=ImGui::GetWindowDrawList();auto base=ImGui::GetWindowPos();
    for(int index=0;index<2;++index){auto& model=models[index];ImVec2 min=base+ImVec2(315,64+index*190.f),max=min+ImVec2(386,168);
        dl->AddRect(min,max,imGuiCustom::ColorU32(imGuiCustom::GetTheme().Accent));dl->AddText(min+ImVec2(12,10),imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),model.name.empty()?(index?"Glock 19":"AR-15"):model.name.c_str());
        if(model.triangles.empty()){dl->AddText(min+ImVec2(12,78),imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),"Preview appears when model data loads");continue;}
        float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(auto& t:model.triangles)for(auto p:t.p){float v[]={p.x,p.y,p.z};for(int k=0;k<3;++k){lo[k]=(std::min)(lo[k],v[k]);hi[k]=(std::max)(hi[k],v[k]);}}
        bool sideZ=hi[2]-lo[2]>hi[0]-lo[0];int axis=sideZ?2:0;float factor=(std::min)(350.f/(std::max)(.01f,hi[axis]-lo[axis]),110.f/(std::max)(.01f,hi[1]-lo[1]));
        dl->PushClipRect(min,max,true);
        for(auto& t:model.triangles){ImVec2 p[3];for(int j=0;j<3;++j){auto v=t.p[j];p[j]=ImVec2(min.x+193+((sideZ?v.z:v.x)-(lo[axis]+hi[axis])*.5f)*factor,min.y+100-(v.y-(lo[1]+hi[1])*.5f)*factor);}dl->AddTriangleFilled(p[0],p[1],p[2],t.color);}
        dl->PopClipRect();
    }
    dl->AddText(base+ImVec2(315,455),imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),"Model geometry / base colors");
}
}
