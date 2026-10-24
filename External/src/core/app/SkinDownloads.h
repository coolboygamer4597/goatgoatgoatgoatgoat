#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace SkinDownloads {
enum class Phase{Idle,Checking,Offered,Downloading,Complete,Error};
struct Item{int index=-1,category=0;std::string name,skin;std::size_t bytes=0;bool installed=false;};
struct State{Phase phase=Phase::Idle;bool visible=false,previewsOnly=false;unsigned revision=0;int preferred=-1;float progress=0;std::size_t commonBytes=0,previewBytes=0,newerSkins=0;bool commonInstalled=false,previewsInstalled=false,previewsAvailable=false;std::string message;std::vector<Item> items;};
void Begin();
void Open(int preferred=-1);
void OpenPreviews();
void VisitPreviews();
State Get();
void Dismiss();
void Download(const std::vector<int>& selected,bool previews);
void Cancel();
void Shutdown();
}
