#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace CustomShading {
inline float Level(float value){return std::isfinite(value)?std::clamp(std::round(value*100.f)/100.f,0.f,10.f):1.f;}
inline float Roughness(float rough,float strength){return strength<=1?1-(1-rough)*strength:rough/std::sqrt(strength);}
inline void Adjust(std::vector<unsigned char>& image,float value){
    const float strength=Level(value);if(strength==1)return;
    for(std::size_t at=0;at+3<image.size();at+=4){
        image[at]=static_cast<unsigned char>(std::clamp(std::round(image[at]*strength),0.f,255.f));
        image[at+1]=static_cast<unsigned char>(std::clamp(std::round(Roughness(image[at+1]/255.f,strength)*255.f),0.f,255.f));
    }
}
inline float ShadowLevel(float value){return std::isfinite(value)?std::clamp(std::round(value*100.f)/100.f,0.f,10.f):0.f;}
inline void Shadow(std::vector<unsigned char>& image,unsigned width,unsigned height,const std::vector<unsigned char>& mask,unsigned maskWidth,unsigned maskHeight,float value){
    const float strength=ShadowLevel(value);if(strength==0||!width||!height||!maskWidth||!maskHeight||image.size()!=static_cast<std::size_t>(width)*height*4||mask.size()!=static_cast<std::size_t>(maskWidth)*maskHeight*4)return;
    float factor[256];for(int i=0;i<256;++i)factor[i]=std::pow(i/255.f,strength/2.2f);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto at=(static_cast<std::size_t>(y)*width+x)*4;const auto lookup=(static_cast<std::size_t>(static_cast<std::uint64_t>(y)*maskHeight/height)*maskWidth+static_cast<std::uint64_t>(x)*maskWidth/width)*4;const float weight=factor[mask[lookup]];for(int c=0;c<3;++c)image[at+c]=static_cast<unsigned char>(std::clamp(std::round(image[at+c]*weight),0.f,255.f));}
}
}
