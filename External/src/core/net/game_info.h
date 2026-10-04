#pragma once
#include <Windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <future>
#include <chrono>
#include <string>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"windowsapp.lib")
namespace GameInfo {
struct HttpHandle { HINTERNET value; ~HttpHandle(){if(value)WinHttpCloseHandle(value);} };
inline std::string Get(const wchar_t* host,const std::wstring& path){
 HttpHandle session{WinHttpOpen(L"goatgoatgoat/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
 if(!session.value)return {};
 WinHttpSetTimeouts(session.value,2000,2000,2000,3000);
 HttpHandle connection{WinHttpConnect(session.value,host,INTERNET_DEFAULT_HTTPS_PORT,0)};
 if(!connection.value)return {};
 HttpHandle request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
 if(!request.value || !WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr))return {};
 DWORD status=0,size=sizeof(status);
 if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return {};
 std::string body;char buffer[4096];DWORD count=0;
 while(WinHttpReadData(request.value,buffer,sizeof(buffer),&count)&&count){body.append(buffer,count);if(body.size()>262144)return {};}
 return body;
}
inline std::string Resolve(std::uint64_t place){
 try {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  struct Apartment {~Apartment(){winrt::uninit_apartment();}} apartment;
  using winrt::Windows::Data::Json::JsonObject;
  const auto universe=JsonObject::Parse(winrt::to_hstring(Get(L"apis.roblox.com",L"/universes/v1/places/"+std::to_wstring(place)+L"/universe"))).GetNamedNumber(L"universeId");
  const auto result=JsonObject::Parse(winrt::to_hstring(Get(L"games.roblox.com",L"/v1/games?universeIds="+std::to_wstring(static_cast<std::uint64_t>(universe))))).GetNamedArray(L"data");
  if(result.Size())return winrt::to_string(result.GetObjectAt(0).GetNamedString(L"name"));
 }catch(...){}
 return {};
}
inline std::string gameName="Roblox";
inline std::uint64_t currentPlace=0,requestPlace=0;
inline std::future<std::string> pending;
inline auto retry=std::chrono::steady_clock::time_point{};
inline void Tick(std::uint64_t place){
 auto now=std::chrono::steady_clock::now();
 if(place!=currentPlace){currentPlace=place;gameName="Roblox";retry={};}
 if(pending.valid()){
  if(pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
  auto name=pending.get();if(requestPlace==currentPlace&&!name.empty())gameName=std::move(name);
 }
 if(place && gameName=="Roblox" && now>=retry){
  retry=now+std::chrono::seconds(60);requestPlace=place;
  pending=std::async(std::launch::async,[place]{return Resolve(place);});
 }
}
inline const std::string& Name(){return gameName;}
}
