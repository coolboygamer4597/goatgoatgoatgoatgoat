#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
namespace AimVisibility {
struct Sample {
    unsigned token=0,frame=0,status=0;
    float point[4]{},camera[4]{},forward[4]{};
};
inline bool Finite(const float* p){return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);}
inline float DistanceSquared(const float* a,const float* b){float d=0;for(int i=0;i<3;++i)d+=(a[i]-b[i])*(a[i]-b[i]);return d;}
class Controller {
public:
    unsigned token=0;float point[4]{};double requestedAt=0;
    void Reset(){identity=0;pending=false;cached={};token=0;}
    bool Check(std::uint64_t target,const float* current,const float* camera,const float* forward,double now,unsigned frame,const Sample* reply,float* proven){
        if(!target||!Finite(current)||!Finite(camera)||!Finite(forward)||!std::isfinite(now)){Reset();return false;}
        if(identity!=target){identity=target;pending=false;cached={};}
        if(reply&&pending&&reply->token==token){cached=*reply;cachedAt=requestedAt;pending=false;}
        const bool fresh=cached.token&&cached.status==1&&now>=cachedAt&&now-cachedAt<=.075&&unsigned(frame-cached.frame)<=32;
        const float facing=cached.forward[0]*forward[0]+cached.forward[1]*forward[1]+cached.forward[2]*forward[2];
        const bool clear=fresh&&Finite(cached.point)&&Finite(cached.camera)&&Finite(cached.forward)&&DistanceSquared(cached.point,current)<=1.f&&DistanceSquared(cached.camera,camera)<=.0225f&&facing>=.9998f;
        if(clear)std::memcpy(proven,cached.point,12);
        if(!pending||now<requestedAt||now-requestedAt>.1){token=++serial;if(token>1000000){serial=token=1;}std::memcpy(point,current,12);point[3]=static_cast<float>(token);requestedAt=now;pending=true;}
        return clear;
    }
private:
    std::uint64_t identity=0;unsigned serial=0;bool pending=false;Sample cached{};double cachedAt=0;
};
}
