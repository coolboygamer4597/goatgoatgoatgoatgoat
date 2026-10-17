#include "AppUpdates.h"
#include "AppUpdateData.h"
#include "PreviewUpdateData.h"
#include <winhttp.h>
#include <objbase.h>
#include <memory>
#include <cwctype>
#include <shellapi.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <chrono>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
namespace AppUpdates {
namespace {
using Path=std::filesystem::path;
struct Manager{std::mutex mutex;std::once_flag loaded;std::atomic<bool> cancel{false},previewCancel{false};State state;AppUpdateData::Release release;std::string skipped,previewDeclined;bool updateAfterPreviews=false;Path request;};
std::shared_ptr<Manager> Shared(){static auto state=std::make_shared<Manager>();return state;}
Path Executable(){wchar_t text[32768]{};auto count=GetModuleFileNameW(nullptr,text,32768);if(!count||count>=32768)throw std::runtime_error("Could not locate the executable.");return Path(text);}
Path Storage(){wchar_t text[32768]{};auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",text,32768);if(!count||count>=32768)throw std::runtime_error("Local application storage is unavailable.");return Path(text)/L"GoatGoatGoat"/L"updates";}
bool Normal(const Path& path){if(!path.is_absolute() || path.lexically_normal()!=path)return false;Path part=path.root_path();for(const auto& name:path.relative_path()){part/=name;auto attributes=GetFileAttributesW(part.c_str());if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;}return true;}
bool Identifier(const std::wstring& name){return name.size()==32&&std::all_of(name.begin(),name.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');});}
std::wstring Identifier(){GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Could not prepare the update.");std::wstring text;const auto bytes=reinterpret_cast<const unsigned char*>(&id);for(std::size_t i=0;i<sizeof(id);++i){text+=L"0123456789abcdef"[bytes[i]>>4];text+=L"0123456789abcdef"[bytes[i]&15];}return text;}
void Write(const Path& path,const std::string& text){std::filesystem::create_directories(path.parent_path());auto temporary=path;temporary+=L".tmp";{std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out||!out.write(text.data(),text.size())||!out.flush())throw std::runtime_error("Could not save the update.");}if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not save update settings.");}
void Log(const std::string& message){try{std::filesystem::create_directories(Storage());std::ofstream(Storage()/L"update.log",std::ios::app)<<GetTickCount64()<<" "<<message<<"\n";}catch(...){}}
void Load(const std::shared_ptr<Manager>& s){std::call_once(s->loaded,[&]{try{auto values=OffsetData::Parse(AppUpdateData::Read(Storage()/L"settings.json",4096));s->state.automatic=values.At("automatic").text=="true";s->skipped=values.At("skipped").Str();if(auto value=values.object.find("previews_declined");value!=values.object.end())s->previewDeclined=value->second.Str();if(!s->skipped.empty())AppUpdateData::Version(s->skipped);}catch(...){s->state.automatic=true;s->skipped.clear();}});}
void Save(const Manager& s){Write(Storage()/L"settings.json","{\"automatic\":"+std::string(s.state.automatic?"true":"false")+",\"skipped\":"+OffsetData::Quote(s.skipped)+",\"previews_declined\":"+OffsetData::Quote(s.previewDeclined)+"}");}
struct Http{HINTERNET h=nullptr;~Http(){if(h)WinHttpCloseHandle(h);}};
struct Address{std::wstring host,path;};
Address ParseUrl(const std::wstring& url){
    if(url.size()>16384 || url.find(L'#')!=std::wstring::npos)throw std::runtime_error("Invalid update download address.");URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=443||parts.dwUserNameLength||parts.dwPasswordLength)throw std::runtime_error("Updates require a trusted HTTPS download.");
    Address result{std::wstring(parts.lpszHostName,parts.dwHostNameLength),std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength)};std::transform(result.host.begin(),result.host.end(),result.host.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
    if(result.host!=L"github.com"&&result.host!=L"release-assets.githubusercontent.com"&&result.host!=L"objects.githubusercontent.com"&&result.host!=L"raw.githubusercontent.com")throw std::runtime_error("The update redirected to an unexpected server.");
    if(result.host==L"raw.githubusercontent.com"&&result.path!=L"/coolboygamer4597/goatgoatgoatgoatgoat/codex/update-feed/stable.json")throw std::runtime_error("The update feed points outside this repository.");
    if(result.host==L"github.com"&&result.path.rfind(L"/coolboygamer4597/goatgoatgoatgoatgoat/releases/",0)!=0)throw std::runtime_error("The update points outside this repository.");
    if(parts.dwExtraInfoLength)result.path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);return result;
}
std::string Fetch(std::wstring url,std::size_t maximum,const std::shared_ptr<Manager>& s,const std::function<void(std::size_t)>& progress={},const Path& output={},const std::atomic<bool>* cancelled=nullptr){
    Http session{WinHttpOpen(L"GoatUpdater/1.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};if(!session.h)throw std::runtime_error("Could not start the update check.");WinHttpSetTimeouts(session.h,1500,1500,2000,2000);
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(session.h,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));auto started=GetTickCount64();
    for(int redirects=0;redirects<6;++redirects){
        if(s->cancel)throw std::runtime_error("Update cancelled.");auto address=ParseUrl(url);Http connection{WinHttpConnect(session.h,address.host.c_str(),443,0)};Http request{connection.h?WinHttpOpenRequest(connection.h,L"GET",address.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
        if(!request.h||!WinHttpSendRequest(request.h,L"Cache-Control: no-cache\r\n",static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Could not reach the update server. You can continue with this version.");
        DWORD code=0,size=sizeof(code);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid update server response.");
        if(code==301||code==302||code==303||code==307||code==308){size=0;WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&size,WINHTTP_NO_HEADER_INDEX);if(!size||size>32768)throw std::runtime_error("Invalid update redirect.");std::vector<wchar_t> location(size/sizeof(wchar_t)+1);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid update redirect.");url=location.data();if(!url.empty()&&url[0]==L'/')url=L"https://"+address.host+url;continue;}
        if(code==404)throw std::runtime_error("No update has been published yet. You can continue with this version.");if(code!=200)throw std::runtime_error("Update server returned HTTP "+std::to_string(code)+". You can continue with this version.");
        std::string bytes;char buffer[65536];DWORD count=0;std::size_t received=0;std::ofstream file;
        if(!output.empty()){file.open(output,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Could not save the preview download.");}
        do{if(s->cancel||(cancelled&&cancelled->load())||GetTickCount64()-started>(!output.empty()?900000ULL:maximum>32768?120000ULL:12000ULL))throw std::runtime_error("Update download cancelled or timed out.");if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&count))throw std::runtime_error("The update download was interrupted.");if(count>maximum-received)throw std::runtime_error("The update download exceeds its size limit.");received+=count;
            if(output.empty())bytes.append(buffer,count);else if(count&&!file.write(buffer,count))throw std::runtime_error("Could not save the preview download.");
            if(progress)progress(received);}while(count);if(!output.empty()&&!file.flush())throw std::runtime_error("Could not finish saving the previews.");return bytes;
    }throw std::runtime_error("Too many update redirects.");
}
std::wstring Quote(const std::wstring& value){std::wstring result=L"\"";std::size_t slashes=0;for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;}else{result.append(slashes,L'\\');result+=c;}slashes=0;}result.append(slashes*2,L'\\');return result+L'\"';}
bool StartProcess(const Path& executable,const std::wstring& arguments,bool hidden){
    std::wstring command=Quote(executable.wstring())+(arguments.empty()?L"":L" "+arguments);STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=hidden?SW_HIDE:SW_SHOWNORMAL;PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,hidden?CREATE_NO_WINDOW:0,nullptr,executable.parent_path().c_str(),&startup,&process))return false;CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
std::uint64_t Started(HANDLE process){FILETIME start{},exit{},kernel{},user{};if(!GetProcessTimes(process,&start,&exit,&kernel,&user))throw std::runtime_error("Could not identify the running program.");return(static_cast<std::uint64_t>(start.dwHighDateTime)<<32)|start.dwLowDateTime;}
std::uint64_t Number(const std::string& text){std::uint64_t number=0;auto result=std::from_chars(text.data(),text.data()+text.size(),number);if(result.ec!=std::errc()||result.ptr!=text.data()+text.size())throw std::runtime_error("Invalid updater request.");return number;}
bool Restore(const Path& target,const Path& backup,const std::string& hash){try{if(Normal(target)&&AppUpdateData::HexHash(AppUpdateData::Read(target,AppUpdateData::MaximumBytes))==hash)return true;if(!Normal(backup)||AppUpdateData::HexHash(AppUpdateData::Read(backup,AppUpdateData::MaximumBytes))!=hash)return false;Path failed=target;failed+=L".failed-update";if(Normal(target))CopyFileW(target.c_str(),failed.c_str(),TRUE);return MoveFileExW(backup.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;}catch(...){return false;}}
void Cleanup(){try{const auto root=Storage()/L"stage";if(!std::filesystem::exists(root)||!Normal(root))return;for(const auto& item:std::filesystem::directory_iterator(root)){auto path=item.path();if(!Identifier(path.filename().wstring())||!Normal(path)||!item.is_directory()||std::filesystem::file_time_type::clock::now()-item.last_write_time()<std::chrono::hours(24))continue;std::error_code error;for(const auto* name:{L"helper.exe",L"download.exe",L"previews.cab",L"request.json",L"request.json.tmp"}){auto file=path/name;if(Normal(file)&&std::filesystem::is_regular_file(file))std::filesystem::remove(file,error);}std::filesystem::remove(path,error);}}catch(...){}}
int Apply(const Path& request){
    auto stage=request.parent_path();if(request.filename()!=L"request.json"||!Identifier(stage.filename().wstring())||stage.parent_path()!=Storage()/L"stage"||!Normal(request)||Executable()!=stage/L"helper.exe")throw std::runtime_error("Invalid updater request location.");
    auto data=OffsetData::Parse(AppUpdateData::Read(request,65536));auto release=AppUpdateData::Manifest(data.At("envelope").Str());if(AppUpdateData::Compare(release.version,GOAT_APP_VERSION)<=0)throw std::runtime_error("The update is not newer than this build.");
    if(AppUpdateData::HexHash(AppUpdateData::Read(Executable(),AppUpdateData::MaximumBytes))!=data.At("old_sha256").Str())throw std::runtime_error("The updater does not match the existing program.");
    const Path target=std::filesystem::u8path(data.At("target").Str());const Path replacement=target.parent_path()/(target.filename().wstring()+L".update-"+stage.filename().wstring()+L".exe");
    if(target.extension()!=L".exe"||!Normal(target)||target==Executable()||!Normal(replacement))throw std::runtime_error("Invalid update destination.");AppUpdateData::Binary(replacement,release);
    const auto pid=static_cast<DWORD>(OffsetData::Uint(data.At("parent_pid")));if(!pid)throw std::runtime_error("Invalid parent program.");HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(parent){struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{parent};wchar_t text[32768]{};DWORD length=32768;if(Started(parent)!=Number(data.At("parent_started").Str())||!QueryFullProcessImageNameW(parent,0,text,&length)||_wcsicmp(target.c_str(),text)!=0)throw std::runtime_error("The updater parent program changed.");if(WaitForSingleObject(parent,30000)!=WAIT_OBJECT_0)throw std::runtime_error("The running program did not close. The update was not installed.");}else if(GetLastError()!=ERROR_INVALID_PARAMETER)throw std::runtime_error("Could not wait for the running program.");
    if(AppUpdateData::HexHash(AppUpdateData::Read(target,AppUpdateData::MaximumBytes))!=data.At("old_sha256").Str())throw std::runtime_error("The existing executable changed. The update was not installed.");AppUpdateData::Binary(replacement,release);
    Path backup=target;backup+=L".previous";if(std::filesystem::exists(backup)){if(!Normal(backup)||!DeleteFileW(backup.c_str()))throw std::runtime_error("Could not replace the previous-version backup.");}
    if(!ReplaceFileW(target.c_str(),replacement.c_str(),backup.c_str(),0,nullptr,nullptr)){if(!Restore(target,backup,data.At("old_sha256").Str()))throw std::runtime_error("Update replacement failed. Restore the .previous backup beside the executable.");throw std::runtime_error("Could not replace the executable. The existing version was kept.");}
    if(!StartProcess(target,L"",false)){if(!Restore(target,backup,data.At("old_sha256").Str()))throw std::runtime_error("The new program could not start. Restore the .previous backup beside the executable.");StartProcess(target,L"",false);throw std::runtime_error("The new program could not start. The previous version was restored.");}
    Log("Installed application version "+release.version);return 0;
}
}
const char* CurrentVersion(){return GOAT_APP_VERSION;}
State Get(){auto s=Shared();Load(s);std::lock_guard<std::mutex> lock(s->mutex);return s->state;}
void Begin(){auto s=Shared();Load(s);s->cancel=false;Cleanup();if(Get().automatic)Check(false);}
void SetAutomatic(bool enabled){auto s=Shared();Load(s);std::lock_guard<std::mutex> lock(s->mutex);s->state.automatic=enabled;try{Save(*s);}catch(const std::exception& error){s->state.message=error.what();}}
void SuppressStartupPrompt(){auto s=Shared();std::lock_guard<std::mutex> lock(s->mutex);s->state.suppressed=true;}
void Check(bool manual){
    auto s=Shared();Load(s);{std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase==Phase::Checking||s->state.phase==Phase::Downloading||s->state.phase==Phase::Ready||s->state.phase==Phase::HandedOff||s->state.previewPhase==PreviewPhase::Downloading)return;s->state.phase=Phase::Checking;s->state.manual=manual;s->state.message="Checking for updates...";if(manual)s->state.suppressed=false;}
    try{std::thread([s,manual]{try{auto text=Fetch(L"https://raw.githubusercontent.com/coolboygamer4597/goatgoatgoatgoatgoat/codex/update-feed/stable.json",32768,s);auto release=AppUpdateData::Manifest(text);const bool missing=!PreviewUpdateData::Present(Executable().parent_path()/L"preview-cache");std::lock_guard<std::mutex> lock(s->mutex);s->release=release;s->state.version=release.version;s->state.notes=release.notes;
        bool newer=AppUpdateData::Compare(release.version,GOAT_APP_VERSION)>0;bool skipped=!s->skipped.empty()&&AppUpdateData::Compare(release.version,s->skipped)==0;s->state.phase=newer&&(!skipped||s->state.manual)?Phase::Available:Phase::Idle;s->state.message=newer&&skipped?"This version was skipped.":"You have the latest published version.";
        s->state.previewsAvailable=release.previews.available;s->state.previewsMissing=missing;s->state.previewBytes=release.previews.bytes;s->state.previewPhase=PreviewPhase::Idle;s->updateAfterPreviews=false;
        if(!newer&&missing&&release.previews.available&&(s->previewDeclined!=GOAT_APP_VERSION||manual))s->state.previewPhase=PreviewPhase::Offered;
    }catch(const std::exception& error){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.message=error.what();}}).detach();}catch(...){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.message="Could not start the update check.";}
}
void Skip(){auto s=Shared();std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase==Phase::Available){s->skipped=s->state.version;try{Save(*s);}catch(const std::exception& error){Log(error.what());}}s->state.phase=Phase::Idle;s->state.manual=false;s->state.message.clear();}
void Download(){
    auto s=Shared();AppUpdateData::Release release;{std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase!=Phase::Available||s->state.previewPhase==PreviewPhase::Downloading)return;release=s->release;s->state.phase=Phase::Downloading;s->state.progress=0;s->state.message="Downloading and verifying the update...";}
    try{std::thread([s,release]{Path replacement;bool copied=false;try{const auto url=AppUpdateData::ReleaseUrl(release);auto bytes=Fetch(std::wstring(url.begin(),url.end()),release.bytes,s,[s,release](std::size_t done){std::lock_guard<std::mutex> lock(s->mutex);s->state.progress=static_cast<float>(done)/release.bytes;});if(s->cancel)throw std::runtime_error("Update cancelled.");
        auto stage=Storage()/L"stage"/Identifier();std::filesystem::create_directories(stage);if(!Normal(stage))throw std::runtime_error("Update storage is not a normal directory.");Write(stage/L"download.exe",bytes);AppUpdateData::Binary(stage/L"download.exe",release);const auto target=Executable();if(!Normal(target))throw std::runtime_error("The executable is not in a supported update location.");replacement=target.parent_path()/(target.filename().wstring()+L".update-"+stage.filename().wstring()+L".exe");
        if(!CopyFileW((stage/L"download.exe").c_str(),replacement.c_str(),TRUE))throw std::runtime_error("Cannot write beside the executable. Move the program to a writable folder and try again.");copied=true;AppUpdateData::Binary(replacement,release);
        if(!CopyFileW(target.c_str(),(stage/L"helper.exe").c_str(),TRUE))throw std::runtime_error("Could not prepare the temporary updater.");const auto old=AppUpdateData::HexHash(AppUpdateData::Read(target,AppUpdateData::MaximumBytes));
        Write(stage/L"request.json","{\"target\":"+OffsetData::Quote(target.u8string())+",\"parent_pid\":"+std::to_string(GetCurrentProcessId())+",\"parent_started\":"+OffsetData::Quote(std::to_string(Started(GetCurrentProcess())))+",\"old_sha256\":"+OffsetData::Quote(old)+",\"envelope\":"+OffsetData::Quote(release.envelope)+"}");
        if(s->cancel)throw std::runtime_error("Update cancelled.");std::lock_guard<std::mutex> lock(s->mutex);s->request=stage/L"request.json";s->state.phase=Phase::Ready;s->state.message="Update verified. Restarting...";s->state.progress=1;
    }catch(const std::exception& error){if(copied){std::error_code ignored;std::filesystem::remove(replacement,ignored);}std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.manual=true;s->state.message=error.what();}}).detach();}catch(...){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.manual=true;s->state.message="Could not start the update download.";}
}
void OfferPreviewsForUpdate(){auto s=Shared();std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase==Phase::Available&&s->state.previewsAvailable&&s->state.previewsMissing){s->updateAfterPreviews=true;s->state.previewPhase=PreviewPhase::Offered;}}
void SkipPreviews(){
    auto s=Shared();bool update=false;{
        std::lock_guard<std::mutex> lock(s->mutex);if(s->state.previewPhase==PreviewPhase::Downloading){s->previewCancel=true;return;}
        s->previewDeclined=s->updateAfterPreviews?s->release.version:GOAT_APP_VERSION;s->state.previewPhase=PreviewPhase::Idle;update=s->updateAfterPreviews;s->updateAfterPreviews=false;
        try{Save(*s);}catch(const std::exception& error){Log(error.what());}
    }if(update)Download();
}
void DownloadPreviews(){
    auto s=Shared();AppUpdateData::Release release;{
        std::lock_guard<std::mutex> lock(s->mutex);if(!s->state.previewsAvailable||s->state.previewPhase==PreviewPhase::Downloading||s->state.phase==Phase::Downloading||s->state.phase==Phase::Ready||s->state.phase==Phase::HandedOff)return;
        release=s->release;s->previewCancel=false;s->state.previewPhase=PreviewPhase::Downloading;s->state.previewProgress=0;s->state.previewMessage="Downloading skin previews...";
    }
    try{std::thread([s,release]{Path archive,storage;bool update=false;
        try{
            const auto id=Identifier();storage=Storage()/L"stage"/id;std::filesystem::create_directories(storage);if(!Normal(storage))throw std::runtime_error("Preview download storage is unavailable.");archive=storage/L"previews.cab";
            const auto base=Executable().parent_path();if(!Normal(base))throw std::runtime_error("The program folder is not supported for preview downloads.");
            const std::string url="https://github.com/"+std::string(AppUpdateData::Repository)+"/releases/download/v"+release.version+"/preview-cache.cab";
            Fetch(std::wstring(url.begin(),url.end()),release.previews.bytes,s,[s,release](std::size_t received){std::lock_guard<std::mutex> lock(s->mutex);s->state.previewProgress=.9f*static_cast<float>(received)/release.previews.bytes;},archive,&s->previewCancel);
            {std::lock_guard<std::mutex> lock(s->mutex);s->state.previewMessage="Verifying and installing skin previews...";}
            PreviewUpdateData::Install(archive,release.previews,base/L"preview-cache",base/(L".preview-cache-stage-"+id),base/(L".preview-cache-previous-"+id),s->previewCancel);
            {std::lock_guard<std::mutex> lock(s->mutex);s->state.previewPhase=PreviewPhase::Complete;s->state.previewsMissing=false;s->state.previewProgress=1;s->state.previewMessage="Skin previews installed.";update=s->updateAfterPreviews;s->updateAfterPreviews=false;
                if(!update){s->state.manual=true;s->state.message="Skin previews are installed and ready for the skin changer.";}}
        }catch(const std::exception& error){std::lock_guard<std::mutex> lock(s->mutex);s->state.previewPhase=PreviewPhase::Error;s->state.previewMessage=error.what();}
        if(!archive.empty()){std::error_code ignored;std::filesystem::remove(archive,ignored);std::filesystem::remove(storage,ignored);}
        if(s->previewCancel&&!s->cancel)SkipPreviews();else if(update&&!s->cancel)Download();
    }).detach();}catch(...){std::lock_guard<std::mutex> lock(s->mutex);s->state.previewPhase=PreviewPhase::Error;s->state.previewMessage="Could not start the preview download.";}
}
bool Launch(){auto s=Shared();std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase!=Phase::Ready)return false;if(!StartProcess(s->request.parent_path()/L"helper.exe",L"--apply-update "+Quote(s->request.wstring()),true)){s->state.phase=Phase::Error;s->state.manual=true;s->state.message="Could not start the updater. Your current version is unchanged.";return false;}s->state.phase=Phase::HandedOff;return true;}
void Shutdown(){auto s=Shared();s->cancel=true;s->previewCancel=true;}
bool HelperMode(int& exitCode){int count=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&count);if(!arguments)return false;struct Free{LPWSTR* p;~Free(){LocalFree(p);}} free{arguments};if(count<2||std::wstring(arguments[1])!=L"--apply-update")return false;exitCode=1;try{if(count!=3)throw std::runtime_error("Invalid updater arguments.");exitCode=Apply(Path(arguments[2]));}catch(const std::exception& error){Log(error.what());}return true;}
}
