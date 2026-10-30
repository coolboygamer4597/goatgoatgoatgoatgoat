#pragma once
#include <cstddef>
namespace NativeTextureBudget {
inline constexpr std::size_t Limit=320ull*1024*1024;
inline bool KeepPrevious(std::size_t previous,std::size_t next){return previous<=Limit&&next<=Limit-previous;}
}
