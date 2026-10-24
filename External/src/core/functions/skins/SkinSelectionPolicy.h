#pragma once
#include <string>

namespace SkinSelectionPolicy {
inline void ResolveShared0416(const std::string& previousAr,const std::string& previousGlock,
    std::string& ar,std::string& glock,bool heldGlock){
    if(ar!="0416"||glock!="0416")return;
    const bool arChanged=previousAr!=ar,glockChanged=previousGlock!=glock;
    if(arChanged&&!glockChanged)glock="Default";
    else if(glockChanged&&!arChanged)ar="Default";
    else if(heldGlock)ar="Default";
    else glock="Default";
}
inline bool Overlaps(unsigned long long live,unsigned long long source,
    unsigned long long otherLive,unsigned long long otherSource){
    return live==otherLive||live==otherSource||source==otherLive||source==otherSource;
}
}
