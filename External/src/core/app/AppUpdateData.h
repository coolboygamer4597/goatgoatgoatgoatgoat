#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <charconv>
#include <string>
#include <vector>
#include <stdexcept>
#include "AppVersion.h"
#include "UpdateTrust.h"
#include "UpdateChannels.h"
#include "../../sdk/OffsetData.h"
#pragma comment(lib,"bcrypt.lib")
namespace AppUpdateData {
inline constexpr const char* Repository="coolboygamer4597/goatgoatgoatgoatgoat";
inline constexpr const char* Asset="goatgoatgoatgoat.exe";
inline constexpr std::size_t MaximumBytes=64*1024*1024;
inline std::array<unsigned,3> Version(std::string text){
    std::array<unsigned,3> result{};std::size_t at=0;int parts=0;
    while(at<text.size() && parts<3){auto end=text.find('.',at);if(end==std::string::npos)end=text.size();
        if(end==at || end-at>4 || (end-at>1 && text[at]=='0'))throw std::runtime_error("Invalid release version.");
        auto converted=std::from_chars(text.data()+at,text.data()+end,result[parts++]);
        if(converted.ec!=std::errc() || converted.ptr!=text.data()+end)throw std::runtime_error("Invalid release version.");
        at=end+1;if(end+1==text.size())throw std::runtime_error("Invalid release version.");
    }
    if(parts<2 || at<=text.size())throw std::runtime_error("Invalid release version.");return result;
}
inline int Compare(const std::string& first,const std::string& second){const auto a=Version(first),b=Version(second);return a<b?-1:a>b?1:0;}
inline bool NewerPublication(std::uint64_t sequence,const std::string& version){return sequence?sequence>GOAT_UPDATE_SEQUENCE:Compare(version,GOAT_APP_VERSION)>0;}
inline std::vector<unsigned char> Unhex(const std::string& text,std::size_t bytes){
    if(text.size()!=bytes*2)throw std::runtime_error("Invalid update checksum or signature.");
    std::vector<unsigned char> result(bytes);auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    for(std::size_t i=0;i<bytes;++i){int a=digit(text[i*2]),b=digit(text[i*2+1]);if(a<0||b<0)throw std::runtime_error("Invalid update checksum or signature.");result[i]=static_cast<unsigned char>(a*16+b);}return result;
}
inline std::array<unsigned char,32> Hash(const std::string& bytes){
    BCRYPT_ALG_HANDLE algorithm=nullptr;if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA-256 is unavailable.");
    struct Close{BCRYPT_ALG_HANDLE h;~Close(){BCryptCloseAlgorithmProvider(h,0);}} close{algorithm};std::array<unsigned char,32> digest{};
    if(bytes.size()>MaximumBytes || BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),digest.data(),32)<0)throw std::runtime_error("Update checksum failed.");return digest;
}
inline std::string HexHash(const std::string& bytes){std::string text;const char* alphabet="0123456789abcdef";for(auto c:Hash(bytes)){text+=alphabet[c>>4];text+=alphabet[c&15];}return text;}
inline std::string Read(const std::filesystem::path& file,std::size_t limit){
    std::ifstream stream(file,std::ios::binary|std::ios::ate);if(!stream)throw std::runtime_error("Could not read update file.");auto length=stream.tellg();
    if(length<0 || static_cast<unsigned long long>(length)>limit)throw std::runtime_error("Update file exceeds the size limit.");
    std::string bytes(static_cast<std::size_t>(length),'\0');stream.seekg(0);if(length&&!stream.read(bytes.data(),length))throw std::runtime_error("Update file could not be read completely.");return bytes;
}
inline void Signature(const std::string& payload,const std::string& signature){
    auto bytes=Unhex(signature,64);auto digest=Hash(payload);BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;
    struct Close{BCRYPT_ALG_HANDLE& a;BCRYPT_KEY_HANDLE& k;~Close(){if(k)BCryptDestroyKey(k);if(a)BCryptCloseAlgorithmProvider(a,0);}} close{algorithm,key};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0)<0 || BCryptImportKeyPair(algorithm,nullptr,BCRYPT_ECCPUBLIC_BLOB,&key,const_cast<PUCHAR>(PublicKey),sizeof(PublicKey),0)<0 || BCryptVerifySignature(key,nullptr,digest.data(),32,bytes.data(),64,0)<0)throw std::runtime_error("The update signature could not be verified.");
}
inline constexpr std::size_t MaximumPreviewBytes=256*1024*1024;
inline constexpr std::size_t MaximumAddonBytes=1024*1024*1024;
struct Previews{bool available=false;std::string sha256;std::size_t bytes=0,expanded=0,files=0;std::string asset;};
struct Release{std::string version,displayVersion,tag,channel,notes,sha256,envelope;std::uint64_t sequence=0;std::size_t bytes=0;Previews previews,textures;};
inline bool Label(const std::string& text){return !text.empty()&&text.size()<=64&&std::all_of(text.begin(),text.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='.'||c=='-'||c=='_';});}
inline Release Manifest(const std::string& source,UpdateChannels::Channel channel=UpdateChannels::Default){
    if(source.size()>32768)throw std::runtime_error("Update information is too large.");auto envelope=OffsetData::Parse(source);auto payload=envelope.At("payload").Str();Signature(payload,envelope.At("signature").Str());
    auto data=OffsetData::Parse(payload);if(OffsetData::Uint(data.At("schema"))!=1 || data.At("app").Str()!="GoatGoatGoat" || data.At("channel").Str()!=UpdateChannels::Name(channel) || data.At("asset").Str()!=Asset)throw std::runtime_error("Unsupported application update.");
    Release result;result.version=data.At("version").Str();Version(result.version);result.channel=data.At("channel").Str();result.displayVersion=result.version;result.tag="v"+result.version;if(auto value=data.object.find("display_version");value!=data.object.end()){result.displayVersion=value->second.Str();if(!Label(result.displayVersion))throw std::runtime_error("Invalid display version.");}if(auto value=data.object.find("release_tag");value!=data.object.end()){result.tag=value->second.Str();if(!Label(result.tag))throw std::runtime_error("Invalid release tag.");}if(auto value=data.object.find("sequence");value!=data.object.end()){result.sequence=OffsetData::Uint(value->second);if(!result.sequence)throw std::runtime_error("Invalid publication sequence.");}result.notes=data.At("notes").Str();result.sha256=data.At("sha256").Str();Unhex(result.sha256,32);result.bytes=static_cast<std::size_t>(OffsetData::Uint(data.At("bytes")));
    if(result.bytes<1024 || result.bytes>MaximumBytes || result.notes.size()>8192)throw std::runtime_error("Invalid application update size.");
    if(auto entry=data.object.find("previews");entry!=data.object.end()){
        const auto& pack=entry->second;
        if(pack.At("asset").Str()!="preview-cache.cab")throw std::runtime_error("Unsupported preview package.");
        auto& p=result.previews;p.sha256=pack.At("sha256").Str();Unhex(p.sha256,32);
        p.bytes=static_cast<std::size_t>(OffsetData::Uint(pack.At("bytes")));p.expanded=static_cast<std::size_t>(OffsetData::Uint(pack.At("expanded_bytes")));p.files=static_cast<std::size_t>(OffsetData::Uint(pack.At("files")));
        if(p.bytes<64||p.bytes>MaximumPreviewBytes||!p.expanded||p.expanded>512*1024*1024||p.files<4||p.files>2048)throw std::runtime_error("Invalid preview package size.");p.available=true;
    }
    if(auto entry=data.object.find("textures");entry!=data.object.end()){
        const auto& pack=entry->second;
        if(pack.At("asset").Str()!="custom-textures.cab")throw std::runtime_error("Unsupported texture package.");
        auto& p=result.textures;p.sha256=pack.At("sha256").Str();Unhex(p.sha256,32);
        p.bytes=static_cast<std::size_t>(OffsetData::Uint(pack.At("bytes")));p.expanded=static_cast<std::size_t>(OffsetData::Uint(pack.At("expanded_bytes")));p.files=static_cast<std::size_t>(OffsetData::Uint(pack.At("files")));
        if(p.bytes<64||p.bytes>MaximumPreviewBytes||!p.expanded||p.expanded>512*1024*1024||p.files<2||p.files>128)throw std::runtime_error("Invalid texture package size.");p.available=true;
    }
    if(auto entry=data.object.find("customs");entry!=data.object.end()){
        const auto& pack=entry->second;auto& p=result.textures;
        if(pack.At("asset").Str()!="custom-textures-v2.cab")throw std::runtime_error("Unsupported custom skin package.");
        p.asset="custom-textures-v2.cab";p.sha256=pack.At("sha256").Str();Unhex(p.sha256,32);
        p.bytes=static_cast<std::size_t>(OffsetData::Uint(pack.At("bytes")));p.expanded=static_cast<std::size_t>(OffsetData::Uint(pack.At("expanded_bytes")));p.files=static_cast<std::size_t>(OffsetData::Uint(pack.At("files")));
        if(p.bytes<64||p.bytes>MaximumAddonBytes||!p.expanded||p.expanded>1024*1024*1024||p.files<2||p.files>512)throw std::runtime_error("Invalid custom skin package size.");p.available=true;
    }
    if(result.textures.asset.empty())result.textures.available=false;
    for(unsigned char c:result.notes)if(c<32 && c!='\n' && c!='\r' && c!='\t')throw std::runtime_error("Invalid release notes.");result.envelope=source;return result;
}
inline void Binary(const std::filesystem::path& file,const Release& release){
    auto bytes=Read(file,MaximumBytes);if(bytes.size()!=release.bytes || HexHash(bytes)!=release.sha256)throw std::runtime_error("The downloaded update does not match its signed checksum.");
    DWORD type=0;if(!GetBinaryTypeW(file.c_str(),&type)||type!=SCS_64BIT_BINARY)throw std::runtime_error("The update is not a 64-bit Windows program.");
}
inline std::string ReleaseUrl(const Release& release){Version(release.version);const auto tag=release.tag.empty()?"v"+release.version:release.tag;if(!Label(tag))throw std::runtime_error("Invalid release tag.");return "https://github.com/"+std::string(Repository)+"/releases/download/"+tag+"/"+Asset;}
}
