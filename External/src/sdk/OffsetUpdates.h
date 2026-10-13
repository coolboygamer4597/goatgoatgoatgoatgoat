#pragma once
#include "RuntimeOffsets.h"
#include "OffsetData.h"
#include <filesystem>
#include <functional>

namespace OffsetUpdates {
enum class Source { Automatic=0, Bundled=1, Local=2 };
struct Selection {Source source=Source::Automatic;std::filesystem::path file;std::string seenVersion,seenBuild;};
struct Resolution {OffsetData::Package package;std::string status;};
using Fetch=std::function<std::string(const std::string&)>;
Selection GetSelection();
void SetSelection(Source source,const std::filesystem::path& file);
bool NeedsPrompt(const std::string& version);
std::string RunningVersion();
std::string Status();
std::string Error();
bool Resolve(const std::string& version);
void RememberSuccessfulLoad(const std::string& version);
std::vector<std::string> Required();
std::string ReadFile(const std::filesystem::path& file);
std::string Sha256(const std::string& bytes);
std::string FetchPublished(const std::string& relative);
OffsetData::Package LocalPackage(const std::filesystem::path& file,const std::string& version);
Resolution ResolveData(const std::string& version,const Selection& selection,const std::filesystem::path& cache,const Fetch& fetch);
void Apply(const OffsetData::Package& package,const std::string& version);
}
