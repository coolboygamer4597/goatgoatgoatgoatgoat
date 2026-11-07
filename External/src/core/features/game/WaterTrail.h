#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace WaterTrail {
struct Point {float x=0,y=0,z=0;};
struct Event {Point position{};double born=-100;};
inline bool Finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
class Trail {
public:
    std::array<Event,6> events{};
    void Reset(){events={};identity=0;ready=false;next=0;}
    void Update(std::uint64_t owner,Point position,Point foot,double now,float spacing){
        if(!owner||!Finite(position)||!Finite(foot)||!std::isfinite(now)){Reset();return;}
        if(!ready||owner!=identity||now<lastTime){Reset();identity=owner;previous=lastStep=position;lastTime=lastEmission=now;ready=true;return;}
        const float dx=position.x-previous.x,dy=position.y-previous.y,dz=position.z-previous.z;
        if(dx*dx+dy*dy+dz*dz<.000001f)return;
        const double dt=now-lastTime;previous=position;lastTime=now;
        if(dt<=0||dt>1||dx*dx+dy*dy+dz*dz>400){Reset();identity=owner;previous=lastStep=position;lastTime=lastEmission=now;ready=true;return;}
        if(std::abs(dy)/dt>3.5){lastStep=position;return;}
        const float sx=position.x-lastStep.x,sz=position.z-lastStep.z;
        if(sx*sx+sz*sz<std::max(.4f,spacing)*std::max(.4f,spacing)||now-lastEmission<.16)return;
        events[next]={foot,now};next=(next+1)%events.size();lastStep=position;lastEmission=now;
    }
    std::array<float,4> Sample(unsigned index,double now,float lifetime)const{
        const auto& e=events[index%events.size()];const double age=now-e.born;
        if(!ready||age<0||age>lifetime)return {0,0,0,-1};
        return {e.position.x,e.position.y,e.position.z,static_cast<float>(age)};
    }
private:
    std::uint64_t identity=0;bool ready=false;unsigned next=0;Point previous{},lastStep{};double lastTime=0,lastEmission=0;
};
}
