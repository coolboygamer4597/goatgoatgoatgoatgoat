#pragma once
#include "PreviewCache.h"
#include <d3d11.h>
#include <future>
#include <chrono>
#include <utility>
namespace PreviewCache {


class Thumbnail {
public:
    std::filesystem::path cacheDirectory;
    explicit Thumbnail(std::filesystem::path directory=Directory()):cacheDirectory(std::move(directory)){}
    std::future<PreviewCache::Image> pending;
    std::string requested,attempted,pendingKey,completed;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    UINT width=0,height=0;
    void Poll(ID3D11Device* device,const std::string& weapon,const std::string& skin){
        const auto key=PreviewCache::Key(weapon,skin);
        if(requested!=key){requested=key;srv.Reset();width=height=0;}
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto image=pending.get();
            if(pendingKey==requested)completed=requested;
            if(pendingKey==requested&&image.valid()){
                D3D11_TEXTURE2D_DESC td{};td.Width=image.width;td.Height=image.height;td.MipLevels=td.ArraySize=1;
                td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data{image.rgba.data(),image.width*4,0};Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
                if(SUCCEEDED(device->CreateTexture2D(&td,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&srv))){width=image.width;height=image.height;}
            }
        }
        if(!pending.valid()&&attempted!=requested){attempted=requested;pendingKey=requested;pending=std::async(std::launch::async,[weapon,skin,directory=cacheDirectory]{return DecodeImage(directory/(Key(weapon,skin)+".png"));});}
    }
    bool Unavailable()const{return completed==requested&&!srv;}
};
}
