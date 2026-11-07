#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include "../native/NativeDepthCapture.h"
#include "../native/NativePattern.h"
namespace NativeGameRender {
inline constexpr std::uint64_t VisibilityOnly=0x400000;
struct Frame {
    float right[4]{},up[4]{},forward[4]{},primary[4]{},secondary[4]{},animation[4]{},options[4]{},screen[4]{},bloom[4]{};
    float projection[4]{},fog[4]{},fogRange[4]{},outline[4]{},outlineColor[4]{},hiddenColor[4]{},outlineGlow[4]{},outlineEffect[4]{};
    float weather[4]{},weatherColor[4]{},cameraPosition[4]{},reflection[4]{};
    float aimPoint[4]{},aimOptions[4]{};
    float water[4]{};
};
static_assert(sizeof(Frame)==384);
static_assert(offsetof(Frame,aimPoint)==336&&offsetof(Frame,aimOptions)==352&&offsetof(Frame,water)==368);
struct VisibilityResult {
    volatile unsigned sequence=0;unsigned token=0,frame=0,status=0;
    float point[4]{},camera[4]{},forward[4]{};
};
struct Target {
    std::uint64_t geom;NativePattern::Material mat;std::uint64_t anchorPrimitive,viewMatrix;
    std::uint32_t startIndex,baseVertex,indexCount,topology;std::uint64_t liveBody;
    float occludedColor[4];std::int32_t occludedMode;std::uint32_t occlusion;
    std::uint64_t visibleDepthState,occludedDepthState,weapon=0,renderEntity=0;
};
struct DrawCall {unsigned char reserved0[0x40];std::uint64_t first[4];unsigned char reserved1[0x120];std::uint64_t extra[4];};
static_assert(offsetof(DrawCall,extra)==0x180);
struct Request {
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
    const void* codes[16]{};std::size_t lengths[16]{};
    ID3D11VertexShader* vertex=nullptr;ID3D11PixelShader* sky=nullptr;ID3D11PixelShader* glow=nullptr;ID3D11Buffer* constants=nullptr;
    ID3D11RasterizerState* raster=nullptr;ID3D11DepthStencilState* depth=nullptr;ID3D11BlendState* blend=nullptr;ID3D11SamplerState* sampler=nullptr;
    D3D11_BUFFER_DESC constantDesc{};D3D11_RASTERIZER_DESC rasterDesc{};D3D11_DEPTH_STENCIL_DESC depthDesc{};D3D11_BLEND_DESC blendDesc{};D3D11_SAMPLER_DESC samplerDesc{};
    ID3D11Texture2D* colourCopy=nullptr;ID3D11ShaderResourceView* colourSrv=nullptr;ID3D11RenderTargetView* backbufferView=nullptr;ID3D11Texture2D* backbuffer=nullptr;
    unsigned width=0,height=0;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;GUID textureInterface{};
    volatile unsigned sequence[2]{},bank=0;Frame frames[2]{};
    unsigned ready=0;HRESULT error=S_OK;unsigned skyCalls=0,postCalls=0,failures=0,lastPost=0xffffffff,lastSky=0xffffffff;
    ID3D11PixelShader* skyMS=nullptr;ID3D11Texture2D* depthMS=nullptr;ID3D11ShaderResourceView* depthMSSrv=nullptr;
    unsigned depthSamples=0,depthWidth=0,depthHeight=0,depthDimension=0;DXGI_FORMAT depthFormat=DXGI_FORMAT_UNKNOWN;
    struct Pass{std::uintptr_t target=0;unsigned frame=0xffffffff;};Pass passes[8]{};
    ID3D11PixelShader* maskVisible=nullptr;ID3D11PixelShader* maskAll=nullptr;ID3D11PixelShader* outlineShader=nullptr;
    ID3D11PixelShader* fogShader=nullptr;ID3D11PixelShader* fogMSShader=nullptr;
    ID3D11BlendState* overlayBlend=nullptr;ID3D11BlendState* maximumBlend=nullptr;
    ID3D11RenderTargetView* sceneTarget=nullptr;ID3D11DepthStencilView* sceneView=nullptr;ID3D11Resource* sceneResource=nullptr;ID3D11Buffer* sceneConstants=nullptr;
    D3D11_VIEWPORT sceneViewport{};unsigned sceneFrame=0xffffffff,sceneSamples=0,busy=0,flushCalls=0,fogCalls=0;
    ID3D11Texture2D* maskTexture=nullptr;ID3D11RenderTargetView* maskTarget=nullptr;ID3D11Texture2D* maskResolved=nullptr;ID3D11ShaderResourceView* maskSrv=nullptr;
    unsigned maskWidth=0,maskHeight=0,maskSamples=0,maskQuality=0,maskFrame=0xffffffff,maskCalls=0,outlineCalls=0,lastOutline=0xffffffff,lastFog=0xffffffff;
    ID3D11PixelShader* weatherShader=nullptr;ID3D11PixelShader* reflectionShader=nullptr;
    ID3D11Texture2D* reflectionCopy=nullptr;ID3D11ShaderResourceView* reflectionSrv=nullptr;unsigned reflectionWidth=0,reflectionHeight=0;DXGI_FORMAT reflectionFormat=DXGI_FORMAT_UNKNOWN;
    unsigned weatherCalls=0,reflectionCalls=0,lastWeather=0xffffffff,lastReflection=0xffffffff;
    D3D11_TEXTURE2D_DESC imageDesc[10]{};D3D11_SUBRESOURCE_DATA imageData[10]{};ID3D11Texture2D* skyImages[10]{};ID3D11ShaderResourceView* skyViews[10]{};ID3D11SamplerState* skySampler=nullptr;
    ID3D11RenderTargetView* reflectionTraceTarget=nullptr;ID3D11ShaderResourceView* reflectionTraceSrv=nullptr;ID3D11PixelShader* reflectionComposite=nullptr;unsigned traceWidth=0,traceHeight=0;
    std::uintptr_t originalDraw=0,drawContext=0;
    ID3D11RenderTargetView* skyTargets[8]{};
    unsigned skyTargetCount=0;
    float appliedStretch=1;
    NativeDepthCapture::Cache* visibilityDepth=nullptr;
    ID3D11PixelShader* visibilityShader=nullptr;
    ID3D11Texture2D* visibilityTexture=nullptr;ID3D11RenderTargetView* visibilityTarget=nullptr;ID3D11Texture2D* visibilityReadback=nullptr;
    unsigned visibilityPending=0,visibilityFrame=0,visibilityLast=0xffffffff;float visibilityPoint[4]{};
    VisibilityResult visibilityResult{};
    ID3D11PixelShader* depthResolveShader=nullptr;ID3D11RenderTargetView* nativeDepthTarget=nullptr;ID3D11Texture2D* nativeDepthSurface=nullptr;
    ID3D11PixelShader* waterEnvironmentShader=nullptr;ID3D11VertexShader* weatherVertex=nullptr;
    ID3D11Texture2D* waterSkyTexture=nullptr;ID3D11RenderTargetView* waterSkyTarget=nullptr;ID3D11ShaderResourceView* waterSkySrv=nullptr;ID3D11ShaderResourceView* waterPrevious=nullptr;unsigned waterBound=0,waterSkyCalls=0;Frame waterSkyFrame{};
    ID3D11DepthStencilState* maskDepth[2]{};unsigned rejectedContexts=0,stretchCalls=0,lastStretch=0xffffffff;
    ID3D11RenderTargetView* reflectionBlurTarget=nullptr;
    ID3D11ShaderResourceView* reflectionBlurSrv=nullptr;
};
}
extern "C" void NativeGameBefore(NativeGameRender::Request*,NativeGameRender::Target*);
extern "C" unsigned NativeGameDepth(NativeGameRender::Request*,ID3D11DepthStencilView*,unsigned,unsigned);
extern "C" bool NativeGameInitialize(NativeGameRender::Request*);
extern "C" void NativeGameRelease(NativeGameRender::Request*);
extern "C" void NativeGameSky(NativeGameRender::Request*,NativeDepthCapture::Cache*,unsigned);
extern "C" void NativeGamePost(NativeGameRender::Request*,IDXGISwapChain*,unsigned);
extern "C" void NativeGameCapture(NativeGameRender::Request*,NativeGameRender::Target*,const NativeGameRender::DrawCall*,unsigned);
extern "C" void NativeGameResolve(NativeGameRender::Request*,ID3D11Resource*,unsigned);
extern "C" void NativeGameTargets(NativeGameRender::Request*,unsigned,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*,unsigned);
