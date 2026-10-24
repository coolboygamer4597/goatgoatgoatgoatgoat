#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <fstream>
#include <filesystem>
namespace CustomModelData {
struct Vertex {float position[3],normal[3],uv[2];std::uint32_t part;};
struct Mesh {std::vector<Vertex> vertices;};
inline bool Present(const std::filesystem::path& path){
    try{std::ifstream f(path,std::ios::binary);std::array<unsigned char,16> bytes{};if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||std::memcmp(bytes.data(),"GWM1",4))return false;std::uint32_t h[3]{};std::memcpy(h,bytes.data()+4,12);return h[0]==1&&h[1]>=3&&h[1]<=300000&&h[1]%3==0&&h[2]==sizeof(Vertex)&&std::filesystem::file_size(path)==16+std::size_t(h[1])*sizeof(Vertex);}catch(...){return false;}
}
inline bool Decode(const std::vector<unsigned char>& bytes,Mesh& output){
    if(bytes.size()<16||std::memcmp(bytes.data(),"GWM1",4))return false;
    std::uint32_t header[3]{};std::memcpy(header,bytes.data()+4,12);
    if(header[0]!=1||header[1]<3||header[1]>300000||header[1]%3||header[2]!=sizeof(Vertex)||bytes.size()!=16+std::size_t(header[1])*sizeof(Vertex))return false;
    Mesh mesh;mesh.vertices.resize(header[1]);std::memcpy(mesh.vertices.data(),bytes.data()+16,mesh.vertices.size()*sizeof(Vertex));
    for(const auto& v:mesh.vertices){if(v.part>3)return false;float norm=0;for(int k=0;k<3;++k){if(!std::isfinite(v.position[k])||std::abs(v.position[k])>30||!std::isfinite(v.normal[k]))return false;norm+=v.normal[k]*v.normal[k];}if(norm<.1f||norm>2)return false;for(float uv:v.uv)if(!std::isfinite(uv)||std::abs(uv)>128)return false;}
    output=std::move(mesh);return true;
}
inline bool Load(const std::filesystem::path& path,Mesh& output){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return false;const auto size=f.tellg();if(size<16||size>16+300000*sizeof(Vertex))return false;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return false;return Decode(bytes,output);
}
}
