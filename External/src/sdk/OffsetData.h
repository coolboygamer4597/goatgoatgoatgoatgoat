#pragma once
#include <string>
#include <map>
#include <vector>
#include <regex>
#include <cstdint>
#include <stdexcept>
#include <limits>
#include <charconv>

namespace OffsetData {
struct Json {
    enum Kind { Null, String, Number, Boolean, Object } kind=Null;
    std::string text;
    std::map<std::string,Json> object;
    const Json& At(const std::string& key) const {
        if(kind!=Object)throw std::runtime_error("Expected a JSON object.");
        auto it=object.find(key);if(it==object.end())throw std::runtime_error("Missing field: "+key);
        return it->second;
    }
    std::string Str() const { if(kind!=String)throw std::runtime_error("Expected a string.");return text; }
};
inline void Utf8Append(std::string& out,unsigned cp) {
    if(cp<0x80)out+=char(cp);
    else if(cp<0x800){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}
    else if(cp<0x10000){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
    else {out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
}
class Parser {
    const std::string& s;size_t p=0;
    void Space(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
    char Take(){if(p==s.size())throw std::runtime_error("Truncated JSON.");return s[p++];}
    unsigned Hex(){unsigned n=0;for(int i=0;i<4;++i){char c=Take();unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(d>15)throw std::runtime_error("Invalid JSON escape.");n=n*16+d;}return n;}
    std::string String(){
        if(Take()!='"')throw std::runtime_error("Expected a quoted JSON string.");
        std::string out;while(true){char c=Take();if(c=='"')return out;if(static_cast<unsigned char>(c)<32)throw std::runtime_error("Invalid JSON string.");
            if(c!='\\'){out+=c;continue;}c=Take();
            switch(c){case '"':case '\\':case '/':out+=c;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
            case 'u':{unsigned cp=Hex();if(cp>=0xd800&&cp<=0xdbff){if(Take()!='\\'||Take()!='u')throw std::runtime_error("Invalid Unicode pair.");unsigned low=Hex();if(low<0xdc00||low>0xdfff)throw std::runtime_error("Invalid Unicode pair.");cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)throw std::runtime_error("Invalid Unicode pair.");Utf8Append(out,cp);break;}
            default:throw std::runtime_error("Invalid JSON escape.");}
        }
    }
    Json Value(int depth){
        if(depth>12)throw std::runtime_error("JSON nesting limit exceeded.");Space();if(p==s.size())throw std::runtime_error("Empty JSON value.");Json v;
        if(s[p]=='{'){v.kind=Json::Object;++p;Space();if(p<s.size()&&s[p]=='}'){++p;return v;}
            while(true){Space();auto key=String();Space();if(Take()!=':')throw std::runtime_error("Missing JSON colon.");auto item=Value(depth+1);if(!v.object.emplace(key,std::move(item)).second)throw std::runtime_error("Duplicate JSON field: "+key);if(v.object.size()>2048)throw std::runtime_error("Too many JSON fields.");Space();char c=Take();if(c=='}')return v;if(c!=',')throw std::runtime_error("Invalid JSON object.");}
        }
        if(s[p]=='"'){v.kind=Json::String;v.text=String();return v;}
        for(auto literal:{"true","false","null"})if(s.compare(p,std::char_traits<char>::length(literal),literal)==0){p+=std::char_traits<char>::length(literal);v.kind=literal[0]=='n'?Json::Null:Json::Boolean;v.text=literal;return v;}
        size_t start=p;if(s[p]=='-')++p;if(p==s.size()||s[p]<'0'||s[p]>'9')throw std::runtime_error("Invalid JSON value.");
        if(s[p]=='0')++p;else while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;
        if(p<s.size()&&(s[p]=='.'||s[p]=='e'||s[p]=='E'))throw std::runtime_error("Only integer JSON numbers are supported.");
        v.kind=Json::Number;v.text=s.substr(start,p-start);return v;
    }
public:
    explicit Parser(const std::string& text):s(text){if(s.size()>524288)throw std::runtime_error("Offsets file is too large.");if(s.compare(0,3,"\xef\xbb\xbf")==0)p=3;}
    Json Parse(){auto value=Value(0);Space();if(p!=s.size())throw std::runtime_error("Unexpected content after JSON.");return value;}
};
inline Json Parse(const std::string& text){return Parser(text).Parse();}
inline std::uint64_t Unsigned(const std::string& text){
    std::uint64_t value=0;int base=10;size_t begin=0;if(text.size()>2&&text[0]=='0'&&(text[1]=='x'||text[1]=='X')){begin=2;base=16;}
    if(begin==text.size())throw std::runtime_error("Invalid offset value.");
    auto r=std::from_chars(text.data()+begin,text.data()+text.size(),value,base);
    if(r.ec!=std::errc()||r.ptr!=text.data()+text.size()||value>0xffffffffULL)throw std::runtime_error("Offset value is invalid or out of range.");return value;
}
inline std::uint64_t Uint(const Json& value){if(value.kind!=Json::Number&&value.kind!=Json::String)throw std::runtime_error("Expected an integer offset.");return Unsigned(value.text);}
inline bool Version(const std::string& text){return std::regex_match(text,std::regex("version-[0-9a-f]{16,64}"));}
inline std::string Quote(const std::string& text){
    std::string out="\"";const char* hex="0123456789abcdef";
    for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
struct Package {std::string version;unsigned revision=1;std::map<std::string,std::uint64_t> fields;};
inline Package JsonPackage(const std::string& text){
    auto json=Parse(text);if(Uint(json.At("schema"))!=1)throw std::runtime_error("Unsupported offsets schema.");Package p;p.version=json.At("version").Str();p.revision=static_cast<unsigned>(Uint(json.At("revision")));
    if(!Version(p.version)||!p.revision)throw std::runtime_error("Invalid offsets version or revision.");auto& offsets=json.At("offsets");if(offsets.kind!=Json::Object)throw std::runtime_error("Missing offsets object.");
    for(auto& pair:offsets.object)p.fields.emplace(pair.first,Uint(pair.second));return p;
}
inline std::string WithoutComments(const std::string& s){
    std::string out;bool quoted=false,escaped=false;for(size_t i=0;i<s.size();++i){char c=s[i];if(quoted){out+=c;if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}if(c=='"'){quoted=true;out+=c;continue;}
        if(c=='/'&&i+1<s.size()&&s[i+1]=='/'){while(i<s.size()&&s[i]!='\n')++i;out+='\n';}
        else if(c=='/'&&i+1<s.size()&&s[i+1]=='*'){auto end=s.find("*/",i+2);if(end==std::string::npos)throw std::runtime_error("Unterminated header comment.");i=end+1;out+=' ';}else out+=c;
    }return out;
}
inline Package HeaderPackage(const std::string& source){
    if(source.size()>524288)throw std::runtime_error("Offsets file is too large.");auto text=WithoutComments(source);Package p;std::smatch match;
    if(std::regex_search(text,match,std::regex(R"RX(\bClientVersion\s*=\s*"(version-[0-9a-f]{16,64})")RX")))p.version=match[1];
    else {std::regex version("version-[0-9a-f]{16,64}");for(std::sregex_iterator it(source.begin(),source.end(),version),end;it!=end;++it){if(!p.version.empty()&&p.version!=it->str())throw std::runtime_error("Conflicting header versions.");p.version=it->str();}}
    if(!Version(p.version))throw std::runtime_error("The header must identify its Roblox version.");
    std::regex token(R"RX(\bnamespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\{|[{}]|\b([A-Za-z_]\w*)\s*=\s*(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*\s*;)RX");std::vector<std::string> scopes;
    for(std::sregex_iterator it(text.begin(),text.end(),token),end;it!=end;++it){auto m=*it;if(m[1].matched)scopes.push_back(m[1]);else if(m.str()=="{")scopes.push_back("");else if(m.str()=="}"){if(scopes.empty())throw std::runtime_error("Unbalanced header.");scopes.pop_back();}else if(m[2].matched){
        std::string key;for(auto& scope:scopes)if(!scope.empty()&&scope!="Offsets"&&scope!="offsets"){if(!key.empty())key+='.';auto name=scope;size_t at;while((at=name.find("::"))!=std::string::npos)name.replace(at,2,".");key+=name;}
        if(key.empty())continue;key+='.'+m[2].str();if(!p.fields.emplace(key,Unsigned(m[3])).second)throw std::runtime_error("Duplicate header offset: "+key);
    }}
    if(!scopes.empty())throw std::runtime_error("Unbalanced header.");return p;
}
inline std::string Serialize(const Package& p){std::string out="{\"schema\":1,\"version\":"+Quote(p.version)+",\"revision\":"+std::to_string(p.revision)+",\"offsets\":{";bool comma=false;for(auto& pair:p.fields){if(comma)out+=',';comma=true;out+=Quote(pair.first)+':'+std::to_string(pair.second);}return out+"}}";}
inline void Validate(const Package& p,const std::string& version,const std::vector<std::string>& required){
    if(p.version!=version)throw std::runtime_error("Offsets target "+p.version+", but Roblox is "+version+".");
    for(auto& key:required)if(!p.fields.count(key))throw std::runtime_error("Missing required offset: "+key);
}
}
