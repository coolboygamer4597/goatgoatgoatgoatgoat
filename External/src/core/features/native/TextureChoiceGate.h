#pragma once
#include <chrono>
namespace TextureChoiceGate {
struct Gate {
    using Clock=std::chrono::steady_clock;
    int latest=-2;
    Clock::time_point changed{};
    bool Ready(int selection,Clock::time_point now){if(latest!=selection){latest=selection;changed=now;}return selection<0||now-changed>=std::chrono::milliseconds(150);}
    void Reset(){latest=-2;changed={};}
};
}
