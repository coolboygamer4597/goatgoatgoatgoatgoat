#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

namespace NativeWorldDepth {
struct Snapshot {
    float view[16]{};
    float camera_range[4]{};
    float info[4]{};
};
static_assert(sizeof(Snapshot)==96);
struct Slot {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
    HANDLE handle=nullptr;
};
class Exchange {
public:
    std::array<Slot,3> slots{};
    unsigned width=0,height=0;
    bool Init(ID3D11Device* device,unsigned w,unsigned h) {
        if(width)return true;
        if(!device || !w || !h)return false;
        std::array<Slot,3> created{};
        D3D11_TEXTURE2D_DESC d{};
        d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R32_FLOAT;d.SampleDesc.Count=1;
        d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        d.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        for(auto& s:created) {
            Microsoft::WRL::ComPtr<IDXGIResource> resource;
            if(FAILED(device->CreateTexture2D(&d,nullptr,&s.texture)) ||
               FAILED(device->CreateRenderTargetView(s.texture.Get(),nullptr,&s.rtv)) ||
               FAILED(s.texture.As(&s.mutex)) || FAILED(s.texture.As(&resource)) ||
               FAILED(resource->GetSharedHandle(&s.handle)))return false;
        }
        slots=std::move(created);width=w;height=h;return true;
    }
    int AcquireWrite() {
        for(unsigned n=0;n<3;++n) {
            const unsigned i=(next+n)%3;
            if(slots[i].mutex && slots[i].mutex->AcquireSync(0,0)==S_OK) {
                next=(i+1)%3;return int(i);
            }
        }
        return -1;
    }
    void Reset(){slots={};width=height=next=0;}
private:
    unsigned next=0;
};
}
