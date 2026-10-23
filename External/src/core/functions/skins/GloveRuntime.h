#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GloveRuntime {
using Size = std::array<float,3>;
struct SizeChange {
    std::uintptr_t field=0;
    Size original{},wanted{};
};
using Sizes = std::array<SizeChange,2>;
inline bool ValidSize(const Size& size) {
    for (float value : size)
        if (!std::isfinite(value)||value<=0.00001f||value>=10000.0f) return false;
    return true;
}
template<class Reader>
bool ResolveSizes(Reader&& read,std::uintptr_t stockPart,std::uintptr_t sourcePart,
    std::uintptr_t stockPrimitive,std::uintptr_t sourcePrimitive,std::uintptr_t primitiveSize,Sizes& result) {
    auto pointer=[](std::uintptr_t value){return value>=0x10000&&value<0x00007FFFFFFFF000ULL;};
    if (!pointer(stockPart)||!pointer(sourcePart)||!pointer(stockPrimitive)||!pointer(sourcePrimitive)||
        stockPart==sourcePart||stockPrimitive==sourcePrimitive||primitiveSize>0x1000) return false;
    constexpr std::uintptr_t initialMeshSize=0x210;
    Sizes candidate;
    candidate[0].field=stockPart+initialMeshSize;
    candidate[1].field=stockPrimitive+primitiveSize;
    const std::uintptr_t sources[]={sourcePart+initialMeshSize,sourcePrimitive+primitiveSize};
    for (std::size_t i=0;i<candidate.size();++i)
        if (!read(candidate[i].field,candidate[i].original.data(),sizeof(Size))||
            !read(sources[i],candidate[i].wanted.data(),sizeof(Size))||
            !ValidSize(candidate[i].original)||!ValidSize(candidate[i].wanted)) return false;
    result=candidate;
    return true;
}
inline bool NeedsAssembly(const std::vector<std::string>& rootClasses) {
    unsigned parts=0;
    for (const auto& cls : rootClasses)
        if (cls=="MeshPart"&&++parts>2) return true;
    return false;
}
using Frame = std::array<float,12>;
using Frames = std::array<Frame,2>;
inline bool ValidFrame(const Frame& frame) {
    for (float value : frame)
        if (!std::isfinite(value) || std::abs(value)>10.0f) return false;
    for (int row=0;row<3;++row)
        for (int other=row;other<3;++other) {
            float dot=0;
            for (int col=0;col<3;++col) dot+=frame[row*3+col]*frame[other*3+col];
            if (std::abs(dot-(row==other?1.0f:0.0f))>0.002f) return false;
        }
    const float determinant=frame[0]*(frame[4]*frame[8]-frame[5]*frame[7])-
        frame[1]*(frame[3]*frame[8]-frame[5]*frame[6])+frame[2]*(frame[3]*frame[7]-frame[4]*frame[6]);
    return std::abs(determinant-1.0f)<0.003f;
}
struct Attachment {
    std::uintptr_t stockFrames=0,sourceFrames=0,part0Offset=0,part1Offset=0;
    Frames original{},wanted{};
};
template<class Reader>
bool ResolveAttachment(Reader&& read,std::uintptr_t stockMotor,std::uintptr_t sourceMotor,
    std::uintptr_t stockPart,std::uintptr_t sourcePart,Attachment& result,std::uintptr_t templateSourcePart=0) {
    auto pointer=[](std::uintptr_t value){return value>=0x10000&&value<0x00007FFFFFFFF000ULL;};
    if (!pointer(stockMotor)||!pointer(sourceMotor)||!pointer(stockPart)||!pointer(sourcePart)) return false;
    for (std::uintptr_t offset : {std::uintptr_t(0xF8),std::uintptr_t(0xF0)}) {
        std::uintptr_t stockData=0,sourceData=0,stockLink=0,sourceLink=0;
        if (!read(stockMotor+offset,&stockData,sizeof(stockData))||!read(sourceMotor+offset,&sourceData,sizeof(sourceData))||
            !pointer(stockData)||!pointer(sourceData)||stockData==sourceData||
            !read(stockMotor+offset+0x20,&stockLink,sizeof(stockLink))||stockLink!=stockPart||
            !read(sourceMotor+offset+0x20,&sourceLink,sizeof(sourceLink))||
            (sourceLink!=sourcePart&&(!pointer(templateSourcePart)||sourceLink!=templateSourcePart))) continue;
        Attachment candidate;
        candidate.stockFrames=stockData+0x38;candidate.sourceFrames=sourceData+0x38;
        candidate.part0Offset=offset+0x10;candidate.part1Offset=offset+0x20;
        if (!read(candidate.stockFrames,candidate.original.data(),sizeof(candidate.original))||
            !read(candidate.sourceFrames,candidate.wanted.data(),sizeof(candidate.wanted))) continue;
        if (ValidFrame(candidate.original[0])&&ValidFrame(candidate.original[1])&&
            ValidFrame(candidate.wanted[0])&&ValidFrame(candidate.wanted[1])) {result=candidate;return true;}
    }
    return false;
}
struct RetryGate {
    using Clock=std::chrono::steady_clock;
    std::string context;
    unsigned failures=0;
    Clock::time_point next{};
    bool Ready(const std::string& key,Clock::time_point now) {
        if (context!=key) {context=key;failures=0;next={};}
        return now>=next;
    }
    void Failed(Clock::time_point now) {
        if (failures<7) ++failures;
        const auto delay=failures>=7?10000:250*(1u<<(failures-1));
        next=now+std::chrono::milliseconds(delay);
    }
    void Reset() {context.clear();failures=0;next={};}
};
}
