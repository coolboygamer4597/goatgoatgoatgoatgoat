#pragma once
#include "AppUpdateData.h"
#include "CustomTextureData.h"
#include <setupapi.h>
#include <set>
#include <functional>
#include <atomic>
#pragma comment(lib,"setupapi.lib")
namespace PreviewUpdateData {
using Path=std::filesystem::path;
enum class Kind{Previews,Textures};
inline bool Normal(const Path& path){
    if(!path.is_absolute()||path.lexically_normal()!=path)return false;
    Path part=path.root_path();
    for(const auto& name:path.relative_path()){part/=name;const auto flags=GetFileAttributesW(part.c_str());if(flags==INVALID_FILE_ATTRIBUTES||(flags&FILE_ATTRIBUTE_REPARSE_POINT))return false;}
    return true;
}
inline bool Name(const std::wstring& name,Kind kind=Kind::Previews){
    if(kind==Kind::Textures)return CustomTextureData::KnownFile(name);
    auto hex=[](const std::wstring& text){return std::all_of(text.begin(),text.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');});};
    if(name==L"manifest.json")return true;
    if(name.size()==20&&hex(name.substr(0,16)))return name.substr(16)==L".png"||name.substr(16)==L".ref"||name.substr(16)==L".bin";
    return name.size()==75&&name.substr(0,7)==L"models/"&&hex(name.substr(7,64))&&name.substr(71)==L".bin";
}
inline bool Present(const Path& directory,Kind kind=Kind::Previews,const std::string& revision={}){
    try{
        if(!Normal(directory)||!std::filesystem::is_directory(directory))return false;
        if(kind==Kind::Textures){for(int i=0;i<static_cast<int>(CustomTextureData::Presets.size());++i)for(int g=0;g<CustomTextureData::Presets[i].groups;++g)for(auto c:{CustomTextureData::Channel::Color,CustomTextureData::Channel::Material,CustomTextureData::Channel::Normal})if(!Normal(directory/CustomTextureData::Name(i,c,g)))return false;
            for(int i=0;i<static_cast<int>(CustomTextureData::Presets.size());++i){if(!Normal(directory/CustomTextureData::SourceName(i))||!Normal(directory/CustomTextureData::SourceName(i,CustomTextureData::Channel::Material))||!Normal(directory/CustomTextureData::ModelName(i))||!Normal(directory/CustomTextureData::PreviewName(i))||(CustomTextureData::Presets[i].model<=2&&!Normal(directory/CustomTextureData::PreviewName(i,true))))return false;}
            if(!CustomTextureData::Present(directory))return false;
            if(!revision.empty()){const auto record=directory/L".package-sha256";if(!Normal(record)||!std::filesystem::is_regular_file(record)||std::filesystem::file_size(record)!=64||AppUpdateData::Read(record,64)!=revision)return false;}
            return true;}
        const auto manifest=directory/L"manifest.json";
        if(!Normal(manifest)||!std::filesystem::is_regular_file(manifest)||!std::filesystem::file_size(manifest))return false;
        bool image=false,reference=false,model=false;
        for(const auto& item:std::filesystem::directory_iterator(directory)){
            if(item.is_regular_file()&&Normal(item.path())&&std::filesystem::file_size(item.path())){
                image|=item.path().extension()==L".png";reference|=item.path().extension()==L".ref";
            }
        }
        const auto models=directory/L"models";
        if(Normal(models)&&std::filesystem::is_directory(models))for(const auto& item:std::filesystem::directory_iterator(models))
            if(item.is_regular_file()&&item.path().extension()==L".bin"&&Normal(item.path())&&item.file_size()){model=true;break;}
        return image&&reference&&model;
    }catch(...){return false;}
}
inline std::string FileHash(const Path& file,std::size_t maximum){
    std::ifstream stream(file,std::ios::binary);if(!stream)throw std::runtime_error("Could not read the preview download.");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    struct Close{BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Close(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}}close{algorithm,hash};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Preview verification is unavailable.");
    std::array<char,65536> buffer{};std::size_t total=0;
    while(stream){stream.read(buffer.data(),buffer.size());const auto size=static_cast<std::size_t>(stream.gcount());
        if(size>maximum-total)throw std::runtime_error("Preview download exceeds its size limit.");total+=size;
        if(size&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(size),0)<0)throw std::runtime_error("Preview verification failed.");
    }
    if(!stream.eof())throw std::runtime_error("Preview download could not be read completely.");
    std::array<unsigned char,32> digest{};if(BCryptFinishHash(hash,digest.data(),32,0)<0)throw std::runtime_error("Preview verification failed.");
    std::string result;for(auto c:digest){result+="0123456789abcdef"[c>>4];result+="0123456789abcdef"[c&15];}return result;
}
struct Extraction{
    Path root;const AppUpdateData::Previews& package;std::set<std::wstring> names;std::size_t bytes=0;
    const std::atomic<bool>& cancelled;std::string error;Kind kind=Kind::Previews;
    void Cleanup()noexcept{try{for(const auto& name:names){auto path=root/Path(name);if(Normal(path))std::filesystem::remove(path);}const auto revision=root/L".package-sha256";if(Normal(revision))std::filesystem::remove(revision);std::filesystem::remove(root/L"models");std::filesystem::remove(root);}catch(...){}}
};
inline UINT CALLBACK Extract(PVOID context,UINT notification,UINT_PTR first,UINT_PTR){
    auto& state=*static_cast<Extraction*>(context);
    try{
        if(state.cancelled)throw std::runtime_error("Preview download cancelled.");
        if(notification==SPFILENOTIFY_NEEDNEWCABINET)throw std::runtime_error("Split preview archives are not supported.");
        if(notification==SPFILENOTIFY_FILEEXTRACTED){auto info=reinterpret_cast<FILEPATHS_W*>(first);if(info->Win32Error)throw std::runtime_error("Could not extract the previews.");return NO_ERROR;}
        if(notification!=SPFILENOTIFY_FILEINCABINET)return NO_ERROR;
        auto info=reinterpret_cast<FILE_IN_CABINET_INFO_W*>(first);std::wstring name=info->NameInCabinet;std::replace(name.begin(),name.end(),L'\\',L'/');
        if(!Name(name,state.kind)||!state.names.insert(name).second||state.names.size()>state.package.files||!info->FileSize||info->FileSize>64*1024*1024||info->FileSize>state.package.expanded-state.bytes||
           (info->DosAttribs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("Invalid file in the preview download.");
        state.bytes+=info->FileSize;const auto target=state.root/Path(name);
        if(!Normal(target.parent_path())||std::filesystem::exists(target)||target.wstring().size()>=MAX_PATH)throw std::runtime_error("Unsupported preview installation path.");
        wcscpy_s(info->FullTargetName,target.c_str());return FILEOP_DOIT;
    }catch(const std::exception& error){state.error=error.what();SetLastError(ERROR_INVALID_DATA);return notification==SPFILENOTIFY_FILEINCABINET?FILEOP_ABORT:ERROR_INVALID_DATA;}
}
inline void Install(const Path& archive,const AppUpdateData::Previews& package,const Path& target,const Path& stage,const Path& previous,const std::atomic<bool>& cancelled,Kind kind=Kind::Previews){
    if(!package.available||!Normal(archive)||std::filesystem::file_size(archive)!=package.bytes)throw std::runtime_error("The preview download failed its signed checksum.");
    if(target.filename()!=(kind==Kind::Textures?L"custom-textures":L"preview-cache")||stage.parent_path()!=target.parent_path()||previous.parent_path()!=target.parent_path()||stage==target||previous==target||stage==previous||
       !Normal(target.parent_path())||std::filesystem::exists(stage)||std::filesystem::exists(previous))throw std::runtime_error("Unsupported preview installation folder.");
    HANDLE lock=CreateFileW(archive.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not lock the verified preview archive.");
    struct Close{HANDLE h;~Close(){CloseHandle(h);}}close{lock};
    if(FileHash(archive,kind==Kind::Textures?AppUpdateData::MaximumAddonBytes:AppUpdateData::MaximumPreviewBytes)!=package.sha256)throw std::runtime_error("The preview download failed its signed checksum.");
    std::filesystem::create_directory(stage);std::filesystem::create_directory(stage/L"models");
    Extraction state{stage,package,{},0,cancelled,{},kind};
    try{
        if(!SetupIterateCabinetW(archive.c_str(),0,Extract,&state)||!state.error.empty()||state.names.size()!=package.files||state.bytes!=package.expanded||!Present(stage,kind))throw std::runtime_error(state.error.empty()?"The downloaded add-on is incomplete.":state.error);
        if(cancelled)throw std::runtime_error("Preview download cancelled.");
        if(kind==Kind::Textures){std::ofstream revision(stage/L".package-sha256",std::ios::binary|std::ios::trunc);if(!revision||!revision.write(package.sha256.data(),package.sha256.size())||!revision.flush())throw std::runtime_error("Could not save the installed texture version.");}
        bool moved=false;
        if(std::filesystem::exists(target)){if(!Normal(target)||!MoveFileExW(target.c_str(),previous.c_str(),0))throw std::runtime_error("Could not preserve the existing preview folder.");moved=true;}
        if(!MoveFileExW(stage.c_str(),target.c_str(),0)){
            if(moved&&!MoveFileExW(previous.c_str(),target.c_str(),0))throw std::runtime_error("Could not install previews. The old cache is preserved in the previous-cache folder.");
            throw std::runtime_error("Could not install previews beside the executable. Use a writable program folder.");
        }
    }catch(...){state.Cleanup();throw;}
}
}
