#pragma once
#include <filesystem>
#include <string>
namespace AppUpdates {
enum class Phase{Idle,Checking,Available,Downloading,Ready,HandedOff,Error};
enum class PreviewPhase{Idle,Offered,Downloading,Complete,Error};
struct State{Phase phase=Phase::Idle;bool automatic=true,manual=false,suppressed=false;float progress=0;std::string version,notes,message;
    PreviewPhase previewPhase=PreviewPhase::Idle;bool previewsAvailable=false,previewsMissing=false;float previewProgress=0;std::size_t previewBytes=0;std::string previewMessage;};
const char* CurrentVersion();
void Begin();
State Get();
void SetAutomatic(bool enabled);
void Check(bool manual=true);
void Skip();
void SuppressStartupPrompt();
void Download();
void OfferPreviewsForUpdate();
void DownloadPreviews();
void SkipPreviews();
bool Launch();
void Shutdown();
bool HelperMode(int& exitCode);
}
