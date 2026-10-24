#pragma once
#include "AntiAfkState.h"
#include "../../../memory/memory.h"
#include "../../../sdk/RuntimeOffsets.h"
#include <cstring>
namespace AntiAfk {
struct Session {
 HANDLE handle=nullptr;DWORD pid=0;std::uintptr_t base=0;Patch patch;
 static constexpr std::uintptr_t timeoutRva=0x80219c0,disabledRva=0x85c7658;
 bool Read(std::uintptr_t at,void* out,size_t size)const{SIZE_T done=0;return handle&&ReadProcessMemory(handle,reinterpret_cast<LPCVOID>(at),out,size,&done)&&done==size;}
 template<class T>bool Write(std::uintptr_t rva,T value)const{SIZE_T done=0;return handle&&WriteProcessMemory(handle,reinterpret_cast<LPVOID>(base+rva),&value,sizeof(value),&done)&&done==sizeof(value);}
 bool ValuesNow(Values& v)const{return Read(base+timeoutRva,&v.timeout,sizeof(v.timeout))&&Read(base+disabledRva,&v.disabled,sizeof(v.disabled));}
 bool Named(std::uintptr_t rva,const char* expected)const{
  std::uint64_t pointer=0,length=0;char name[80]{};size_t n=std::strlen(expected);std::uint32_t peOffset=0,size=0;
  if(!Read(base+60,&peOffset,4)||peOffset>4096||!Read(base+peOffset+80,&size,4)||size<4096||size>512*1024*1024||rva>size-24)return false;
  if(!Read(base+rva+8,&pointer,8)||!Read(base+rva+16,&length,8)||length!=n||pointer<base||pointer-base>size-n-1||!Read(pointer,name,n+1)||std::memcmp(name,expected,n+1))return false;
  MEMORY_BASIC_INFORMATION info{};if(!VirtualQueryEx(handle,reinterpret_cast<LPCVOID>(base+rva),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  auto protect=info.Protect&255;return protect==PAGE_READWRITE||protect==PAGE_WRITECOPY;
 }
 bool Valid()const{return Named(timeoutRva,"PartyPlayerInactivityTimeoutInSeconds")&&Named(disabledRva,"DebugDisableTimeoutDisconnect");}
 bool Restore(){return patch.Restore([&](Values& v){return ValuesNow(v);},[&](std::uint32_t v){return Write(timeoutRva,v);},[&](std::uint8_t v){return Write(disabledRva,v);});}
 void Close(){if(handle){if(Valid())Restore();CloseHandle(handle);}handle=nullptr;pid=0;base=0;patch=Patch{};}
 ~Session(){Close();}
};
inline Session session;
inline ULONGLONG next=0;
inline void Stop(){session.Close();}
inline void Tick(){
 if(GetTickCount64()<next)return;next=GetTickCount64()+500;
 auto pid=memory?memory->get_process_id():0;auto base=memory?memory->get_module_address():0;
 if(session.handle&&(session.pid!=pid||session.base!=base))session.Close();
 if(!enabled){if(session.handle&&session.Restore())session.Close();error.clear();return;}
 if(!memory||!memory->IsConnected()||!base){session.Close();error="Waiting for Roblox";return;}
 if(Offsets::ClientVersion!="version-11f155f3450f4c8e"){session.Close();error="Anti-AFK unavailable for this client";return;}
 if(!session.handle){HANDLE copy=nullptr;if(!DuplicateHandle(GetCurrentProcess(),memory->GetHandle(),GetCurrentProcess(),&copy,0,FALSE,DUPLICATE_SAME_ACCESS)){error="Anti-AFK could not access Roblox";return;}session.handle=copy;session.pid=pid;session.base=base;}
 if(!session.Valid()){session.Close();error="Anti-AFK flags could not be verified";return;}
 bool ok=session.patch.Apply([&](Values& v){return session.ValuesNow(v);},[&](std::uint32_t v){return session.Write(Session::timeoutRva,v);},[&](std::uint8_t v){return session.Write(Session::disabledRva,v);});error=ok?"":"Anti-AFK could not update the timeout";
}
}
