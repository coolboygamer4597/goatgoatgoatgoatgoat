#pragma once
#include <string>
namespace UpdateChannels {
enum class Channel{Stable,Experimental};
inline const char* Name(Channel channel){return channel==Channel::Experimental?"experimental":"stable";}
inline std::wstring Feed(Channel channel){return channel==Channel::Experimental?L"experimental.json":L"stable.json";}
inline constexpr Channel Default=Channel::Stable;
}
