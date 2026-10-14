#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <cstring>
#include <string_view>

namespace RecoilModel {
inline int ParseAmmo(std::string_view text) {
    auto spaces = [&] { while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1); };
    auto number = [&] {
        int value = 0, digits = 0;
        while (!text.empty() && text.front() >= '0' && text.front() <= '9') {
            if (++digits > 4) return -1;
            value = value * 10 + text.front() - '0'; text.remove_prefix(1);
        }
        return digits && value <= 1000 ? value : -1;
    };
    spaces(); const int ammo = number(); if (ammo < 0) return -1; spaces();
    if (text.empty()) return ammo;
    if (text.front() != '/') return -1;
    text.remove_prefix(1); spaces(); if (number() < 0) return -1; spaces();
    return text.empty() ? ammo : -1;
}
template<class Read>
inline int ReadAmmo(std::uint64_t field, Read read) {
    std::array<unsigned char, 32> header{};
    if (!field || !read(field, header.data(), header.size())) return -1;
    std::uint64_t size = 0, capacity = 0;
    std::memcpy(&size, header.data() + 16, 8); std::memcpy(&capacity, header.data() + 24, 8);
    if (!size || size > 32 || capacity < size || capacity > 0x100000 || (capacity < 16 && size > 15)) return -1;
    std::array<char, 32> text{};
    if (capacity < 16) std::memcpy(text.data(), header.data(), static_cast<std::size_t>(size));
    else {
        std::uint64_t pointer = 0; std::memcpy(&pointer, header.data(), 8);
        if (pointer < 0x10000 || pointer >= 0x7fffffffffffULL || !read(pointer, text.data(), static_cast<std::size_t>(size))) return -1;
    }
    return ParseAmmo(std::string_view(text.data(), static_cast<std::size_t>(size)));
}
template<class Read>
inline std::uint64_t FindAmmoField(std::uint64_t label, Read read) {
    std::array<unsigned char, 0x1000> object{};
    if (!label || !read(label, object.data(), object.size())) return 0;
    auto captured = [&](std::uint64_t address, void* out, std::size_t size) {
        if (address >= label && address - label <= object.size() && size <= object.size() - (address - label)) {
            std::memcpy(out, object.data() + (address - label), size); return true;
        }
        return read(address, out, size);
    };
    for (std::size_t offset = 0x80; offset + 32 <= object.size(); offset += 8)
        if (ReadAmmo(label + offset, captured) >= 0) return label + offset;
    return 0;
}
inline bool RotationValid(const float* frame) {
    for(int i=0;i<12;++i)if(!std::isfinite(frame[i]))return false;
    for(int r=0;r<3;++r) {
        float length=0;for(int c=0;c<3;++c)length+=frame[r*3+c]*frame[r*3+c];
        if(std::abs(length-1.f)>.08f)return false;
        for(int k=0;k<r;++k){float dot=0;for(int c=0;c<3;++c)dot+=frame[r*3+c]*frame[k*3+c];if(std::abs(dot)>.08f)return false;}
    }
    return true;
}
inline int Stance(float height,int previous) {
    if(!std::isfinite(height) || height < -4 || height > 4)return -1;
    if(previous==0 && height>.75f)return 0;
    if(previous==1 && height>-.65f && height<1.f)return 1;
    if(previous==2 && height<-.45f)return 2;
    return height>.9f?0:height<-.55f?2:1;
}
inline float ProjectionFov(const float* m) {
    float y=0,w=0,dot=0;
    for(int i=0;i<16;++i)if(!std::isfinite(m[i]))return 0;
    for(int i=0;i<3;++i){y+=m[4+i]*m[4+i];w+=m[12+i]*m[12+i];dot+=m[4+i]*m[12+i];}
    if(y<1e-8f || w<1e-8f || std::abs(dot/std::sqrt(y*w))>.08f)return 0;
    return 2.f*std::atan(std::sqrt(w/y))*57.295779513f;
}
inline float Scale(int stance,float fov) {
    if(stance<0 || stance>2 || !std::isfinite(fov) || fov<=1 || fov>=179)return 0;
    const float rates[3][2]={{6.5f,4.f},{4.f,2.1f},{2.2f,2.1f}};
    const float scoped=std::clamp((70.f-fov)/10.f,0.f,1.f);
    return (rates[stance][0]*(1.f-scoped)+rates[stance][1]*scoped)/6.5f;
}
inline float ArScale(int stance,float fov) {
    return Scale(stance,fov)*(stance==0?1.05f:1.f);
}
inline float GlockScale(int stance,float fov,bool scoped) {
    const float scale=Scale(stance,fov);
    if(!scoped || stance<0 || stance>2)return scale;
    const float multiplier[3]={.6f,.55f,1.f};
    return scale*multiplier[stance];
}
struct Integrator {
    double remainder=0,held=0;
    bool requireRelease=true,wasDown=false;
    void Reset(bool down) {remainder=held=0;requireRelease=down;wasDown=down;}
    int Step(double dt,bool allowed,bool down,double rate,std::uint64_t identity,bool reload=false) {
        if(!std::isfinite(dt) || dt<=0 || dt>.1 || !std::isfinite(rate) || rate<0 || rate>2000 || !allowed || reload || identity!=identity_) {
            identity_=identity;Reset(down);return 0;
        }
        if(!down){Reset(false);return 0;}
        if(requireRelease)return 0;
        if(!wasDown){held=0;remainder=0;wasDown=true;return 0;}
        held+=dt;
        if(held>3.5){remainder=0;return 0;}
        remainder+=rate*dt;
        const int counts=static_cast<int>(std::floor(remainder));
        remainder-=counts;
        return counts;
    }
private:
    std::uint64_t identity_=0;
};
struct PulseIntegrator {
    double pending=0,remaining=0,remainder=0;
    bool requireRelease=true,wasDown=false;
    void Reset(bool down){pending=remaining=remainder=0;requireRelease=down;wasDown=down;}
    int Step(double dt,bool allowed,bool down,double amount,std::uint64_t identity,bool reload=false) {
        if(!std::isfinite(dt) || dt<=0 || dt>.1 || !std::isfinite(amount) || amount<0 || amount>160 ||
            !allowed || reload || identity!=identity_) {
            identity_=identity;Reset(down);return 0;
        }
        if(requireRelease){if(!down)Reset(false);return 0;}
        if(down && !wasDown && amount>0){pending=std::min(pending+amount,320.);remaining=.12;}
        wasDown=down;
        if(remaining<=0 || pending<=0){pending=remaining=0;return 0;}
        const double interval=std::min(dt,remaining);
        const double movement=pending*interval/remaining;
        pending=std::max(0.,pending-movement);remaining=std::max(0.,remaining-interval);
        remainder+=movement;
        const int counts=static_cast<int>(std::floor(remainder));remainder-=counts;
        return counts;
    }
private:
    std::uint64_t identity_=0;
};
struct Horizontal {
    struct Calibration {
        std::array<double,12> x{},y{};
        int count=0,index=0;
        double gain=0,elapsed=0,dx=0,angle=0;
        void Bin(double input,double output) {
            if(std::abs(input)<8 || std::abs(output)<.04)return;
            if(gain && std::abs(output-gain*input)>.12+.1*std::abs(output))gain=0;
            x[index]=input;y[index]=output;index=(index+1)%12;count=(std::min)(count+1,12);
            if(count<8)return;
            double mx=0,my=0;for(int i=0;i<count;++i){mx+=x[i];my+=y[i];}mx/=count;my/=count;
            double xx=0,yy=0,xy=0;for(int i=0;i<count;++i){xx+=(x[i]-mx)*(x[i]-mx);yy+=(y[i]-my)*(y[i]-my);xy+=(x[i]-mx)*(y[i]-my);}
            if(xx<1 || yy<.001)return;
            const double next=xy/xx;
            gain=xy*xy/(xx*yy)>.995 && std::abs(next)>.001 && std::abs(next)<.25?next:0;
        }
    };
    std::array<Calibration,4> calibration{};
    double previousYaw=0,target=0,remainder=0;
    std::uint64_t identity=0;
    int profile=-1;
    bool valid=false,active=false;
    static double Wrap(double angle){return std::remainder(angle,360.);}
    void Reset(){valid=false;active=false;remainder=0;for(auto& c:calibration){c.elapsed=c.dx=c.angle=0;}}
    int Step(double dt,double yaw,double dx,int selected,std::uint64_t id,bool trusted,bool measure,bool candidate,bool correct,double strength) {
        active=false;
        if(!trusted || !std::isfinite(dt) || dt<=0 || dt>.1 || !std::isfinite(yaw) || !std::isfinite(dx) || std::abs(dx)>10000 || selected<0 || selected>3){Reset();return 0;}
        if(!valid || profile!=selected || identity!=id){Reset();valid=true;profile=selected;identity=id;target=previousYaw=yaw;return 0;}
        const double delta=Wrap(yaw-previousYaw);previousYaw=yaw;
        auto& c=calibration[selected];
        if(measure){c.elapsed+=dt;c.dx+=dx;c.angle+=delta;if(c.elapsed>=.12){c.Bin(c.dx,c.angle);c.elapsed=c.dx=c.angle=0;}}
        else c.elapsed=c.dx=c.angle=0;
        if(!candidate || c.gain==0){target=yaw;remainder=0;return 0;}
        target=Wrap(target+c.gain*dx);
        const double error=Wrap(target-yaw);
        if(!std::isfinite(error) || std::abs(error)>2){target=yaw;remainder=0;return 0;}
        if(!correct || !std::isfinite(strength) || strength<=0){remainder=0;return 0;}
        active=true;
        if(std::abs(error)<std::abs(c.gain)*.5){remainder=0;return 0;}
        if(std::abs(c.gain*dx)>std::abs(error)*2)return 0;
        const double limit=std::clamp(400*dt,2.,8.);
        const double movement=std::clamp(error/c.gain*.35*std::clamp(strength,0.,1.),-limit,limit)+remainder;
        const int counts=static_cast<int>(movement);remainder=movement-counts;return counts;
    }
};
}
