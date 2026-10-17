#pragma once
#include <filesystem>
#include <string>
namespace AppUpdates {
enum class Phase{Idle,Checking,Available,Downloading,Ready,HandedOff,Error};
struct State{Phase phase=Phase::Idle;bool automatic=true,manual=false,suppressed=false;float progress=0;std::string version,notes,message;};
const char* CurrentVersion();
void Begin();
State Get();
void SetAutomatic(bool enabled);
void Check(bool manual=true);
void Skip();
void SuppressStartupPrompt();
void Download();
bool Launch();
void Shutdown();
bool HelperMode(int& exitCode);
}
