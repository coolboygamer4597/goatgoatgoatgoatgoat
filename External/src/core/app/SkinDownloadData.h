#pragma once
#include "PreviewUpdateData.h"
#include <map>
#include <set>
#include <atomic>
#include <functional>
namespace SkinDownloadData {
using Path=std::filesystem::path;
inline constexpr const char* Repository="coolboygamer4597/Ekte-lidenskap-har-et-navn";
struct RecoveryError:std::runtime_error{using std::runtime_error::runtime_error;};
struct File{std::size_t bytes=0;std::string sha256;};
struct Package{std::string id,asset,sha256;int index=-1;std::size_t bytes=0,expanded=0;std::map<std::wstring,File> files;};
struct Catalog{std::size_t newer=0;std::string tag,key;std::vector<Package> packages;};
inline std::set<std::wstring> Expected(int index){
    std::set<std::wstring> result;
    if(index==-1){for(const wchar_t* name:{L"neutral-normal.png",L"native-x5.gwm",L"native-m4a1.gwm",L"native-0416.gwm",L"native-deagle.gwm",L"native-glock.gwm"})result.insert(name);const wchar_t* models[]={L"x5",L"m4a1",L"0416",L"deagle",L"glock"};const int groups[]={1,3,4,1,4};for(int i=0;i<5;++i)for(int g=0;g<groups[i];++g)result.insert(L"shadow-"+std::wstring(models[i])+L"-"+std::to_wstring(g)+L".png");}
    else if(CustomTextureData::Available(index)){if(CustomTextureData::RequiresModel(index)){result.insert(CustomTextureData::ModelName(index));result.insert(CustomTextureData::NativeModelName(index));}result.insert(CustomTextureData::PreviewName(index));for(int g=0;g<CustomTextureData::Presets[index].groups;++g)for(auto c:{CustomTextureData::Channel::Color,CustomTextureData::Channel::Material})result.insert(CustomTextureData::Name(index,c,g));}
    return result;
}
inline Catalog Parse(const std::string& envelope){
    if(envelope.size()>512*1024)throw std::runtime_error("Skin catalog exceeds its size limit.");
    const auto signedData=OffsetData::Parse(envelope);const auto payload=signedData.At("payload").Str();AppUpdateData::Signature(payload,signedData.At("signature").Str());const auto data=OffsetData::Parse(payload);
    if(OffsetData::Uint(data.At("schema"))!=1||data.At("kind").Str()!="GoatNativeSkins")throw std::runtime_error("Unsupported skin catalog.");
    Catalog result;result.tag=data.At("tag").Str();result.key=AppUpdateData::HexHash(payload);
    if(result.tag.rfind("skins-",0)!=0||result.tag.size()>64||result.tag.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=std::string::npos)throw std::runtime_error("Invalid skin release identifier.");
    const auto& packages=data.At("packages");if(packages.kind!=OffsetData::Json::Object||packages.object.size()>256)throw std::runtime_error("Invalid skin package list.");
    bool common=false;
    for(const auto& item:packages.object){
        Package p;p.id=item.first;
        if(p.id=="common"){p.index=-1;common=true;}
        else if(p.id=="previews")p.index=-2;
        else{if(p.id.rfind("skin-",0)!=0)throw std::runtime_error("Invalid skin package identifier.");const auto number=p.id.substr(5);unsigned index=0;const auto parsed=std::from_chars(number.data(),number.data()+number.size(),index);if(parsed.ec!=std::errc()||parsed.ptr!=number.data()+number.size()||std::to_string(index)!=number||index>4095)throw std::runtime_error("Invalid skin index.");if(!CustomTextureData::Available(static_cast<int>(index))){if(index>=CustomTextureData::Presets.size())++result.newer;continue;}p.index=static_cast<int>(index);}
        const auto& pack=item.second;p.sha256=pack.At("sha256").Str();AppUpdateData::Unhex(p.sha256,32);p.asset=pack.At("asset").Str();
        if(p.asset!="pack-"+p.id+"-"+p.sha256.substr(0,12)+".cab")throw std::runtime_error("Invalid skin archive name.");
        p.bytes=static_cast<std::size_t>(OffsetData::Uint(pack.At("bytes")));p.expanded=static_cast<std::size_t>(OffsetData::Uint(pack.At("expanded_bytes")));
        if(p.bytes<64||p.bytes>256*1024*1024||p.expanded<1||p.expanded>512*1024*1024)throw std::runtime_error("Invalid skin package size.");
        const auto& files=pack.At("files");if(files.kind!=OffsetData::Json::Object||files.object.empty()||files.object.size()>2048)throw std::runtime_error("Invalid skin archive contents.");
        std::size_t expanded=0;const auto expected=Expected(p.index);
        for(const auto& entry:files.object){
            const auto path=std::filesystem::u8path(entry.first);const auto name=path.generic_wstring();
            if(path.is_absolute()||name.find(L'\\')!=std::wstring::npos||path.lexically_normal()!=path||name.size()>128||name.find(L':')!=std::wstring::npos||name.find(L"..")!=std::wstring::npos||(p.index==-2?!PreviewUpdateData::Name(name):!expected.count(name)))throw std::runtime_error("Invalid file in skin package metadata.");
            File file;file.bytes=static_cast<std::size_t>(OffsetData::Uint(entry.second.At("bytes")));file.sha256=entry.second.At("sha256").Str();AppUpdateData::Unhex(file.sha256,32);
            if(!file.bytes||file.bytes>64*1024*1024||file.bytes>p.expanded-expanded)throw std::runtime_error("Invalid skin file size.");expanded+=file.bytes;p.files.emplace(name,std::move(file));
        }
        if(expanded!=p.expanded||(p.index!=-2&&p.files.size()!=expected.size()))throw std::runtime_error("Incomplete skin package metadata.");
        result.packages.push_back(std::move(p));
    }
    if(!common)throw std::runtime_error("The common skin resources are missing from the catalog.");return result;
}
inline Path Target(const Path& base,const Package& p){return base/(p.index==-2?L"preview-cache":L"custom-textures");}
inline std::wstring Marker(const Package& p){return L".skin-package-"+std::wstring(p.id.begin(),p.id.end())+L".sha256";}
inline bool Present(const Path& base,const Package& p){
    try{const auto target=Target(base,p);if(!PreviewUpdateData::Normal(target))return false;const auto marker=target/Marker(p);
        if(std::filesystem::exists(marker)&&(!PreviewUpdateData::Normal(marker)||AppUpdateData::Read(marker,64)!=p.sha256))return false;
        for(const auto& item:p.files){const auto file=target/item.first;if(!PreviewUpdateData::Normal(file)||!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)!=item.second.bytes)return false;if(p.index!=-2&&file.extension()==L".png"&&!CustomTextureData::ImagePresent(file))return false;if(file.extension()==L".gwm"&&!CustomModelData::Present(file))return false;}
        return p.index!=-2||PreviewUpdateData::Present(target);
    }catch(...){return false;}
}
inline void Directory(const Path& path){
    if(std::filesystem::exists(path)){if(!PreviewUpdateData::Normal(path)||!std::filesystem::is_directory(path))throw std::runtime_error("Unsupported skin installation folder.");return;}
    if(!PreviewUpdateData::Normal(path.parent_path())||!std::filesystem::create_directory(path))throw std::runtime_error("Could not create the skin installation folder.");
}
struct Extraction{Path root;const Package& package;const std::atomic<bool>& cancelled;std::set<std::wstring> seen;std::string error;};
inline UINT CALLBACK Extract(PVOID context,UINT notice,UINT_PTR first,UINT_PTR){
    auto& state=*static_cast<Extraction*>(context);
    try{
        if(state.cancelled)throw std::runtime_error("Skin download cancelled.");
        if(notice==SPFILENOTIFY_NEEDNEWCABINET)throw std::runtime_error("Split skin archives are not supported.");
        if(notice==SPFILENOTIFY_FILEEXTRACTED){if(reinterpret_cast<FILEPATHS_W*>(first)->Win32Error)throw std::runtime_error("Could not extract a skin file.");return NO_ERROR;}
        if(notice!=SPFILENOTIFY_FILEINCABINET)return NO_ERROR;
        auto info=reinterpret_cast<FILE_IN_CABINET_INFO_W*>(first);std::wstring name=info->NameInCabinet;std::replace(name.begin(),name.end(),L'\\',L'/');const auto expected=state.package.files.find(name);
        if(expected==state.package.files.end()||!state.seen.insert(name).second||info->FileSize!=expected->second.bytes||(info->DosAttribs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("Unexpected file in the skin download.");
        const auto target=state.root/name;if(!PreviewUpdateData::Normal(target.parent_path())||std::filesystem::exists(target)||target.wstring().size()>=MAX_PATH)throw std::runtime_error("Unsupported skin extraction path.");wcscpy_s(info->FullTargetName,target.c_str());return FILEOP_DOIT;
    }catch(const std::exception& error){state.error=error.what();SetLastError(ERROR_INVALID_DATA);return notice==SPFILENOTIFY_FILEINCABINET?FILEOP_ABORT:ERROR_INVALID_DATA;}
}
inline void Cleanup(const Path& stage,const Package& p)noexcept{
    try{if(!PreviewUpdateData::Normal(stage)||stage.filename().wstring().rfind(L".skin-stage-",0)!=0)return;
        for(const auto* folder:{L"new",L"old"}){const auto root=stage/folder;if(!PreviewUpdateData::Normal(root))continue;for(const auto& file:p.files){const auto path=root/file.first;if(PreviewUpdateData::Normal(path))std::filesystem::remove(path);}const auto marker=root/Marker(p);if(PreviewUpdateData::Normal(marker))std::filesystem::remove(marker);std::error_code error;std::filesystem::remove(root/L"models",error);std::filesystem::remove(root,error);}
        const auto cab=stage/L"package.cab";if(PreviewUpdateData::Normal(cab))std::filesystem::remove(cab);std::error_code error;std::filesystem::remove(stage,error);
    }catch(...){}}
inline void Install(const Path& archive,const Path& base,const Package& p,const std::atomic<bool>& cancelled){
    const auto stage=archive.parent_path(),fresh=stage/L"new",previous=stage/L"old",target=Target(base,p);
    if(archive.filename()!=L"package.cab"||stage.parent_path()!=base||stage.filename().wstring().rfind(L".skin-stage-",0)!=0||!PreviewUpdateData::Normal(base)||!PreviewUpdateData::Normal(archive)||std::filesystem::file_size(archive)!=p.bytes)throw std::runtime_error("Invalid skin installation request.");
    HANDLE locked=CreateFileW(archive.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(locked==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not lock the skin archive for verification.");struct Close{HANDLE h;~Close(){CloseHandle(h);}}close{locked};
    if(PreviewUpdateData::FileHash(archive,p.bytes)!=p.sha256)throw std::runtime_error("The skin archive does not match its signed checksum.");
    Directory(fresh);Directory(previous);Directory(fresh/L"models");Directory(previous/L"models");Extraction extraction{fresh,p,cancelled};
    std::vector<std::wstring> saved,installed;bool rollbackFailed=false;
    try{
        if(!SetupIterateCabinetW(archive.c_str(),0,Extract,&extraction)||!extraction.error.empty()||extraction.seen.size()!=p.files.size())throw std::runtime_error(extraction.error.empty()?"The skin download is incomplete.":extraction.error);
        for(const auto& item:p.files)if(PreviewUpdateData::FileHash(fresh/item.first,item.second.bytes)!=item.second.sha256)throw std::runtime_error("A downloaded skin file failed verification.");
        if(cancelled)throw std::runtime_error("Skin download cancelled.");Directory(target);if(p.index==-2)Directory(target/L"models");
        std::vector<std::wstring> names;for(const auto& file:p.files)names.push_back(file.first);names.push_back(Marker(p));
        {std::ofstream marker(fresh/Marker(p),std::ios::binary);if(!marker.write(p.sha256.data(),p.sha256.size())||!marker.flush())throw std::runtime_error("Could not save the installed skin revision.");}
        for(const auto& name:names){
            if(cancelled)throw std::runtime_error("Skin download cancelled.");const auto file=target/name;if(!PreviewUpdateData::Normal(file.parent_path()))throw std::runtime_error("Skin destination changed during installation.");
            if(std::filesystem::exists(file)){if(!PreviewUpdateData::Normal(file)||!std::filesystem::is_regular_file(file)||!MoveFileExW(file.c_str(),(previous/name).c_str(),0))throw std::runtime_error("Could not preserve an existing skin file.");saved.push_back(name);}
            if(!MoveFileExW((fresh/name).c_str(),file.c_str(),0))throw std::runtime_error("Could not install the selected skin. Use a writable program folder.");installed.push_back(name);
        }
    }catch(...){
        for(auto i=installed.rbegin();i!=installed.rend();++i){const auto path=target/ *i;if(!PreviewUpdateData::Normal(path)||!DeleteFileW(path.c_str()))rollbackFailed=true;}
        for(auto i=saved.rbegin();i!=saved.rend();++i)if(!MoveFileExW((previous/ *i).c_str(),(target/ *i).c_str(),0))rollbackFailed=true;
        if(rollbackFailed)throw RecoveryError("Skin installation stopped. Original files are preserved in "+stage.u8string());throw;
    }
}
}
