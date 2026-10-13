#pragma once
#include "PackedPreview.h"
#include "../core/app/RuntimePaths.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")

namespace PreviewCache {
inline std::string Key(const std::string& weapon,const std::string& skin){
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char c:weapon+" / "+skin){hash^=c;hash*=1099511628211ull;}
    std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return s.str();
}
inline std::filesystem::path Directory(){return RuntimePaths::File(L"preview-cache");}
inline std::vector<unsigned char> Read(const std::filesystem::path& file,std::size_t limit){
    std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f)return {};
    const auto size=f.tellg();if(size<=0||std::uint64_t(size)>limit)return {};
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    return f?std::move(bytes):std::vector<unsigned char>{};
}
template<class Vertex> bool LoadModelAt(const std::filesystem::path& directory,const std::string& weapon,const std::string& skin,std::vector<Vertex>& vertices){
    const auto key=Key(weapon,skin);auto reference=Read(directory/(key+".ref"),128);
    std::filesystem::path file=directory/(key+".bin");
    if(reference.size()==64&&std::all_of(reference.begin(),reference.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        file=directory/L"models"/(std::string(reference.begin(),reference.end())+".bin");
    auto data=Read(file,64*1024*1024);
    return PackedPreview::Decode(data.data(),data.size(),vertices);
}
template<class Vertex> bool LoadModel(const std::string& weapon,const std::string& skin,std::vector<Vertex>& vertices){return LoadModelAt(Directory(),weapon,skin,vertices);}
struct Image {std::vector<unsigned char> rgba;UINT width=0,height=0;bool valid()const{return width&&height&&rgba.size()==std::size_t(width)*height*4;}};
inline Image DecodeImage(const std::filesystem::path& path){
    Image result;
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE)return result;
    struct Cleanup{bool own;~Cleanup(){if(own)CoUninitialize();}}cleanup{SUCCEEDED(apartment)};
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> format;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
       FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
       FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&result.width,&result.height))||
       !result.width||!result.height||result.width>4096||result.height>4096||FAILED(factory->CreateFormatConverter(&format))||
       FAILED(format->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    result.rgba.resize(std::size_t(result.width)*result.height*4);
    if(FAILED(format->CopyPixels(nullptr,result.width*4,static_cast<UINT>(result.rgba.size()),result.rgba.data())))return {};
    return result;
}
inline Image LoadImage(const std::string& weapon,const std::string& skin){return DecodeImage(Directory()/(Key(weapon,skin)+".png"));}
}
