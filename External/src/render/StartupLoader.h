#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
#include <cmath>
#include <string>
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_impl_win32.h"
#include "../../ext/imgui/imgui_impl_dx11.h"
#include "menu/UiText.h"
#include "RetroUi.h"
#include "LaunchPrivacy.h"
#include "CaptureProtection.h"
#include <future>
#include <functional>
#pragma comment(lib,"d3d11.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace StartupLoader {
inline bool cancelled=false;
inline std::wstring startupFailure;
inline const std::wstring& Error(){return startupFailure;}
inline HWND loaderWindow=nullptr;
inline bool retained=false;
inline void Dismiss(){if(loaderWindow){DestroyWindow(loaderWindow);loaderWindow=nullptr;UnregisterClassW(L"GoatSunsetLoader",GetModuleHandleW(nullptr));}retained=false;}
inline ID3D11ShaderResourceView* sunImage=nullptr;
inline float uiScale=1.75f;
inline ImVec2 Scaled(float x,float y){return ImVec2(x*uiScale,y*uiScale);}
struct LoadState {
 static constexpr double MinimumSeconds=3.4;
 bool loading=false;double started=0,preparedAt=-1;float actualProgress=-1;
 void Update(bool detected,bool click,double now){
  if(!detected)loading=false;
  else if(click&&!loading){loading=true;started=now;preparedAt=-1;actualProgress=-1;}
 }
 void Prepared(double now){actualProgress=1.f;preparedAt=now;}
 float Progress(double now)const{
  if(!loading)return 0;
  const double t=(std::max)(0.0,(std::min)(1.0,(now-started)/MinimumSeconds));

  float shown=static_cast<float>(t-.06*std::sin(t*6.283185307)+.012*std::sin(t*18.849555922));
  if(actualProgress>=0 && actualProgress<1)return (std::min)(shown,actualProgress);
  if(preparedAt>=0){
   const float finish=static_cast<float>((std::max)(0.0,(std::min)(1.0,(now-preparedAt)/.5)));
   shown=(std::min)(shown,.88f+.12f*finish*finish*(3.f-2.f*finish));
  }
  return (std::max)(0.f,(std::min)(1.f,shown));
 }
 bool Ready(double now)const{return loading && now-started>=MinimumSeconds && Progress(now)>=.9999f;}
};
inline bool RobloxRunning(){
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return false;
 PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
 if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"RobloxPlayerBeta.exe")==0){found=true;break;}}while(Process32NextW(snapshot,&entry));
 CloseHandle(snapshot);return found;
}
inline LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 if(retained)return DefWindowProcW(hwnd,msg,w,l);
 if(ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd,msg,w,l))return 1;
 if(msg==WM_CLOSE){cancelled=true;return 0;}
 if(msg==WM_NCHITTEST){POINT p{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};ScreenToClient(hwnd,&p);if(p.y<35*uiScale && p.x<610*uiScale)return HTCAPTION;}
 if(msg==WM_SYSCOMMAND && (w&0xfff0)==SC_KEYMENU)return 0;
 return DefWindowProcW(hwnd,msg,w,l);
}
inline const char* Text(const char* en,const char* no){return UiText::language?en:no;}
inline void Draw(LoadState& state,bool detected,ImFont* titleFont,ImFont* smallFont){
 float t=float(ImGui::GetTime());
 ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(Scaled(680,380));
 ImGui::Begin("##retro_loader",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground);
 auto d=ImGui::GetWindowDrawList();RetroUi::Frame(d,Scaled(1,1),Scaled(679,379));
 if(RetroUi::CaptionButton("##loader_close",Scaled(645,9),true))cancelled=true;
 RetroUi::Stars(d,Scaled(12,42),Scaled(655,232),t);
 d->AddText(titleFont,26*uiScale,Scaled(45,71),RetroUi::White,"goatgoatgoatgoatgoatgoat");
 RetroUi::Frame(d,Scaled(35,131),Scaled(644,247));
 if(!detected)d->AddText(smallFont,16*uiScale,Scaled(55,175),RetroUi::White,"Waiting for Roblox");
 if(state.loading)d->AddText(smallFont,16*uiScale,Scaled(55,175),RetroUi::White,"Loading");
 ImGui::SetCursorPos(Scaled(360,163));
 if(RetroUi::Choice(LaunchPrivacy::streamproof?"STREAMPROOF: ON":"STREAMPROOF: OFF",
     LaunchPrivacy::streamproof,Scaled(264,48))){
  const bool previous=LaunchPrivacy::streamproof;
  LaunchPrivacy::streamproof=!previous;
  if(loaderWindow && !CaptureProtection::Apply(loaderWindow,LaunchPrivacy::streamproof))
   LaunchPrivacy::streamproof=previous;
 }
 ImGui::SetCursorPos(Scaled(35,264));ImGui::BeginDisabled(!detected||state.loading);
 bool click=RetroUi::Choice(state.loading?"LOADING...":"LOAD",detected,Scaled(275,46));
 ImGui::EndDisabled();state.Update(detected,click,t);
 ImGui::SetCursorPos(Scaled(369,264));if(RetroUi::Choice("QUIT",false,Scaled(275,46)))cancelled=true;
 for(int i=0;i<30;++i){ImVec2 p=Scaled(36+i*20.f,331);d->AddRectFilled(p,ImVec2(p.x+16*uiScale,p.y+8*uiScale),i<int(state.Progress(t)*30)?RetroUi::Gold:IM_COL32(40,40,49,255));}
 ImGui::End();
}
inline bool StreamproofEnabled(){
 return LaunchPrivacy::streamproof;
}
inline bool Run(const std::function<bool()>& prepare={}){
 using Microsoft::WRL::ComPtr;
 cancelled=false;startupFailure.clear();LaunchPrivacy::Begin();auto instance=GetModuleHandleW(nullptr);
 WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"GoatSunsetLoader";
 if(!RegisterClassExW(&wc)){startupFailure=L"Could not register the loader window (Windows error "+std::to_wstring(GetLastError())+L").";return false;}

 POINT cursor{};GetCursorPos(&cursor);
 MONITORINFO monitor{sizeof(monitor)};
 GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&monitor);
 const RECT work=monitor.rcWork;
 uiScale=(std::min)(1.25f,(std::min)((work.right-work.left)*.92f/680.f,(work.bottom-work.top)*.92f/380.f));
 const int width=static_cast<int>(680*uiScale),height=static_cast<int>(380*uiScale);
 HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"goatgoatgoat",WS_POPUP,
  work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,
  width,height,nullptr,nullptr,instance,nullptr);
 loaderWindow=hwnd;
 if(!hwnd){startupFailure=L"Could not create the loader window (Windows error "+std::to_wstring(GetLastError())+L").";Dismiss();return false;}
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=hwnd;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swap;
 HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,swap.GetAddressOf(),device.GetAddressOf(),nullptr,context.GetAddressOf());
 if(FAILED(hr))hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,swap.GetAddressOf(),device.GetAddressOf(),nullptr,context.GetAddressOf());
 if(FAILED(hr)){startupFailure=L"The loader could not initialize DirectX 11. Check the graphics driver.";Dismiss();return false;}
 ComPtr<ID3D11Texture2D> buffer;ComPtr<ID3D11RenderTargetView> target;
 swap->GetBuffer(0,IID_PPV_ARGS(buffer.GetAddressOf()));
 if(!buffer || FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,target.GetAddressOf()))){startupFailure=L"Could not create the loader rendering surface.";Dismiss();return false;}
 const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 struct ComScope {HRESULT result;~ComScope(){if(SUCCEEDED(result))CoUninitialize();}} comScope{comResult};
 Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;

 if(!CaptureProtection::Apply(hwnd,StreamproofEnabled())){startupFailure=L"Windows could not enable capture protection for the loader.";Dismiss();return false;}
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
 ImGui::StyleColorsDark();ImGui::GetStyle().WindowBorderSize=0;
 ImFontConfig cfg;cfg.SizePixels=13;cfg.OversampleH=1;cfg.OversampleV=1;cfg.PixelSnapH=true;
 ImFont* bodyFont=UiAssets::Font(16*uiScale);ImFont* title=bodyFont;
 ImGui_ImplWin32_Init(hwnd);ImGui_ImplDX11_Init(device.Get(),context.Get());
 SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
 UpdateWindow(hwnd);
 LoadState state;bool detected=false,ready=false,prepared=false;double nextScan=0;std::future<bool> preparing;
 while(!cancelled){
  MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)cancelled=true;TranslateMessage(&msg);DispatchMessageW(&msg);}
  ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
  double now=ImGui::GetTime();if(now>=nextScan){detected=RobloxRunning();nextScan=now+.4;}
  if(state.loading && prepare && !prepared && !preparing.valid())preparing=std::async(std::launch::async,prepare);
  if(preparing.valid())state.actualProgress=.88f;
  if(preparing.valid() && preparing.wait_for(std::chrono::seconds(0))==std::future_status::ready){
   try{prepared=preparing.get();}catch(...){startupFailure=L"An unexpected error occurred while preparing the game connection.";prepared=false;}
   if(prepared)state.Prepared(now);else cancelled=true;
  }
  Draw(state,detected,title,bodyFont);ImGui::Render();
  float clear[]={.07f,.05f,.04f,1};auto view=target.Get();context->OMSetRenderTargets(1,&view,nullptr);context->ClearRenderTargetView(view,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  if(FAILED(swap->Present(1,0))){startupFailure=L"The loader lost its graphics device.";cancelled=true;break;}
  if(prepared && state.Ready(now)){ready=true;break;}
  if(!prepare && state.Ready(now)){ready=RobloxRunning();if(ready)break;state.loading=false;}
 }
 ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();retained=ready&&!cancelled;if(!retained)Dismiss();
 sunImage=nullptr;
 return ready&&!cancelled;
}
}
