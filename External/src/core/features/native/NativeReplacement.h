#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <cstdint>
#include <cstddef>
#include "../../app/CustomModelPresentation.h"
namespace NativeReplacement {
struct Request {
    ID3D11Device* device=nullptr;
    ID3D11DeviceContext* context=nullptr;
    const void* vertexCode=nullptr;
    const void* pixelCode=nullptr;
    std::size_t vertexCodeBytes=0,pixelCodeBytes=0;
    D3D11_INPUT_ELEMENT_DESC elements[4]{};
    D3D11_BUFFER_DESC vertexDescription{};
    D3D11_SUBRESOURCE_DATA vertexData{};
    D3D11_SAMPLER_DESC sampling{};
    D3D11_RASTERIZER_DESC rasterDescription{};
    ID3D11VertexShader* vertexShader=nullptr;
    ID3D11PixelShader* pixelShader=nullptr;
    ID3D11InputLayout* layout=nullptr;
    ID3D11Buffer* vertices=nullptr;
    ID3D11SamplerState* sampler=nullptr;
    ID3D11RasterizerState* raster=nullptr;
    ID3D11ShaderResourceView* maps[8]{};
    unsigned vertexCount=0,ready=0,calls=0,attempted=0,lastFrame=0xffffffffu;
    HRESULT error=S_OK;
    struct Pass{std::uintptr_t color=0,depth=0;unsigned frame=0xffffffffu;};
    Pass passes[8]{};
    unsigned colorCalls=0,depthCalls=0;
    const void* encodedCode=nullptr;
    std::size_t encodedCodeBytes=0;
    ID3D11PixelShader* encodedShader=nullptr;
    unsigned outputFormat=0;
    ID3D11ShaderResourceView* pendingMaps[8]{};
    unsigned poseFrame=0xffffffffu;
    float framePose[136]{};
    ID3D11BlendState* opaqueBlend=nullptr;
    float passViewports[8][6]{};
    unsigned additiveSkips=0,invalidPoses=0,viewportPasses=0;
    ID3D11DepthStencilState* depthStandard=nullptr;
    ID3D11DepthStencilState* depthReversed=nullptr;
    unsigned depthOwned=0;
    D3D11_BLEND_DESC blending=[](){D3D11_BLEND_DESC value{};value.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;return value;}();
};
inline constexpr const char* Shader=GOAT_CUSTOM_MODEL_PRESENTATION_HLSL R"(
cbuffer NativeFrame:register(b13){float4 padding[14];float4 view[4];float4 body[3];float4 status;float4 unused[9];float4 paintOptions;float4 inverseSpan;float4 options;};
cbuffer GameFrame:register(b0){float4 gameView[4];};
struct In{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;uint material:TEXCOORD1;};
struct Out{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;nointerpolation uint material:TEXCOORD1;float3 world:TEXCOORD2;};
float3 Rotate(float3 p){return float3(body[0].x,body[0].w,body[1].z)*p.x+float3(body[0].y,body[1].x,body[1].w)*p.y+float3(body[0].z,body[1].y,body[2].x)*p.z;}
bool GameCamera(){
 float3 lengths=float3(length(gameView[0].xyz),length(gameView[1].xyz),length(gameView[3].xyz));float3 previous=float3(length(view[0].xyz),length(view[1].xyz),length(view[3].xyz));
 if(status.y<.5||any(lengths<.0001)||any(previous<.0001)||any(abs(lengths/previous-1)>.1))return false;
 float3 right=gameView[0].xyz/lengths.x,up=gameView[1].xyz/lengths.y,forward=gameView[3].xyz/lengths.z;
 if(abs(dot(right,up))>.01||abs(dot(right,forward))>.01||abs(dot(up,forward))>.01)return false;
 float3 current=-gameView[0].w/lengths.x*right-gameView[1].w/lengths.y*up-gameView[3].w/lengths.z*forward;
 float3 sampled=-view[0].w/previous.x*view[0].xyz/previous.x-view[1].w/previous.y*view[1].xyz/previous.y-view[3].w/previous.z*view[3].xyz/previous.z;
 return dot(current-sampled,current-sampled)<100;
}
Out vs(In i){
 Out o;bool live=GameCamera();float4 r=live?gameView[0]:view[0],u=live?gameView[1]:view[1],d=live?gameView[2]:view[2],f=live?gameView[3]:view[3];float4 world=float4(Rotate(i.p)+body[2].yzw,1);float3 normal=Rotate(i.n);
 o.p=float4(dot(r,world),dot(u,world),dot(d,world),dot(f,world));o.n=normal;o.uv=i.uv;o.material=i.material;o.world=world.xyz;return o;
}
Texture2D c0:register(t0);Texture2D c1:register(t1);Texture2D c2:register(t2);Texture2D c3:register(t3);
Texture2D m0:register(t4);Texture2D m1:register(t5);Texture2D m2:register(t6);Texture2D m3:register(t7);SamplerState filtering:register(s0);
float3 Linear(float3 c){return lerp(c/12.92,pow((c+.055)/1.055,2.4),step(.04045,c));}
float3 MetalSurface(float3 color,float3 n,float3 eye,float3 light,float rough){
 float3 ray=reflect(-eye,n);float environment=.12+.4*saturate(ray.y*.5+.5);float strip=pow(saturate(1-abs(ray.y-.12)/.55),4);float highlight=pow(saturate(dot(n,normalize(light+eye))),lerp(96,8,rough));
 return color*environment+float3(1,.83,.38)*strip*.7+float3(1,.91,.65)*highlight*.65;
}
float4 PaintOutput(float3 color){return float4(saturate(color),1);}
float4 PaintRaw(Out i){
 float4 albedo;float3 color,packed;if(i.material==0){albedo=c0.Sample(filtering,i.uv);packed=m0.Sample(filtering,i.uv).rgb;}else if(i.material==1){albedo=c1.Sample(filtering,i.uv);packed=m1.Sample(filtering,i.uv).rgb;}else if(i.material==2){albedo=c2.Sample(filtering,i.uv);packed=m2.Sample(filtering,i.uv).rgb;}else{albedo=c3.Sample(filtering,i.uv);packed=m3.Sample(filtering,i.uv).rgb;}clip(albedo.a-.5);color=Linear(albedo.rgb);
 float3 n=normalize(i.n),light=normalize(float3(-.35,.8,-.65));
 if(options.y<.5){float diffuse=1.35+.35*saturate(dot(n,light));return float4(saturate(color*diffuse),1);}
 color*=paintOptions.w>0?paintOptions.w:1;
 if(options.x<=0)return PaintOutput(color*(1+.5*max(options.w,0))*pow(max(packed.b,.001),max(options.z,0)));
 float ax=max(length(view[0].xyz),.0001),ay=max(length(view[1].xyz),.0001),az=max(length(view[3].xyz),.0001);float3 right=view[0].xyz/ax,up=view[1].xyz/ay,forward=view[3].xyz/az;float3 camera=-view[0].w/ax*right-view[1].w/ay*up-view[3].w/az*forward;float3 eye=normalize(camera-i.world);if(dot(n,eye)<0)n=-n;
 light=normalize(-.35*right+.8*up-.65*forward);float3 halfway=normalize(light+eye);float rough=clamp(packed.y,.16,1);
 float3 result=color*(.78+.22*saturate(dot(n,light)))+lerp(.02,color,saturate(packed.x))*pow(saturate(dot(n,halfway)),lerp(128,8,rough))*.025*min(options.x,4)*step(.001,options.w);result+=color*.5*max(options.w,0);
 if(paintOptions.x>.5&&packed.x>.98&&rough<.3)result=MetalSurface(Linear(albedo.rgb),n,eye,light,rough)+color*.5*max(options.w,0);
 result*=pow(max(packed.b,.001),max(options.z,0));return PaintOutput(result);
}
float4 ps(Out i):SV_TARGET{float4 color=PaintRaw(i);if(paintOptions.y>.5)color.rgb=ModelPaintLinear(color.rgb,paintOptions.w>0?paintOptions.w:1,options.w);return color;}
float4 psEncoded(Out i):SV_TARGET{float4 color=PaintRaw(i);color.rgb=paintOptions.y>.5?ModelPaintDisplay(color.rgb,paintOptions.w>0?paintOptions.w:1,options.w):ModelPaintEncode(color.rgb);return color;}
)";
__declspec(noinline) bool Initialize(Request* r){
    if(!r||r->ready||r->attempted||!r->device||!r->context||!r->vertexCode||!r->pixelCode||!r->vertexData.pSysMem||!r->vertexCount||r->vertexCount>300000||r->vertexCount%3||r->vertexDescription.ByteWidth!=r->vertexCount*36)return false;
    r->attempted=1;
    if(r->depthStandard&&r->depthReversed){r->depthStandard->AddRef();r->depthReversed->AddRef();r->depthOwned=1;}
    HRESULT hr=r->device->CreateVertexShader(r->vertexCode,r->vertexCodeBytes,nullptr,&r->vertexShader);
    if(SUCCEEDED(hr))hr=r->device->CreatePixelShader(r->pixelCode,r->pixelCodeBytes,nullptr,&r->pixelShader);
    if(SUCCEEDED(hr)&&r->encodedCode&&r->encodedCodeBytes)hr=r->device->CreatePixelShader(r->encodedCode,r->encodedCodeBytes,nullptr,&r->encodedShader);
    if(SUCCEEDED(hr))hr=r->device->CreateInputLayout(r->elements,4,r->vertexCode,r->vertexCodeBytes,&r->layout);
    if(SUCCEEDED(hr))hr=r->device->CreateBuffer(&r->vertexDescription,&r->vertexData,&r->vertices);
    if(SUCCEEDED(hr))hr=r->device->CreateSamplerState(&r->sampling,&r->sampler);
    if(SUCCEEDED(hr))hr=r->device->CreateRasterizerState(&r->rasterDescription,&r->raster);
    if(SUCCEEDED(hr))hr=r->device->CreateBlendState(&r->blending,&r->opaqueBlend);
    r->error=hr;r->ready=SUCCEEDED(hr);return r->ready!=0;
}
__declspec(noinline) void SetMaps(Request* r){
    if(!r)return;
    for(unsigned i=0;i<8;++i){auto next=r->pendingMaps[i];if(next)next->AddRef();auto previous=r->maps[i];r->maps[i]=next;r->pendingMaps[i]=nullptr;if(previous)previous->Release();r->passes[i].frame=0xffffffffu;}
}
__declspec(noinline) bool Draw(Request* r,ID3D11Buffer* frame,const float* pose,unsigned frameId){
    if(r&&r->poseFrame==frameId)pose=r->framePose;
    if(!r||!r->ready||!r->context||!frame||!pose||!r->maps[0])return false;
    if(pose[84]<.5f||pose[84]>1.5f){++r->invalidPoses;return false;}
    for(unsigned i=56;i<88;++i){union{float value;unsigned bits;}number;number.value=pose[i];if((number.bits&0x7f800000)==0x7f800000)return false;}
    if(pose[56]*pose[56]+pose[57]*pose[57]+pose[58]*pose[58]<1e-10f||pose[60]*pose[60]+pose[61]*pose[61]+pose[62]*pose[62]<1e-10f)return false;
    auto context=r->context;
    ID3D11RenderTargetView* colorTarget=nullptr;ID3D11DepthStencilView* depthTarget=nullptr;context->OMGetRenderTargets(1,&colorTarget,&depthTarget);const auto colorKey=reinterpret_cast<std::uintptr_t>(colorTarget),depthKey=reinterpret_cast<std::uintptr_t>(depthTarget);DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;if(colorTarget){D3D11_RENDER_TARGET_VIEW_DESC description;colorTarget->GetDesc(&description);format=description.Format;colorTarget->Release();}r->outputFormat=format;if(depthTarget)depthTarget->Release();if(!colorKey&&!depthKey)return false;
    D3D11_VIEWPORT viewport{};UINT viewportCount=1;context->RSGetViewports(&viewportCount,&viewport);if(!colorKey&&pose[48]>64&&pose[49]>64&&viewportCount&&viewport.Height>0){const float ratio=viewport.Width*pose[49]/(viewport.Height*pose[48]);if(ratio<.9f||ratio>1.1f)return true;}
    const bool firstPerson=pose[85]>.5f&&r->depthStandard&&r->depthReversed;
    if(firstPerson&&!colorKey)return true;
    ID3D11BlendState* originalBlend=nullptr;float blendFactors[4]{};UINT sampleMask=0xffffffffu;
    if(pose[133]>.5f){context->OMGetBlendState(&originalBlend,blendFactors,&sampleMask);if(originalBlend){D3D11_BLEND_DESC description;originalBlend->GetDesc(&description);const auto& target=description.RenderTarget[0];if(target.BlendEnable&&target.DestBlend==D3D11_BLEND_ONE&&target.SrcBlend!=D3D11_BLEND_ZERO){++r->additiveSkips;originalBlend->Release();return true;}}}
    const auto* viewportKey=reinterpret_cast<const unsigned char*>(&viewport);
    unsigned pass=0;for(unsigned i=0;i<8;++i){if(r->passes[i].frame==frameId&&r->passes[i].color==colorKey&&r->passes[i].depth==depthKey){bool same=true;for(unsigned v=0;v<sizeof(viewport);++v)if(reinterpret_cast<const unsigned char*>(r->passViewports[i])[v]!=viewportKey[v])same=false;if(same){if(originalBlend)originalBlend->Release();return true;}++r->viewportPasses;}if(r->passes[i].frame!=frameId)pass=i;}
    if(r->poseFrame!=frameId){for(unsigned i=0;i<32;++i){r->framePose[i]=r->framePose[88+i];r->framePose[88+i]=r->framePose[56+i];}for(unsigned i=32;i<88;++i)r->framePose[i]=pose[i];for(unsigned i=120;i<136;++i)r->framePose[i]=pose[i];r->poseFrame=frameId;}
    context->UpdateSubresource(frame,0,nullptr,r->framePose,0,0);
    UINT vertexClasses=16,pixelClasses=16;
    ID3D11VertexShader* vertexShader=nullptr;ID3D11PixelShader* pixelShader=nullptr;ID3D11ClassInstance* vertexInstances[16]{};ID3D11ClassInstance* pixelInstances[16]{};
    context->VSGetShader(&vertexShader,vertexInstances,&vertexClasses);context->PSGetShader(&pixelShader,pixelInstances,&pixelClasses);
    if(vertexClasses>16||pixelClasses>16){if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();for(unsigned i=0;i<16;++i){if(vertexInstances[i])vertexInstances[i]->Release();if(pixelInstances[i])pixelInstances[i]->Release();}if(originalBlend)originalBlend->Release();return false;}
    ID3D11InputLayout* layout=nullptr;ID3D11Buffer* vertices=nullptr;ID3D11Buffer* constants=nullptr;ID3D11Buffer* pixelConstants=nullptr;UINT stride=0,offset=0;D3D11_PRIMITIVE_TOPOLOGY topology;ID3D11RasterizerState* raster=nullptr;ID3D11SamplerState* sampler=nullptr;ID3D11ShaderResourceView* maps[8]{};
    context->IAGetInputLayout(&layout);context->IAGetVertexBuffers(0,1,&vertices,&stride,&offset);context->IAGetPrimitiveTopology(&topology);context->VSGetConstantBuffers(13,1,&constants);context->PSGetConstantBuffers(13,1,&pixelConstants);context->RSGetState(&raster);context->PSGetSamplers(0,1,&sampler);context->PSGetShaderResources(0,8,maps);
    const bool linear=(pose[133]>.5f&&pose[125]<.5f&&format==DXGI_FORMAT_R10G10B10A2_UNORM)||format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB||format==DXGI_FORMAT_R16G16B16A16_FLOAT||format==DXGI_FORMAT_R11G11B10_FLOAT;
    ID3D11DepthStencilState* originalDepth=nullptr;UINT stencil=0;
    if(firstPerson){context->OMGetDepthStencilState(&originalDepth,&stencil);context->OMSetDepthStencilState(pose[53]>0?r->depthReversed:r->depthStandard,stencil);}
    if(pose[133]>.5f)context->OMSetBlendState(r->opaqueBlend,nullptr,sampleMask);
    context->VSSetShader(r->vertexShader,nullptr,0);context->PSSetShader(!linear&&r->encodedShader?r->encodedShader:r->pixelShader,nullptr,0);context->IASetInputLayout(r->layout);UINT newStride=36,newOffset=0;context->IASetVertexBuffers(0,1,&r->vertices,&newStride,&newOffset);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetConstantBuffers(13,1,&frame);context->PSSetConstantBuffers(13,1,&frame);context->RSSetState(r->raster);context->PSSetSamplers(0,1,&r->sampler);context->PSSetShaderResources(0,8,r->maps);context->Draw(r->vertexCount,0);++r->calls;r->lastFrame=frameId;r->passes[pass].frame=frameId;r->passes[pass].color=colorKey;r->passes[pass].depth=depthKey;for(unsigned v=0;v<sizeof(viewport);++v)reinterpret_cast<unsigned char*>(r->passViewports[pass])[v]=viewportKey[v];if(colorKey)++r->colorCalls;else ++r->depthCalls;
    if(firstPerson)context->OMSetDepthStencilState(originalDepth,stencil);if(originalDepth)originalDepth->Release();
    if(pose[133]>.5f)context->OMSetBlendState(originalBlend,blendFactors,sampleMask);if(originalBlend)originalBlend->Release();
    context->VSSetShader(vertexShader,vertexInstances,vertexClasses);context->PSSetShader(pixelShader,pixelInstances,pixelClasses);context->IASetInputLayout(layout);context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);context->IASetPrimitiveTopology(topology);context->VSSetConstantBuffers(13,1,&constants);context->PSSetConstantBuffers(13,1,&pixelConstants);context->RSSetState(raster);context->PSSetSamplers(0,1,&sampler);context->PSSetShaderResources(0,8,maps);
    if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();if(layout)layout->Release();if(vertices)vertices->Release();if(constants)constants->Release();if(pixelConstants)pixelConstants->Release();if(raster)raster->Release();if(sampler)sampler->Release();for(unsigned i=0;i<8;++i)if(maps[i])maps[i]->Release();for(unsigned i=0;i<vertexClasses;++i)if(vertexInstances[i])vertexInstances[i]->Release();for(unsigned i=0;i<pixelClasses;++i)if(pixelInstances[i])pixelInstances[i]->Release();return true;
}
__declspec(noinline) void Release(Request* r){if(!r)return;r->ready=0;r->attempted=0;r->poseFrame=0xffffffffu;for(unsigned i=0;i<8;++i){if(r->maps[i])r->maps[i]->Release();r->maps[i]=nullptr;r->pendingMaps[i]=nullptr;}if(r->vertexShader){r->vertexShader->Release();r->vertexShader=nullptr;}if(r->pixelShader){r->pixelShader->Release();r->pixelShader=nullptr;}if(r->encodedShader){r->encodedShader->Release();r->encodedShader=nullptr;}if(r->layout){r->layout->Release();r->layout=nullptr;}if(r->vertices){r->vertices->Release();r->vertices=nullptr;}if(r->sampler){r->sampler->Release();r->sampler=nullptr;}if(r->raster){r->raster->Release();r->raster=nullptr;}if(r->opaqueBlend){r->opaqueBlend->Release();r->opaqueBlend=nullptr;}if(r->depthOwned){r->depthStandard->Release();r->depthReversed->Release();}r->depthStandard=r->depthReversed=nullptr;r->depthOwned=0;}
}
