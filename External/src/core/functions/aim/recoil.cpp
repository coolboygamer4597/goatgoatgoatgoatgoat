#include "recoil.h"
#include "recoil_model.h"
#include "../../globals/globals.h"
#include "../../variables/variables.h"
#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include <chrono>
#include <string>

namespace Recoil {
namespace {
using Clock=std::chrono::steady_clock;
RecoilModel::Integrator integrator;
RecoilModel::PulseIntegrator pulse;
Clock::time_point previous{},nextResolve{};
std::uintptr_t character=0,root=0,humanoid=0,tool=0;
int stance=-1;
bool glock=false;
const char* status="Disabled";
bool Foreground(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return memory && pid==memory->get_process_id();}
bool Refresh() {
    const auto current=Globals::localPlayer.GetModelRef();
    if(!current.Addr){character=root=humanoid=tool=0;return false;}
    character=current.Addr;tool=0;glock=false;
    for(const auto& child:current.GetChildList()) {
        const auto cls=child.GetClass();
        if(cls!="Tool")continue;
        const auto name=child.GetName();
        if(name=="AR-15" || name=="Glock 19"){tool=child.Addr;glock=name=="Glock 19";break;}
    }
    const auto rp=current.FindChild("HumanoidRootPart");
    root=rp.GetPrimitivePtr();humanoid=current.FindChildByClass("Humanoid").Addr;
    return root && humanoid && tool;
}
}
void Stop(){const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;integrator.Reset(down);pulse.Reset(down);previous={};nextResolve={};character=root=humanoid=tool=0;stance=-1;glock=false;variables::Aimbot::recoilActive=false;status="Disabled";}
const char* Status(){return status;}
void Tick() {
    variables::Aimbot::recoilActive=false;
    const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    const auto now=Clock::now();
    const double dt=previous.time_since_epoch().count()?std::chrono::duration<double>(now-previous).count():0;
    previous=now;
    auto idle=[&](const char* message){integrator.Reset(down);pulse.Reset(down);status=message;};
    if(!variables::Aimbot::recoilEnabled){idle("Disabled");return;}
    if(!memory || !Globals::running || !memory->IsConnected()){idle("Paused: game disconnected");return;}
    if(variables::menuOpen){idle("Paused: menu open");return;}
    if(!Foreground()){idle("Paused: Roblox not focused");return;}
    std::uint64_t place=0;
    if(!memory->read_raw(Globals::dataModel.Addr+Offsets::DataModel::PlaceId,&place,sizeof(place)) || place!=4888256398ULL){idle("Available in the skin game");return;}
    if(now>=nextResolve || Globals::localPlayer.GetModelRef().Addr!=character) {
        nextResolve=now+std::chrono::milliseconds(50);
        if(!Refresh()){idle("Equip AR-15 or Glock 19");return;}
    }
    if(!tool || !root || !humanoid || RBX::RbxInstance(tool).GetParent().Addr!=character){idle("Equip AR-15 or Glock 19");return;}
    float health=0;
    if(!memory->read_raw(humanoid+Offsets::Humanoid::Health,&health,sizeof(health)) || !std::isfinite(health)){idle("Waiting for character health");return;}
    if(health<=0){idle("Paused: character dead");return;}
    auto camera=Globals::workspace.GetCurrentCamera();
    RBX::CFrame cf{},rf{};RBX::Mat4 projection{};
    if(!camera.Addr || !memory->read_raw(camera.Addr+Offsets::Camera::Rotation,&cf,sizeof(cf)) ||
       !memory->read_raw(root+Offsets::Primitive::Rotation,&rf,sizeof(rf)) ||
       !memory->read_raw(Globals::renderEngine.Addr+Offsets::VisualEngine::ViewMatrix,&projection,sizeof(projection)) ||
       !RecoilModel::RotationValid(cf.data) || !RecoilModel::RotationValid(rf.data)){idle("Waiting for camera");return;}
    stance=RecoilModel::Stance(cf.data[10]-rf.data[10],stance);
    const float scale=RecoilModel::Scale(stance,RecoilModel::ProjectionFov(projection.data));
    if(scale<=0){idle("Waiting for stance");return;}
    const float chosenStrength=glock?variables::Aimbot::recoilGlockStrength:variables::Aimbot::recoilStrength;
    const float chosenPull=glock?variables::Aimbot::recoilGlockPull:variables::Aimbot::recoilPull;
    const float strength=std::isfinite(chosenStrength)?std::clamp(chosenStrength,0.f,1.f):0.f;
    const float pull=std::isfinite(chosenPull)?std::clamp(chosenPull,0.f,glock?160.f:1200.f):0.f;
    const bool reload=(GetAsyncKeyState('R')&0x8000)!=0;
    const auto identity=tool ^ (camera.Addr<<1) ^ (character<<2);
    int counts=0;
    if(glock) {
        integrator.Reset(down);
        counts=pulse.Step(dt,true,down,strength*pull*scale,identity,reload);
        status=pulse.requireRelease?"Release shoot to resume":pulse.remaining>0?"Dampening: Glock pulse":down?"Glock: waiting for next click":"Ready: click Left Mouse";
        variables::Aimbot::recoilActive=pulse.remaining>0 && !pulse.requireRelease && strength>0 && pull>0;
    } else {
        pulse.Reset(down);
        counts=integrator.Step(dt,true,down,strength*pull*scale,identity,reload);
        status=down?(integrator.requireRelease?"Release shoot to resume":integrator.held>3.5?"Release shoot after the burst":"Dampening: AR-15"):"Ready: hold Left Mouse";
        variables::Aimbot::recoilActive=down && !integrator.requireRelease && integrator.held<=3.5 && strength>0 && pull>0;
    }
    if(reload)status="Paused: reload key held";
    if(!counts)return;
    if(variables::menuOpen || !Foreground() || (!glock && !(GetAsyncKeyState(VK_LBUTTON)&0x8000))){idle("Paused: input interrupted");variables::Aimbot::recoilActive=false;return;}
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dy=counts;input.mi.dwFlags=MOUSEEVENTF_MOVE;
    if(SendInput(1,&input,sizeof(input))!=1){idle("Mouse input unavailable");variables::Aimbot::recoilActive=false;}
}
}
