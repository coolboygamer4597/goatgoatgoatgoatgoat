#pragma once
#include "PreviewCache.h"
#include "../core/app/CustomShading.h"
#include "../core/app/CustomModelPresentation.h"
#include "../core/app/CustomTextureData.h"
#include "../core/app/TextureProjectionData.h"
#include "../core/app/TextureProjectionShader.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <future>
#include <array>
#include <cmath>
#include <chrono>
namespace CustomTextureViewer {
using Microsoft::WRL::ComPtr;
inline const std::string ShaderStorage=std::string(GOAT_CUSTOM_MODEL_PRESENTATION_HLSL)+std::string(R"(
cbuffer View:register(b0){float yaw;float pitch;float zoom;float intensity;float4 projectionRows[3];float4 projectionMinimum;float4 projectionInverseSpan;float4 projectionOptions;float4 modelControls;};
struct In{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;uint group:TEXCOORD1;};
struct Out{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float3 world:TEXCOORD1;nointerpolation uint group:TEXCOORD2;float3 nativePoint:TEXCOORD3;float3 nativeNormal:TEXCOORD4;};
)")+TextureProjectionShader::Coordinates+R"(
float3 PreviewNative(float3 p){return float3(dot(projectionRows[0].xyz,p)+projectionRows[0].w,dot(projectionRows[1].xyz,p)+projectionRows[1].w,dot(projectionRows[2].xyz,p)+projectionRows[2].w);}
float3 PreviewNormal(float3 p){return normalize(float3(dot(projectionRows[0].xyz,p),dot(projectionRows[1].xyz,p),dot(projectionRows[2].xyz,p)));}
float3 Rotate(float3 p){p=float3(p.x*cos(yaw)+p.z*sin(yaw),p.y,-p.x*sin(yaw)+p.z*cos(yaw));return float3(p.x,p.y*cos(pitch)-p.z*sin(pitch),p.y*sin(pitch)+p.z*cos(pitch));}
Out vs(In i){Out o;float3 p=Rotate(i.p);o.p=float4(p.x*zoom,p.y*zoom*1.7066667,.5+p.z*.15,1);o.n=Rotate(i.n);o.uv=i.uv;o.world=p;o.group=i.group;o.nativePoint=PreviewNative(i.p);o.nativeNormal=PreviewNormal(i.n);return o;}
Texture2D c0:register(t0);Texture2D c1:register(t1);Texture2D c2:register(t2);Texture2D c3:register(t3);
Texture2D m0:register(t4);Texture2D m1:register(t5);Texture2D m2:register(t6);Texture2D m3:register(t7);SamplerState filtering:register(s0);
float3 Encode(float3 x){return lerp(x*12.92,1.055*pow(max(x,0),1/2.4)-.055,step(.0031308,x));}
float4 PreviewProjected(Texture2D<float4> image,float3 surfacePoint,float3 normal){float3 weights=projected_weights(normal);return image.Sample(filtering,projected_axis_uv(surfacePoint,normal,projectionMinimum.xyz,projectionInverseSpan.xyz,0))*weights.x+image.Sample(filtering,projected_axis_uv(surfacePoint,normal,projectionMinimum.xyz,projectionInverseSpan.xyz,1))*weights.y+image.Sample(filtering,projected_axis_uv(surfacePoint,normal,projectionMinimum.xyz,projectionInverseSpan.xyz,2))*weights.z;}
float3 MetalSurface(float3 color,float3 n,float3 eye,float3 light,float rough){
 float3 ray=reflect(-eye,n);float environment=.12+.4*saturate(ray.y*.5+.5);float strip=pow(saturate(1-abs(ray.y-.12)/.55),4);float highlight=pow(saturate(dot(n,normalize(light+eye))),lerp(96,8,rough));
 return color*environment+float3(1,.83,.38)*strip*.7+float3(1,.91,.65)*highlight*.65;
}
float4 ps(Out i):SV_TARGET{
 float3 color;float coverage=1,ao=1;float2 mr;float2 uv=projectionOptions.x>.5?projected_uv(i.nativePoint,i.nativeNormal,projectionMinimum.xyz,projectionInverseSpan.xyz):i.uv;
 if(i.group==0){float4 paint=c0.Sample(filtering,uv);color=paint.rgb;coverage=paint.a;float4 material=m0.Sample(filtering,uv);mr=material.rg;ao=material.b;}
 else if(i.group==1){float4 paint=c1.Sample(filtering,uv);color=paint.rgb;coverage=paint.a;float4 material=m1.Sample(filtering,uv);mr=material.rg;ao=material.b;}
 else if(i.group==2){float4 paint=c2.Sample(filtering,uv);color=paint.rgb;coverage=paint.a;float4 material=m2.Sample(filtering,uv);mr=material.rg;ao=material.b;}
 else{float4 paint=c3.Sample(filtering,uv);color=paint.rgb;coverage=paint.a;float4 material=m3.Sample(filtering,uv);mr=material.rg;ao=material.b;}
 if(projectionOptions.x>.5){color=PreviewProjected(c0,i.nativePoint,i.nativeNormal).rgb;mr=PreviewProjected(m0,i.nativePoint,i.nativeNormal).rg;}
 if(projectionOptions.z>.5)clip(coverage-.5);
 if(projectionOptions.z>.5)color*=modelControls.y;
 if(projectionOptions.y<.5)return float4(saturate(Encode(color*(1+.5*modelControls.x))),1);
 mr.x=saturate(mr.x*intensity);mr.y=intensity<=1?1-(1-mr.y)*intensity:mr.y/sqrt(intensity);
 float3 n=normalize(i.n),v=normalize(float3(0,0,-4)-i.world);if(dot(n,v)<0)n=-n;
 float3 light=normalize(float3(-.35,.8,-.65)),h=normalize(light+v);float rough=clamp(mr.y,.16,1);
 float3 result=color*(.65+.35*saturate(dot(n,light)))+lerp(.04,color,saturate(mr.x))*pow(saturate(dot(n,h)),lerp(128,8,rough))*(.15+.2*(1-rough));
 if(projectionOptions.z>.5){result=intensity<=0?color:color*(.78+.22*saturate(dot(n,light)))+lerp(.02,color,saturate(mr.x))*pow(saturate(dot(n,h)),lerp(128,8,rough))*.025*min(intensity,4)*step(.001,modelControls.x);result+=color*.5*modelControls.x;}
 if(modelControls.z>.5&&intensity>0&&mr.x>.98&&rough<.3)result=MetalSurface(projectionOptions.z>.5?color/max(modelControls.y,.001):color,n,v,light,rough)+color*.5*modelControls.x;
 if(projectionOptions.z>.5)result*=pow(max(ao,.001),max(projectionOptions.w,0));
 return float4(saturate(projectionOptions.z>.5&&modelControls.z<.5?ModelPaintDisplay(result,modelControls.y,modelControls.x):Encode(result)),1);
}
)";
inline const char* Shader=ShaderStorage.c_str();
struct Bundle{CustomModelData::Mesh mesh;std::array<PreviewCache::Image,4> colors,materials;int groups=0;bool valid=false;};
class Viewer {
    ComPtr<ID3D11DeviceContext> deferred;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices,constants;
    ComPtr<ID3D11Texture2D> target,depth;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> image;std::array<ComPtr<ID3D11ShaderResourceView>,4> colors,materials;
    ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depthState;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11BlendState> blend;
    std::future<Bundle> pending;int pendingIndex=-1,requested=-1,loaded=-1;UINT count=0;
    std::chrono::steady_clock::time_point retry{},shadowChanged{};float shadow=0,pendingShadow=0,loadedShadow=-1;bool dirty=true,shading=true;float modelGlow=0;
    bool Texture(ID3D11Device* device,const PreviewCache::Image& decoded,bool srgb,ComPtr<ID3D11ShaderResourceView>& view){
        if(!decoded.valid())return false;D3D11_TEXTURE2D_DESC d{};d.Width=decoded.width;d.Height=decoded.height;d.MipLevels=1;d.ArraySize=1;d.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{decoded.rgba.data(),decoded.width*4,0};ComPtr<ID3D11Texture2D> texture;return SUCCEEDED(device->CreateTexture2D(&d,&pixels,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
    }
    bool Init(ID3D11Device* device){
        if(deferred&&vs&&ps&&layout&&constants&&target&&image&&sampler)return true;
        ComPtr<ID3DBlob> v,p,error;if(FAILED(D3DCompile(Shader,std::strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&error))||FAILED(D3DCompile(Shader,std::strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&error)))return false;
        if(FAILED(device->CreateDeferredContext(0,&deferred))||FAILED(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs))||FAILED(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps)))return false;
        D3D11_INPUT_ELEMENT_DESC e[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_UINT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device->CreateInputLayout(e,4,v->GetBufferPointer(),v->GetBufferSize(),&layout)))return false;
        D3D11_BUFFER_DESC b{};b.ByteWidth=128;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(device->CreateBuffer(&b,nullptr,&constants)))return false;
        D3D11_TEXTURE2D_DESC d{};d.Width=1024;d.Height=600;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&target))||FAILED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv))||FAILED(device->CreateShaderResourceView(target.Get(),nullptr,&image)))return false;
        d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;if(FAILED(device->CreateTexture2D(&d,nullptr,&depth))||FAILED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)))return false;
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;r.MultisampleEnable=TRUE;if(FAILED(device->CreateRasterizerState(&r,&raster)))return false;
        D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;ds.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;if(FAILED(device->CreateDepthStencilState(&ds,&depthState)))return false;
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_ANISOTROPIC;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxAnisotropy=16;sd.MaxLOD=D3D11_FLOAT32_MAX;if(FAILED(device->CreateSamplerState(&sd,&sampler)))return false;
        D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;return SUCCEEDED(device->CreateBlendState(&bd,&blend));
    }
public:
    float yaw=.3f,pitch=-.1f,zoom=1,intensity=1;
    float cropHalf=.5f;
    bool Missing=false,Failed=false;
    ~Viewer(){if(pending.valid())try{pending.get();}catch(...){}}
    void Suspend(){if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready)try{pending.get();}catch(...){}pendingIndex=-1;vertices.Reset();colors={};materials={};count=0;requested=loaded=-1;image.Reset();target.Reset();rtv.Reset();depth.Reset();dsv.Reset();deferred.Reset();dirty=true;}
    void Orbit(float x,float y){yaw=std::remainder(yaw+x*.01f,6.2831853f);pitch=std::clamp(pitch+y*.01f,-1.5f,1.5f);dirty=true;}
    void Zoom(float wheel){zoom=std::clamp(zoom*std::pow(1.12f,wheel),.4f,3.f);dirty=true;}
    void Intensity(float value){value=std::isfinite(value)?std::clamp(value,0.f,10.f):1.f;if(value!=intensity){intensity=value;dirty=true;}}
    void Shadow(float value){value=CustomShading::ShadowLevel(value);if(value!=shadow){shadow=value;shadowChanged=std::chrono::steady_clock::now();}}
    void Glow(float value){value=std::isfinite(value)?std::clamp(value,0.f,5.f):0.f;if(modelGlow!=value){modelGlow=value;dirty=true;}}
    void Shading(bool value){if(value!=shading){shading=value;dirty=true;}}
    void Reset(){yaw=.3f;pitch=-.1f;zoom=1;dirty=true;}
    ID3D11ShaderResourceView* Image()const{return count?image.Get():nullptr;}
    void Poll(ID3D11Device* device,int selected){
        if(requested!=selected){requested=selected;loaded=-1;vertices.Reset();colors={};materials={};count=0;Missing=Failed=false;dirty=true;retry={};}
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            Bundle b;try{b=pending.get();}catch(...){}if(pendingIndex==requested&&pendingShadow==shadow){
                Missing=!b.valid;
                if(b.valid&&Init(device)){
                    float extent=0;for(const auto& vertex:b.mesh.vertices){const float z=-vertex.position[0]*std::sin(.3f)+vertex.position[2]*std::cos(.3f);const float y=vertex.position[1]*std::cos(-.1f)-z*std::sin(-.1f);extent=(std::max)(extent,std::abs(y));}cropHalf=std::clamp(extent*1.7066667f*.5f+.045f,.15f,.5f);
                    D3D11_BUFFER_DESC d{};d.ByteWidth=static_cast<UINT>(b.mesh.vertices.size()*sizeof(CustomModelData::Vertex));d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.Usage=D3D11_USAGE_IMMUTABLE;D3D11_SUBRESOURCE_DATA data{b.mesh.vertices.data(),0,0};
                    bool ok=SUCCEEDED(device->CreateBuffer(&d,&data,&vertices));for(int g=0;ok&&g<b.groups;++g)ok=Texture(device,b.colors[g],true,colors[g])&&Texture(device,b.materials[g],false,materials[g]);
                    if(ok){for(int g=b.groups;g<4;++g){colors[g]=colors[0];materials[g]=materials[0];}loaded=requested;loadedShadow=pendingShadow;count=static_cast<UINT>(b.mesh.vertices.size());dirty=true;}else Failed=true;
                }else if(b.valid)Failed=true;
                retry=std::chrono::steady_clock::now()+std::chrono::seconds(1);
            }
        }
        if(pending.valid()||(loaded==selected&&loadedShadow==shadow)||std::chrono::steady_clock::now()-shadowChanged<std::chrono::milliseconds(250)||std::chrono::steady_clock::now()<retry||!CustomTextureData::Valid(selected))return;
        pendingIndex=selected;const auto model=CustomTextureData::ModelFile(selected);const int groups=CustomTextureData::Presets[selected].groups;std::array<std::filesystem::path,4> c,m,a;
        for(int g=0;g<groups;++g){c[g]=CustomTextureData::File(selected,CustomTextureData::Channel::Color,g);m[g]=CustomTextureData::File(selected,CustomTextureData::Channel::Material,g);a[g]=CustomTextureData::ShadowFile(selected,g);}
        pendingShadow=shadow;const auto level=shadow;
        pending=std::async(std::launch::async,[model,groups,c,m,a,level]{Bundle b;b.groups=groups;try{b.valid=CustomModelData::Load(model,b.mesh);for(int g=0;b.valid&&g<groups;++g){b.colors[g]=PreviewCache::DecodeImage(c[g]);b.materials[g]=PreviewCache::DecodeImage(m[g]);if(level>0&&!a[g].filename().empty()){auto mask=PreviewCache::DecodeImage(a[g]);CustomShading::Shadow(b.colors[g].rgba,b.colors[g].width,b.colors[g].height,mask.rgba,mask.width,mask.height,level);}b.valid=b.colors[g].valid()&&b.materials[g].valid();}}catch(...){b.valid=false;}return b;});
    }
    bool Render(ID3D11DeviceContext* context){
        if(!count||!deferred||!vertices||!image)return false;if(!dirty)return true;
        auto dc=deferred.Get();dc->ClearState();auto output=rtv.Get();dc->OMSetRenderTargets(1,&output,dsv.Get());const float clear[]={.035f,.04f,.055f,1};dc->ClearRenderTargetView(output,clear);dc->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
        D3D11_VIEWPORT viewport{0,0,1024,600,0,1};dc->RSSetViewports(1,&viewport);dc->RSSetState(raster.Get());dc->OMSetDepthStencilState(depthState.Get(),0);dc->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
        std::array<float,32> view{};view[0]=yaw;view[1]=pitch;view[2]=zoom;view[3]=intensity;
        if(CustomTextureData::Valid(loaded)&&CustomTextureData::Presets[loaded].model==3&&!CustomTextureData::RequiresModel(loaded)){std::copy(TextureProjectionData::DeaglePreview.begin(),TextureProjectionData::DeaglePreview.end(),view.begin()+4);std::copy(TextureProjectionData::DeagleMinimum.begin(),TextureProjectionData::DeagleMinimum.end(),view.begin()+16);std::copy(TextureProjectionData::DeagleInverseSpan.begin(),TextureProjectionData::DeagleInverseSpan.end(),view.begin()+20);view[24]=1;}view[25]=shading?1.f:0.f;view[26]=CustomTextureData::RequiresModel(loaded)?1.f:0.f;view[27]=shadow;view[28]=CustomTextureData::RequiresModel(loaded)?modelGlow:0.f;view[29]=CustomTextureData::NativeExposure(loaded);view[30]=CustomTextureData::NativeMetal(loaded)?1.f:0.f;dc->UpdateSubresource(constants.Get(),0,nullptr,view.data(),0,0);auto cb=constants.Get();dc->VSSetConstantBuffers(0,1,&cb);dc->PSSetConstantBuffers(0,1,&cb);
        dc->VSSetShader(vs.Get(),nullptr,0);dc->PSSetShader(ps.Get(),nullptr,0);dc->IASetInputLayout(layout.Get());dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);auto vb=vertices.Get();const UINT stride=sizeof(CustomModelData::Vertex),offset=0;dc->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        ID3D11ShaderResourceView* maps[8]{};for(int g=0;g<4;++g){maps[g]=colors[g].Get();maps[g+4]=materials[g].Get();}dc->PSSetShaderResources(0,8,maps);auto ss=sampler.Get();dc->PSSetSamplers(0,1,&ss);dc->Draw(count,0);
        ComPtr<ID3D11CommandList> commands;if(FAILED(dc->FinishCommandList(FALSE,&commands)))return false;context->ExecuteCommandList(commands.Get(),TRUE);dirty=false;return true;
    }
};
}
