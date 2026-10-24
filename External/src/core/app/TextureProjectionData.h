#pragma once
#include <algorithm>
#include <array>
#include <string_view>
namespace TextureProjectionData {
inline constexpr int Mode=32;
struct Binding{std::string_view mesh;std::array<float,12> rows;};
inline constexpr std::array<Binding,9> DeagleBindings{{
    {"17788240085",{0.0f,1.0f,0.0f,0.3374785334f,1.0f,0.0f,0.0f,6.484985352e-05f,0.0f,0.0f,-1.0f,-0.1046905816f}},
    {"5165152169",{0.0f,1.0f,0.0f,0.289154008f,1.0f,0.0f,0.0f,0.002723693848f,0.0f,0.0f,-1.0f,0.5145566761f}},
    {"5165153537",{0.0f,1.0f,0.0f,0.01036071777f,1.0f,0.0f,0.0f,-0.0005493164062f,0.0f,0.0f,-1.0f,0.3430326879f}},
    {"5165062009",{1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f}},
    {"5160693646",{0.0f,1.0f,0.0f,0.2074584812f,1.0f,0.0f,0.0f,0.08166885376f,0.0f,0.0f,-1.0f,0.2395781577f}},
    {"5165147746",{0.0f,1.0f,0.0f,-0.03974914551f,1.0f,0.0f,0.0f,0.003253936768f,0.0f,0.0f,-1.0f,0.382232517f}},
    {"5160693225",{0.0f,1.0f,0.0f,0.04139707983f,1.0f,0.0f,0.0f,0.06478881836f,0.0f,0.0f,-1.0f,0.1791457236f}},
    {"5160689305",{0.0f,1.0f,0.0f,0.113815289f,1.0f,0.0f,0.0f,0.0005302429199f,0.0f,0.0f,-1.0f,0.08113852143f}},
    {"5160695315",{0.0f,1.0f,0.0f,0.2156371772f,1.0f,0.0f,0.0f,-0.06969070435f,0.0f,0.0f,-1.0f,-0.01905822754f}}
}};
inline constexpr std::array<float,4> DeagleMinimum{-0.3299518228f,-0.1028451994f,-0.7477449477f,0.0f};
inline constexpr std::array<float,4> DeagleInverseSpan{1.254672411f,4.858611998f,0.7114900776f,0.0f};
inline constexpr std::array<float,12> DeaglePreview{-0.1187020688f,0.9684091234f,0.002535937067f,0.06855857372f,-0.005224515754f,0.001914492313f,-0.9756443417f,6.484985352e-05f,-0.9683983507f,-0.118713718f,0.004952763761f,-0.04499445856f};
inline bool Deagle(std::string_view mesh,float* rows){for(const auto& binding:DeagleBindings)if(binding.mesh==mesh){std::copy(binding.rows.begin(),binding.rows.end(),rows);return true;}return false;}
}
