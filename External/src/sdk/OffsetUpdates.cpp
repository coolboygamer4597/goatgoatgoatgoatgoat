#include "OffsetUpdates.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <algorithm>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"bcrypt.lib")

namespace OffsetUpdates {
namespace {
std::mutex stateMutex;
std::once_flag loaded;
std::map<std::string,std::uintptr_t> optionalFields;
Selection selection;
std::string currentStatus="Offsets will be selected when loading.",failure;
std::filesystem::path Executable(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return path;}
std::filesystem::path Storage(){wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);return (n&&n<32768?std::filesystem::path(path):Executable().parent_path())/L"GoatGoatGoat"/L"offsets";}
std::string ReadBytes(const std::filesystem::path& file,size_t limit){
    std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Could not open offsets file.");auto size=in.tellg();if(size<0||static_cast<unsigned long long>(size)>limit)throw std::runtime_error("File size exceeds the limit.");std::string bytes(static_cast<size_t>(size),'\0');in.seekg(0);if(size&&!in.read(bytes.data(),size))throw std::runtime_error("Could not read the complete file.");return bytes;
}
void Write(const std::filesystem::path& file,const std::string& bytes){
    std::filesystem::create_directories(file.parent_path());auto temp=file;temp+=L".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out||!out.write(bytes.data(),bytes.size()))throw std::runtime_error("Could not save offsets settings or cache.");}
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace offsets settings or cache.");
}
std::string Build(){static const auto id=[] {try{return Sha256(ReadBytes(Executable(),67108864));}catch(...){return std::string(__DATE__ " " __TIME__);}}();return id;}
void Save(){std::string json="{\"source\":"+std::to_string(static_cast<int>(selection.source))+",\"file\":"+OffsetData::Quote(selection.file.u8string())+",\"seenVersion\":"+OffsetData::Quote(selection.seenVersion)+",\"seenBuild\":"+OffsetData::Quote(selection.seenBuild)+"}";Write(Storage()/L"source.json",json);}
void Load(){std::call_once(loaded,[]{try{auto data=OffsetData::Parse(ReadFile(Storage()/L"source.json"));auto source=OffsetData::Uint(data.At("source"));if(source>2)return;selection.source=static_cast<Source>(source);selection.file=std::filesystem::u8path(data.At("file").Str());selection.seenVersion=data.At("seenVersion").Str();selection.seenBuild=data.At("seenBuild").Str();}catch(...){selection={};}});}
struct Http {HINTERNET handle=nullptr;~Http(){if(handle)WinHttpCloseHandle(handle);}};
std::string Download(const std::string& relative){
    const std::wstring host=L"raw.githubusercontent.com";
    std::wstring path=L"/coolboygamer4597/Ekte-lidenskap-har-et-navn/main/"+std::wstring(relative.begin(),relative.end());
    if(relative=="index.json")path+=L"?t="+std::to_wstring(GetTickCount64());
    Http session{WinHttpOpen(L"GoatOffsets/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.handle)throw std::runtime_error("Could not start the offsets download.");
    WinHttpSetTimeouts(session.handle,4000,4000,4000,6000);
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(session.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    Http connection{WinHttpConnect(session.handle,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0)};
    Http request{connection.handle?WinHttpOpenRequest(connection.handle,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
    if(!request.handle||!WinHttpSendRequest(request.handle,L"Cache-Control: no-cache\r\n",-1L,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.handle,nullptr))throw std::runtime_error("Offsets download failed (Windows error "+std::to_string(GetLastError())+").");
    DWORD code=0,size=sizeof(code);if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)||code!=200)throw std::runtime_error("Offsets download returned HTTP "+std::to_string(code)+".");
    std::string bytes;const auto start=GetTickCount64();char chunk[8192];DWORD read=0;
    do{if(GetTickCount64()-start>15000)throw std::runtime_error("Offsets download timed out.");if(!WinHttpReadData(request.handle,chunk,sizeof(chunk),&read))throw std::runtime_error("Offsets download was interrupted.");if(bytes.size()+read>524288)throw std::runtime_error("Downloaded offsets file is too large.");bytes.append(chunk,read);}while(read);return bytes;
}
OffsetData::Package Bundled(){OffsetData::Package p;p.version=BundledOffsets::ClientVersion;for(auto& f:OffsetCatalog::Fields())p.fields[f.name]=f.bundled;
#ifdef GOAT_OPTIONAL_PARTICLEEMITTER_RATE
p.fields["ParticleEmitter.Rate"]=GOAT_OPTIONAL_PARTICLEEMITTER_RATE;
#endif
#ifdef GOAT_OPTIONAL_PARTICLEEMITTER_TEXTURE
p.fields["ParticleEmitter.Texture"]=GOAT_OPTIONAL_PARTICLEEMITTER_TEXTURE;
#endif
#ifdef GOAT_OPTIONAL_PARTICLEEMITTER_LOCALTRANSPARENCYMODIFIER
p.fields["ParticleEmitter.LocalTransparencyModifier"]=GOAT_OPTIONAL_PARTICLEEMITTER_LOCALTRANSPARENCYMODIFIER;
#endif
#ifdef GOAT_OPTIONAL_PARTICLEEMITTER_ENABLED
p.fields["ParticleEmitter.Enabled"]=GOAT_OPTIONAL_PARTICLEEMITTER_ENABLED;
#endif
#ifdef GOAT_OPTIONAL_PARTICLEEMITTER_LIFETIME
p.fields["ParticleEmitter.Lifetime"]=GOAT_OPTIONAL_PARTICLEEMITTER_LIFETIME;
#endif
#ifdef GOAT_OPTIONAL_HIGHLIGHT_OUTLINETRANSPARENCY
p.fields["Highlight.OutlineTransparency"]=GOAT_OPTIONAL_HIGHLIGHT_OUTLINETRANSPARENCY;
#endif
#ifdef GOAT_OPTIONAL_ANTIAFK_TIMEOUTRVA
p.fields["AntiAfk.TimeoutRva"]=GOAT_OPTIONAL_ANTIAFK_TIMEOUTRVA;
#endif
#ifdef GOAT_OPTIONAL_ANTIAFK_DISABLEDRVA
p.fields["AntiAfk.DisabledRva"]=GOAT_OPTIONAL_ANTIAFK_DISABLEDRVA;
#endif
#ifdef GOAT_OPTIONAL_ANTIAFK_NAMEOFFSET
p.fields["AntiAfk.NameOffset"]=GOAT_OPTIONAL_ANTIAFK_NAMEOFFSET;
#endif
#ifdef GOAT_OPTIONAL_ANTIAFK_NAMELENGTHOFFSET
p.fields["AntiAfk.NameLengthOffset"]=GOAT_OPTIONAL_ANTIAFK_NAMELENGTHOFFSET;
#endif
return p;}
Resolution Cached(const std::filesystem::path& cache,const std::string& version,const std::string& expected={}){
    auto envelope=OffsetData::Parse(ReadFile(cache/std::filesystem::u8path(version+".json")));auto body=envelope.At("package").Str(),hash=envelope.At("sha256").Str();
    if(Sha256(body)!=hash||(!expected.empty()&&hash!=expected))throw std::runtime_error("No verified cached offsets match this version and revision.");auto p=OffsetData::JsonPackage(body);OffsetData::Validate(p,version,Required());return {p,"Automatic updates: cached revision "+std::to_string(p.revision)+" (offline)"};
}
}
std::string ReadFile(const std::filesystem::path& file){return ReadBytes(file,524288);}
std::string Sha256(const std::string& bytes){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA-256 is unavailable.");
    struct Release {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Release(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} release{algorithm,hash};
    DWORD size=0,used=0;if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&used,0)<0)throw std::runtime_error("SHA-256 initialization failed.");
    std::vector<unsigned char> object(size);unsigned char digest[32]{};
    if(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)<0||BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0)<0||BCryptFinishHash(hash,digest,sizeof(digest),0)<0)throw std::runtime_error("SHA-256 validation failed.");
    std::string out;const char* hex="0123456789abcdef";for(auto c:digest){out+=hex[c>>4];out+=hex[c&15];}return out;
}
std::vector<std::string> Required(){std::vector<std::string> names;for(auto& f:OffsetCatalog::Fields())names.emplace_back(f.name);return names;}
Selection GetSelection(){Load();std::lock_guard<std::mutex> lock(stateMutex);return selection;}
void SetSelection(Source source,const std::filesystem::path& file){Load();std::lock_guard<std::mutex> lock(stateMutex);selection.source=source;selection.file=file;selection.seenVersion.clear();selection.seenBuild.clear();Save();}
bool NeedsPrompt(const std::string& version){auto s=GetSelection();return s.seenVersion!=version||s.seenBuild!=Build();}
std::string Status(){std::lock_guard<std::mutex> lock(stateMutex);return currentStatus;}
std::string Error(){std::lock_guard<std::mutex> lock(stateMutex);return failure;}
std::string RunningVersion(){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return {};PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);DWORD pid=0;
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"RobloxPlayerBeta.exe")==0){pid=entry.th32ProcessID;break;}}while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);if(!pid)return {};
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return {};wchar_t path[32768]{};DWORD length=32768;bool ok=QueryFullProcessImageNameW(process,0,path,&length)!=FALSE;CloseHandle(process);if(!ok)return {};
    std::wstring full(path,length);auto start=full.rfind(L"version-");if(start==std::wstring::npos)return {};auto end=full.find_first_of(L"\\/",start);auto part=full.substr(start,end-start);std::string version;for(wchar_t c:part){if(c>127)return {};version+=static_cast<char>(c);}return OffsetData::Version(version)?version:std::string{};
}
std::string FetchPublished(const std::string& relative){if(relative!="index.json"&&!std::regex_match(relative,std::regex("versions/version-[0-9a-f]{16,64}/r[1-9][0-9]*\\.json")))throw std::runtime_error("Invalid offsets download path.");return Download(relative);}
OffsetData::Package LocalPackage(const std::filesystem::path& file,const std::string& version){auto text=ReadFile(file);auto first=text.find_first_not_of(" \t\r\n");if(text.compare(0,3,"\xef\xbb\xbf")==0){text.erase(0,3);first=text.find_first_not_of(" \t\r\n");}auto p=first!=std::string::npos&&text[first]=='{'?OffsetData::JsonPackage(text):OffsetData::HeaderPackage(text);OffsetData::Validate(p,version,Required());return p;}
Resolution ResolveData(const std::string& version,const Selection& selected,const std::filesystem::path& cache,const Fetch& fetch){
    if(!OffsetData::Version(version))throw std::runtime_error("Could not identify the running Roblox version.");
    if(selected.source==Source::Bundled){auto p=Bundled();OffsetData::Validate(p,version,Required());return {p,"Bundled offsets"};}
    if(selected.source==Source::Local)return {LocalPackage(selected.file,version),"Local offsets: "+selected.file.filename().u8string()};
    std::string index;
    try{index=fetch("index.json");}catch(const std::exception& download){try{return Cached(cache,version);}catch(...){throw std::runtime_error(std::string(download.what())+" No verified cached offsets are available for "+version+".");}}
    auto manifest=OffsetData::Parse(index);if(OffsetData::Uint(manifest.At("schema"))!=1)throw std::runtime_error("Unsupported offsets index schema.");auto& versions=manifest.At("versions");if(versions.kind!=OffsetData::Json::Object)throw std::runtime_error("Invalid offsets index.");auto entry=versions.object.find(version);if(entry==versions.object.end())throw std::runtime_error("Published offsets are not available for "+version+". Choose bundled or local offsets, or wait for an upload.");
    auto& data=entry->second;unsigned revision=static_cast<unsigned>(OffsetData::Uint(data.At("revision")));auto file=data.At("file").Str(),hash=data.At("sha256").Str();
    if(!revision||file!="versions/"+version+"/r"+std::to_string(revision)+".json"||!std::regex_match(hash,std::regex("[0-9a-f]{64}")))throw std::runtime_error("Invalid published offsets entry.");
    std::string body;try{body=fetch(file);}catch(...){return Cached(cache,version,hash);}
    if(Sha256(body)!=hash)throw std::runtime_error("Downloaded offsets failed SHA-256 validation.");auto p=OffsetData::JsonPackage(body);OffsetData::Validate(p,version,Required());if(p.revision!=revision)throw std::runtime_error("Published offsets revision does not match its index.");
    std::string status="Automatic updates: published revision "+std::to_string(revision);
    try{Write(cache/std::filesystem::u8path(version+".json"),"{\"sha256\":"+OffsetData::Quote(hash)+",\"package\":"+OffsetData::Quote(body)+"}");}catch(...){status+=" (cache could not be saved)";}
    return {p,status};
}
void Apply(const OffsetData::Package& package,const std::string& version){
    OffsetData::Validate(package,version,Required());for(auto& field:OffsetCatalog::Fields())if(package.fields.at(field.name)>0xffffffffULL)throw std::runtime_error("Offset exceeds its range.");std::map<std::string,std::uintptr_t> optional;
    for(const auto* name:{"ParticleEmitter.Rate","ParticleEmitter.Texture","ParticleEmitter.LocalTransparencyModifier","ParticleEmitter.Enabled","ParticleEmitter.Lifetime","Highlight.OutlineTransparency","AntiAfk.TimeoutRva","AntiAfk.DisabledRva","AntiAfk.NameOffset","AntiAfk.NameLengthOffset"})if(auto value=package.fields.find(name);value!=package.fields.end()){
        const std::string key=name;const auto number=value->second;const bool anti=key.rfind("AntiAfk.",0)==0,layout=key=="AntiAfk.NameOffset"||key=="AntiAfk.NameLengthOffset";
        if(anti?(layout?(number<8||number>72||number%8):(number<4096||number>0xffffffffULL)):(number<(key=="Highlight.OutlineTransparency"?0x80:0x100)||number>=0x1000))throw std::runtime_error("Optional offset exceeds its range.");optional[key]=static_cast<std::uintptr_t>(number);
    }
    for(auto& field:OffsetCatalog::Fields())*field.active=static_cast<std::uintptr_t>(package.fields.at(field.name));Offsets::ClientVersion=version;{std::lock_guard<std::mutex> lock(stateMutex);optionalFields=std::move(optional);}
}
std::uintptr_t OptionalField(const std::string& name){std::lock_guard<std::mutex> lock(stateMutex);const auto found=optionalFields.find(name);return found==optionalFields.end()?0:found->second;}
bool Resolve(const std::string& version){
    auto selected=GetSelection();try{auto result=ResolveData(version,selected,Storage()/L"cache",FetchPublished);Apply(result.package,version);std::lock_guard<std::mutex> lock(stateMutex);currentStatus=result.status;failure.clear();return true;}
    catch(const std::exception& e){std::lock_guard<std::mutex> lock(stateMutex);failure=e.what();return false;}
}
void RememberSuccessfulLoad(const std::string& version){Load();std::lock_guard<std::mutex> lock(stateMutex);selection.seenVersion=version;selection.seenBuild=Build();try{Save();}catch(...){currentStatus+=" (selection could not be saved)";}}
}
