#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <cstdint>
#include <cstddef>
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
};
inline constexpr const char* Shader=R"(
cbuffer NativeFrame:register(b13){float4 padding[14];float4 view[4];float4 body[3];float4 status;};
struct In{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;uint material:TEXCOORD1;};
struct Out{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;nointerpolation uint material:TEXCOORD1;};
float3 Rotate(float3 p){return float3(body[0].x,body[0].w,body[1].z)*p.x+float3(body[0].y,body[1].x,body[1].w)*p.y+float3(body[0].z,body[1].y,body[2].x)*p.z;}
Out vs(In i){Out o;float4 world=float4(Rotate(i.p)+body[2].yzw,1);o.p=float4(dot(view[0],world),dot(view[1],world),dot(view[2],world),dot(view[3],world));o.n=Rotate(i.n);o.uv=i.uv;o.material=i.material;return o;}
Texture2D c0:register(t0);Texture2D c1:register(t1);Texture2D c2:register(t2);Texture2D c3:register(t3);
Texture2D m0:register(t4);Texture2D m1:register(t5);Texture2D m2:register(t6);Texture2D m3:register(t7);SamplerState filtering:register(s0);
float3 Linear(float3 c){return lerp(c/12.92,pow((c+.055)/1.055,2.4),step(.04045,c));}
float4 ps(Out i):SV_TARGET{float3 color;float2 mr;if(i.material==0){color=c0.Sample(filtering,i.uv).rgb;mr=m0.Sample(filtering,i.uv).rg;}else if(i.material==1){color=c1.Sample(filtering,i.uv).rgb;mr=m1.Sample(filtering,i.uv).rg;}else if(i.material==2){color=c2.Sample(filtering,i.uv).rgb;mr=m2.Sample(filtering,i.uv).rg;}else{color=c3.Sample(filtering,i.uv).rgb;mr=m3.Sample(filtering,i.uv).rg;}color=Linear(color);float3 n=normalize(i.n),light=normalize(float3(-.35,.8,-.65));float diffuse=1.35+.35*saturate(dot(n,light));return float4(saturate(color*diffuse),1);}
float4 psEncoded(Out i):SV_TARGET{float4 color=ps(i);color.rgb=lerp(color.rgb*12.92,1.055*pow(max(color.rgb,0),1/2.4)-.055,step(.0031308,color.rgb));return color;}
)";
__declspec(noinline) bool Initialize(Request* r){
    if(!r||r->ready||r->attempted||!r->device||!r->context||!r->vertexCode||!r->pixelCode||!r->vertexData.pSysMem||!r->vertexCount||r->vertexCount>300000||r->vertexCount%3||r->vertexDescription.ByteWidth!=r->vertexCount*36)return false;
    r->attempted=1;
    HRESULT hr=r->device->CreateVertexShader(r->vertexCode,r->vertexCodeBytes,nullptr,&r->vertexShader);
    if(SUCCEEDED(hr))hr=r->device->CreatePixelShader(r->pixelCode,r->pixelCodeBytes,nullptr,&r->pixelShader);
    if(SUCCEEDED(hr)&&r->encodedCode&&r->encodedCodeBytes)hr=r->device->CreatePixelShader(r->encodedCode,r->encodedCodeBytes,nullptr,&r->encodedShader);
    if(SUCCEEDED(hr))hr=r->device->CreateInputLayout(r->elements,4,r->vertexCode,r->vertexCodeBytes,&r->layout);
    if(SUCCEEDED(hr))hr=r->device->CreateBuffer(&r->vertexDescription,&r->vertexData,&r->vertices);
    if(SUCCEEDED(hr))hr=r->device->CreateSamplerState(&r->sampling,&r->sampler);
    if(SUCCEEDED(hr))hr=r->device->CreateRasterizerState(&r->rasterDescription,&r->raster);
    r->error=hr;r->ready=SUCCEEDED(hr);return r->ready!=0;
}
__declspec(noinline) bool Draw(Request* r,ID3D11Buffer* frame,const float* pose,unsigned frameId){
    if(!r||!r->ready||!r->context||!frame||!pose||pose[84]<.5f||pose[84]>1.5f||!r->maps[0])return false;
    for(unsigned i=56;i<88;++i){union{float value;unsigned bits;}number;number.value=pose[i];if((number.bits&0x7f800000)==0x7f800000)return false;}
    if(pose[56]*pose[56]+pose[57]*pose[57]+pose[58]*pose[58]<1e-10f||pose[60]*pose[60]+pose[61]*pose[61]+pose[62]*pose[62]<1e-10f)return false;
    auto context=r->context;
    ID3D11RenderTargetView* colorTarget=nullptr;ID3D11DepthStencilView* depthTarget=nullptr;context->OMGetRenderTargets(1,&colorTarget,&depthTarget);const auto colorKey=reinterpret_cast<std::uintptr_t>(colorTarget),depthKey=reinterpret_cast<std::uintptr_t>(depthTarget);DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;if(colorTarget){D3D11_RENDER_TARGET_VIEW_DESC description;colorTarget->GetDesc(&description);format=description.Format;colorTarget->Release();}r->outputFormat=format;if(depthTarget)depthTarget->Release();if(!colorKey&&!depthKey)return false;
    D3D11_VIEWPORT viewport;UINT viewportCount=1;context->RSGetViewports(&viewportCount,&viewport);if(!colorKey&&pose[48]>64&&pose[49]>64&&viewportCount&&viewport.Height>0){const float ratio=viewport.Width*pose[49]/(viewport.Height*pose[48]);if(ratio<.9f||ratio>1.1f)return true;}
    unsigned pass=0;for(unsigned i=0;i<8;++i){if(r->passes[i].frame==frameId&&r->passes[i].color==colorKey&&r->passes[i].depth==depthKey)return true;if(r->passes[i].frame!=frameId)pass=i;}
    UINT vertexClasses=16,pixelClasses=16;
    ID3D11VertexShader* vertexShader=nullptr;ID3D11PixelShader* pixelShader=nullptr;ID3D11ClassInstance* vertexInstances[16]{};ID3D11ClassInstance* pixelInstances[16]{};
    context->VSGetShader(&vertexShader,vertexInstances,&vertexClasses);context->PSGetShader(&pixelShader,pixelInstances,&pixelClasses);
    if(vertexClasses>16||pixelClasses>16){if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();for(unsigned i=0;i<16;++i){if(vertexInstances[i])vertexInstances[i]->Release();if(pixelInstances[i])pixelInstances[i]->Release();}return false;}
    ID3D11InputLayout* layout=nullptr;ID3D11Buffer* vertices=nullptr;ID3D11Buffer* constants=nullptr;UINT stride=0,offset=0;D3D11_PRIMITIVE_TOPOLOGY topology;ID3D11RasterizerState* raster=nullptr;ID3D11SamplerState* sampler=nullptr;ID3D11ShaderResourceView* maps[8]{};
    context->IAGetInputLayout(&layout);context->IAGetVertexBuffers(0,1,&vertices,&stride,&offset);context->IAGetPrimitiveTopology(&topology);context->VSGetConstantBuffers(13,1,&constants);context->RSGetState(&raster);context->PSGetSamplers(0,1,&sampler);context->PSGetShaderResources(0,8,maps);
    const bool linear=format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB||format==DXGI_FORMAT_R16G16B16A16_FLOAT||format==DXGI_FORMAT_R11G11B10_FLOAT;
    context->VSSetShader(r->vertexShader,nullptr,0);context->PSSetShader(!linear&&r->encodedShader?r->encodedShader:r->pixelShader,nullptr,0);context->IASetInputLayout(r->layout);UINT newStride=36,newOffset=0;context->IASetVertexBuffers(0,1,&r->vertices,&newStride,&newOffset);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetConstantBuffers(13,1,&frame);context->RSSetState(r->raster);context->PSSetSamplers(0,1,&r->sampler);context->PSSetShaderResources(0,8,r->maps);context->Draw(r->vertexCount,0);++r->calls;r->lastFrame=frameId;r->passes[pass].frame=frameId;r->passes[pass].color=colorKey;r->passes[pass].depth=depthKey;if(colorKey)++r->colorCalls;else ++r->depthCalls;
    context->VSSetShader(vertexShader,vertexInstances,vertexClasses);context->PSSetShader(pixelShader,pixelInstances,pixelClasses);context->IASetInputLayout(layout);context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);context->IASetPrimitiveTopology(topology);context->VSSetConstantBuffers(13,1,&constants);context->RSSetState(raster);context->PSSetSamplers(0,1,&sampler);context->PSSetShaderResources(0,8,maps);
    if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();if(layout)layout->Release();if(vertices)vertices->Release();if(constants)constants->Release();if(raster)raster->Release();if(sampler)sampler->Release();for(unsigned i=0;i<8;++i)if(maps[i])maps[i]->Release();for(unsigned i=0;i<vertexClasses;++i)if(vertexInstances[i])vertexInstances[i]->Release();for(unsigned i=0;i<pixelClasses;++i)if(pixelInstances[i])pixelInstances[i]->Release();return true;
}
__declspec(noinline) void Release(Request* r){if(!r)return;r->ready=0;r->attempted=0;if(r->vertexShader){r->vertexShader->Release();r->vertexShader=nullptr;}if(r->pixelShader){r->pixelShader->Release();r->pixelShader=nullptr;}if(r->encodedShader){r->encodedShader->Release();r->encodedShader=nullptr;}if(r->layout){r->layout->Release();r->layout=nullptr;}if(r->vertices){r->vertices->Release();r->vertices=nullptr;}if(r->sampler){r->sampler->Release();r->sampler=nullptr;}if(r->raster){r->raster->Release();r->raster=nullptr;}}
}
