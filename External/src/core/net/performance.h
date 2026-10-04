#pragma once
#include <Windows.h>
#include <psapi.h>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#pragma comment(lib,"psapi.lib")
namespace Performance {
using Clock=std::chrono::steady_clock;
enum Part { Menu, PlayerCache, SilentAim, Native, Mesh, Overlay, Present, Count };
inline std::atomic<bool> enabled{false};
inline std::mutex mutex;
inline std::array<double,Count> totals{}, average{};
inline unsigned frames=0;
inline double fps=0,cpu=0,memoryMb=0,skinCpu=0;
inline std::atomic<unsigned long long> skinTicks{0};
inline unsigned long long Ticks(FILETIME t){return (static_cast<unsigned long long>(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
inline unsigned long long ThreadTicks(){FILETIME a{},b{},k{},u{};GetThreadTimes(GetCurrentThread(),&a,&b,&k,&u);return Ticks(k)+Ticks(u);}
struct WorkerScope {bool on;unsigned long long start;WorkerScope():on(enabled.load()),start(on?ThreadTicks():0){}~WorkerScope(){if(on)skinTicks.fetch_add(ThreadTicks()-start);}};
struct Scope {
 Part part;bool on;Clock::time_point start;
 Scope(Part p):part(p),on(enabled.load()),start(on?Clock::now():Clock::time_point{}){}
 ~Scope(){if(on){double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();std::lock_guard<std::mutex> lock(mutex);totals[part]+=ms;}}
};
inline void Frame(){if(enabled.load())++frames;}
inline void Sample(bool on){
 static auto last=Clock::now();static unsigned long long lastCpu=0;static bool was=false;
 enabled.store(on);
 if(on!=was){std::lock_guard<std::mutex> lock(mutex);totals={};frames=0;skinTicks=0;lastCpu=0;last=Clock::now();was=on;}
 if(!on)return;
 const auto now=Clock::now();const double elapsed=std::chrono::duration<double>(now-last).count();if(elapsed<0.5)return;
 FILETIME a{},b{},k{},u{};GetProcessTimes(GetCurrentProcess(),&a,&b,&k,&u);auto ticks=Ticks(k)+Ticks(u);
 const auto cores=(std::max)(1u,std::thread::hardware_concurrency());
 cpu=lastCpu?100.0*double(ticks-lastCpu)/(elapsed*1e7*cores):0;lastCpu=ticks;
 skinCpu=100.0*double(skinTicks.exchange(0))/(elapsed*1e7*cores);
 PROCESS_MEMORY_COUNTERS pm{};if(GetProcessMemoryInfo(GetCurrentProcess(),&pm,sizeof(pm)))memoryMb=double(pm.WorkingSetSize)/(1024*1024);
 {std::lock_guard<std::mutex> lock(mutex);for(int i=0;i<Count;++i)average[i]=frames?totals[i]/frames:0;totals={};}
 fps=frames/elapsed;frames=0;last=now;
}
}
