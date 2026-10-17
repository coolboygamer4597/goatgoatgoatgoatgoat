#include "recoil.h"
#include "recoil_model.h"
#include "../../globals/globals.h"
#include "../../variables/variables.h"
#include "../../../memory/memory.h"
#include "../../../sdk/RuntimeOffsets.h"
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic>

namespace Recoil {
namespace {
using Clock=std::chrono::steady_clock;
class RawObserver {
    struct Move{int x;ULONGLONG time;};
    std::thread worker;
    std::atomic<bool> quit{false},ready{false},deviceSeen{false};
    std::atomic<DWORD> threadId{0},gamePid{0};
    std::atomic<long long> physicalX{0};
    std::atomic<bool> physicalLeft{false},buttonKnown{false};
    std::atomic<ULONGLONG> bad{0};
    std::mutex mutex;
    std::deque<Move> pending;
    static LRESULT CALLBACK Window(HWND window,UINT message,WPARAM wp,LPARAM lp){
        if(message==WM_NCCREATE)SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        auto self=reinterpret_cast<RawObserver*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_INPUT && self){
            RAWINPUT input{};UINT size=sizeof(input);
            if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&input,&size,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1) && input.header.dwType==RIM_TYPEMOUSE){
                const auto& mouse=input.data.mouse;
                const bool hardware=input.header.hDevice && mouse.ulExtraInformation!=0x52434c31;
                if(hardware){
                    if(mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_DOWN){self->physicalLeft=true;self->buttonKnown=true;}
                    if(mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_UP){self->physicalLeft=false;self->buttonKnown=true;}
                }
                DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
                if(pid==self->gamePid.load()){
                    if(mouse.usFlags&MOUSE_MOVE_ABSOLUTE)self->bad=GetTickCount64();
                    else if(hardware){self->deviceSeen=true;self->physicalX.fetch_add(mouse.lLastX);}
                    else if(mouse.lLastX){
                        std::lock_guard<std::mutex> lock(self->mutex);
                        int remaining=mouse.lLastX;
                        while(remaining && !self->pending.empty()){
                            auto& move=self->pending.front();
                            if((remaining>0)!=(move.x>0))break;
                            const int used=(std::min)(std::abs(remaining),std::abs(move.x))*(remaining>0?1:-1);
                            remaining-=used;move.x-=used;if(!move.x)self->pending.pop_front();
                        }
                        if(remaining){self->pending.clear();self->bad=GetTickCount64();}
                    }
                }
            }
        }
        return DefWindowProcW(window,message,wp,lp);
    }
    void Run(){
        MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);threadId=GetCurrentThreadId();
        WNDCLASSW wc{};wc.lpfnWndProc=Window;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"RecoilMouseObserver";
        if(!RegisterClassW(&wc))return;
        HWND window=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,this);
        RAWINPUTDEVICE registration{1,2,RIDEV_INPUTSINK,window};
        if(window && RegisterRawInputDevices(&registration,1,sizeof(registration))){
            ready=true;
            while(!quit && GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
            ready=false;RAWINPUTDEVICE remove{1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&remove,1,sizeof(remove));
        }
        if(window)DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    }
public:
    ~RawObserver(){Stop();}
    bool Connected()const{return ready.load();}
    bool PhysicalLeft()const{return physicalLeft.load();}
    bool ButtonReady()const{return ready&&buttonKnown;}
    void Start(DWORD pid){gamePid=pid;if(!worker.joinable()){physicalLeft=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;buttonKnown=false;quit=false;try{worker=std::thread([this]{Run();});}catch(...){ready=false;quit=true;}}}
    void Stop(){quit=true;if(threadId)PostThreadMessageW(threadId.load(),WM_QUIT,0,0);if(worker.joinable())worker.join();threadId=0;ready=false;deviceSeen=false;physicalX=0;buttonKnown=false;physicalLeft=false;std::lock_guard<std::mutex> lock(mutex);pending.clear();}
    void Expect(int x){if(!x)return;std::lock_guard<std::mutex> lock(mutex);pending.push_back({x,GetTickCount64()});}
    bool Take(long long& dx){
        dx=physicalX.exchange(0);const auto now=GetTickCount64();
        {std::lock_guard<std::mutex> lock(mutex);if(!pending.empty() && now-pending.front().time>250){pending.clear();bad=now;}}
        return ready && deviceSeen && (!bad || now-bad.load()>500);
    }
};
RawObserver observer;
RecoilModel::AutoClick automatic;
bool ownedClickDown=false;
std::atomic<bool> automaticHolding{false};
bool Click(int edge){
    if(!edge)return true;
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=edge>0?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;input.mi.dwExtraInfo=0x52434c31;
    if(SendInput(1,&input,sizeof(input))!=1)return false;
    ownedClickDown=edge>0;return true;
}
void StopAutomatic(){automaticHolding=false;const auto edge=automatic.Stop(observer.PhysicalLeft());if(edge||ownedClickDown)Click(-1);}
RecoilModel::Horizontal horizontal;
std::array<float,4> horizontalFov{};
Clock::time_point lastShot{};
int previousAmmo=-1;
std::uintptr_t shotIdentity=0;
const char* horizontalStatus="Horizontal: calibrating";
RecoilModel::Integrator integrator;
RecoilModel::PulseIntegrator pulse;
Clock::time_point previous{},nextResolve{};
Clock::time_point nextAmmoPoll{},nextAmmoResolve{};
std::uintptr_t ammoField=0,ammoIdentity=0;
int currentAmmo=-1;
std::uintptr_t character=0,root=0,humanoid=0,tool=0;
int stance=-1;
bool glock=false;
const char* status="Disabled";
bool Foreground(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return memory && pid==memory->get_process_id();}
int Ammo(Clock::time_point now,std::uintptr_t identity) {
    if(identity!=ammoIdentity){ammoIdentity=identity;ammoField=0;currentAmmo=-1;nextAmmoPoll=nextAmmoResolve={};}
    if(now<nextAmmoPoll)return currentAmmo;
    nextAmmoPoll=now+std::chrono::milliseconds(20);
    auto read=[](std::uint64_t address,void* out,std::size_t size){return memory->read_raw(address,out,size);};
    if(now>=nextAmmoResolve){
        nextAmmoResolve=now+std::chrono::seconds(1);
        const auto gui=Globals::localPlayer.FindChildByClass("PlayerGui");
        const auto label=gui.FindChild("CarbonEngineUI").FindChild("MainFrame").FindChild("Ammo");
        const auto next=label.Addr && label.GetClass()=="TextLabel"?RecoilModel::FindAmmoField(label.Addr,read):0;
        ammoField=next;
    }
    currentAmmo=ammoField?RecoilModel::ReadAmmo(ammoField,read):-1;
    if(currentAmmo<0){ammoField=0;nextAmmoResolve=now;}
    return currentAmmo;
}
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
void Stop(){StopAutomatic();observer.Stop();horizontal.Reset();lastShot={};previousAmmo=-1;shotIdentity=0;const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;integrator.Reset(down);pulse.Reset(down);previous={};nextResolve={};nextAmmoPoll=nextAmmoResolve={};ammoField=ammoIdentity=0;currentAmmo=-1;character=root=humanoid=tool=0;stance=-1;glock=false;variables::Aimbot::recoilActive=false;status="Disabled";}
bool AutomaticHeld(){return automaticHolding&&observer.PhysicalLeft();}
const char* Status(){return status;}
const char* HorizontalStatus(){return horizontalStatus;}
void Tick() {
    variables::Aimbot::recoilActive=false;
    bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    const auto now=Clock::now();
    const double dt=previous.time_since_epoch().count()?std::chrono::duration<double>(now-previous).count():0;
    previous=now;
    long long physicalDx=0;const bool rawReady=observer.Take(physicalDx);
    auto idle=[&](const char* message){StopAutomatic();integrator.Reset(down);pulse.Reset(down);horizontal.Reset();lastShot={};previousAmmo=-1;status=message;};
    if(!variables::Aimbot::recoilEnabled&&!variables::Aimbot::glockAutomatic){idle("Disabled");return;}
    if(!memory || !Globals::running || !memory->IsConnected()){idle("Paused: game disconnected");return;}
    if(variables::menuOpen){idle("Paused: menu open");return;}
    if(!Foreground()){idle("Paused: Roblox not focused");return;}
    if(variables::Aimbot::recoilHorizontal||variables::Aimbot::glockAutomatic)observer.Start(memory->get_process_id());
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
    const auto ammo=Ammo(now,tool ^ (character<<1));
    if(ammo<0){idle("Waiting for ammo display");return;}
    if(ammo==0){idle("Paused: magazine empty");return;}
    const auto autoIdentity=tool ^ (character<<1);
    const bool autoAllowed=variables::Aimbot::glockAutomatic&&glock&&observer.ButtonReady()&&!(GetAsyncKeyState('R')&0x8000);
    const auto edge=automatic.Step(std::chrono::duration<double>(now.time_since_epoch()).count(),autoAllowed,observer.PhysicalLeft(),variables::Aimbot::glockClickDelay,autoIdentity);
    if(!Click(edge)){StopAutomatic();idle("Mouse input unavailable");return;}
    automaticHolding=autoAllowed&&automatic.active;
    if(autoAllowed&&automatic.active)down=automatic.outputDown;
    if(!variables::Aimbot::recoilEnabled){integrator.Reset(down);pulse.Reset(down);horizontal.Reset();lastShot={};previousAmmo=-1;status="Disabled";return;}
    const auto currentShotIdentity=tool ^ (character<<1);
    if(shotIdentity!=currentShotIdentity){shotIdentity=currentShotIdentity;previousAmmo=ammo;lastShot={};horizontal.Reset();}
    if(previousAmmo>=0 && ammo<previousAmmo && (down || (glock && pulse.remaining>0)))lastShot=now;
    previousAmmo=ammo;
    auto camera=Globals::workspace.GetCurrentCamera();
    RBX::CFrame cf{},rf{};RBX::Mat4 projection{};
    if(!camera.Addr || !memory->read_raw(camera.Addr+Offsets::Camera::Rotation,&cf,sizeof(cf)) ||
       !memory->read_raw(root+Offsets::Primitive::Rotation,&rf,sizeof(rf)) ||
       !memory->read_raw(Globals::renderEngine.Addr+Offsets::VisualEngine::ViewMatrix,&projection,sizeof(projection)) ||
       !RecoilModel::RotationValid(cf.data) || !RecoilModel::RotationValid(rf.data)){idle("Waiting for camera");return;}
    stance=RecoilModel::Stance(cf.data[10]-rf.data[10],stance);
    const float fov=RecoilModel::ProjectionFov(projection.data);
    const float scale=glock?RecoilModel::GlockScale(stance,fov,(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0):RecoilModel::ArScale(stance,fov);
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
    int sideways=0;
    const int profile=(glock?2:0)+((GetAsyncKeyState(VK_RBUTTON)&0x8000)?1:0);
    if(horizontalFov[profile] && std::abs(horizontalFov[profile]-fov)>2){horizontal.calibration[profile]={};horizontal.Reset();}
    horizontalFov[profile]=fov;
    const bool recentShot=lastShot.time_since_epoch().count() && now-lastShot<std::chrono::milliseconds(200);
    const bool candidate=down || recentShot;
    const bool eligible=variables::Aimbot::recoilHorizontal && stance==0 && !reload && std::abs(cf.data[5])<.98f;
    const double yaw=std::atan2(-static_cast<double>(cf.data[2]),-static_cast<double>(cf.data[8]))*57.295779513;
    const bool canCorrect=recentShot && (glock?!pulse.requireRelease:!integrator.requireRelease && integrator.held<=3.5) && strength>0 && pull>0;
    sideways=horizontal.Step(dt,yaw,static_cast<double>(physicalDx),profile,identity,rawReady&&eligible,!candidate,candidate,canCorrect,strength);
    horizontalStatus=!variables::Aimbot::recoilHorizontal?"Horizontal: off":stance!=0?"Horizontal: standing only":!rawReady?"Horizontal: waiting for mouse input":!horizontal.calibration[profile].gain?"Move left/right briefly without firing to calibrate.":horizontal.active?"Horizontal: stabilizing":"Horizontal: ready";
    if(!counts && !sideways)return;
    if(variables::menuOpen || !Foreground()){idle("Paused: input interrupted");variables::Aimbot::recoilActive=false;return;}
    if(!glock && !(GetAsyncKeyState(VK_LBUTTON)&0x8000))counts=0;
    if(!counts && !sideways)return;
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dx=sideways;input.mi.dy=counts;input.mi.dwFlags=MOUSEEVENTF_MOVE;input.mi.dwExtraInfo=0x52434c31;
    observer.Expect(sideways);
    if(SendInput(1,&input,sizeof(input))!=1){idle("Mouse input unavailable");variables::Aimbot::recoilActive=false;}
}
}
