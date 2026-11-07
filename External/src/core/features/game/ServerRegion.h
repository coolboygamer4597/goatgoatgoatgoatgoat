#pragma once
#include "ServerRegionData.h"
#include "../../variables/variables.h"
#include "../../../memory/memory.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace ServerRegion {
inline std::string location="Off",notice,lastServer;
inline ULONGLONG next=0,until=0;
inline DWORD pid=0;
inline std::uint32_t Address(const std::string& text){unsigned a,b,c,d;int used=0;if(std::sscanf(text.c_str(),"%u.%u.%u.%u%n",&a,&b,&c,&d,&used)!=4||used!=text.size()||a>255||b>255||c>255||d>255)return 0;return a<<24|b<<16|c<<8|d;}
inline std::string Lookup(std::uint32_t address){for(const auto& row:ServerRegionData::entries)if(address>=row.first&&address<=row.last)return row.location;return {};}
inline std::string Extract(const std::string& text){const auto start=text.rfind("UDMUX Address = ");if(start==std::string::npos)return {};const auto first=start+16,last=text.find_first_not_of("0123456789.",first);const auto ip=text.substr(first,last-first);return Address(ip)?ip:std::string{};}
inline std::string Status(){return location;}
inline bool Visible(){return variables::Misc::serverRegionAlerts&&!notice.empty()&&GetTickCount64()<until;}
inline void Tick(){
    if(!variables::Misc::serverRegionAlerts){notice.clear();location="Off";pid=0;return;}
    const auto now=GetTickCount64();if(now<next)return;next=now+3000;
    if(!memory||!memory->IsConnected()){location="Waiting for the game";return;}
    if(pid!=memory->get_process_id()){pid=memory->get_process_id();lastServer.clear();notice.clear();}
    FILETIME created,ended,kernel,user;if(!GetProcessTimes(memory->GetHandle(),&created,&ended,&kernel,&user))return;
    wchar_t local[32768]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768))return;
    std::filesystem::path selected;std::uint64_t best=0;std::error_code error;
    const auto logs=std::filesystem::path(local)/L"Roblox/logs";
    for(const auto& file:std::filesystem::directory_iterator(logs,error)){
        const auto name=file.path().filename().wstring();if(name.find(L"_Player_")==name.npos||name.find(L"CrashHandler")!=name.npos||file.path().extension()!=L".log")continue;
        WIN32_FILE_ATTRIBUTE_DATA info{};if(!GetFileAttributesExW(file.path().c_str(),GetFileExInfoStandard,&info))continue;
        ULARGE_INTEGER time{},process{};time.HighPart=info.ftCreationTime.dwHighDateTime;time.LowPart=info.ftCreationTime.dwLowDateTime;process.HighPart=created.dwHighDateTime;process.LowPart=created.dwLowDateTime;
        const auto difference=time.QuadPart>process.QuadPart?time.QuadPart-process.QuadPart:process.QuadPart-time.QuadPart;
        if(difference>100000000ULL)continue;
        const auto write=(std::uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;if(write>best){best=write;selected=file.path();}
    }
    if(selected.empty()){location="Server region unavailable";return;}
    std::ifstream file(selected,std::ios::binary);if(!file)return;file.seekg(0,std::ios::end);const auto size=static_cast<std::streamoff>(file.tellg());if(size<0)return;
    const auto read=[&](std::streamoff start,std::streamoff length){file.clear();file.seekg(start);std::string text(static_cast<std::size_t>(length),0);file.read(text.data(),length);text.resize(static_cast<std::size_t>(file.gcount()));return text;};
    std::string address=Extract(read((std::max)(std::streamoff(0),size-524288),(std::min)(size,std::streamoff(524288))));if(address.empty()&&size>524288)address=Extract(read(0,131072));
    if(address.empty()){location="Waiting for server information";return;}
    const auto found=Lookup(Address(address));location=found.empty()?"Server region unavailable":found+" (approx.)";
    if(address!=lastServer){lastServer=address;notice="Server region: "+location;until=now+7000;}
}
}
