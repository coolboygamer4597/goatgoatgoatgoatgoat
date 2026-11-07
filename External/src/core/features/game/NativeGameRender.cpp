#include "NativeGameRender.h"
#include <cstring>
#include <intrin.h>
#pragma intrinsic(memcpy,memset)
namespace {
using NativeGameRender::Request;using NativeGameRender::Frame;
template<class T>void release(T*& object){if(object){object->Release();object=nullptr;}}
struct Saved {
    ID3D11VertexShader* vs;ID3D11PixelShader* ps;ID3D11GeometryShader* gs;ID3D11HullShader* hs;ID3D11DomainShader* ds;
    ID3D11ClassInstance* vi[32];ID3D11ClassInstance* pi[32];ID3D11ClassInstance* gi[32];ID3D11ClassInstance* hi[32];ID3D11ClassInstance* di[32];UINT vn,pn,gn,hn,dn;
    ID3D11Buffer* vc;ID3D11Buffer* pc;ID3D11ShaderResourceView* srvs[6];ID3D11SamplerState* samplers[2];
    ID3D11InputLayout* layout;D3D11_PRIMITIVE_TOPOLOGY topology;ID3D11RasterizerState* raster;
    ID3D11DepthStencilState* depth;UINT stencil;ID3D11BlendState* blend;float factor[4];UINT mask;
    ID3D11RenderTargetView* targets[8];ID3D11DepthStencilView* dsv;ID3D11Predicate* predicate;BOOL predicateValue;
    D3D11_VIEWPORT viewports[16];UINT viewportCount;bool valid;
};
bool frame(Request* r,Frame& f){const auto bank=r->bank&1u,a=r->sequence[bank];if(a&1)return false;memcpy(&f,&r->frames[bank],sizeof(f));const auto b=r->sequence[bank];return a==b&&!(b&1)&&f.screen[0]>=64&&f.screen[1]>=64;}
__declspec(noinline) void save(ID3D11DeviceContext* c,Saved& s){
    memset(&s,0,sizeof(s));s.vn=s.pn=s.gn=s.hn=s.dn=32;
    c->VSGetShader(&s.vs,s.vi,&s.vn);c->PSGetShader(&s.ps,s.pi,&s.pn);c->GSGetShader(&s.gs,s.gi,&s.gn);c->HSGetShader(&s.hs,s.hi,&s.hn);c->DSGetShader(&s.ds,s.di,&s.dn);
    c->VSGetConstantBuffers(13,1,&s.vc);c->PSGetConstantBuffers(13,1,&s.pc);c->PSGetShaderResources(0,6,s.srvs);c->PSGetSamplers(0,2,s.samplers);c->IAGetInputLayout(&s.layout);c->IAGetPrimitiveTopology(&s.topology);c->RSGetState(&s.raster);c->OMGetDepthStencilState(&s.depth,&s.stencil);c->OMGetBlendState(&s.blend,s.factor,&s.mask);c->OMGetRenderTargets(8,s.targets,&s.dsv);c->GetPredication(&s.predicate,&s.predicateValue);s.viewportCount=16;c->RSGetViewports(&s.viewportCount,s.viewports);s.valid=s.vn<=32&&s.pn<=32&&s.gn<=32&&s.hn<=32&&s.dn<=32&&s.viewportCount<=16;
}
__declspec(noinline) void restore(ID3D11DeviceContext* c,Saved& s){
    if(s.valid){ID3D11ShaderResourceView* empty[6]{};c->PSSetShaderResources(0,6,empty);c->OMSetRenderTargets(8,s.targets,s.dsv);c->VSSetShader(s.vs,s.vi,s.vn);c->PSSetShader(s.ps,s.pi,s.pn);c->GSSetShader(s.gs,s.gi,s.gn);c->HSSetShader(s.hs,s.hi,s.hn);c->DSSetShader(s.ds,s.di,s.dn);c->VSSetConstantBuffers(13,1,&s.vc);c->PSSetConstantBuffers(13,1,&s.pc);c->PSSetShaderResources(0,6,s.srvs);c->PSSetSamplers(0,2,s.samplers);c->IASetInputLayout(s.layout);c->IASetPrimitiveTopology(s.topology);c->RSSetState(s.raster);c->OMSetDepthStencilState(s.depth,s.stencil);c->OMSetBlendState(s.blend,s.factor,s.mask);c->RSSetViewports(s.viewportCount,s.viewports);c->SetPredication(s.predicate,s.predicateValue);}
    release(s.vs);release(s.ps);release(s.gs);release(s.hs);release(s.ds);for(unsigned i=0;i<32;++i){release(s.vi[i]);release(s.pi[i]);release(s.gi[i]);release(s.hi[i]);release(s.di[i]);}release(s.vc);release(s.pc);for(auto& srv:s.srvs)release(srv);for(auto& sampler:s.samplers)release(sampler);release(s.layout);release(s.raster);release(s.depth);release(s.blend);for(auto& target:s.targets)release(target);release(s.dsv);release(s.predicate);
}
void bind(Request* r,Frame& f,ID3D11RenderTargetView* target,ID3D11PixelShader* ps){
    auto* c=r->context;const unsigned style=static_cast<unsigned>(f.animation[1]);ID3D11ShaderResourceView* image=style<10?r->skyViews[style]:nullptr;f.bloom[3]=image?1.f:0.f;c->PSSetShaderResources(4,1,&image);c->PSSetSamplers(1,1,&r->skySampler);c->SetPredication(nullptr,FALSE);c->OMSetRenderTargets(1,&target,nullptr);c->VSSetShader(r->vertex,nullptr,0);c->PSSetShader(ps,nullptr,0);c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->RSSetState(r->raster);c->OMSetDepthStencilState(r->depth,0);c->OMSetBlendState(r->blend,nullptr,0xffffffff);c->UpdateSubresource(r->constants,0,nullptr,&f,0,0);c->VSSetConstantBuffers(13,1,&r->constants);c->PSSetConstantBuffers(13,1,&r->constants);c->PSSetSamplers(0,1,&r->sampler);
}
__declspec(noinline) void restoreWaterBinding(Request* r){
    if(!r||!r->waterBound)return;r->context->PSSetShaderResources(11,1,&r->waterPrevious);release(r->waterPrevious);r->waterBound=0;
}
__declspec(noinline) bool updateWaterEnvironment(Request* r,Frame& f){
    bool changed=!r->waterSkySrv||f.options[2]!=r->waterSkyFrame.options[2]||f.water[2]!=r->waterSkyFrame.water[2]||f.options[0]!=r->waterSkyFrame.options[0]||f.reflection[3]!=r->waterSkyFrame.reflection[3];
    for(unsigned i=1;i<4;++i)changed=changed||f.animation[i]!=r->waterSkyFrame.animation[i];
    for(unsigned i=0;i<3;++i)changed=changed||f.primary[i]!=r->waterSkyFrame.primary[i]||f.secondary[i]!=r->waterSkyFrame.secondary[i];
    const float elapsed=f.animation[0]-r->waterSkyFrame.animation[0];changed=changed||(f.options[2]>.5f&&(elapsed>.06f||elapsed<-.06f));
    if(!changed)return true;
    if(!r->waterSkyTexture){
        D3D11_TEXTURE2D_DESC d{};d.Width=1024;d.Height=512;d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
        HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->waterSkyTexture);if(SUCCEEDED(h))h=r->device->CreateRenderTargetView(r->waterSkyTexture,nullptr,&r->waterSkyTarget);if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(r->waterSkyTexture,nullptr,&r->waterSkySrv);
        if(FAILED(h)){release(r->waterSkySrv);release(r->waterSkyTarget);release(r->waterSkyTexture);r->error=h;return false;}
    }
    auto* c=r->context;Saved saved;save(c,saved);if(!saved.valid){restore(c,saved);return false;}r->busy=1;
    D3D11_VIEWPORT viewport{0,0,1024,512,0,1};c->RSSetViewports(1,&viewport);bind(r,f,r->waterSkyTarget,r->waterEnvironmentShader);c->Draw(3,0);c->OMSetRenderTargets(0,nullptr,nullptr);c->GenerateMips(r->waterSkySrv);restore(c,saved);r->busy=0;r->waterSkyFrame=f;++r->waterSkyCalls;return true;
}
extern "C" __declspec(noinline) void NativeGameBefore(Request* r,NativeGameRender::Target* target){
    if(!r||!r->ready||r->busy||r->waterBound||!target||!(target->weapon&0x200000)||target->mat.org_pos[0]<.5f)return;Frame f;if(!frame(r,f)||f.water[0]<.5f||!updateWaterEnvironment(r,f))return;
    r->context->PSGetShaderResources(11,1,&r->waterPrevious);r->context->PSSetShaderResources(11,1,&r->waterSkySrv);r->waterBound=1;
}

void dropScene(Request* r){release(r->sceneTarget);release(r->sceneView);release(r->sceneResource);release(r->sceneConstants);r->sceneFrame=0xffffffff;}
__declspec(noinline) bool depthCopy(Request* r,ID3D11DepthStencilView* view,Frame& f){
    if(!view)return false;D3D11_DEPTH_STENCIL_VIEW_DESC vd;view->GetDesc(&vd);r->depthDimension=vd.ViewDimension;
    const bool ms=vd.ViewDimension==D3D11_DSV_DIMENSION_TEXTURE2DMS;
    if((!ms&&vd.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D)||(vd.Format!=DXGI_FORMAT_D32_FLOAT&&vd.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT))return false;
    ID3D11Resource* resource=nullptr;view->GetResource(&resource);auto* source=static_cast<ID3D11Texture2D*>(resource);if(!source)return false;
    D3D11_TEXTURE2D_DESC d;source->GetDesc(&d);if(d.SampleDesc.Count<1||d.SampleDesc.Count>8||d.ArraySize!=1||d.MipLevels!=1||d.Width!=unsigned(f.screen[0])||d.Height!=unsigned(f.screen[1])){release(source);return false;}
    if(!r->depthMS||r->depthSamples!=d.SampleDesc.Count||r->depthWidth!=d.Width||r->depthHeight!=d.Height||r->depthFormat!=vd.Format){
        release(r->depthMSSrv);release(r->depthMS);r->depthSamples=d.SampleDesc.Count;r->depthWidth=d.Width;r->depthHeight=d.Height;r->depthFormat=vd.Format;
        d.Format=vd.Format==DXGI_FORMAT_D32_FLOAT?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R24G8_TYPELESS;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;d.MiscFlags=0;
        HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->depthMS);D3D11_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=vd.Format==DXGI_FORMAT_D32_FLOAT?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R24_UNORM_X8_TYPELESS;desc.ViewDimension=ms?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;desc.Texture2D.MipLevels=1;
        if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(r->depthMS,&desc,&r->depthMSSrv);r->error=h;if(FAILED(h)){++r->failures;release(source);return false;}
    }
    auto* c=r->context;ID3D11ShaderResourceView* empty[6]{};c->PSSetShaderResources(0,6,empty);c->OMSetRenderTargets(0,nullptr,nullptr);c->SetPredication(nullptr,FALSE);c->CopyResource(r->depthMS,source);release(source);return true;
}
__declspec(noinline) bool maskResources(Request* r,ID3D11RenderTargetView* target,Frame& f){
    ID3D11Resource* resource=nullptr;target->GetResource(&resource);auto* texture=static_cast<ID3D11Texture2D*>(resource);if(!texture)return false;D3D11_TEXTURE2D_DESC d;texture->GetDesc(&d);release(texture);
    if(d.Width!=unsigned(f.screen[0])||d.Height!=unsigned(f.screen[1])||d.ArraySize!=1||d.MipLevels!=1)return false;
    if(r->maskTarget&&r->maskWidth==d.Width&&r->maskHeight==d.Height&&r->maskSamples==d.SampleDesc.Count&&r->maskQuality==d.SampleDesc.Quality)return true;
    release(r->maskSrv);release(r->maskTarget);release(r->maskTexture);release(r->maskResolved);r->maskFrame=0xffffffff;
    r->maskWidth=d.Width;r->maskHeight=d.Height;r->maskSamples=d.SampleDesc.Count;r->maskQuality=d.SampleDesc.Quality;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET;d.CPUAccessFlags=0;d.MiscFlags=0;
    if(d.SampleDesc.Count==1)d.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
    HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->maskTexture);if(SUCCEEDED(h))h=r->device->CreateRenderTargetView(r->maskTexture,nullptr,&r->maskTarget);
    if(d.SampleDesc.Count>1){d.SampleDesc.Count=1;d.SampleDesc.Quality=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;if(SUCCEEDED(h))h=r->device->CreateTexture2D(&d,nullptr,&r->maskResolved);}
    if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(r->maskResolved?r->maskResolved:r->maskTexture,nullptr,&r->maskSrv);r->error=h;if(FAILED(h)){++r->failures;return false;}return true;
}
__declspec(noinline) void track(Request* r,Frame& f,unsigned current,Saved& s){
    if(!s.valid||s.viewportCount!=1||!s.targets[0]||!s.dsv||s.viewports[0].Width!=f.screen[0]||s.viewports[0].Height!=f.screen[1]||s.viewports[0].TopLeftX||s.viewports[0].TopLeftY)return;
    if(r->sceneFrame==current&&r->sceneTarget==s.targets[0]&&r->sceneView==s.dsv){if(!r->sceneConstants)r->context->VSGetConstantBuffers(0,1,&r->sceneConstants);r->sceneViewport=s.viewports[0];return;}
    dropScene(r);r->sceneFrame=current;r->sceneTarget=s.targets[0];r->sceneView=s.dsv;r->sceneViewport=s.viewports[0];r->sceneTarget->AddRef();r->sceneView->AddRef();r->sceneTarget->GetResource(&r->sceneResource);r->context->VSGetConstantBuffers(0,1,&r->sceneConstants);
    D3D11_TEXTURE2D_DESC d;static_cast<ID3D11Texture2D*>(r->sceneResource)->GetDesc(&d);r->sceneSamples=d.SampleDesc.Count;
}
__declspec(noinline) void visibilityProbe(Request* r,Frame& f,unsigned current){
    if(f.aimOptions[0]<.5f||f.aimPoint[3]<1)return;
    auto* c=r->context;
    if(r->visibilityPending&&r->visibilityReadback){
        D3D11_MAPPED_SUBRESOURCE mapped;
        const auto h=c->Map(r->visibilityReadback,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
        if(SUCCEEDED(h)){
            const auto* values=static_cast<const float*>(mapped.pData);auto& result=r->visibilityResult;
            const unsigned sequence=result.sequence+1;result.sequence=sequence|1u;
            result.token=r->visibilityPending;result.frame=r->visibilityFrame;
            result.status=values[3]>.5f&&values[7]>.5f&&values[11]>.5f?(values[0]>.5f?1u:2u):0u;
            memcpy(result.point,r->visibilityPoint,16);memcpy(result.camera,values+4,16);memcpy(result.forward,values+8,16);
            _ReadWriteBarrier();result.sequence=(sequence+1)&~1u;
            c->Unmap(r->visibilityReadback,0);r->visibilityPending=0;
        }else if(h!=DXGI_ERROR_WAS_STILL_DRAWING){r->visibilityPending=0;}
    }
    auto* depth=r->visibilityDepth;
    if(r->visibilityPending||r->visibilityLast==current||!depth||depth->frame!=current||!depth->srv||!depth->bits)return;
    if(!r->visibilityTexture){
        D3D11_TEXTURE2D_DESC d{};d.Width=3;d.Height=d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->visibilityTexture);if(SUCCEEDED(h))h=r->device->CreateRenderTargetView(r->visibilityTexture,nullptr,&r->visibilityTarget);
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(SUCCEEDED(h))h=r->device->CreateTexture2D(&d,nullptr,&r->visibilityReadback);
        if(FAILED(h)){release(r->visibilityTarget);release(r->visibilityReadback);release(r->visibilityTexture);return;}
    }
    Saved saved;save(c,saved);if(!saved.valid||saved.viewportCount!=1||saved.dsv!=depth->view||saved.viewports[0].Width!=f.screen[0]||saved.viewports[0].Height!=f.screen[1]||saved.viewports[0].TopLeftX||saved.viewports[0].TopLeftY||saved.viewports[0].MinDepth!=0||saved.viewports[0].MaxDepth!=1){restore(c,saved);return;}
    r->busy=1;
    D3D11_VIEWPORT tiny{0,0,3,1,0,1};c->RSSetViewports(1,&tiny);
    bind(r,f,r->visibilityTarget,r->visibilityShader);c->PSSetShaderResources(0,1,&depth->srv);c->Draw(3,0);
    ID3D11ShaderResourceView* empty=nullptr;c->PSSetShaderResources(0,1,&empty);c->OMSetRenderTargets(0,nullptr,nullptr);c->CopyResource(r->visibilityReadback,r->visibilityTexture);
    r->visibilityPending=static_cast<unsigned>(f.aimPoint[3]);r->visibilityFrame=current;r->visibilityLast=current;memcpy(r->visibilityPoint,f.aimPoint,16);restore(c,saved);r->busy=0;
}

__declspec(noinline) void fogDraw(Request* r,Frame& f,unsigned current){
    if(f.projection[3]<.5f||r->lastFog==current)return;auto* c=r->context;
    bind(r,f,r->sceneTarget,r->depthSamples>1?r->fogMSShader:r->fogShader);c->OMSetBlendState(r->overlayBlend,nullptr,0xffffffff);c->PSSetShaderResources(r->depthSamples>1?2:0,1,&r->depthMSSrv);c->Draw(3,0);r->lastFog=current;++r->fogCalls;
}
__declspec(noinline) void outlineDraw(Request* r,Frame& f,unsigned current){
    if(f.outline[0]<.5f||r->maskFrame!=current||!r->maskSrv||r->lastOutline==current)return;auto* c=r->context;
    ID3D11ShaderResourceView* empty[6]{};c->PSSetShaderResources(0,6,empty);c->OMSetRenderTargets(0,nullptr,nullptr);c->SetPredication(nullptr,FALSE);
    if(r->maskResolved)c->ResolveSubresource(r->maskResolved,0,r->maskTexture,0,DXGI_FORMAT_R8G8B8A8_UNORM);
    bind(r,f,r->sceneTarget,r->outlineShader);c->OMSetBlendState(r->overlayBlend,nullptr,0xffffffff);c->PSSetShaderResources(3,1,&r->maskSrv);c->Draw(3,0);r->lastOutline=current;++r->outlineCalls;
}

__declspec(noinline) bool makeReflectionTarget(Request* r,unsigned width,unsigned height,
    ID3D11RenderTargetView*& target,ID3D11ShaderResourceView*& srv){
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width;desc.Height=height;
    desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* texture=nullptr;
    HRESULT h=r->device->CreateTexture2D(&desc,nullptr,&texture);
    if(SUCCEEDED(h))h=r->device->CreateRenderTargetView(texture,nullptr,&target);
    if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(texture,nullptr,&srv);
    release(texture);
    if(SUCCEEDED(h))return true;
    release(target);release(srv);r->error=h;++r->failures;
    return false;
}

__declspec(noinline) void reflectionDraw(Request* r,Frame& f,unsigned current){
    if(f.fogRange[3]<.5f||f.reflection[0]<=0||r->lastReflection==current)return;
    auto* source=static_cast<ID3D11Texture2D*>(r->sceneResource);D3D11_TEXTURE2D_DESC d;source->GetDesc(&d);
    if(!r->reflectionCopy||!r->reflectionSrv||r->reflectionWidth!=d.Width||r->reflectionHeight!=d.Height||r->reflectionFormat!=d.Format){
        release(r->reflectionSrv);release(r->reflectionCopy);r->reflectionWidth=d.Width;r->reflectionHeight=d.Height;r->reflectionFormat=d.Format;d.SampleDesc.Count=1;d.SampleDesc.Quality=0;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;d.MiscFlags=0;
        HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->reflectionCopy);if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(r->reflectionCopy,nullptr,&r->reflectionSrv);r->error=h;if(FAILED(h)){++r->failures;return;}
    }
    auto* c=r->context;ID3D11ShaderResourceView* empty[6]{};c->PSSetShaderResources(0,6,empty);c->OMSetRenderTargets(0,nullptr,nullptr);
    if(r->sceneSamples>1)c->ResolveSubresource(r->reflectionCopy,0,source,0,r->reflectionFormat);else c->CopyResource(r->reflectionCopy,source);
    const bool broadBlur=f.reflection[1]>=.6f;
    const unsigned maxWidth=broadBlur?640:960,maxHeight=broadBlur?360:540;
    unsigned divisor=(d.Width+maxWidth-1)/maxWidth;
    const unsigned verticalDivisor=(d.Height+maxHeight-1)/maxHeight;
    if(verticalDivisor>divisor)divisor=verticalDivisor;
    if(divisor<2)divisor=2;
    const unsigned width=(d.Width+divisor-1)/divisor,height=(d.Height+divisor-1)/divisor;
    if(r->traceWidth!=width||r->traceHeight!=height){
        release(r->reflectionTraceSrv);release(r->reflectionTraceTarget);
        release(r->reflectionBlurSrv);release(r->reflectionBlurTarget);
        r->traceWidth=width;r->traceHeight=height;
    }
    if(!r->reflectionTraceTarget&&!makeReflectionTarget(r,width,height,r->reflectionTraceTarget,r->reflectionTraceSrv))return;
    D3D11_VIEWPORT low{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    c->RSSetViewports(1,&low);
    bind(r,f,r->reflectionTraceTarget,r->reflectionShader);
    c->PSSetShaderResources(r->depthSamples>1?2:0,1,&r->depthMSSrv);
    c->PSSetShaderResources(1,1,&r->reflectionSrv);c->Draw(3,0);
    if(f.reflection[1]>=.001f&&(r->reflectionBlurTarget||makeReflectionTarget(r,width,height,r->reflectionBlurTarget,r->reflectionBlurSrv))){
        f.screen[3]=1;
        bind(r,f,r->reflectionBlurTarget,r->reflectionComposite);
        c->PSSetShaderResources(5,1,&r->reflectionTraceSrv);c->Draw(3,0);
        c->PSSetShaderResources(5,1,empty);
        f.screen[3]=2;
        bind(r,f,r->reflectionTraceTarget,r->reflectionComposite);
        c->PSSetShaderResources(5,1,&r->reflectionBlurSrv);c->Draw(3,0);
        f.screen[3]=0;
    }
    c->RSSetViewports(1,&r->sceneViewport);
    bind(r,f,r->sceneTarget,r->reflectionComposite);
    c->OMSetBlendState(r->overlayBlend,nullptr,0xffffffff);
    c->PSSetShaderResources(r->depthSamples>1?2:0,1,&r->depthMSSrv);
    c->PSSetShaderResources(5,1,&r->reflectionTraceSrv);c->Draw(3,0);
    r->lastReflection=current;++r->reflectionCalls;
}
__declspec(noinline) void weatherDraw(Request* r,Frame& f,unsigned current){
    if(f.weather[0]<.5f||r->lastWeather==current)return;auto* c=r->context;bind(r,f,r->sceneTarget,r->weatherShader);c->OMSetBlendState(r->overlayBlend,nullptr,0xffffffff);c->PSSetShaderResources(r->depthSamples>1?2:0,1,&r->depthMSSrv);c->VSSetShader(r->weatherVertex,nullptr,0);const unsigned particles=f.weather[1]<.5f?2048:f.weather[1]<1.5f?864:576;c->DrawInstanced(6,particles,0,0);r->lastWeather=current;++r->weatherCalls;
}

bool skyPending(const Request* r,ID3D11RenderTargetView* target,unsigned current) {
    if(r->lastSky!=current)return true;
    for(unsigned i=0;i<r->skyTargetCount;++i)
        if(r->skyTargets[i]==target)return false;
    return r->skyTargetCount<sizeof(r->skyTargets)/sizeof(r->skyTargets[0]);
}

__declspec(noinline) void drawSky(Request* r,Frame& f,ID3D11RenderTargetView* target,unsigned current) {
    auto* c=r->context;
    bind(r,f,target,r->depthSamples>1?r->skyMS:r->sky);
    c->PSSetShaderResources(r->depthSamples>1?2:0,1,&r->depthMSSrv);
    c->Draw(3,0);
    if(r->lastSky!=current){r->lastSky=current;r->skyTargetCount=0;}
    r->skyTargets[r->skyTargetCount++]=target;
    ++r->skyCalls;
}

__declspec(noinline) void flush(Request* r,unsigned current){
    if(!r||!r->ready||r->busy||r->sceneFrame!=current||!r->sceneTarget||!r->sceneView)return;Frame f;if(!frame(r,f))return;
    r->busy=1;auto* c=r->context;Saved s;save(c,s);if(s.valid){ID3D11Buffer* old=nullptr;c->VSGetConstantBuffers(0,1,&old);if(r->sceneConstants)c->VSSetConstantBuffers(0,1,&r->sceneConstants);c->RSSetViewports(1,&r->sceneViewport);
        const bool sky=f.options[2]>.5f&&skyPending(r,r->sceneTarget,current);
        const bool sceneDepth=sky||(f.projection[3]>.5f&&r->lastFog!=current)||
            (f.weather[0]>.5f&&r->lastWeather!=current)||(f.fogRange[3]>.5f&&r->lastReflection!=current);
        if(sceneDepth&&depthCopy(r,r->sceneView,f)){
            f.screen[2]=static_cast<float>(r->depthSamples);
            if(sky)drawSky(r,f,r->sceneTarget,current);
            reflectionDraw(r,f,current);fogDraw(r,f,current);weatherDraw(r,f,current);
        }
        outlineDraw(r,f,current);c->VSSetConstantBuffers(0,1,&old);release(old);++r->flushCalls;
    }restore(c,s);dropScene(r);r->busy=0;
}
}
extern "C" __declspec(noinline) unsigned NativeGameDepth(Request* r,ID3D11DepthStencilView* view,unsigned current,unsigned capture){
    if(!r||!r->ready||r->busy||!view||!r->visibilityDepth)return 0;Frame f;if(!frame(r,f))return 0;
    auto* cache=r->visibilityDepth;auto* c=r->context;unsigned count=1;D3D11_VIEWPORT viewport;c->RSGetViewports(&count,&viewport);
    if(count!=1||viewport.Width!=f.screen[0]||viewport.Height!=f.screen[1]||viewport.TopLeftX||viewport.TopLeftY||viewport.MinDepth!=0||viewport.MaxDepth!=1)return 0;
    D3D11_DEPTH_STENCIL_VIEW_DESC vd;view->GetDesc(&vd);if(vd.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2DMS||(vd.Format!=DXGI_FORMAT_D32_FLOAT&&vd.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT))return 0;
    if(cache->view!=view||!cache->bits||!cache->copy||!cache->srv){
        release(cache->srv);release(cache->copy);release(cache->source);release(cache->view);release(r->nativeDepthTarget);r->nativeDepthSurface=nullptr;cache->bits=0;cache->failure=0;cache->frame=0xffffffff;
        ID3D11Resource* resource=nullptr;view->GetResource(&resource);if(!resource){cache->failure=8;return 0;}cache->source=static_cast<ID3D11Texture2D*>(resource);cache->view=view;view->AddRef();
        D3D11_TEXTURE2D_DESC d;cache->source->GetDesc(&d);if(d.Width!=unsigned(f.screen[0])||d.Height!=unsigned(f.screen[1])||d.SampleDesc.Count<2||d.SampleDesc.Count>8||d.ArraySize!=1||d.MipLevels!=1){cache->failure=9;return 0;}
        d.Format=DXGI_FORMAT_R32_FLOAT;d.SampleDesc.Count=1;d.SampleDesc.Quality=0;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;d.MiscFlags=0;
        HRESULT h=r->device->CreateTexture2D(&d,nullptr,&cache->copy);if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(cache->copy,nullptr,&cache->srv);if(SUCCEEDED(h))h=r->device->CreateRenderTargetView(cache->copy,nullptr,&r->nativeDepthTarget);
        if(FAILED(h)){cache->failure=10;r->error=h;return 0;}r->nativeDepthSurface=cache->copy;cache->bits=vd.Format==DXGI_FORMAT_D24_UNORM_S8_UINT?24:32;
    }
    if(!capture)return cache->bits;
    if(cache->frame!=current){
        Saved saved;save(c,saved);if(!saved.valid){restore(c,saved);return 0;}r->busy=1;
        const bool copied=depthCopy(r,view,f);
        if(copied&&r->depthSamples>1){f.screen[2]=float(r->depthSamples);bind(r,f,r->nativeDepthTarget,r->depthResolveShader);c->PSSetShaderResources(2,1,&r->depthMSSrv);c->Draw(3,0);cache->frame=current;++cache->copies;}
        restore(c,saved);r->busy=0;if(!copied)return 0;
    }
    return cache->frame==current?cache->bits|0x100:0;
}

extern "C" __declspec(noinline) bool NativeGameInitialize(Request* r){
    auto* d=r->device;if(!d||!r->context)return false;HRESULT h=d->CreateVertexShader(r->codes[0],r->lengths[0],nullptr,&r->vertex);
    ID3D11PixelShader** outputs[]={&r->sky,&r->glow,&r->skyMS,&r->maskVisible,&r->maskAll,&r->outlineShader,&r->fogShader,&r->fogMSShader,&r->weatherShader,&r->reflectionShader,&r->reflectionComposite,&r->visibilityShader};
    for(unsigned i=0;i<12&&SUCCEEDED(h);++i)h=d->CreatePixelShader(r->codes[i+1],r->lengths[i+1],nullptr,outputs[i]);
    if(SUCCEEDED(h))h=d->CreateBuffer(&r->constantDesc,nullptr,&r->constants);if(SUCCEEDED(h))h=d->CreateRasterizerState(&r->rasterDesc,&r->raster);if(SUCCEEDED(h))h=d->CreateDepthStencilState(&r->depthDesc,&r->depth);if(SUCCEEDED(h))h=d->CreateBlendState(&r->blendDesc,&r->blend);if(SUCCEEDED(h))h=d->CreateSamplerState(&r->samplerDesc,&r->sampler);
    auto bd=r->blendDesc;auto& rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    if(SUCCEEDED(h))h=d->CreateBlendState(&bd,&r->overlayBlend);rt.SrcBlend=rt.DestBlend=rt.SrcBlendAlpha=rt.DestBlendAlpha=D3D11_BLEND_ONE;rt.BlendOp=rt.BlendOpAlpha=D3D11_BLEND_OP_MAX;
    if(SUCCEEDED(h))h=d->CreateBlendState(&bd,&r->maximumBlend);auto skySampler=r->samplerDesc;skySampler.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;skySampler.AddressV=D3D11_TEXTURE_ADDRESS_CLAMP;if(SUCCEEDED(h))h=d->CreateSamplerState(&skySampler,&r->skySampler);
    for(unsigned i=0;i<10&&SUCCEEDED(h);++i){const auto& desc=r->imageDesc[i];if(!r->imageData[i].pSysMem)continue;if(desc.Width<2||desc.Height<2||desc.Width>4096||desc.Height>2048||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1){h=E_INVALIDARG;break;}h=d->CreateTexture2D(&desc,&r->imageData[i],&r->skyImages[i]);if(SUCCEEDED(h))h=d->CreateShaderResourceView(r->skyImages[i],nullptr,&r->skyViews[i]);}
    if(SUCCEEDED(h))h=d->CreatePixelShader(r->codes[13],r->lengths[13],nullptr,&r->depthResolveShader);if(SUCCEEDED(h))h=d->CreatePixelShader(r->codes[14],r->lengths[14],nullptr,&r->waterEnvironmentShader);if(SUCCEEDED(h))h=d->CreateVertexShader(r->codes[15],r->lengths[15],nullptr,&r->weatherVertex);
    for(unsigned i=0;i<2&&SUCCEEDED(h);++i){D3D11_DEPTH_STENCIL_DESC desc{};desc.DepthEnable=TRUE;desc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;desc.DepthFunc=i?D3D11_COMPARISON_GREATER_EQUAL:D3D11_COMPARISON_LESS_EQUAL;h=d->CreateDepthStencilState(&desc,&r->maskDepth[i]);}
    r->error=h;r->ready=SUCCEEDED(h);return r->ready!=0;
}
extern "C" __declspec(noinline) void NativeGameRelease(Request* r){
    restoreWaterBinding(r);r->ready=0;release(r->maskDepth[0]);release(r->maskDepth[1]);release(r->waterSkySrv);release(r->waterSkyTarget);release(r->waterSkyTexture);release(r->nativeDepthTarget);r->nativeDepthSurface=nullptr;release(r->depthResolveShader);release(r->waterEnvironmentShader);release(r->weatherVertex);release(r->visibilityShader);release(r->visibilityTarget);release(r->visibilityReadback);release(r->visibilityTexture);r->visibilityPending=0;r->visibilityResult.status=0;for(unsigned i=0;i<10;++i){release(r->skyViews[i]);release(r->skyImages[i]);}release(r->skySampler);release(r->vertex);release(r->sky);release(r->skyMS);release(r->glow);release(r->constants);release(r->raster);release(r->depth);release(r->blend);release(r->sampler);release(r->colourSrv);release(r->colourCopy);release(r->backbufferView);release(r->backbuffer);release(r->depthMSSrv);release(r->depthMS);
    release(r->maskVisible);release(r->maskAll);release(r->outlineShader);release(r->fogShader);release(r->fogMSShader);release(r->overlayBlend);release(r->maximumBlend);release(r->weatherShader);release(r->reflectionShader);release(r->reflectionCopy);release(r->reflectionSrv);release(r->reflectionTraceSrv);release(r->reflectionTraceTarget);release(r->reflectionBlurSrv);release(r->reflectionBlurTarget);release(r->reflectionComposite);release(r->maskSrv);release(r->maskTarget);release(r->maskTexture);release(r->maskResolved);dropScene(r);
}
extern "C" __declspec(noinline) void NativeGameSky(Request* r,NativeDepthCapture::Cache* cache,unsigned current){
    if(!r||!r->ready||r->busy||!cache||!cache->view)return;Frame f;if(!frame(r,f)||f.options[2]<.5f)return;
    auto* c=r->context;Saved s;r->busy=1;save(c,s);
    if(s.valid&&s.viewportCount==1&&s.targets[0]&&skyPending(r,s.targets[0],current)&&s.dsv==cache->view&&s.viewports[0].Width==f.screen[0]&&s.viewports[0].Height==f.screen[1]&&depthCopy(r,cache->view,f))
        drawSky(r,f,s.targets[0],current);
    restore(c,s);r->busy=0;
}
extern "C" __declspec(noinline) void NativeGameCapture(Request* r,NativeGameRender::Target* target,const NativeGameRender::DrawCall* call,unsigned current){
    restoreWaterBinding(r);if(!r||!r->ready||r->busy||!target)return;Frame f;if(!frame(r,f))return;
    const bool world=(target->weapon&0x200000)!=0,character=(target->weapon&0x1ff)==0;
    if(character&&f.aimOptions[0]>.5f)visibilityProbe(r,f,current);
    if(target->weapon&NativeGameRender::VisibilityOnly)return;
    if(world&&r->sceneFrame==current&&r->sceneConstants)return;
    if(!world) {
        if(!character||f.outline[0]<.5f||!call||!r->originalDraw||!target->visibleDepthState)return;
        if(!r->drawContext||call->first[0]!=r->drawContext){++r->rejectedContexts;return;}
    }
    auto* c=r->context;
    Saved s{};
    s.viewportCount=16;
    c->RSGetViewports(&s.viewportCount,s.viewports);
    if(s.viewportCount!=1||s.viewports[0].Width!=f.screen[0]||s.viewports[0].Height!=f.screen[1])return;
    c->OMGetRenderTargets(world?1:8,s.targets,&s.dsv);
    s.valid=true;
    if(world)track(r,f,current,s);
    else if(s.targets[0]&&s.dsv&&maskResources(r,s.targets[0],f)) {
        track(r,f,current,s);
        s.pn=32;c->PSGetShader(&s.ps,s.pi,&s.pn);
        c->OMGetDepthStencilState(&s.depth,&s.stencil);
        c->OMGetBlendState(&s.blend,s.factor,&s.mask);
        c->GetPredication(&s.predicate,&s.predicateValue);
        r->busy=1;c->SetPredication(nullptr,FALSE);
        if(r->maskFrame!=current){const float clear[4]{};c->ClearRenderTargetView(r->maskTarget,clear);r->maskFrame=current;}
        c->OMSetRenderTargets(1,&r->maskTarget,s.dsv);c->OMSetBlendState(r->maximumBlend,nullptr,0xffffffff);
        using Draw=std::uint64_t(*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
        const auto draw=reinterpret_cast<Draw>(r->originalDraw);
        if(f.outline[2]>.5f){
            c->PSSetShader(r->maskAll,nullptr,0);c->OMSetDepthStencilState(r->depth,0);
            draw(call->first[0],call->first[1],call->first[2],call->first[3],call->extra[0],call->extra[1],call->extra[2],call->extra[3]);
        }
        c->PSSetShader(r->maskVisible,nullptr,0);
        c->OMSetDepthStencilState(r->maskDepth[f.options[1]>.5f?1:0],s.stencil);
        draw(call->first[0],call->first[1],call->first[2],call->first[3],call->extra[0],call->extra[1],call->extra[2],call->extra[3]);
        ++r->maskCalls;
        c->OMSetRenderTargets(8,s.targets,s.dsv);
        c->PSSetShader(s.ps,s.pi,s.pn);
        c->OMSetDepthStencilState(s.depth,s.stencil);
        c->OMSetBlendState(s.blend,s.factor,s.mask);
        c->SetPredication(s.predicate,s.predicateValue);
        r->busy=0;
        release(s.ps);release(s.depth);release(s.blend);release(s.predicate);
        for(auto& instance:s.pi)release(instance);
    }
    for(auto& surface:s.targets)release(surface);
    release(s.dsv);
}
extern "C" __declspec(noinline) void NativeGameResolve(Request* r,ID3D11Resource* source,unsigned current){if(r&&source&&source==r->sceneResource)flush(r,current);}
extern "C" __declspec(noinline) void NativeGameTargets(Request* r,unsigned count,ID3D11RenderTargetView* const* targets,ID3D11DepthStencilView* view,unsigned current){
    if(!r||!r->ready||r->busy)return;
    if(r->sceneSamples==1&&r->sceneTarget&&(count==0||!targets||targets[0]!=r->sceneTarget))flush(r,current);
    if(count!=1||!targets||!targets[0]||!view)return;Frame f;if(!frame(r,f))return;
    if(f.options[2]<.5f&&f.projection[3]<.5f&&f.weather[0]<.5f&&f.fogRange[3]<.5f&&f.outline[0]<.5f)return;
    if(r->sceneFrame==current&&r->sceneTarget==targets[0]&&r->sceneView==view)return;
    D3D11_RENDER_TARGET_VIEW_DESC colourView;targets[0]->GetDesc(&colourView);
    if(colourView.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D&&colourView.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2DMS)return;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthView;view->GetDesc(&depthView);
    if(depthView.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D&&depthView.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2DMS)return;
    ID3D11Resource* colourResource=nullptr;ID3D11Resource* depthResource=nullptr;targets[0]->GetResource(&colourResource);view->GetResource(&depthResource);
    if(!colourResource||!depthResource){release(colourResource);release(depthResource);return;}
    D3D11_RESOURCE_DIMENSION ct,dt;colourResource->GetType(&ct);depthResource->GetType(&dt);
    if(ct!=D3D11_RESOURCE_DIMENSION_TEXTURE2D||dt!=D3D11_RESOURCE_DIMENSION_TEXTURE2D){release(colourResource);release(depthResource);return;}
    D3D11_TEXTURE2D_DESC colour,depth;static_cast<ID3D11Texture2D*>(colourResource)->GetDesc(&colour);static_cast<ID3D11Texture2D*>(depthResource)->GetDesc(&depth);release(depthResource);
    const bool valid=colour.Width==unsigned(f.screen[0])&&colour.Height==unsigned(f.screen[1])&&colour.MipLevels==1&&colour.ArraySize==1&&colour.SampleDesc.Count>=1&&colour.SampleDesc.Count<=8&&depth.Width==colour.Width&&depth.Height==colour.Height&&depth.SampleDesc.Count==colour.SampleDesc.Count&&depth.ArraySize==1;
    if(!valid){release(colourResource);return;}
    dropScene(r);r->sceneFrame=current;r->sceneTarget=targets[0];r->sceneView=view;r->sceneResource=colourResource;r->sceneSamples=colour.SampleDesc.Count;r->sceneViewport={0,0,f.screen[0],f.screen[1],0,1};r->sceneTarget->AddRef();r->sceneView->AddRef();
}
extern "C" __declspec(noinline) void NativeGamePost(Request* r,IDXGISwapChain* chain,unsigned current){
    if(!r||r->busy)return;
    dropScene(r);
    if(!r->ready||!chain||r->lastPost==current)return;
    r->appliedStretch=1;
    Frame f;if(!frame(r,f))return;
    const bool stretch=f.water[1]>1.0001f;
    const bool glow=f.options[3]>=.5f&&f.bloom[2]>0;
    if(!stretch&&!glow)return;
    if(!glow)f.bloom[2]=0;
    ID3D11Texture2D* buffer=nullptr;const auto hr=chain->GetBuffer(0,r->textureInterface,reinterpret_cast<void**>(&buffer));if(FAILED(hr)||!buffer){++r->failures;return;}
    D3D11_TEXTURE2D_DESC d;buffer->GetDesc(&d);if(d.SampleDesc.Count!=1||d.MipLevels!=1||d.ArraySize!=1||d.Width<64||d.Height<64){release(buffer);return;}
    if(!r->colourCopy||r->width!=d.Width||r->height!=d.Height||r->format!=d.Format){release(r->colourSrv);release(r->colourCopy);r->width=d.Width;r->height=d.Height;r->format=d.Format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;d.MiscFlags=0;HRESULT h=r->device->CreateTexture2D(&d,nullptr,&r->colourCopy);if(SUCCEEDED(h))h=r->device->CreateShaderResourceView(r->colourCopy,nullptr,&r->colourSrv);r->error=h;if(FAILED(h)){++r->failures;release(buffer);return;}}
    ID3D11RenderTargetView* target=nullptr;if(FAILED(r->device->CreateRenderTargetView(buffer,nullptr,&target))||!r->colourCopy||!r->colourSrv){release(target);release(buffer);return;}
    auto* c=r->context;Saved s;r->busy=1;save(c,s);if(!s.valid){restore(c,s);r->busy=0;release(target);release(buffer);return;}
    ID3D11ShaderResourceView* empty[6]{};c->PSSetShaderResources(0,6,empty);c->OMSetRenderTargets(0,nullptr,nullptr);c->SetPredication(nullptr,FALSE);c->CopyResource(r->colourCopy,buffer);f.screen[0]=static_cast<float>(d.Width);f.screen[1]=static_cast<float>(d.Height);D3D11_VIEWPORT viewport{0,0,f.screen[0],f.screen[1],0,1};c->RSSetViewports(1,&viewport);
    bind(r,f,target,r->glow);c->PSSetShaderResources(1,1,&r->colourSrv);c->Draw(3,0);r->lastPost=current;++r->postCalls;
    if(stretch){r->lastStretch=current;r->appliedStretch=f.water[1];++r->stretchCalls;}
    restore(c,s);r->busy=0;release(target);release(buffer);
}
