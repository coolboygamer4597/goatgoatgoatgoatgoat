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
struct Release{std::string version,notes,sha256,envelope;std::size_t bytes=0;};
inline Release Manifest(const std::string& source){
    if(source.size()>32768)throw std::runtime_error("Update information is too large.");auto envelope=OffsetData::Parse(source);auto payload=envelope.At("payload").Str();Signature(payload,envelope.At("signature").Str());
    auto data=OffsetData::Parse(payload);if(OffsetData::Uint(data.At("schema"))!=1 || data.At("app").Str()!="GoatGoatGoat" || data.At("channel").Str()!="stable" || data.At("asset").Str()!=Asset)throw std::runtime_error("Unsupported application update.");
    Release result;result.version=data.At("version").Str();Version(result.version);result.notes=data.At("notes").Str();result.sha256=data.At("sha256").Str();Unhex(result.sha256,32);result.bytes=static_cast<std::size_t>(OffsetData::Uint(data.At("bytes")));
    if(result.bytes<1024 || result.bytes>MaximumBytes || result.notes.size()>8192)throw std::runtime_error("Invalid application update size.");
    for(unsigned char c:result.notes)if(c<32 && c!='\n' && c!='\r' && c!='\t')throw std::runtime_error("Invalid release notes.");result.envelope=source;return result;
}
inline void Binary(const std::filesystem::path& file,const Release& release){
    auto bytes=Read(file,MaximumBytes);if(bytes.size()!=release.bytes || HexHash(bytes)!=release.sha256)throw std::runtime_error("The downloaded update does not match its signed checksum.");
    DWORD type=0;if(!GetBinaryTypeW(file.c_str(),&type)||type!=SCS_64BIT_BINARY)throw std::runtime_error("The update is not a 64-bit Windows program.");
}
inline std::string ReleaseUrl(const Release& release){Version(release.version);return "https://github.com/"+std::string(Repository)+"/releases/download/v"+release.version+"/"+Asset;}
}
