#pragma once
#include <Windows.h>
#include <compressapi.h>
#include <cstdint>
#include <cstring>
#include <vector>
#pragma comment(lib,"cabinet.lib")

namespace PackedPreview {
template<class Vertex>
bool Decode(const void* bytes,std::size_t size,std::vector<Vertex>& output){
    output.clear();
    if(!bytes||size<=20)return false;
    std::uint32_t header[5];std::memcpy(header,bytes,sizeof(header));
    const auto count=header[3],unique=header[2];
    const std::uint64_t expected=std::uint64_t(unique)*sizeof(Vertex)+std::uint64_t(count)*4;
    if(header[0]!=0x31565047||header[1]!=sizeof(Vertex)||!unique||unique>count||!count||count%3||count>900000||expected!=header[4])return false;
    DECOMPRESSOR_HANDLE decoder{};
    if(!CreateDecompressor(COMPRESS_ALGORITHM_LZMS,nullptr,&decoder))return false;
    std::vector<unsigned char> raw(static_cast<std::size_t>(expected));SIZE_T written=0;
    const bool ok=Decompress(decoder,static_cast<const unsigned char*>(bytes)+20,size-20,raw.data(),raw.size(),&written)!=FALSE;
    CloseDecompressor(decoder);
    if(!ok||written!=raw.size())return false;
    output.resize(count);
    for(std::size_t i=0;i<count;++i){
        std::uint32_t index;std::memcpy(&index,raw.data()+std::size_t(unique)*sizeof(Vertex)+i*4,4);
        if(index>=unique){output.clear();return false;}
        std::memcpy(&output[i],raw.data()+std::size_t(index)*sizeof(Vertex),sizeof(Vertex));
    }
    return true;
}
}
