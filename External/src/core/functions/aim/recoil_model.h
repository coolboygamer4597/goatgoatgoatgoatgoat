#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace RecoilModel {
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
    if(stance<0 || stance>2 || !std::isfinite(fov) || fov<45 || fov>80)return 0;
    const float rates[3][2]={{6.5f,4.f},{4.f,2.1f},{2.2f,2.1f}};
    const float scoped=std::clamp((70.f-fov)/10.f,0.f,1.f);
    return (rates[stance][0]*(1.f-scoped)+rates[stance][1]*scoped)/6.5f;
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
}
