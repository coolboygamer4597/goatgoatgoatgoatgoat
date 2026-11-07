#pragma once
#include <Windows.h>
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace DiscordPresence {
inline constexpr char ApplicationId[]="1566827468541665320";
struct Config {
    bool enabled=false;
    char details[128]="Customising weapons";
};
inline Config settings;
inline bool ValidId(const char* id){const auto n=std::strlen(id);return n>=17&&n<=20&&std::all_of(id,id+n,[](char c){return c>='0'&&c<='9';});}
inline bool Same(const Config& a,const Config& b){return a.enabled==b.enabled&&!std::strcmp(a.details,b.details);}
inline winrt::hstring String(const winrt::Windows::Data::Json::JsonObject& object,const wchar_t* key){using namespace winrt::Windows::Data::Json;const auto value=object.GetNamedValue(key,JsonValue::CreateNullValue());return value.ValueType()==JsonValueType::String?value.GetString():winrt::hstring{};}
inline std::mutex lock;
inline Config pending;
inline std::string status="Off";
inline std::thread worker;
inline std::atomic<bool> stopping{false};
inline std::string Status(){std::lock_guard<std::mutex> guard(lock);return status;}
inline void Status(std::string value){std::lock_guard<std::mutex> guard(lock);status=std::move(value);}
inline bool Transfer(HANDLE pipe,bool write,void* data,DWORD length){
    OVERLAPPED operation{};operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!operation.hEvent)return false;
    DWORD done=0;BOOL ok=write?WriteFile(pipe,data,length,&done,&operation):ReadFile(pipe,data,length,&done,&operation);
    if(!ok&&GetLastError()==ERROR_IO_PENDING){if(WaitForSingleObject(operation.hEvent,150)==WAIT_OBJECT_0)ok=GetOverlappedResult(pipe,&operation,&done,FALSE);else{CancelIoEx(pipe,&operation);GetOverlappedResult(pipe,&operation,&done,TRUE);}}
    CloseHandle(operation.hEvent);return ok&&done==length;
}
inline bool Send(HANDLE pipe,unsigned opcode,const std::string& json){
    if(json.size()>65536)return false;std::vector<unsigned char> frame(json.size()+8);unsigned header[2]={opcode,static_cast<unsigned>(json.size())};std::memcpy(frame.data(),header,8);std::memcpy(frame.data()+8,json.data(),json.size());return Transfer(pipe,true,frame.data(),static_cast<DWORD>(frame.size()));
}
inline std::string Handshake(const Config& c){using namespace winrt::Windows::Data::Json;JsonObject o;o.SetNamedValue(L"v",JsonValue::CreateNumberValue(1));o.SetNamedValue(L"client_id",JsonValue::CreateStringValue(winrt::to_hstring(ApplicationId)));return winrt::to_string(o.Stringify());}
inline std::string Activity(const Config& c,unsigned nonce,std::int64_t start,bool clear=false){
    using namespace winrt::Windows::Data::Json;JsonObject out,args;out.SetNamedValue(L"cmd",JsonValue::CreateStringValue(L"SET_ACTIVITY"));out.SetNamedValue(L"nonce",JsonValue::CreateStringValue(winrt::to_hstring(std::to_string(nonce))));args.SetNamedValue(L"pid",JsonValue::CreateNumberValue(GetCurrentProcessId()));
    if(clear)args.SetNamedValue(L"activity",JsonValue::CreateNullValue());
    else{JsonObject activity;activity.SetNamedValue(L"details",JsonValue::CreateStringValue(winrt::to_hstring(c.details)));activity.SetNamedValue(L"state",JsonValue::CreateStringValue(L"goatgoatgoat"));
        {JsonObject timestamps;timestamps.SetNamedValue(L"start",JsonValue::CreateNumberValue(static_cast<double>(start)));activity.SetNamedValue(L"timestamps",timestamps);}
        args.SetNamedValue(L"activity",activity);}
    out.SetNamedValue(L"args",args);return winrt::to_string(out.Stringify());
}
inline void Run(const std::wstring& pipePrefix=L"\\\\.\\pipe\\discord-ipc-"){
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    HANDLE pipe=INVALID_HANDLE_VALUE;Config connected{},last{};bool ready=false,published=false,rejected=false;unsigned nonce=0;std::vector<unsigned char> bytes;
    auto reconnect=std::chrono::steady_clock::time_point{},sent=reconnect,handshakeAt=reconnect;
    auto start=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto close=[&](bool clear){if(pipe!=INVALID_HANDLE_VALUE){if(clear&&ready)Send(pipe,1,Activity(connected,++nonce,start,true));CloseHandle(pipe);}pipe=INVALID_HANDLE_VALUE;ready=published=rejected=false;bytes.clear();};
    while(!stopping){Config c;{std::lock_guard<std::mutex> guard(lock);c=pending;}const auto now=std::chrono::steady_clock::now();
        if(!c.enabled){close(true);Status("Off");}
        else{
            if(pipe==INVALID_HANDLE_VALUE&&now>=reconnect){reconnect=now+std::chrono::seconds(5);for(int i=0;i<10&&pipe==INVALID_HANDLE_VALUE;++i){auto name=pipePrefix+std::to_wstring(i);pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);}
                if(pipe!=INVALID_HANDLE_VALUE){connected=c;if(!Send(pipe,0,Handshake(c)))close(false);else{handshakeAt=now;Status("Connecting to Discord...");}}else Status("Waiting for desktop Discord");}
            if(pipe!=INVALID_HANDLE_VALUE){DWORD available=0;if(!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)){close(false);Status("Discord disconnected");}
                else if(available){if(available>65544||bytes.size()+available>131088)close(false);else{const auto old=bytes.size();bytes.resize(old+available);if(!Transfer(pipe,false,bytes.data()+old,available))close(false);else{
                    while(bytes.size()>=8){unsigned h[2];std::memcpy(h,bytes.data(),8);if(h[1]>65536){close(false);break;}if(bytes.size()<8+h[1])break;std::string body(reinterpret_cast<char*>(bytes.data()+8),h[1]);bytes.erase(bytes.begin(),bytes.begin()+8+h[1]);
                        if(h[0]==2){close(false);Status("Discord rejected the connection");break;}
                        if(h[0]==3){if(!Send(pipe,4,body)){close(false);break;}}
                        else if(h[0]==1){try{auto event=winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(body));const auto kind=String(event,L"evt");if(kind==L"READY"){ready=true;rejected=false;Status("Connected");}else if(kind==L"ERROR"){rejected=true;Status("Discord rejected the activity");}}catch(...){close(false);break;}}
                    }
                }}}
                if(!ready&&now-handshakeAt>std::chrono::seconds(10)){close(false);Status("Discord handshake timed out");}
                if(ready&&((!published&&!rejected)||!Same(c,last))&&now-sent>=std::chrono::seconds(5)){if(Send(pipe,1,Activity(c,++nonce,start))){last=c;published=true;rejected=false;sent=now;Status("Presence active");}else close(false);}
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    close(true);winrt::uninit_apartment();
}
inline void Tick(){
    {std::lock_guard<std::mutex> guard(lock);pending=settings;}
    if(settings.enabled&&!worker.joinable()){stopping=false;worker=std::thread([]{try{Run();}catch(...){Status("Discord connection unavailable");}});}
}
inline void Stop(){stopping=true;if(worker.joinable())worker.join();Status("Off");}
}
