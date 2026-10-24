#pragma once
#include <cstdint>
#include <string>
namespace AntiAfk {
inline bool enabled=false;
inline std::string error;
struct Values {std::uint32_t timeout=0;std::uint8_t disabled=0;};
struct Patch {
 Values original;bool captured=false;
 template<class Read,class WriteTimeout,class WriteDisabled>
 bool Restore(Read read,WriteTimeout timeout,WriteDisabled disabled){
  if(!captured)return true;Values current;if(!read(current))return false;

  bool a=current.timeout!=86400||current.timeout==original.timeout||timeout(original.timeout);
  bool b=current.disabled!=1||current.disabled==original.disabled||disabled(original.disabled);
  if(a&&b)captured=false;return !captured;
 }
 template<class Read,class WriteTimeout,class WriteDisabled>
 bool Apply(Read read,WriteTimeout timeout,WriteDisabled disabled){
  Values current;if(!read(current)||current.timeout>31536000||current.disabled>1)return false;
  if(!captured){original=current;captured=true;}
  bool a=current.timeout==86400||timeout(86400);bool b=a&&(current.disabled==1||disabled(1));
  if(!b){Restore(read,timeout,disabled);return false;}return true;
 }
};
}
