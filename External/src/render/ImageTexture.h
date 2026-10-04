#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
inline bool LoadPngTexture(ID3D11Device* device, const void* data, size_t size, ID3D11ShaderResourceView** out_srv) {
    using Microsoft::WRL::ComPtr;
    if (!device || !data || !size || !out_srv)
        return false;
    *out_srv = nullptr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
        return false;
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)))
        return false;
    if (FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<void*>(data)), static_cast<DWORD>(size))))
        return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)))
        return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)))
        return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;
    UINT width = 0, height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || !width || !height)
        return false;
    const UINT stride = width * 4;
    std::vector<BYTE> buffer(stride * height);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(buffer.size()), buffer.data())))
        return false;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = buffer.data();
    init.SysMemPitch = stride;
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &init, &texture)))
        return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = desc.Format;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), &srv, out_srv)))
        return false;
    return true;
}
