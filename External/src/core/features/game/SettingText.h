#pragma once
#include <string>
#include <cstring>
namespace SettingText {
inline std::string Encode(const std::string& value){static const char h[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out+=h[c>>4];out+=h[c&15];}return out;}
inline bool Decode(const std::string& text,std::string& out,std::size_t maximum){
    if(text.size()%2||text.size()>maximum*2)return false;
    const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    std::string next;for(std::size_t i=0;i<text.size();i+=2){const int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0||(a==0&&b==0))return false;next+=char(a*16+b);}out=std::move(next);return true;
}
}
