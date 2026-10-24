#include "SkinDownloads.h"
#include "SkinDownloadData.h"
#include <winhttp.h>
#include <objbase.h>
#include <atomic>
#include <cwctype>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"ole32.lib")
namespace SkinDownloads {
namespace {
using Path=std::filesystem::path;
struct Manager{std::mutex mutex;std::atomic<bool> cancel{false};State state;SkinDownloadData::Catalog catalog;bool previewsVisited=false,firstPreviewOffer=false;};
std::shared_ptr<Manager> Shared(){static auto s=std::make_shared<Manager>();return s;}
Path Base(){return RuntimePaths::File(L"custom-textures").parent_path();}
Path Settings(){wchar_t text[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",text,32768);if(!n||n>=32768)throw std::runtime_error("Local skin settings are unavailable.");return Path(text)/L"GoatGoatGoat"/L"skin-previews-declined.txt";}
std::string Declined(){try{return AppUpdateData::Read(Settings(),64);}catch(...){return {};}}
void Remember(const std::string& key){try{const auto file=Settings();std::filesystem::create_directories(file.parent_path());if(!PreviewUpdateData::Normal(file.parent_path())||(std::filesystem::exists(file)&&!PreviewUpdateData::Normal(file)))return;std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(key.data(),key.size());}catch(...){}}
struct Http{HINTERNET h=nullptr;~Http(){if(h)WinHttpCloseHandle(h);}};
struct Address{std::wstring host,path;};
Address ParseUrl(const std::wstring& url){
    if(url.size()>16384||url.find(L'#')!=std::wstring::npos)throw std::runtime_error("Invalid skin download address.");URL_COMPONENTS p{};p.dwStructSize=sizeof(p);p.dwHostNameLength=p.dwUrlPathLength=p.dwExtraInfoLength=p.dwUserNameLength=p.dwPasswordLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&p)||p.nScheme!=INTERNET_SCHEME_HTTPS||p.nPort!=443||p.dwUserNameLength||p.dwPasswordLength)throw std::runtime_error("Skin downloads require trusted HTTPS.");
    Address a{std::wstring(p.lpszHostName,p.dwHostNameLength),std::wstring(p.lpszUrlPath,p.dwUrlPathLength)};std::transform(a.host.begin(),a.host.end(),a.host.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
    if(a.host==L"raw.githubusercontent.com"){if(a.path!=L"/coolboygamer4597/Ekte-lidenskap-har-et-navn/main/skin-catalog-v3.json")throw std::runtime_error("Unexpected skin catalog location.");}
    else if(a.host==L"github.com"){if(a.path.rfind(L"/coolboygamer4597/Ekte-lidenskap-har-et-navn/releases/download/skins-",0)!=0)throw std::runtime_error("Unexpected skin repository.");}
    else if(a.host!=L"release-assets.githubusercontent.com"&&a.host!=L"objects.githubusercontent.com")throw std::runtime_error("Unexpected skin download server.");
    if(p.dwExtraInfoLength)a.path.append(p.lpszExtraInfo,p.dwExtraInfoLength);return a;
}
std::string Fetch(std::wstring url,std::size_t maximum,const std::shared_ptr<Manager>& s,const Path& output={},const std::function<void(std::size_t)>& progress={}){
    Http session{WinHttpOpen(L"GoatSkinLibrary/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};if(!session.h)throw std::runtime_error("Could not start the skin download.");WinHttpSetTimeouts(session.h,2000,2000,5000,5000);DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(session.h,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));const auto started=GetTickCount64();
    for(int redirects=0;redirects<6;++redirects){
        if(s->cancel)throw std::runtime_error("Skin download cancelled.");const auto address=ParseUrl(url);Http connection{WinHttpConnect(session.h,address.host.c_str(),443,0)},request{connection.h?WinHttpOpenRequest(connection.h,L"GET",address.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
        if(!request.h||!WinHttpSendRequest(request.h,L"Cache-Control: no-cache\r\n",static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Could not reach the skin server. You can continue without downloading.");
        DWORD code=0,size=sizeof(code);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid skin server response.");
        if(code==301||code==302||code==303||code==307||code==308){size=0;WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&size,WINHTTP_NO_HEADER_INDEX);if(!size||size>32768)throw std::runtime_error("Invalid skin download redirect.");std::vector<wchar_t> location(size/sizeof(wchar_t)+1);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid skin redirect.");url=location.data();if(!url.empty()&&url[0]==L'/')url=L"https://"+address.host+url;continue;}
        if(code!=200)throw std::runtime_error(code==404?"The skin library is not available yet. Please retry later.":"Skin server returned HTTP "+std::to_string(code)+".");
        std::string bytes;std::ofstream file;if(!output.empty()){file.open(output,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Could not save the skin download.");}char buffer[65536];DWORD count=0;std::size_t received=0;
        do{if(s->cancel||GetTickCount64()-started>(output.empty()?30000ULL:900000ULL))throw std::runtime_error("Skin download cancelled or timed out.");if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&count))throw std::runtime_error("The skin download was interrupted.");if(count>maximum-received)throw std::runtime_error("Skin download exceeds its size limit.");received+=count;if(output.empty())bytes.append(buffer,count);else if(count&&!file.write(buffer,count))throw std::runtime_error("Could not save the skin archive.");if(progress)progress(received);}while(count);
        if(!output.empty()&&!file.flush())throw std::runtime_error("Could not finish saving the skin archive.");return bytes;
    }throw std::runtime_error("Too many skin download redirects.");
}
std::wstring Id(){GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Could not prepare a skin download.");std::wstring result;for(std::size_t i=0;i<sizeof(id);++i){const auto c=reinterpret_cast<unsigned char*>(&id)[i];result+=L"0123456789abcdef"[c>>4];result+=L"0123456789abcdef"[c&15];}return result;}
void Populate(Manager& s){
    s.state.newerSkins=s.catalog.newer;s.state.items.clear();s.state.previewsAvailable=false;s.state.previewBytes=s.state.commonBytes=0;s.state.commonInstalled=false;s.state.previewsInstalled=false;
    for(const auto& p:s.catalog.packages){const bool installed=SkinDownloadData::Present(Base(),p);if(p.index==-1){s.state.commonBytes=p.bytes;s.state.commonInstalled=installed;}else if(p.index==-2){s.state.previewsAvailable=true;s.state.previewBytes=p.bytes;s.state.previewsInstalled=installed;}else{const auto& preset=CustomTextureData::Presets[p.index];s.state.items.push_back({p.index,preset.category,preset.name,preset.skin,p.bytes,installed});}}
    std::sort(s.state.items.begin(),s.state.items.end(),[](const Item& a,const Item& b){return a.index<b.index;});++s.state.revision;
}
void Offer(Manager& s,const std::string& declined,const std::string& previewKey){
        if(s.state.previewsOnly){if(s.firstPreviewOffer&&(s.state.previewsInstalled||declined==previewKey))s.state.visible=false;s.state.phase=s.state.previewsInstalled?Phase::Complete:Phase::Offered;s.state.message=s.state.previewsInstalled?"Skin previews are already installed.":s.state.previewsAvailable?"Optional previews for the skin changer. Nothing downloads until you choose Download previews.":"Skin previews are not available yet.";}
        else{const bool installed=s.state.commonInstalled&&!s.state.items.empty()&&std::all_of(s.state.items.begin(),s.state.items.end(),[](const Item& item){return item.installed;});const bool complete=installed&&!s.state.newerSkins;s.state.phase=complete?Phase::Complete:Phase::Offered;s.state.message=complete?"All available skins already installed.":s.state.newerSkins?"New skins are available. Update the application to install them. You can still download supported skins below.":"Optional custom skins. Choose everything or select individual skins. Nothing is selected by default.";}
        if(!s.state.visible)s.state.phase=Phase::Idle;
}
void Check(bool manual,int preferred,bool previewsOnly=false,bool firstPreviewOffer=false){
    const auto s=Shared();{std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase==Phase::Downloading)return;s->state.visible=manual;s->state.preferred=preferred;s->state.previewsOnly=previewsOnly;s->firstPreviewOffer=firstPreviewOffer;if(s->state.phase==Phase::Checking){s->cancel=false;return;}s->cancel=false;s->state.phase=Phase::Checking;s->state.message=previewsOnly?"Checking skin previews...":"Checking available skins...";}
    try{std::thread([s]{try{auto catalog=SkinDownloadData::Parse(Fetch(L"https://raw.githubusercontent.com/coolboygamer4597/Ekte-lidenskap-har-et-navn/main/skin-catalog-v3.json?v="+std::to_wstring(GetTickCount64()),512*1024,s));std::string previewKey;for(const auto& p:catalog.packages)if(p.index==-2)previewKey=p.sha256;const auto declined=Declined();std::lock_guard<std::mutex> lock(s->mutex);s->catalog=std::move(catalog);Populate(*s);
        Offer(*s,declined,previewKey);
    }catch(const std::exception& error){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=s->state.visible?Phase::Error:Phase::Idle;s->state.message=error.what();}}).detach();}catch(...){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.message="Could not start the skin catalog check.";}
}
}
void Begin(){}
void Open(int preferred){Check(true,preferred);}
void OpenPreviews(){Check(true,-1,true);}
void VisitPreviews(){const auto s=Shared();{std::lock_guard<std::mutex> lock(s->mutex);if(s->previewsVisited)return;s->previewsVisited=true;}if(!PreviewUpdateData::Present(Base()/L"preview-cache"))Check(true,-1,true,true);}

State Get(){const auto s=Shared();std::lock_guard<std::mutex> lock(s->mutex);return s->state;}
void Dismiss(){const auto s=Shared();std::string key;{std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase==Phase::Downloading)return;if(s->state.previewsOnly)for(const auto& p:s->catalog.packages)if(p.index==-2)key=p.sha256;if(s->state.phase==Phase::Checking)s->cancel=true;s->state.visible=false;if(s->state.phase!=Phase::Checking)s->state.phase=Phase::Idle;}if(!key.empty())Remember(key);}

void Cancel(){Shared()->cancel=true;}
void Download(const std::vector<int>& selected,bool previews){
    const auto s=Shared();std::vector<SkinDownloadData::Package> packages;std::string tag;
    {std::lock_guard<std::mutex> lock(s->mutex);if(s->state.phase!=Phase::Offered&&s->state.phase!=Phase::Error)return;std::set<int> choices;for(int index:selected)if(CustomTextureData::Available(index)&&!s->state.previewsOnly)choices.insert(index);previews=previews&&s->state.previewsOnly;const bool commonNeeded=!choices.empty();
        for(const auto& p:s->catalog.packages)if(((p.index==-1&&commonNeeded)||(p.index==-2&&previews)||choices.count(p.index))&&!SkinDownloadData::Present(Base(),p))packages.push_back(p);
        std::stable_sort(packages.begin(),packages.end(),[](const auto& a,const auto& b){return a.index<b.index;});if(packages.empty()){s->state.message="Selected skins are already installed.";s->state.phase=Phase::Complete;return;}tag=s->catalog.tag;s->cancel=false;s->state.phase=Phase::Downloading;s->state.progress=0;s->state.message="Preparing selected downloads...";}
    try{std::thread([s,packages,tag]{try{
        std::size_t total=0,expanded=0,largest=0,completed=0;for(const auto& p:packages){total+=p.bytes;expanded+=p.expanded;largest=std::max(largest,p.bytes);}if(std::filesystem::space(Base()).available<expanded+largest)throw std::runtime_error("There is not enough free space for the selected skins.");
        for(const auto& p:packages){if(s->cancel)throw std::runtime_error("Skin download cancelled.");const auto stage=Base()/(L".skin-stage-"+Id());SkinDownloadData::Directory(stage);bool preserve=false;
            try{const auto archive=stage/L"package.cab";const std::string url="https://github.com/"+std::string(SkinDownloadData::Repository)+"/releases/download/"+tag+"/"+p.asset;
                {std::lock_guard<std::mutex> lock(s->mutex);s->state.message=p.index==-1?"Downloading common skin resources...":p.index==-2?"Downloading skin previews...":"Downloading "+std::string(CustomTextureData::Presets[p.index].name)+"...";}
                Fetch(std::wstring(url.begin(),url.end()),p.bytes,s,archive,[s,total,completed](std::size_t bytes){std::lock_guard<std::mutex> lock(s->mutex);s->state.progress=static_cast<float>(completed+bytes)/static_cast<float>(total);});
                SkinDownloadData::Install(archive,Base(),p,s->cancel);
            }catch(const SkinDownloadData::RecoveryError&){preserve=true;throw;}catch(...){SkinDownloadData::Cleanup(stage,p);throw;}
            if(!preserve)SkinDownloadData::Cleanup(stage,p);completed+=p.bytes;
        }
        std::lock_guard<std::mutex> lock(s->mutex);Populate(*s);s->state.progress=1;s->state.phase=Phase::Complete;s->state.message=s->state.previewsOnly?"Skin previews are installed.":"Selected skins are installed. You can use them now.";
    }catch(const std::exception& error){std::lock_guard<std::mutex> lock(s->mutex);Populate(*s);s->state.phase=Phase::Error;s->state.message=error.what();}}).detach();}catch(...){std::lock_guard<std::mutex> lock(s->mutex);s->state.phase=Phase::Error;s->state.message="Could not start the skin download.";}
}
void Shutdown(){Shared()->cancel=true;}
}
