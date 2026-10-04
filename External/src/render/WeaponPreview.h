#pragma once

#include "../core/features/mesh/parser/MeshParser.h"
#include "../core/features/mesh/cache/MeshCache.h"
#include "../core/features/native/NativeChams.h"
#include "../core/features/native/NativePattern.h"
#include "../sdk/MeshBridge.h"
#include "../core/globals/globals.h"
#include "menu/library.h"
#include "../core/functions/skins/skins.h"
#include "PackedPreview.h"
#include <d3dcompiler.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <future>
#include <chrono>
#include <array>
#include <cfloat>

namespace WeaponPreview {
using Microsoft::WRL::ComPtr;
using V3=Mesh::Vector3;
struct Vertex { float p[3],local[3],tint[4],seed; };
struct Snapshot { std::vector<Vertex> vertices; std::string name,status; std::uintptr_t root=0; };
inline bool Finite(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline V3 Rotate(const Mesh::Matrix4x4& r,V3 v){return {r.m[0][0]*v.x+r.m[0][1]*v.y+r.m[0][2]*v.z,r.m[1][0]*v.x+r.m[1][1]*v.y+r.m[1][2]*v.z,r.m[2][0]*v.x+r.m[2][1]*v.y+r.m[2][2]*v.z};}
inline V3 Inverse(const Mesh::Matrix4x4& r,V3 v){return {r.m[0][0]*v.x+r.m[1][0]*v.y+r.m[2][0]*v.z,r.m[0][1]*v.x+r.m[1][1]*v.y+r.m[2][1]*v.z,r.m[0][2]*v.x+r.m[1][2]*v.y+r.m[2][2]*v.z};}
inline float Seed(std::uint64_t part){auto h=part*0x9E3779B97F4A7C15ull;h^=h>>33;h*=0xFF51AFD7ED558CCDull;h^=h>>33;return float(h&0xffff)*(1024.f/65536.f);}
inline void Fit(Snapshot& s){
    float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    for(const auto& v:s.vertices)for(int k=0;k<3;++k){lo[k]=(std::min)(lo[k],v.p[k]);hi[k]=(std::max)(hi[k],v.p[k]);}

    std::array<int,3> axes{0,1,2};
    std::sort(axes.begin(),axes.end(),[&](int a,int b){return hi[a]-lo[a]>hi[b]-lo[b];});
    const int horizontal=axes[0],vertical=axes[1],depth=axes[2];
    const float extent=(std::max)(hi[horizontal]-lo[horizontal],.01f);
    const float thickness=(std::max)(hi[depth]-lo[depth],.01f);
    for(auto& v:s.vertices){
        float x=(v.p[horizontal]-(lo[horizontal]+hi[horizontal])*.5f)*1.8f/extent;
        float y=(v.p[vertical]-(lo[vertical]+hi[vertical])*.5f)*1.8f/extent;
        float z=.1f+.8f*(v.p[depth]-lo[depth])/thickness;
        v.p[0]=x;v.p[1]=y;v.p[2]=z;
    }
}
inline void FitAvatar(Snapshot& s){
    if(s.vertices.empty())return;
    float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    for(const auto& v:s.vertices)for(int k=0;k<3;++k){lo[k]=(std::min)(lo[k],v.p[k]);hi[k]=(std::max)(hi[k],v.p[k]);}
    float center[3]={(lo[0]+hi[0])*.5f,(lo[1]+hi[1])*.5f,(lo[2]+hi[2])*.5f};float radius=.01f;
    for(const auto& v:s.vertices){float r=0;for(int k=0;k<3;++k)r+=(v.p[k]-center[k])*(v.p[k]-center[k]);radius=(std::max)(radius,std::sqrt(r));}
    for(auto& v:s.vertices)for(int k=0;k<3;++k)v.p[k]=(v.p[k]-center[k])/radius;
}
inline Snapshot Avatar(){
    Snapshot s;s.name="Avatar preview";
    const int triangles[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    auto box=[&](V3 center,V3 half){
        V3 v[8];for(int i=0;i<8;++i){v[i]={center.x+((i==1||i==2||i==5||i==6)?half.x:-half.x),center.y+((i==2||i==3||i==6||i==7)?half.y:-half.y),center.z+(i>=4?half.z:-half.z)};}
        for(auto index:triangles){auto q=v[index];
            s.vertices.push_back({{q.x,q.y,q.z},{q.x,q.y,q.z},{.5f,.5f,.5f,1},0});}
    };
    box({0,0,0},{1,1,.5f});box({0,1.5f,0},{1,.5f,.5f});
    box({-1.5f,0,0},{.5f,1,.5f});box({1.5f,0,0},{.5f,1,.5f});
    box({-.5f,-2,0},{.5f,1,.5f});box({.5f,-2,0},{.5f,1,.5f});FitAvatar(s);return s;
}
inline Snapshot CaptureRoot(RBX::RbxInstance root,const std::string& label,const std::vector<Cheat::Visuals::MeshParser::Entry>* capturedParts=nullptr,bool avatar=false){
    using namespace Cheat::Visuals;
    Snapshot result;result.name=label;result.status="Model is not available in the catalog";
    if(!root.Addr)return result;
    result.root=root.Addr;
    const auto parts=capturedParts?*capturedParts:MeshParser::CollectWeapon(root.Addr);
    std::vector<std::string> wanted;for(const auto& part:parts)if(!part.mesh_id.empty())wanted.push_back(part.mesh_id);
    if(!wanted.empty())MeshCache::Get().Refresh(true,true,wanted);
    struct Part {MeshParser::Entry entry;V3 pos,size;Mesh::Matrix4x4 rotation;};
    std::vector<Part> frames;
    for(const auto& entry:parts){
        Part part;part.entry=entry;
        if(Cheat::BasePart(entry.part).GetFrameData(part.pos,part.rotation,part.size)&&Finite(part.pos)&&Finite(part.size))frames.push_back(part);
    }
    if(frames.empty()){result.status="Waiting for weapon geometry";return result;}
    auto anchor=frames.begin();
    for(auto it=frames.begin();it!=frames.end();++it)if(it->entry.name==(avatar?"Torso":"Handle")){anchor=it;break;}
    const auto origin=anchor->pos;const auto basis=anchor->rotation;
    int missing=0;
    for(const auto& part:frames){

        if(part.entry.mesh_id.empty()&&!avatar)continue;
        auto mesh=MeshCache::Get().FindShared(part.entry.mesh_id);bool primitive=false;
        if(avatar&&(!mesh||mesh->vertices.empty())&&part.entry.kind==MeshParser::Kind::Body){
            auto box=std::make_shared<CachedMesh>();
            for(int i=0;i<8;++i){MeshVertex v{};v.pos[0]=(i==1||i==2||i==5||i==6)?1.f:-1.f;v.pos[1]=(i==2||i==3||i==6||i==7)?1.f:-1.f;v.pos[2]=i>=4?1.f:-1.f;box->vertices.push_back(v);}
            const std::uint32_t tris[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
            for(int i=0;i<36;i+=3)box->faces.push_back({{tris[i],tris[i+1],tris[i+2]}});
            mesh=std::move(box);primitive=true;if(!part.entry.mesh_id.empty())++missing;
        }
        if(!mesh||mesh->vertices.empty()||mesh->faces.empty()){++missing;continue;}
        V3 lo{FLT_MAX,FLT_MAX,FLT_MAX},hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(const auto& v:mesh->vertices){lo.x=(std::min)(lo.x,v.pos[0]);lo.y=(std::min)(lo.y,v.pos[1]);lo.z=(std::min)(lo.z,v.pos[2]);hi.x=(std::max)(hi.x,v.pos[0]);hi.y=(std::max)(hi.y,v.pos[1]);hi.z=(std::max)(hi.z,v.pos[2]);}
        V3 scale{part.size.x/(std::max)(.001f,hi.x-lo.x),part.size.y/(std::max)(.001f,hi.y-lo.y),part.size.z/(std::max)(.001f,hi.z-lo.z)};
        V3 offset{-(lo.x+hi.x)*.5f*scale.x,-(lo.y+hi.y)*.5f*scale.y,-(lo.z+hi.z)*.5f*scale.z};
        if(part.entry.special_mesh&&!primitive){scale=memory->read<V3>(part.entry.special_mesh+Offsets::SpecialMesh::Scale);offset=memory->read<V3>(part.entry.special_mesh+Offsets::SpecialMesh::Offset);}
        if(!Finite(scale)||!Finite(offset)){++missing;continue;}

        const auto packed=memory->read<std::uint32_t>(part.entry.part+Offsets::BasePart::Color3);
        Mesh::Color3 tint{float(packed&255)/255.f,float((packed>>8)&255)/255.f,float((packed>>16)&255)/255.f};
        std::vector<Vertex> vertices;vertices.reserve(mesh->vertices.size());
        for(const auto& v:mesh->vertices){
            V3 local{v.pos[0]*scale.x+offset.x,v.pos[1]*scale.y+offset.y,v.pos[2]*scale.z+offset.z};
            auto world=Rotate(part.rotation,local);world={world.x+part.pos.x-origin.x,world.y+part.pos.y-origin.y,world.z+part.pos.z-origin.z};
            auto p=Inverse(basis,world);
            vertices.push_back({{p.x,p.y,p.z},{local.x,local.y,local.z},{tint.r,tint.g,tint.b,1},Seed(part.entry.part)});
        }
        for(const auto& face:mesh->faces){
            bool valid=true;for(auto index:face.indices)if(index>=vertices.size())valid=false;
            if(!valid)continue;
            for(auto index:face.indices){auto& v=vertices[index];if(!Finite({v.p[0],v.p[1],v.p[2]}))valid=false;}
            if(!valid)continue;
            if(result.vertices.size()+3>900000){result.vertices.clear();result.status="Model exceeds preview budget";return result;}
            for(auto index:face.indices)result.vertices.push_back(vertices[index]);
        }
    }
    if(!result.vertices.empty()){if(avatar)FitAvatar(result);else Fit(result);}
    result.status=missing?"Some model parts are still loading":"";
    if(result.vertices.empty())result.status="Waiting for cached weapon meshes";
    return result;
}
inline Snapshot CaptureAvatar(std::uintptr_t previous=0){
    if(!memory->IsConnected())return {};
    const auto character=Globals::localPlayer.GetModelRef();if(!character.Addr||character.Addr==previous)return {};
    const auto parts=Cheat::Visuals::MeshParser::CollectDrawable(character.Addr);
    return CaptureRoot(character,"Local character",&parts,true);
}
inline Snapshot Capture(std::uintptr_t =0){return Skins::CapturePreview("AR-15","Default");}
inline Snapshot Bundled(const std::string& weapon){
    Snapshot s;s.name=weapon+" / Default";
    const auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(weapon=="AR-15"?205:206),MAKEINTRESOURCEW(10));
    if(!resource)return s;const auto size=SizeofResource(module,resource);
    auto bytes=LockResource(LoadResource(module,resource));
    PackedPreview::Decode(bytes,size,s.vertices);return s;
}
constexpr const char* vertexSource=R"HLSL(
cbuffer PreviewView : register(b12) {float yaw;float pitch;float zoom;float orbit;};
struct Input {float3 p:POSITION;float3 local:TEXCOORD0;float4 tint:COLOR0;float seed:TEXCOORD1;};
struct Output {float4 p:SV_POSITION;float3 local:TEXCOORD0;float4 tint:COLOR0;float seed:TEXCOORD1;};
Output main(Input i){Output o;o.p=float4(i.p,1);
 if(orbit>.5){float3 p=i.p;
  p=float3(p.x*cos(yaw)+p.z*sin(yaw),p.y,-p.x*sin(yaw)+p.z*cos(yaw));
  p=float3(p.x,p.y*cos(pitch)-p.z*sin(pitch),p.y*sin(pitch)+p.z*cos(pitch));
  float w=3.2+p.z;o.p=float4(p.x*2.4*zoom,p.y*2.4*zoom,w*(10.0/9.9)-1.0/9.9,w);
 }
 o.local=i.local;o.tint=i.tint;o.seed=i.seed;return o;}
)HLSL";
struct Programs {ComPtr<ID3DBlob> vs,ps;std::string error;};
inline Programs Compile(){
    Programs p;ComPtr<ID3DBlob> errors;
    auto source=Cheat::Visuals::NativeChams::PreviewShaderSource();
    HRESULT a=D3DCompile(vertexSource,strlen(vertexSource),"preview",nullptr,nullptr,"main","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&p.vs,&errors);
    HRESULT b=D3DCompile(source.data(),source.size(),"preview",nullptr,nullptr,"preview_ps","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&p.ps,&errors);
    if(FAILED(a)||FAILED(b))p.error=errors?std::string((char*)errors->GetBufferPointer(),errors->GetBufferSize()):"Preview shader compilation failed";
    return p;
}
class Viewer {
public:
    std::future<Snapshot> pending;
    std::future<Programs> compiling;
    Snapshot snapshot;
    Programs programs;
    std::chrono::steady_clock::time_point next{};
    ComPtr<ID3D11DeviceContext> deferred;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vb,constants,viewConstants;
    ComPtr<ID3D11Texture2D> texture,depth;
    ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11DepthStencilView> dsv;ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> depthState;
    bool started=false,dirty=false,failed=false;float phase=0,thumbnailCrop=.5f;std::string error,requestedKey,pendingKey;
    float yaw=.35f,pitch=0,zoom=1;
    void Orbit(float dx,float dy){yaw=std::remainder(yaw+dx*.012f,6.2831853f);pitch=std::clamp(pitch+dy*.012f,-1.5f,1.5f);}
    void Zoom(float wheel){zoom=std::clamp(zoom*std::pow(1.12f,wheel),.5f,2.f);}
    void PollAvatar(){
        if(!started){started=true;compiling=std::async(std::launch::async,Compile);}
        if(compiling.valid()&&compiling.wait_for(std::chrono::seconds(0))==std::future_status::ready){programs=compiling.get();if(!programs.error.empty()){error=programs.error;failed=true;}}
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto fresh=pending.get();if(!fresh.vertices.empty()){snapshot=std::move(fresh);dirty=true;}
        }
        auto now=std::chrono::steady_clock::now();
        if(!pending.valid()&&now>=next){
            next=now+std::chrono::seconds(5);const auto previous=snapshot.status.empty()?snapshot.root:0;
            pending=std::async(std::launch::async,[previous]{return CaptureAvatar(previous);});
        }
    }
    void Poll(const std::string& weapon="AR-15",const std::string& skin="Default",bool completeOnly=false){
        auto now=std::chrono::steady_clock::now();
        const auto key=weapon+" / "+skin;
        if(requestedKey!=key){
            requestedKey=key;snapshot=(skin=="Default"||completeOnly)?Bundled(weapon):Snapshot{};
            if(snapshot.vertices.empty()){snapshot.name=key;snapshot.status="Loading selected model...";}
            else if(skin!="Default")snapshot.status="Selected skin is loading / showing Default";
            dirty=true;next={};
        }
        if(!started){started=true;compiling=std::async(std::launch::async,Compile);}
        if(compiling.valid()&&compiling.wait_for(std::chrono::seconds(0))==std::future_status::ready){programs=compiling.get();if(!programs.error.empty()){error=programs.error;failed=true;}}
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto fresh=pending.get();

            if(pendingKey==requestedKey && (!completeOnly||(!fresh.vertices.empty()&&fresh.status.empty())) && (snapshot.vertices.empty()||(!fresh.vertices.empty()&&(fresh.status.empty()||fresh.vertices.size()>=snapshot.vertices.size())))){
                snapshot=std::move(fresh);dirty=true;
            }
        }
        if(!pending.valid()&&now>=next&&(snapshot.vertices.empty()||!snapshot.status.empty())){
            next=now+std::chrono::seconds(2);pendingKey=key;
            pending=std::async(std::launch::async,[weapon,skin]{return Skins::CapturePreview(weapon,skin);});
        }
    }

    bool AdoptComplete(const Viewer& cached){
        if(cached.snapshot.name!=requestedKey||cached.snapshot.vertices.empty()||!cached.snapshot.status.empty())return false;
        if(snapshot.name==requestedKey&&!snapshot.vertices.empty()&&snapshot.status.empty())return false;
        snapshot=cached.snapshot;dirty=true;next=std::chrono::steady_clock::time_point::max();return true;
    }
    bool Init(ID3D11Device* device){
        if(srv)return true;if(failed||!programs.vs||!programs.ps)return false;
        auto ok=[&](HRESULT hr){if(FAILED(hr)){failed=true;error="Preview graphics initialization failed";return false;}return true;};
        if(!ok(device->CreateDeferredContext(0,&deferred))||!ok(device->CreateVertexShader(programs.vs->GetBufferPointer(),programs.vs->GetBufferSize(),nullptr,&vs))||!ok(device->CreatePixelShader(programs.ps->GetBufferPointer(),programs.ps->GetBufferSize(),nullptr,&ps)))return false;
        D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(!ok(device->CreateInputLayout(elements,4,programs.vs->GetBufferPointer(),programs.vs->GetBufferSize(),&layout)))return false;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=512;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(!ok(device->CreateTexture2D(&desc,nullptr,&texture))||!ok(device->CreateRenderTargetView(texture.Get(),nullptr,&rtv)))return false;
        desc.Format=DXGI_FORMAT_D32_FLOAT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(!ok(device->CreateTexture2D(&desc,nullptr,&depth))||!ok(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)))return false;
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(NativePattern::Material);bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(!ok(device->CreateBuffer(&bd,nullptr,&constants)))return false;
        bd.ByteWidth=16;if(!ok(device->CreateBuffer(&bd,nullptr,&viewConstants)))return false;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=true;
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=true;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        if(!ok(device->CreateRasterizerState(&rd,&raster))||!ok(device->CreateDepthStencilState(&dd,&depthState)))return false;
        return ok(device->CreateShaderResourceView(texture.Get(),nullptr,&srv));
    }
    void Render(ID3D11Device* device,ID3D11DeviceContext* context,float dt,bool skinOnly=false,bool avatar=false,bool occluded=false){
        if(!Init(device))return;
        if(dirty){
            vb.Reset();dirty=false;
            float span=.01f;for(const auto& vertex:snapshot.vertices)span=(std::max)(span,std::abs(vertex.p[1]));
            thumbnailCrop=std::clamp(span*.56f,.18f,.5f);
            if(!snapshot.vertices.empty()){
                D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(snapshot.vertices.size()*sizeof(Vertex));bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA data{snapshot.vertices.data(),0,0};
                if(FAILED(device->CreateBuffer(&bd,&data,&vb))){error="Cannot upload preview model";return;}
            }
        }
        using namespace variables::Weapons;
        const float selectedSpeed=avatar?variables::ESP::nativeChamsAnimationSpeed:speed;
        phase+=std::clamp(dt,0.f,.1f)*std::clamp(selectedSpeed,0.f,15.f);
        NativePattern::Material mat{};
        const auto fill=avatar?(occluded?variables::ESP::nativeChamsOccludedColor:variables::ESP::nativeChamsColor):color;
        const auto emissive=avatar?variables::ESP::nativeChamsGlowColor:glowColor;
        for(int k=0;k<3;++k){mat.color[k]=fill[k];mat.glow[k]=emissive[k];}
        mat.color[3]=(avatar?variables::ESP::nativeChamsOpacity:opacity)*fill[3];
        mat.glow[3]=avatar?(variables::ESP::nativeChamsGlow?variables::ESP::nativeChamsGlowStrength:0):(glow?glowStrength:0);
        mat.mode=avatar?(occluded?variables::ESP::nativeChamsOccludedStyle:variables::ESP::nativeChamsStyle):nativeStyle;mat.time=phase;mat.depth_scale=1;
        mat.pattern[0]=1.2f/std::clamp(variables::ESP::nativeChamsPatternSize,.25f,4.f)/(avatar?1.f:std::clamp(scale,.01f,100.f)*.1f);
        mat.pattern[3]=avatar?(!variables::ESP::nativeChamsOnly?1.f:0.f):(showOriginal?1.f:0.f);
        if(skinOnly){mat.color[3]=0;mat.pattern[3]=1;}
        auto dc=deferred.Get();dc->ClearState();auto target=rtv.Get();dc->OMSetRenderTargets(1,&target,dsv.Get());
        float view[]={yaw,pitch,zoom,avatar?1.f:0.f};dc->UpdateSubresource(viewConstants.Get(),0,nullptr,view,0,0);auto viewBuffer=viewConstants.Get();dc->VSSetConstantBuffers(12,1,&viewBuffer);
        float clear[]={.035f,.04f,.055f,1};dc->ClearRenderTargetView(target,clear);dc->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
        D3D11_VIEWPORT viewport{0,0,512,512,0,1};dc->RSSetViewports(1,&viewport);dc->RSSetState(raster.Get());dc->OMSetDepthStencilState(depthState.Get(),0);
        if(vb&&(!hide||skinOnly||avatar)){
            dc->UpdateSubresource(constants.Get(),0,nullptr,&mat,0,0);auto cb=constants.Get();dc->PSSetConstantBuffers(13,1,&cb);
            dc->VSSetShader(vs.Get(),nullptr,0);dc->PSSetShader(ps.Get(),nullptr,0);dc->IASetInputLayout(layout.Get());dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto buffer=vb.Get();UINT stride=sizeof(Vertex),offset=0;dc->IASetVertexBuffers(0,1,&buffer,&stride,&offset);dc->Draw(UINT(snapshot.vertices.size()),0);
        }
        ComPtr<ID3D11CommandList> commands;
        if(SUCCEEDED(dc->FinishCommandList(FALSE,&commands)))context->ExecuteCommandList(commands.Get(),TRUE);
    }
};
inline std::unique_ptr<Viewer> viewer;
inline std::unique_ptr<Viewer> skinViewers[2];
inline std::unique_ptr<Viewer> avatarViewer;
inline void Shutdown(){viewer.reset();avatarViewer.reset();for(auto& v:skinViewers)v.reset();}
inline void DrawSkins(ID3D11Device* device,ID3D11DeviceContext* context,const std::string& selection){
    const auto base=ImGui::GetWindowPos();auto dl=ImGui::GetWindowDrawList();
    for(int i=0;i<2;++i){
        if(!skinViewers[i])skinViewers[i]=std::make_unique<Viewer>();auto& v=*skinViewers[i];
        v.Poll(i?"Glock 19":"AR-15",selection.empty()?"Default":selection);v.Render(device,context,0,true);
        auto min=base+ImVec2(315,64+i*190.f),max=min+ImVec2(386,180);
        dl->AddRect(min,max,imGuiCustom::ColorU32(imGuiCustom::GetTheme().Accent));

        const float crop=v.thumbnailCrop;
        const float width=(std::min)(362.f,132.f/(crop*2)),height=width*crop*2;
        if(v.srv)dl->AddImage((ImTextureID)v.srv.Get(),min+ImVec2((386-width)*.5f,30),min+ImVec2((386+width)*.5f,30+height),ImVec2(0,.5f-crop),ImVec2(1,.5f+crop));
        dl->AddText(min+ImVec2(12,8),imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),v.snapshot.name.c_str());
        if(!v.snapshot.status.empty())dl->AddText(min+ImVec2(12,157),imGuiCustom::ColorU32(imGuiCustom::GetTheme().Text),v.snapshot.status.c_str());
    }
}

inline ImVec2 ImageFit(ImVec2 available,float crop){
    crop=std::clamp(crop,.01f,.5f);
    const float width=(std::max)(1.f,(std::min)(available.x,available.y/(crop*2)));
    return ImVec2(width,width*crop*2);
}
inline void Draw(ID3D11Device* device,ID3D11DeviceContext* context,ImVec2 mainPos,ImVec2 mainSize){
    if(!viewer)viewer=std::make_unique<Viewer>();
    static int weapon=0;
    const auto display=ImGui::GetIO().DisplaySize;
    const ImVec2 preferred=weapon?ImVec2(420,430):ImVec2(600,330);
    ImVec2 position{(std::max)(0.f,(std::min)(mainPos.x+mainSize.x+12,display.x-preferred.x)),(std::max)(0.f,(std::min)(mainPos.y+42,display.y-preferred.y))};
    ImGui::SetNextWindowPos(position,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(preferred,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(360,260),ImVec2((std::max)(360.f,display.x),(std::max)(260.f,display.y)));
    const auto& theme=imGuiCustom::GetTheme();
    ImGui::PushStyleColor(ImGuiCol_WindowBg,theme.WindowBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,theme.WindowBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,theme.CardBg);
    ImGui::PushStyleColor(ImGuiCol_Border,theme.Accent);
    ImGui::PushStyleColor(ImGuiCol_Text,theme.Text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(9,9));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1);
    if(ImGui::Begin("Weapon preview",&variables::Weapons::preview,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)){
        const int previous=weapon;
        ImGui::RadioButton("AR-15",&weapon,0);ImGui::SameLine();ImGui::RadioButton("Glock",&weapon,1);
        if(weapon!=previous)ImGui::SetWindowSize(weapon?ImVec2(420,430):ImVec2(600,330));
        auto selection=Skins::PreviewSelection(weapon?"Glock 19":"AR-15");if(selection.empty())selection="Default";

        if(!viewer->started&&skinViewers[weapon]&&skinViewers[weapon]->programs.vs&&skinViewers[weapon]->programs.ps){
            viewer->programs=skinViewers[weapon]->programs;viewer->started=true;
        }
        viewer->Poll(weapon?"Glock 19":"AR-15",selection,true);
        if(skinViewers[weapon])viewer->AdoptComplete(*skinViewers[weapon]);
        viewer->Render(device,context,ImGui::GetIO().DeltaTime);
        ImGui::TextUnformatted(viewer->snapshot.name.c_str());
        const auto available=ImGui::GetContentRegionAvail();
        const bool status=!viewer->error.empty()||!viewer->srv||!viewer->snapshot.status.empty()||variables::Weapons::hide;
        const ImVec2 box{available.x,(std::max)(80.f,available.y-(status?24.f:0.f))};
        const auto size=ImageFit(box,viewer->thumbnailCrop);
        const auto origin=ImGui::GetCursorScreenPos();
        const auto min=origin+ImVec2((box.x-size.x)*.5f,(box.y-size.y)*.5f);
        if(viewer->srv)ImGui::GetWindowDrawList()->AddImage((ImTextureID)viewer->srv.Get(),min,min+size,ImVec2(0,.5f-viewer->thumbnailCrop),ImVec2(1,.5f+viewer->thumbnailCrop));
        ImGui::Dummy(box);
        if(!viewer->error.empty())ImGui::TextUnformatted("Preview unavailable (graphics error)");
        else if(!viewer->srv)ImGui::TextUnformatted("Preparing preview shader...");
        else if(!viewer->snapshot.status.empty())ImGui::TextWrapped("%s",viewer->snapshot.status.c_str());
        else if(variables::Weapons::hide)ImGui::TextUnformatted("Weapon hidden");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);ImGui::PopStyleColor(5);
}
inline void DrawAvatar(ID3D11Device* device,ID3D11DeviceContext* context,ImVec2 mainPos,ImVec2 mainSize){
    if(!avatarViewer){avatarViewer=std::make_unique<Viewer>();avatarViewer->snapshot=Avatar();avatarViewer->dirty=true;}
    auto& v=*avatarViewer;
    v.PollAvatar();
    auto display=ImGui::GetIO().DisplaySize;ImVec2 pos{std::clamp(mainPos.x+mainSize.x+12,0.f,(std::max)(0.f,display.x-340)),std::clamp(mainPos.y+42,0.f,(std::max)(0.f,display.y-390))};
    ImGui::SetNextWindowPos(pos,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340,390),ImGuiCond_FirstUseEver);ImGui::SetNextWindowSizeConstraints(ImVec2(250,290),ImVec2(900,1000));
    const auto& theme=imGuiCustom::GetTheme();
    ImGui::PushStyleColor(ImGuiCol_WindowBg,theme.WindowBg);ImGui::PushStyleColor(ImGuiCol_TitleBg,theme.WindowBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,theme.CardBg);ImGui::PushStyleColor(ImGuiCol_Border,theme.Accent);ImGui::PushStyleColor(ImGuiCol_Text,theme.Text);
    if(ImGui::Begin("Native preview",&variables::ESP::nativePreview,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)){
        static bool occluded=false;ImGui::Checkbox("Occluded material",&occluded);
        ImGui::SameLine(0,12);if(ImGui::SmallButton("Refresh")){v.next={};v.snapshot.root=0;}
        auto space=ImGui::GetContentRegionAvail();const float edge=(std::max)(1.f,(std::min)(space.x,space.y));
        const auto min=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##avatar_orbit",ImVec2(edge,edge));
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left))v.Orbit(ImGui::GetIO().MouseDelta.x,ImGui::GetIO().MouseDelta.y);
        if(ImGui::IsItemHovered()){
            if(ImGui::GetIO().MouseWheel!=0)v.Zoom(ImGui::GetIO().MouseWheel);
            ImGui::SetTooltip("Drag to rotate; scroll to zoom");
        }
        v.Render(device,context,ImGui::GetIO().DeltaTime,false,true,occluded);
        if(v.srv)ImGui::GetWindowDrawList()->AddImage((ImTextureID)v.srv.Get(),min,min+ImVec2(edge,edge));
        else ImGui::GetWindowDrawList()->AddText(min,imGuiCustom::ColorU32(theme.Text),v.error.empty()?"Preparing preview...":"Preview graphics error");
    }ImGui::End();ImGui::PopStyleColor(5);
}

}
