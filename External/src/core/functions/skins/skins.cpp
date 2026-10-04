#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "skins.h"
#include "SkinMix.h"
#include "../../variables/variables.h"
#include "../../../render/WeaponPreview.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "../../globals/globals.h"
#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include "../../../sdk/sdk.h"
#include "../../../render/menu/library.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <vector>

namespace {

constexpr std::uintptr_t kIdentityField = 0x278;
constexpr std::uintptr_t kFlagsField    = 0x164;
constexpr std::uintptr_t kMotorArmPart  = 0x118;
constexpr std::uintptr_t kMotorGloveLink = 0x128;

std::mutex g_stateMutex;
std::vector<std::string> g_catalog;
std::string g_selected;
bool g_editorActive=false;
SkinMix::Choices g_editorChoices;
std::map<std::string,std::vector<std::string>> g_slotCatalog;
std::vector<std::string> g_gloveCatalog;
std::string g_requestedEditorGlove;
std::string WeaponSelection(const std::string& weapon,const std::string& fallback){
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if(!g_editorActive)return fallback;
    return weapon=="AR-15"?g_editorChoices.ar:weapon=="Glock 19"?g_editorChoices.glock:"Default";
}
std::string GloveSelection(const std::string& fallback);
void RestoreSoundsCore();
void ApplySoundsCore();

std::string g_appliedSkin = "None";
std::string g_appliedGloves = "-";
std::string g_currentWeapon = "None";
std::string g_status = "Scanning replicated weapon skins...";

RBX::RbxInstance g_dataModel;

struct SwapRecord {
    bool active = false;
    bool dirty = false;

    std::string weapon;
    std::string liveName;
    std::string skinName;

    std::uintptr_t live = 0;
    std::uintptr_t source = 0;
    std::uintptr_t liveVec = 0;
    std::uintptr_t sourceVec = 0;
    std::uintptr_t liveIdent = 0;
    std::uintptr_t sourceIdent = 0;
    std::uint32_t liveFlags = 0;
    std::uint32_t sourceFlags = 0;

    std::vector<std::uintptr_t> liveChildren;
    std::vector<std::uintptr_t> sourceChildren;

    std::uintptr_t ghostEntries = 0;
    std::vector<std::uint8_t> ghostOriginalEntries, ghostOrderedEntries;
    std::uintptr_t ghostBolt = 0, donorBolt = 0;
    std::uintptr_t ghostBoltVec = 0, donorBoltVec = 0;
    std::vector<std::uintptr_t> donorBoltChildren;

};

std::vector<SwapRecord> g_records;
std::vector<SwapRecord> g_gloveRecords;

std::mutex g_swapMutex;
std::atomic<std::uint64_t> g_pendingRevision{0};
std::atomic<bool> g_pendingGlove{false};
std::atomic<bool> g_gloveMode{false};
std::atomic<bool> g_gloveActive{false};
std::atomic<bool> g_gloveUnsupported{false};
std::string g_activeGloveSkin;
std::uintptr_t g_activeGloveModel = 0;
std::uintptr_t g_activeGloveHome = 0;
std::uintptr_t g_parkedStockModel = 0;
struct GloveVisualSwap {
    std::uintptr_t stockPart = 0;
    std::uintptr_t sourcePart = 0;
    std::uintptr_t stockVec = 0;
    std::uintptr_t sourceVec = 0;
    std::uintptr_t stockMotorEntry = 0;
    std::uintptr_t sourceMotorEntry = 0;
    std::uintptr_t sourceBegin = 0;
    std::uint64_t stockMotorPair[2] = {};
    std::uint64_t sourceMotorPair[2] = {};
    std::uintptr_t sourceEnd = 0;
    std::vector<std::uint8_t> sourceEntries;
    std::vector<std::uintptr_t> visualChildren;
};
struct GloveMeshChange {
    std::uintptr_t stockField = 0;
    std::uintptr_t sourceField = 0;
    std::uint8_t stockOriginal[32] = {};
    std::uint8_t sourceOriginal[32] = {};
};
struct GloveSizeChange {
    std::uintptr_t field = 0;
    RBX::Vec3 original;
};
std::vector<GloveVisualSwap> g_gloveVisualSwaps;
std::vector<GloveMeshChange> g_gloveMeshChanges;
std::vector<GloveSizeChange> g_gloveSizeChanges;
struct GloveJointChange {
    std::uintptr_t field = 0;
    RBX::CFrame original;
};
std::vector<GloveJointChange> g_gloveJointChanges;
SwapRecord g_gloveAssembly;
struct GloveArmLinkSwap {
    std::uintptr_t stock = 0, source = 0;
    std::uint64_t stockLink[2]{}, sourceLink[2]{};
};
std::vector<GloveArmLinkSwap> g_gloveArmLinks;
std::atomic<bool> g_useInGameGlove{false};
std::atomic<bool> g_wantDefault{false};

std::string g_lastLiveWeapon;
std::string g_lastLiveName;
std::string GloveSelection(const std::string& fallback){
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if(!g_editorActive)return fallback;
    const auto pick=SkinMix::GlovesForWeapon(g_editorChoices,g_lastLiveWeapon.empty()?"AR-15":g_lastLiveWeapon);
    if(pick!=g_requestedEditorGlove){g_requestedEditorGlove=pick;g_gloveUnsupported.store(false);g_useInGameGlove.store(false);}
    return pick;
}

std::map<std::string, std::string> g_nativeNames;

std::map<std::string, std::string> g_ingamePick;

void ConsumePendingRevision() {
    g_pendingRevision.store(0, std::memory_order_relaxed);
}

std::string Trim(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    return value;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), value.begin());
}

bool EnsureDataModel() {
    if (g_dataModel.Addr)
        return true;
    const std::uintptr_t fake = memory->read<std::uintptr_t>(
        memory->get_module_address() + Offsets::FakeDataModel::Pointer);
    if (!fake)
        return false;
    g_dataModel = RBX::RbxInstance(memory->read<std::uintptr_t>(
        fake + Offsets::FakeDataModel::RealDataModel));
    return g_dataModel.Addr != 0;
}

RBX::RbxInstance FindServiceInstance(const char* className) {
    if (!EnsureDataModel())
        return {};
    return g_dataModel.FindChildByClass(className);
}

RBX::RbxInstance FindPath(RBX::RbxInstance root,
                          std::initializer_list<const char*> names) {
    for (const char* name : names) {
        root = root.FindChild(name);
        if (!root.Addr)
            break;
    }
    return root;
}

RBX::RbxInstance WeaponCatalogRoot() {
    return FindPath(FindServiceInstance("ReplicatedStorage"), {"Models", "Weapons"});
}

RBX::RbxInstance ResolveArmModel() {
    const RBX::RbxInstance workspace = FindServiceInstance("Workspace");
    if (!workspace.Addr)
        return {};
    RBX::RbxInstance camera = workspace.GetCurrentCamera();
    if (camera.GetClass() != "Camera")
        camera = workspace.FindChildByClass("Camera");
    if (!camera.Addr)
        return {};
    return camera.FindChild("ArmModel");
}

RBX::RbxInstance FindModelRecursive(const RBX::RbxInstance& root,
                                    const std::string& name, int depth = 0) {
    if (!root.Addr || depth > 4)
        return {};
    for (const auto& child : root.GetChildList()) {
        if (child.GetClass() == "Model" && child.GetName() == name)
            return child;
        const RBX::RbxInstance hit = FindModelRecursive(child, name, depth + 1);
        if (hit.Addr)
            return hit;
    }
    return {};
}

RBX::RbxInstance ResolveCatalogModel(const RBX::RbxInstance& container,
                                     const std::string& fullName) {
    RBX::RbxInstance hit = container.FindChild(fullName.c_str());
    if (hit.Addr)
        return hit;
    const RBX::RbxInstance gamepass = container.FindChild("Gamepass");
    if (gamepass.Addr) {
        hit = gamepass.FindChild(fullName.c_str());
        if (hit.Addr)
            return hit;
    }
    return FindModelRecursive(container, fullName);
}

RBX::RbxInstance GlovesCatalogRoot() {
    static RBX::RbxInstance cached;
    static std::chrono::steady_clock::time_point next;
    const auto now = std::chrono::steady_clock::now();
    if (cached.Addr && now < next)
        return cached;
    cached = FindPath(FindServiceInstance("ReplicatedStorage"),
        {"Models", "Gloves"});
    next = now + std::chrono::seconds(10);
    return cached;
}

bool IsGloveSkin(const std::string& skin) {
    if (skin.empty() || skin == "Default")
        return false;
    if (g_gloveActive.load(std::memory_order_relaxed) &&
        g_activeGloveSkin == skin && (g_activeGloveModel || g_gloveAssembly.dirty))
        return true;
    return FindModelRecursive(GlovesCatalogRoot(), skin).Addr != 0;
}

bool IsWeaponSkin(const std::string& skin) {
    if (skin.empty() || skin == "Default")
        return false;
    const RBX::RbxInstance root = WeaponCatalogRoot();
    if (!root.Addr)
        return false;
    for (const auto& weaponFolder : root.GetChildList()) {
        const std::string weapon = weaponFolder.GetName();
        if (!weapon.empty() &&
            weaponFolder.FindChild((weapon + "_" + skin).c_str()).Addr)
            return true;
    }
    return false;
}

std::uint64_t Rq(std::uintptr_t address) {
    return address ? memory->read<std::uint64_t>(address) : 0;
}

bool Wq(std::uintptr_t address, std::uint64_t value) {
    return address != 0 && memory->Write<std::uint64_t>(address, value);
}

bool Wd(std::uintptr_t address, std::uint32_t value) {
    return address != 0 && memory->Write<std::uint32_t>(address, value);
}

class ProcessSuspend {
public:
    ProcessSuspend() {
        if (!memory->IsConnected())
            return;
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) {
            reason_ = "ntdll not loaded";
            return;
        }
        resume_ = reinterpret_cast<ResumeFn>(GetProcAddress(ntdll, "NtResumeProcess"));
        const auto suspend =
            reinterpret_cast<SuspendFn>(GetProcAddress(ntdll, "NtSuspendProcess"));
        if (!resume_ || !suspend) {
            reason_ = "NtSuspendProcess/NtResumeProcess missing";
            return;
        }

        const HANDLE shared = memory->get_process_handle();
        const DWORD pid = shared ? GetProcessId(shared) : 0;
        if (!pid) {
            reason_ = "no Roblox pid";
            return;
        }
        HANDLE owned = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (!owned) {
            reason_ = "OpenProcess(SUSPEND_RESUME) err " +
                std::to_string(GetLastError());
            return;
        }
        const long st = suspend(owned);
        if (st < 0) {
            CloseHandle(owned);
            reason_ = "NtSuspendProcess status 0x" +
                ([](long v) {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%08lX",
                        static_cast<unsigned long>(v));
                    return std::string(buf);
                })(st);
            return;
        }
        handle_ = owned;
        ok_ = true;
    }
    ~ProcessSuspend() {
        if (ok_) {
            resume_(handle_);
            CloseHandle(handle_);
        }
    }
    bool ok() const { return ok_; }
    const std::string& reason() const { return reason_; }

private:
    using SuspendFn = long(__stdcall*)(HANDLE);
    using ResumeFn = long(__stdcall*)(HANDLE);
    ResumeFn resume_ = nullptr;
    HANDLE handle_ = nullptr;
    bool ok_ = false;
    std::string reason_;
};

void RestoreCore(SwapRecord& rec) {

    Wq(rec.live + Offsets::Instance::ChildrenStart, rec.liveVec);
    Wq(rec.source + Offsets::Instance::ChildrenStart, rec.sourceVec);
    for (const std::uintptr_t child : rec.liveChildren)
        Wq(child + Offsets::Instance::Parent, rec.live);
    for (const std::uintptr_t child : rec.sourceChildren)
        Wq(child + Offsets::Instance::Parent, rec.source);
    Wq(rec.live + kIdentityField, rec.liveIdent);
    Wq(rec.source + kIdentityField, rec.sourceIdent);
    Wd(rec.live + kFlagsField, rec.liveFlags);
    Wd(rec.source + kFlagsField, rec.sourceFlags);
    if(rec.ghostEntries)
        memory->write_raw(rec.ghostEntries,rec.ghostOriginalEntries.data(),rec.ghostOriginalEntries.size());
    if(rec.ghostBolt){
        Wq(rec.ghostBolt+Offsets::Instance::ChildrenStart,rec.ghostBoltVec);
        Wq(rec.donorBolt+Offsets::Instance::ChildrenStart,rec.donorBoltVec);
        for(auto child:rec.donorBoltChildren)Wq(child+Offsets::Instance::Parent,rec.donorBolt);
    }
}

void SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_status != status)
        g_status = status;
}

void RestoreBeforeBuild() {
    for (auto& rec : g_records) {
        if (!rec.dirty)
            continue;
        RestoreCore(rec);
        rec.active = false;
        rec.dirty = false;
    }
    for (auto& rec : g_gloveRecords) {
        if (!rec.dirty)
            continue;
        RestoreCore(rec);
        rec.active = false;
        rec.dirty = false;
    }
}

bool BuildRecord(const std::string& weapon, const std::string& liveName,
                 const std::string& skinName, SwapRecord& rec, std::string& error,
                 const std::string& sourceOverride = "",
                 const RBX::RbxInstance& liveOverride = {}) {
    const RBX::RbxInstance container =
        (weapon == "Gloves" || weapon == "gloves")
            ? GlovesCatalogRoot()
            : WeaponCatalogRoot().FindChild(weapon.c_str());
    if (!container.Addr) {
        error = "catalog folder " + weapon + " is missing";
        return false;
    }

    const RBX::RbxInstance liveModel = liveOverride.Addr
        ? liveOverride
        : ResolveCatalogModel(container, liveName);

    const std::string sourceName = sourceOverride.empty()
        ? weapon + "_" + skinName
        : sourceOverride;
    const RBX::RbxInstance sourceModel = ResolveCatalogModel(container, sourceName);
    if (!liveModel.Addr) {
        error = "catalog is missing " + liveName;
        return false;
    }
    if (!sourceModel.Addr) {
        error = "catalog is missing " + sourceName;
        return false;
    }
    if (liveModel.Addr == sourceModel.Addr) {
        error = "already active";
        return false;
    }

    SwapRecord next;
    next.weapon = weapon;
    next.liveName = liveName;
    next.skinName = skinName;
    next.live = liveModel.Addr;
    next.source = sourceModel.Addr;

    for (const RBX::RbxInstance& child : liveModel.GetChildList())
        if (child.Addr)
            next.liveChildren.push_back(child.Addr);
    for (const RBX::RbxInstance& child : sourceModel.GetChildList())
        if (child.Addr)
            next.sourceChildren.push_back(child.Addr);
    if (next.liveChildren.empty() || next.sourceChildren.empty()) {
        error = "one of the models has no children to swap";
        return false;
    }

    next.liveVec = Rq(next.live + Offsets::Instance::ChildrenStart);
    next.sourceVec = Rq(next.source + Offsets::Instance::ChildrenStart);
    if (!next.liveVec || !next.sourceVec) {
        error = "children vectors unreadable";
        return false;
    }
    if (next.liveVec == next.sourceVec) {
        error = "models already share one children vector";
        return false;
    }

    for (const std::uintptr_t child : next.liveChildren) {
        if (Rq(child + Offsets::Instance::Parent) != next.live) {
            error = liveName + " is being rebuilt - retrying";
            return false;
        }
    }
    for (const std::uintptr_t child : next.sourceChildren) {
        if (Rq(child + Offsets::Instance::Parent) != next.source) {
            error = sourceName + " is being rebuilt - retrying";
            return false;
        }
    }

    next.liveIdent = Rq(next.live + kIdentityField);
    next.sourceIdent = Rq(next.source + kIdentityField);
    next.liveFlags = memory->read<std::uint32_t>(next.live + kFlagsField);
    next.sourceFlags = memory->read<std::uint32_t>(next.source + kFlagsField);

    if (weapon == "AR-15" && skinName == "Ghost") {

        RBX::RbxInstance functionalGrip;
        for(const auto& child:sourceModel.GetChildList())
            if(child.GetName()=="Grip" && child.FindChild("Fire").Addr &&
               child.FindChild("MagIn").Addr && child.FindChild("MagOut").Addr){functionalGrip=child;break;}
        if(!functionalGrip.Addr){error="Ghost is missing its functional Grip";return false;}
        next.ghostEntries=Rq(next.sourceVec);
        const auto end=Rq(next.sourceVec+Offsets::Instance::ChildrenEnd);
        const auto stride=Offsets::Instance::ChildrenStride;
        if(!next.ghostEntries || end<next.ghostEntries || end-next.ghostEntries>65536 ||
           (end-next.ghostEntries)%stride){error="Ghost child entries are invalid";return false;}
        next.ghostOriginalEntries.resize(end-next.ghostEntries);
        if(!memory->read_raw(next.ghostEntries,next.ghostOriginalEntries.data(),next.ghostOriginalEntries.size())){
            error="could not read Ghost child entries";return false;
        }

        size_t gripIndex=next.ghostOriginalEntries.size();
        for(size_t i=0;i<next.ghostOriginalEntries.size();i+=stride){
            std::uintptr_t child=0;std::memcpy(&child,next.ghostOriginalEntries.data()+i,sizeof(child));
            if(child==functionalGrip.Addr){gripIndex=i;break;}
        }
        if(gripIndex==next.ghostOriginalEntries.size()){error="Ghost Grip changed during selection";return false;}
        next.ghostOrderedEntries=next.ghostOriginalEntries;
        std::rotate(next.ghostOrderedEntries.begin(),next.ghostOrderedEntries.begin()+gripIndex,
                    next.ghostOrderedEntries.begin()+gripIndex+stride);
        const auto bolt=sourceModel.FindChild("Bolt");
        if(!bolt.Addr){error="Ghost Bolt is missing";return false;}
        if(!bolt.FindChild("BoltBack").Addr || !bolt.FindChild("BoltForward").Addr){
            const auto donor=liveModel.FindChild("Bolt");

            if(!bolt.GetChildList().empty() || !donor.FindChild("BoltBack").Addr ||
               !donor.FindChild("BoltForward").Addr){error="Ghost Bolt sounds cannot be preserved safely";return false;}
            next.ghostBolt=bolt.Addr;next.donorBolt=donor.Addr;
            next.ghostBoltVec=Rq(bolt.Addr+Offsets::Instance::ChildrenStart);
            next.donorBoltVec=Rq(donor.Addr+Offsets::Instance::ChildrenStart);
            for(const auto& child:donor.GetChildList()){
                if(child.GetParent().Addr!=donor.Addr){error="Bolt sounds changed during selection";return false;}
                next.donorBoltChildren.push_back(child.Addr);
            }
        }
    }

    rec = std::move(next);
    rec.active = true;
    rec.dirty = false;
    return true;
}

bool ApplySwapCore(SwapRecord& rec, std::string& error) {
    if(rec.ghostEntries){
        bool prepared=memory->write_raw(rec.ghostEntries,rec.ghostOrderedEntries.data(),rec.ghostOrderedEntries.size());
        if(prepared && rec.ghostBolt){
            prepared=Wq(rec.ghostBolt+Offsets::Instance::ChildrenStart,rec.donorBoltVec) &&
                Wq(rec.donorBolt+Offsets::Instance::ChildrenStart,rec.ghostBoltVec);
            for(auto child:rec.donorBoltChildren)
                prepared=Wq(child+Offsets::Instance::Parent,rec.ghostBolt) && prepared;
        }
        if(!prepared){RestoreCore(rec);error="Ghost compatibility update failed - rolled back";return false;}
    }
    bool ok = Wq(rec.live + Offsets::Instance::ChildrenStart, rec.sourceVec) &&
        Wq(rec.source + Offsets::Instance::ChildrenStart, rec.liveVec);
    if (ok) {
        for (const std::uintptr_t child : rec.liveChildren) {
            if (!Wq(child + Offsets::Instance::Parent, rec.source)) {
                ok = false;
                break;
            }
        }
    }
    if (ok) {
        for (const std::uintptr_t child : rec.sourceChildren) {
            if (!Wq(child + Offsets::Instance::Parent, rec.live)) {
                ok = false;
                break;
            }
        }
    }
    if (ok)
        ok = Wq(rec.live + kIdentityField, rec.sourceIdent) &&
             Wq(rec.source + kIdentityField, rec.liveIdent);
    if (ok)
        ok = Wd(rec.live + kFlagsField, rec.sourceFlags) &&
             Wd(rec.source + kFlagsField, rec.liveFlags);

    if (!ok) {
        RestoreCore(rec);
        error = "a write failed mid-swap - rolled back";
        return false;
    }
    rec.dirty = true;
    return true;
}

bool ApplySwap(SwapRecord& rec, std::string& error) {
    std::lock_guard<std::mutex> lock(g_swapMutex);
    ProcessSuspend guard;
    if (!guard.ok()) {
        error = "could not suspend the Roblox process: " + guard.reason();
        return false;
    }
    return ApplySwapCore(rec, error);
}

bool RestoreSwap(SwapRecord& rec, std::string& error) {
    std::lock_guard<std::mutex> lock(g_swapMutex);
    ProcessSuspend guard;
    if (!guard.ok()) {
        error = "could not suspend the Roblox process: " + guard.reason();
        return false;
    }
    RestoreCore(rec);
    rec.dirty = false;
    return true;
}

bool ValidateSwapped(const SwapRecord& rec) {
    if (Rq(rec.live + Offsets::Instance::ChildrenStart) != rec.sourceVec)
        return false;
    if (Rq(rec.source + Offsets::Instance::ChildrenStart) != rec.liveVec)
        return false;
    for (const std::uintptr_t child : rec.liveChildren) {
        if (Rq(child + Offsets::Instance::Parent) != rec.source)
            return false;
    }
    for (const std::uintptr_t child : rec.sourceChildren) {
        if (Rq(child + Offsets::Instance::Parent) != rec.live)
            return false;
    }
    return true;
}

bool StripPartMotor(const RBX::RbxInstance& part) {
    if (!part.Addr)
        return false;
    const std::uintptr_t vecObj = Rq(part.Addr + Offsets::Instance::ChildrenStart);
    if (!vecObj)
        return false;
    const std::uintptr_t begin = Rq(vecObj);
    const std::uintptr_t end = Rq(vecObj + Offsets::Instance::ChildrenEnd);
    if (!begin || !end || end < begin)
        return false;
    constexpr std::uintptr_t kStride = Offsets::Instance::ChildrenStride;
    const std::size_t count = (end - begin) / kStride;
    std::size_t hole = SIZE_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uintptr_t entry = Rq(begin + i * kStride);
        if (!entry)
            continue;
        if (RBX::RbxInstance(entry).GetClass() == "Motor6D") {
            hole = i;
            break;
        }
    }
    if (hole == SIZE_MAX)
        return false;
    const std::uintptr_t holeAddr = begin + hole * kStride;
    const std::uintptr_t tailSrc = holeAddr + kStride;
    const std::size_t tailBytes = end - tailSrc;
    if (tailBytes) {
        std::vector<std::uint8_t> tail(tailBytes);
        if (!memory->read_raw(tailSrc, tail.data(), tailBytes))
            return false;
        if (!memory->write_raw(holeAddr, tail.data(), tailBytes))
            return false;
    }
    Wq(vecObj + Offsets::Instance::ChildrenEnd, end - kStride);
    return true;
}

void StripSwappedGloveMotors(const SwapRecord& rec) {
    for (const std::uintptr_t addr : rec.sourceChildren) {
        const RBX::RbxInstance part(addr);
        if (part.GetClass() == "MeshPart" || part.GetClass() == "Part")
            StripPartMotor(part);
    }
}

std::atomic<bool> g_gloveWatcherRun{false};
std::thread g_gloveWatcher;

struct MotorFix {
    std::uintptr_t motor = 0;
    std::uintptr_t glovePart = 0;
    std::uintptr_t armPart = 0;
};

bool WeldPass(const RBX::RbxInstance& liveArm, const std::string& renameTo = {}) {
    if (!liveArm.Addr)
        return false;
    RBX::RbxInstance gloveModel;
    for (const auto& child : liveArm.GetChildList()) {
        if (child.GetClass() != "Model")
            continue;
        if (child.FindChild("LeftGlove").Addr || child.FindChild("RightGlove").Addr) {
            gloveModel = child;
            break;
        }
    }
    if (!gloveModel.Addr)
        return false;

    std::uintptr_t refMotor = 0;
    const RBX::RbxInstance animBase = liveArm.FindChild("AnimBase");
    if (animBase.Addr) {
        const RBX::RbxInstance raw = animBase.FindChild("RAW");
        if (raw.Addr)
            refMotor = raw.Addr;
        else {
            const RBX::RbxInstance law = animBase.FindChild("LAW");
            if (law.Addr)
                refMotor = law.Addr;
        }
    }
    std::vector<MotorFix> fixes;
    bool mismatch = false;
    const char* pairs[2][2] = {{"LeftGlove", "Left Arm"}, {"RightGlove", "Right Arm"}};
    for (auto& pair : pairs) {
        const RBX::RbxInstance glovePart = gloveModel.FindChild(pair[0]);
        if (!glovePart.Addr)
            continue;
        const RBX::RbxInstance motor = glovePart.FindChild("Motor6D");
        if (!motor.Addr)
            continue;
        const RBX::RbxInstance armPart = liveArm.FindChild(pair[1]);
        if (!armPart.Addr)
            continue;
        const bool ok = Rq(motor.Addr + kMotorArmPart) == armPart.Addr &&
            Rq(motor.Addr + kMotorGloveLink) == glovePart.Addr &&
            (!refMotor ||
             (Rq(motor.Addr + 0x110) == Rq(refMotor + 0x110) &&
              Rq(motor.Addr + 0x138) == Rq(refMotor + 0x138) &&
              Rq(motor.Addr + 0x140) == Rq(refMotor + 0x140)));
        mismatch = mismatch || !ok;
        fixes.push_back({motor.Addr, glovePart.Addr, armPart.Addr});
    }
    if (!mismatch || fixes.empty())
        return false;
    ProcessSuspend guard;
    if (!guard.ok())
        return false;
    for (const MotorFix& fix : fixes) {
        Wq(fix.motor + kMotorGloveLink, fix.glovePart);
        Wq(fix.motor + kMotorArmPart, fix.armPart);
        if (refMotor) {
            Wq(fix.motor + 0x110, Rq(refMotor + 0x110));
            Wq(fix.motor + 0x138, Rq(refMotor + 0x138));
            Wq(fix.motor + 0x140, Rq(refMotor + 0x140));
        }
    }

    if (!renameTo.empty() && renameTo != "Default" &&
        gloveModel.GetName() == "Default") {
        const std::uintptr_t nameContainer =
            Rq(gloveModel.Addr + Offsets::Instance::NameContainer);
        if (nameContainer)
            RBX::WriteString(nameContainer + Offsets::Instance::Name, renameTo);
    }
    return true;
}

struct LiveWeapon {
    RBX::RbxInstance model;
    std::string weapon;
    std::string name;
    std::string skin;
    bool valid() const { return model.Addr != 0 && !weapon.empty(); }
};

std::vector<LiveWeapon> ResolveLiveWeapons(const RBX::RbxInstance& armModel,
                                           const std::vector<std::string>& weapons) {
    std::vector<LiveWeapon> out;
    if (!armModel.Addr)
        return out;
    for (const auto& child : armModel.GetChildList()) {
        if (child.GetClass() != "Model")
            continue;
        const std::string name = child.GetName();
        for (const std::string& weapon : weapons) {
            const std::string prefix = weapon + "_";
            if (StartsWith(name, prefix) && name.size() > prefix.size()) {
                LiveWeapon candidate{child, weapon, name, name.substr(prefix.size())};
                out.push_back(std::move(candidate));
                break;
            }
        }
    }
    return out;
}

std::vector<std::string> WeaponNames(const RBX::RbxInstance& root) {
    std::vector<std::string> weapons;
    if (root.Addr) {
        for (const auto& child : root.GetChildList()) {
            const std::string name = Trim(child.GetName());
            if (!name.empty())
                weapons.push_back(name);
        }
    }
    return weapons;
}
std::string EquippedWeapon(const std::vector<std::string>& weapons){
    for(auto child:Globals::localPlayer.GetModelRef().GetChildList())if(child.GetClass()=="Tool"){
        const auto name=child.GetName();if(std::find(weapons.begin(),weapons.end(),name)!=weapons.end())return name;
    }
    return {};
}

SwapRecord* FindRecord(const std::string& weapon) {
    for (auto& rec : g_records) {
        if (rec.weapon == weapon)
            return &rec;
    }
    return nullptr;
}

SwapRecord* FindGloveRecord() {
    if (!g_gloveRecords.empty())
        return &g_gloveRecords.front();
    return nullptr;
}

bool RemoveChildEntry(std::uintptr_t parentInst, std::uintptr_t childInst) {
    const std::uintptr_t vecObj = Rq(parentInst + Offsets::Instance::ChildrenStart);
    if (!vecObj)
        return false;
    const std::uintptr_t begin = Rq(vecObj);
    const std::uintptr_t end = Rq(vecObj + Offsets::Instance::ChildrenEnd);
    if (!begin || !end || end < begin)
        return false;
    const std::uintptr_t stride = Offsets::Instance::ChildrenStride;
    for (std::uintptr_t p = begin; p + stride <= end; p += stride) {
        if (Rq(p) != childInst)
            continue;
        const std::uintptr_t tailSrc = p + stride;
        const std::size_t tailBytes = end - tailSrc;
        if (tailBytes) {
            std::vector<std::uint8_t> tail(tailBytes);
            if (!memory->read_raw(tailSrc, tail.data(), tailBytes))
                return false;
            if (!memory->write_raw(p, tail.data(), tailBytes))
                return false;
        }
        Wq(vecObj + Offsets::Instance::ChildrenEnd, end - stride);
        return true;
    }
    return false;
}

bool AppendChildEntry(std::uintptr_t parentInst, std::uintptr_t childInst) {
    const std::uintptr_t vecObj = Rq(parentInst + Offsets::Instance::ChildrenStart);
    if (!vecObj)
        return false;
    const std::uintptr_t begin = Rq(vecObj);
    const std::uintptr_t end = Rq(vecObj + Offsets::Instance::ChildrenEnd);
    if (!begin || !end || end < begin)
        return false;
    const std::uintptr_t stride = Offsets::Instance::ChildrenStride;
    const std::uintptr_t capacity = Rq(vecObj + 0x10);
    if (capacity >= end && capacity - end >= stride) {
        const std::uintptr_t lastPair = (end - stride >= begin) ? end - stride : 0;
        const std::uint64_t second = lastPair ? Rq(lastPair + 8) : 0;
        Wq(end, childInst);
        Wq(end + 8, second);
        Wq(vecObj + Offsets::Instance::ChildrenEnd, end + stride);
        return true;
    }

    const std::size_t oldBytes = end - begin;
    const std::size_t newBytes = oldBytes + stride * 64;
    const std::uintptr_t nb = memory->Alloc(newBytes, PAGE_READWRITE);
    if (!nb)
        return false;
    std::vector<std::uint8_t> copy(newBytes, 0);
    if (oldBytes && !memory->read_raw(begin, copy.data(), oldBytes)) {
        memory->Free(nb);
        return false;
    }
    const std::uint64_t second = oldBytes >= 16 ? Rq(end - stride + 8) : 0;
    *reinterpret_cast<std::uint64_t*>(copy.data() + oldBytes) = childInst;
    *reinterpret_cast<std::uint64_t*>(copy.data() + oldBytes + 8) = second;
    if (!memory->write_raw(nb, copy.data(), newBytes)) {
        memory->Free(nb);
        return false;
    }
    Wq(vecObj, nb);
    Wq(vecObj + Offsets::Instance::ChildrenEnd, nb + oldBytes + stride);
    Wq(vecObj + 0x10, nb + newBytes);
    return true;
}

bool Reparent(std::uintptr_t child, std::uintptr_t newParent, std::string& error) {
    const std::uintptr_t oldParent = Rq(child + Offsets::Instance::Parent);
    if (oldParent == newParent)
        return true;
    if (oldParent && !RemoveChildEntry(oldParent, child)) {
        error = "could not detach the model from its parent";
        return false;
    }
    if (!AppendChildEntry(newParent, child)) {
        if (oldParent)
            AppendChildEntry(oldParent, child);
        error = "could not attach the model to the template";
        return false;
    }
    Wq(child + Offsets::Instance::Parent, newParent);
    return true;
}

std::string ReadInstanceString(std::uintptr_t field) {
    const std::size_t length = static_cast<std::size_t>(Rq(field + 0x10));
    if (length > 512)
        return {};
    const std::uintptr_t data = Rq(field + 0x18) >= 16 ? Rq(field) : field;
    if (!data)
        return {};
    std::string result(length, '\0');
    if (length && !memory->read_raw(data, result.data(), length))
        return {};
    return result;
}

bool RestoreGloveVisuals(std::string& error) {
    if (g_gloveAssembly.dirty) {
        RestoreCore(g_gloveAssembly);
        if (Rq(g_gloveAssembly.live + Offsets::Instance::ChildrenStart) != g_gloveAssembly.liveVec ||
            Rq(g_gloveAssembly.source + Offsets::Instance::ChildrenStart) != g_gloveAssembly.sourceVec) {
            error = "could not restore multipart glove assembly";
            return false;
        }
        g_gloveAssembly = {};
    }
    for (const auto& link : g_gloveArmLinks) {
        if (!memory->write_raw(link.stock, link.stockLink, sizeof(link.stockLink)) ||
            !memory->write_raw(link.source, link.sourceLink, sizeof(link.sourceLink))) {
            error = "could not restore multipart glove arm links";
            return false;
        }
    }
    g_gloveArmLinks.clear();
    for (auto it = g_gloveVisualSwaps.rbegin(); it != g_gloveVisualSwaps.rend(); ++it) {
        const GloveVisualSwap& swap = *it;
        if (!Wq(swap.stockPart + Offsets::Instance::ChildrenStart, swap.stockVec) ||
            !Wq(swap.sourcePart + Offsets::Instance::ChildrenStart, swap.sourceVec) ||
            !memory->write_raw(swap.stockMotorEntry, swap.stockMotorPair, 16) ||
            !memory->write_raw(swap.sourceBegin, swap.sourceEntries.data(),
                               swap.sourceEntries.size()) ||
            !Wq(swap.sourceVec + Offsets::Instance::ChildrenEnd, swap.sourceEnd)) {
            error = "could not restore the glove appearance vectors";
            return false;
        }
        for (const std::uintptr_t child : swap.visualChildren)
            if (!Wq(child + Offsets::Instance::Parent, swap.sourcePart)) {
                error = "could not restore the glove texture parent";
                return false;
            }
    }
    g_gloveVisualSwaps.clear();
    for (const auto& change : g_gloveMeshChanges) {
        if (!memory->write_raw(change.stockField, change.stockOriginal, 32) ||
            !memory->write_raw(change.sourceField, change.sourceOriginal, 32)) {
            error = "could not restore a glove asset string";
            return false;
        }
    }
    g_gloveMeshChanges.clear();
    for (const auto& change : g_gloveSizeChanges)
        if (!memory->Write<RBX::Vec3>(change.field,
                                       change.original)) {
            error = "could not restore glove size";
            return false;
        }
    g_gloveSizeChanges.clear();
    for (const auto& change : g_gloveJointChanges) {
        if (!memory->write_raw(change.field, &change.original, sizeof(change.original))) {
            error = "could not restore glove attachment";
            return false;
        }
    }
    g_gloveJointChanges.clear();
    g_activeGloveSkin.clear();
    return true;
}

bool SwapGloveString(std::uintptr_t stockPart, std::uintptr_t sourcePart,
                     std::uintptr_t offset, std::string& error) {
    if (ReadInstanceString(stockPart + offset) ==
        ReadInstanceString(sourcePart + offset))
        return true;
    GloveMeshChange change;
    change.stockField = stockPart + offset;
    change.sourceField = sourcePart + offset;
    if (!memory->read_raw(change.stockField, change.stockOriginal, 32) ||
        !memory->read_raw(change.sourceField, change.sourceOriginal, 32)) {
        error = "could not read a glove asset string";
        return false;
    }
    if (!memory->write_raw(change.stockField, change.sourceOriginal, 32) ||
        !memory->write_raw(change.sourceField, change.stockOriginal, 32)) {
        memory->write_raw(change.stockField, change.stockOriginal, 32);
        memory->write_raw(change.sourceField, change.sourceOriginal, 32);
        error = "could not swap a glove asset string";
        return false;
    }
    g_gloveMeshChanges.push_back(change);
    return true;
}

constexpr std::uintptr_t kGloveInitialMeshSize = 0x218;

bool CopyGloveVector(std::uintptr_t dst,std::uintptr_t src,std::string& error){
    RBX::Vec3 wanted{},original{};
    if(!memory->read_raw(src,&wanted,sizeof(wanted)) || !memory->read_raw(dst,&original,sizeof(original))){
        error="could not read glove sizing metadata";return false;
    }
    auto valid=[](const RBX::Vec3& v){return std::isfinite(v.X)&&std::isfinite(v.Y)&&std::isfinite(v.Z)&&
        v.X>0.00001f&&v.Y>0.00001f&&v.Z>0.00001f&&v.X<10000&&v.Y<10000&&v.Z<10000;};
    if(!valid(wanted)||!valid(original)){error="glove sizing metadata is invalid";return false;}
    if(wanted.X==original.X&&wanted.Y==original.Y&&wanted.Z==original.Z)return true;
    g_gloveSizeChanges.push_back({dst,original});
    if(!memory->Write<RBX::Vec3>(dst,wanted)){error="could not copy glove sizing metadata";return false;}
    return true;
}
bool CopyGloveSize(const RBX::RbxInstance& stockPart,
                   const RBX::RbxInstance& sourcePart,std::string& error){
    const auto stockPrim=stockPart.GetPrimitivePtr(),sourcePrim=sourcePart.GetPrimitivePtr();
    if(!stockPrim||!sourcePrim||stockPart.GetClass()!="MeshPart"||sourcePart.GetClass()!="MeshPart"){
        error="glove mesh primitive is missing";return false;
    }
    return CopyGloveVector(stockPart.Addr+kGloveInitialMeshSize,sourcePart.Addr+kGloveInitialMeshSize,error) &&
        CopyGloveVector(stockPrim+Offsets::Primitive::Size,sourcePrim+Offsets::Primitive::Size,error);
}

bool ValidGloveFrame(const RBX::CFrame& frame) {
    for (float value : frame.data)
        if (!std::isfinite(value) || std::abs(value) > 10.0f) return false;
    for (int row = 0; row < 3; ++row)
        for (int other = row; other < 3; ++other) {
            float dot = 0;
            for (int col = 0; col < 3; ++col)
                dot += frame.data[row * 3 + col] * frame.data[other * 3 + col];
            if (std::abs(dot - (row == other ? 1.0f : 0.0f)) > 0.002f) return false;
        }
    const auto& m = frame.data;
    const float determinant = m[0]*(m[4]*m[8]-m[5]*m[7]) -
        m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
    return std::abs(determinant - 1.0f) < 0.003f;
}

bool CopyGloveAttachment(std::uintptr_t stockMotor, std::uintptr_t sourceMotor,
                         std::string& error) {
    const auto dst = Rq(stockMotor + 0xF8), src = Rq(sourceMotor + 0xF8);
    RBX::CFrame original[2]{}, wanted[2]{};
    if (dst < 0x10000 || src < 0x10000 || dst == src ||
        !memory->read_raw(dst + 0x38, original, sizeof(original)) ||
        !memory->read_raw(src + 0x38, wanted, sizeof(wanted)) ||
        !ValidGloveFrame(original[0]) || !ValidGloveFrame(original[1]) ||
        !ValidGloveFrame(wanted[0]) || !ValidGloveFrame(wanted[1])) {
        error = "glove attachment layout is invalid";
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        if (std::memcmp(&original[i], &wanted[i], sizeof(RBX::CFrame)) == 0) continue;
        const auto field = dst + 0x38 + i * sizeof(RBX::CFrame);
        g_gloveJointChanges.push_back({field, original[i]});
        if (!memory->write_raw(field, &wanted[i], sizeof(RBX::CFrame))) {
            error = "could not copy glove attachment";
            return false;
        }
    }
    return true;
}

bool SwapGlovePartVisuals(const RBX::RbxInstance& stockPart,
                          const RBX::RbxInstance& sourcePart,
                          std::string& error) {
    const auto stockChildren = stockPart.GetChildList();
    const auto sourceChildren = sourcePart.GetChildList();
    if (stockChildren.size() != 1 || sourceChildren.empty() ||
        stockChildren.front().GetClass() != "Motor6D") {
        error = "glove parts do not match the supported joint layout";
        return false;
    }
    const std::uintptr_t stockMotor = stockChildren.front().Addr;
    std::uintptr_t sourceMotor = 0;
    GloveVisualSwap swap;
    swap.stockPart = stockPart.Addr;
    swap.sourcePart = sourcePart.Addr;
    for (const auto& child : sourceChildren) {
        const std::string cls = child.GetClass();
        if (cls == "Motor6D") {
            if (sourceMotor) {
                error = "selected glove has more than one joint";
                return false;
            }
            sourceMotor = child.Addr;
        } else if (cls == "Texture" || cls == "Decal" ||
                   cls == "SurfaceAppearance" || cls == "Attachment" ||
                   cls == "Highlight" || cls == "ParticleEmitter" ||
                   cls == "Beam" || cls == "Trail" ||
                   cls == "PointLight" || cls == "SpotLight" ||
                   cls == "SurfaceLight") {
            swap.visualChildren.push_back(child.Addr);
        } else {
            error = "selected glove has an unsupported child";
            return false;
        }
    }
    if (!sourceMotor) {
        error = "selected glove has no joint";
        return false;
    }
    if (Rq(stockMotor + Offsets::Instance::Parent) != stockPart.Addr ||
        Rq(sourceMotor + Offsets::Instance::Parent) != sourcePart.Addr) {
        error = "glove joints changed during selection";
        return false;
    }
    if (!CopyGloveAttachment(stockMotor, sourceMotor, error)) return false;
    for (const std::uintptr_t child : swap.visualChildren)
        if (Rq(child + Offsets::Instance::Parent) != sourcePart.Addr) {
            error = "glove visual changed during selection";
            return false;
        }
    swap.stockVec = Rq(stockPart.Addr + Offsets::Instance::ChildrenStart);
    swap.sourceVec = Rq(sourcePart.Addr + Offsets::Instance::ChildrenStart);
    const std::uintptr_t stockBegin = Rq(swap.stockVec);
    const std::uintptr_t sourceBegin = Rq(swap.sourceVec);
    const std::uintptr_t sourceEnd = Rq(swap.sourceVec + Offsets::Instance::ChildrenEnd);
    if (!stockBegin || !sourceBegin || sourceEnd < sourceBegin) {
        error = "glove child vectors are unreadable";
        return false;
    }
    swap.stockMotorEntry = stockBegin;
    swap.sourceBegin = sourceBegin;
    swap.sourceEnd = sourceEnd;
    swap.sourceEntries.resize(sourceEnd - sourceBegin);
    if (!memory->read_raw(sourceBegin, swap.sourceEntries.data(),
                          swap.sourceEntries.size())) {
        error = "selected glove entries are unreadable";
        return false;
    }
    for (std::uintptr_t p = sourceBegin;
         p + Offsets::Instance::ChildrenStride <= sourceEnd;
         p += Offsets::Instance::ChildrenStride) {
        if (Rq(p) == sourceMotor) {
            swap.sourceMotorEntry = p;
            break;
        }
    }
    if (!swap.sourceMotorEntry ||
        !memory->read_raw(swap.stockMotorEntry, swap.stockMotorPair, 16) ||
        !memory->read_raw(swap.sourceMotorEntry, swap.sourceMotorPair, 16)) {
        error = "glove joint entries are unreadable";
        return false;
    }

    std::vector<std::uint8_t> selectedEntries;
    selectedEntries.reserve(swap.sourceEntries.size());
    for (std::uintptr_t p = sourceBegin; p < sourceEnd;
         p += Offsets::Instance::ChildrenStride) {
        const std::uintptr_t child = Rq(p);
        if (child != sourceMotor &&
            std::find(swap.visualChildren.begin(), swap.visualChildren.end(), child) ==
                swap.visualChildren.end())
            continue;
        const auto* pair = child == sourceMotor
            ? reinterpret_cast<const std::uint8_t*>(swap.stockMotorPair)
            : swap.sourceEntries.data() + (p - sourceBegin);
        selectedEntries.insert(selectedEntries.end(), pair,
            pair + Offsets::Instance::ChildrenStride);
    }
    if (!memory->write_raw(swap.stockMotorEntry, swap.sourceMotorPair, 16) ||
        !memory->write_raw(sourceBegin, selectedEntries.data(), selectedEntries.size()) ||
        !Wq(swap.sourceVec + Offsets::Instance::ChildrenEnd,
            sourceBegin + selectedEntries.size()) ||
        !Wq(stockPart.Addr + Offsets::Instance::ChildrenStart, swap.sourceVec) ||
        !Wq(sourcePart.Addr + Offsets::Instance::ChildrenStart, swap.stockVec)) {
        memory->write_raw(swap.stockMotorEntry, swap.stockMotorPair, 16);
        memory->write_raw(sourceBegin, swap.sourceEntries.data(),
                          swap.sourceEntries.size());
        Wq(swap.sourceVec + Offsets::Instance::ChildrenEnd, swap.sourceEnd);
        Wq(stockPart.Addr + Offsets::Instance::ChildrenStart, swap.stockVec);
        Wq(sourcePart.Addr + Offsets::Instance::ChildrenStart, swap.sourceVec);
        error = "glove appearance swap failed and was rolled back";
        return false;
    }
    g_gloveVisualSwaps.push_back(swap);
    for (const std::uintptr_t child : swap.visualChildren) {
        if (!Wq(child + Offsets::Instance::Parent, stockPart.Addr)) {
            error = "could not attach a glove visual layer";
            return false;
        }
    }
    return true;
}

bool PlanGloveAssembly(const RBX::RbxInstance& tmpl, const RBX::RbxInstance& stock,
                       const RBX::RbxInstance& source,
                       std::vector<GloveArmLinkSwap>& links, std::string& error) {
    links.clear();
    for (const auto& child : source.GetChildList()) {
        if (child.GetClass() != "MeshPart") {
            error = "multipart glove contains an unsupported root child"; return false;
        }
    }
    const char* names[2][2] = {{"LeftGlove", "Left Arm"}, {"RightGlove", "Right Arm"}};
    for (const auto& namesForSide : names) {
        const auto dst = stock.FindChild(namesForSide[0]), src = source.FindChild(namesForSide[0]);
        const auto dm = dst.FindChildByClass("Motor6D"), sm = src.FindChildByClass("Motor6D");
        const auto arm = tmpl.FindChild(namesForSide[1]);
        GloveArmLinkSwap link;
        link.stock = dm.Addr + 0x108; link.source = sm.Addr + 0x108;
        RBX::CFrame frames[2]{};
        if (!dm.Addr || !sm.Addr || !arm.Addr ||
            Rq(dm.Addr + 0x118) != dst.Addr || Rq(sm.Addr + 0x118) != src.Addr ||
            !memory->read_raw(link.stock, link.stockLink, sizeof(link.stockLink)) ||
            !memory->read_raw(link.source, link.sourceLink, sizeof(link.sourceLink)) ||
            link.stockLink[0] != arm.Addr || link.sourceLink[0] || link.sourceLink[1] ||
            !memory->read_raw(Rq(sm.Addr + 0xF8) + 0x38, frames, sizeof(frames)) ||
            !ValidGloveFrame(frames[0]) || !ValidGloveFrame(frames[1])) {
            error = "multipart glove attachment layout is unsupported"; return false;
        }
        links.push_back(link);
    }
    return true;
}

bool ApplyGloveVisuals(const std::string& selected, std::string& error) {
    if (!RestoreGloveVisuals(error))
        return false;
    const RBX::RbxInstance source = FindModelRecursive(GlovesCatalogRoot(),
        selected == "S1S1bow" ? "S1S1" : selected);
    const RBX::RbxInstance tmpl = FindPath(FindServiceInstance("ReplicatedStorage"),
        {"Models", "Viewmodel", "ArmModel"});
    const RBX::RbxInstance stock = tmpl.FindChild("Default");
    if (!source.Addr || !stock.Addr) {
        error = "glove catalog or stock template is missing";
        return false;
    }
    if (source.GetChildList().size() > 2) {
        std::vector<GloveArmLinkSwap> links;
        SwapRecord assembly;
        if (!PlanGloveAssembly(tmpl, stock, source, links, error) ||
            !BuildRecord("Gloves", "Default", selected, assembly, error, source.GetName(), stock))
            return false;

        g_gloveArmLinks = links;
        for (const auto& link : links) {
            if (!memory->write_raw(link.source, link.stockLink, sizeof(link.stockLink)) ||
                !memory->write_raw(link.stock, link.sourceLink, sizeof(link.sourceLink))) {
                error = "could not prepare multipart glove attachments";
                std::string rollback; RestoreGloveVisuals(rollback); return false;
            }
        }
        g_gloveAssembly = assembly;
        if (!ApplySwapCore(g_gloveAssembly, error)) {
            std::string rollback; RestoreGloveVisuals(rollback); return false;
        }
        g_activeGloveSkin = selected;
        return true;
    }
    for (const char* name : {"LeftGlove", "RightGlove"}) {
        const RBX::RbxInstance src = source.FindChild(name);
        const RBX::RbxInstance dst = stock.FindChild(name);
        if (!src.Addr || !dst.Addr || !dst.FindChildByClass("Motor6D").Addr) {
            error = "the stock glove joint or selected glove part is missing";
            RestoreGloveVisuals(error);
            return false;
        }
        if (!SwapGloveString(dst.Addr, src.Addr, Offsets::MeshPart::MeshId, error) ||
            !SwapGloveString(dst.Addr, src.Addr, Offsets::MeshPart::Texture, error) ||
            !CopyGloveSize(dst, src, error) ||
            !SwapGlovePartVisuals(dst, src, error)) {
            std::string rollbackError;
            RestoreGloveVisuals(rollbackError);
            return false;
        }
    }
    g_activeGloveSkin = selected;
    return true;
}

bool ReparentGloveModelCore(const std::string& selected, std::string& error,
                            bool& changed) {
    const RBX::RbxInstance rs = FindServiceInstance("ReplicatedStorage");
    const RBX::RbxInstance vm = FindPath(rs, {"Models", "Viewmodel"});
    const RBX::RbxInstance tmpl = FindPath(rs, {"Models", "Viewmodel", "ArmModel"});
    if (!vm.Addr || !tmpl.Addr) {
        error = "Viewmodel template not found";
        changed = false;
        return false;
    }
    const std::string bare = selected == "S1S1bow" ? "S1S1" : selected;
    RBX::RbxInstance src;
    if (g_activeGloveSkin == selected && g_activeGloveModel)
        src = RBX::RbxInstance(g_activeGloveModel);
    if (!src.Addr)
        src = FindModelRecursive(GlovesCatalogRoot(), bare);
    if (!src.Addr) {
        const RBX::RbxInstance gl = WeaponCatalogRoot().FindChild("Gloves");
        if (gl.Addr)
            src = ResolveCatalogModel(gl, "Gloves_" + selected);
    }
    if (!src.Addr) {
        error = "catalog is missing " + selected;
        changed = false;
        return false;
    }
    const RBX::RbxInstance left = src.FindChild("LeftGlove");
    const RBX::RbxInstance right = src.FindChild("RightGlove");
    if (!left.Addr || !right.Addr ||
        !left.FindChildByClass("Motor6D").Addr ||
        !right.FindChildByClass("Motor6D").Addr) {
        error = "special gloves need a native joint; kept stock gloves";
        changed = false;
        return false;
    }

    if (src.GetParent().Addr == tmpl.Addr) {
        g_activeGloveSkin = selected;
        g_activeGloveModel = src.Addr;
        changed = false;
        return true;
    }

    RBX::RbxInstance resident;
    for (const auto& child : tmpl.GetChildList()) {
        if (child.GetClass() != "Model" || child.Addr == src.Addr)
            continue;
        if (child.FindChild("LeftGlove").Addr || child.FindChild("RightGlove").Addr) {
            resident = child;
            break;
        }
    }

    if (resident.Addr && resident.Addr == src.Addr) {
        g_activeGloveSkin = selected;
        g_activeGloveModel = src.Addr;
        changed = false;
        return true;
    }

    if (g_activeGloveModel && g_activeGloveModel != src.Addr &&
        g_activeGloveHome) {
        const bool wasResident = resident.Addr == g_activeGloveModel;
        std::string restoreError;
        if (!Reparent(g_activeGloveModel, g_activeGloveHome, restoreError)) {
            error = "could not restore the previous glove: " + restoreError;
            return false;
        }
        g_activeGloveModel = 0;
        g_activeGloveSkin.clear();
        if (wasResident)
            resident = {};
    }
    const std::uintptr_t sourceHome = src.GetParent().Addr;
    if (!sourceHome) {
        error = "selected glove has no catalog parent";
        return false;
    }

    if (resident.Addr && !Reparent(resident.Addr, vm.Addr, error))
        return false;
    if (resident.Addr && resident.GetName() == "Default")
        g_parkedStockModel = resident.Addr;

    if (!Reparent(src.Addr, tmpl.Addr, error)) {
        if (resident.Addr)
            Reparent(resident.Addr, tmpl.Addr, error);
        return false;
    }
    changed = true;
    g_activeGloveSkin = selected;
    g_activeGloveModel = src.Addr;
    g_activeGloveHome = sourceHome;

    if (src.GetName() != bare) {
        const std::uintptr_t nameContainer = Rq(src.Addr + Offsets::Instance::NameContainer);
        if (nameContainer)
            RBX::WriteString(nameContainer + Offsets::Instance::Name, bare);
    }
    return true;
}

bool RestoreStockGloves(std::string& error) {
    const bool hadVisuals = g_gloveAssembly.dirty || !g_gloveArmLinks.empty() || !g_gloveVisualSwaps.empty() ||
        !g_gloveMeshChanges.empty() || !g_gloveSizeChanges.empty() || !g_gloveJointChanges.empty();
    if (!RestoreGloveVisuals(error))
        return false;
    const RBX::RbxInstance rs = FindServiceInstance("ReplicatedStorage");
    const RBX::RbxInstance vm = FindPath(rs, {"Models", "Viewmodel"});
    const RBX::RbxInstance tmpl = FindPath(rs, {"Models", "Viewmodel", "ArmModel"});
    if (!vm.Addr || !tmpl.Addr) {
        error = "Viewmodel template not found";
        return false;
    }
    bool changed = hadVisuals;
    if (g_activeGloveModel && g_activeGloveHome &&
        Rq(g_activeGloveModel + Offsets::Instance::Parent) != g_activeGloveHome) {
        if (!Reparent(g_activeGloveModel, g_activeGloveHome, error))
            return false;
        changed = true;
    }
    RBX::RbxInstance stock(g_parkedStockModel);
    if (!stock.Addr)
        stock = vm.FindChild("Default");
    if (!stock.Addr)
        stock = tmpl.FindChild("Default");
    if (!stock.Addr) {
        error = "original Default glove model is missing";
        return false;
    }
    if (stock.GetParent().Addr != tmpl.Addr) {
        if (!Reparent(stock.Addr, tmpl.Addr, error))
            return false;
        changed = true;
    }
    g_activeGloveSkin.clear();
    g_activeGloveModel = 0;
    g_activeGloveHome = 0;
    g_parkedStockModel = 0;
    return changed;
}

void ApplyGlovesCore(const RBX::RbxInstance& armModel,
                     const std::string& selected, std::string& error) {
    (void)armModel;

    for (auto& rec : g_gloveRecords) {
        if (rec.dirty)
            RestoreCore(rec);
        rec.active = false;
        rec.dirty = false;
    }
    g_gloveRecords.clear();
    g_gloveActive.store(false, std::memory_order_relaxed);
    if (ApplyGloveVisuals(selected, error))
        g_gloveActive.store(true, std::memory_order_relaxed);
}

void ApplySelectedSkin(const RBX::RbxInstance& armModel,
                       const std::vector<std::string>& weapons) {

    std::lock_guard<std::mutex> swapSerial(g_swapMutex);
    std::string selected;bool editor;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        selected = g_selected;
        editor=g_editorActive;
    }
    if (selected.empty()&&!editor)
        return;
    if(selected.empty())selected="Default";

    const std::vector<LiveWeapon> live = ResolveLiveWeapons(armModel, weapons);
    const auto held=EquippedWeapon(weapons);
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        for (const auto& lw : live) {
            g_lastLiveWeapon = lw.weapon;
            g_lastLiveName = lw.name;
        }
        if(!held.empty())g_lastLiveWeapon=held;
    }

    for (auto& rec : g_records) {
        if (!rec.active)
            continue;
        const LiveWeapon* match = nullptr;
        for (const auto& candidate : live) {
            if (candidate.weapon == rec.weapon) {
                match = &candidate;
                break;
            }
        }
        if (!match || match->name == rec.liveName)
            continue;

        const std::string rendered = rec.weapon + "_" + rec.skinName;
        if (match->name == rendered && ValidateSwapped(rec))
            continue;

        {
            ProcessSuspend guard;
            if (guard.ok())
                RestoreCore(rec);
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_nativeNames[rec.weapon] = match->name;
        }
        rec.active = false;
        rec.dirty = false;
    }
    for (const LiveWeapon& observed : live) {
        const SwapRecord* record = FindRecord(observed.weapon);
        if (record && record->active)
            continue;
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_nativeNames[observed.weapon] = observed.name;
    }

    const auto presetSelected=selected;

    for (auto& rec : g_records) {
        if (rec.active && rec.dirty && !ValidateSwapped(rec)) {
            rec.active = false;
            rec.dirty = false;
        }
    }

    for (const LiveWeapon& weapon : live) {
        const auto selected=WeaponSelection(weapon.weapon,presetSelected);
        const bool wantDefault=selected=="Default";
        SwapRecord* rec = FindRecord(weapon.weapon);

        if (wantDefault) {
            if (rec && rec->active && rec->dirty) {
                std::string error;
                ProcessSuspend guard;
                if (guard.ok())
                    RestoreCore(*rec);
                else
                    error = guard.reason();
                SetStatus(error.empty()
                        ? "Swap reverted - the stock weapon builds on next equip"
                        : "Revert failed: " + error);
            }
            if (rec)
                rec->active = false;
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_appliedSkin = "Default";
            continue;
        }

        std::string nativeName;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            const auto it = g_nativeNames.find(weapon.weapon);
            if (it != g_nativeNames.end())
                nativeName = it->second;
        }
        if (nativeName.empty()) {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            const auto it = g_ingamePick.find(weapon.weapon);
            if (it != g_ingamePick.end()) {
                const RBX::RbxInstance folder =
                    WeaponCatalogRoot().FindChild(weapon.weapon.c_str());
                if (folder.Addr &&
                    ResolveCatalogModel(folder, it->second).Addr)
                    nativeName = it->second;
            }
        }
        if (nativeName.empty())
            nativeName = weapon.name;

        if (rec && rec->active && rec->dirty &&
            rec->skinName == selected &&
            (rec->liveName == nativeName || rec->liveName == weapon.name) &&
            ValidateSwapped(*rec)) {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_appliedSkin = selected;
            g_currentWeapon = weapon.weapon;
            continue;
        }

        bool ok = true;
        {
            ProcessSuspend guard;
            if (!guard.ok()) {
                SetStatus("Swap failed: could not suspend Roblox - " +
                    guard.reason());
                continue;
            }
            for (auto& r : g_records) {
                if (r.weapon == weapon.weapon && r.dirty) {
                    RestoreCore(r);
                    r.active = false;
                    r.dirty = false;
                }
            }
            rec = FindRecord(weapon.weapon);
            SwapRecord fresh;
            std::string error;
            if (!BuildRecord(weapon.weapon, nativeName, selected, fresh, error)) {
                if (error != "already active") {
                    SetStatus("Swap failed: " + error);
                    continue;
                }
                if (rec) {
                    rec->active = false;
                    rec->dirty = false;
                }
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_appliedSkin = selected;
                g_currentWeapon = weapon.weapon;
                continue;
            }
            if (!ApplySwapCore(fresh, error))
                ok = false;
            else if (rec)
                *rec = std::move(fresh);
            else
                g_records.push_back(std::move(fresh));
            if (!ok) {
                SetStatus("Swap failed: " + error);
                continue;
            }
            SetStatus("Swapped " + nativeName + " -> " + weapon.weapon + "_" +
                selected + " - your NEXT equip shows " + selected);
            ConsumePendingRevision();
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_appliedSkin = selected;
            g_currentWeapon = weapon.weapon;
        }
    }

    selected=GloveSelection(presetSelected);
    const bool wantDefault=selected=="Default";

    {
        const bool haveGloveSwap = g_gloveActive.load(std::memory_order_relaxed);
        if (wantDefault && haveGloveSwap) {
            std::string error;
            ProcessSuspend guard;
            if (guard.ok()) {

                for (auto& rec : g_gloveRecords) {
                    if (rec.dirty)
                        RestoreCore(rec);
                }
                g_gloveRecords.clear();
                std::string gloveError;
                RestoreStockGloves(gloveError);
                if (gloveError.empty())
                    g_gloveActive.store(false, std::memory_order_relaxed);
                g_useInGameGlove.store(false, std::memory_order_relaxed);
                SetStatus(gloveError.empty()
                        ? "Gloves restored - stock pair on next equip"
                        : "Glove restore failed: " + gloveError);
            } else {
                SetStatus("Glove restore failed: " + guard.reason());
            }
        } else if (!wantDefault && haveGloveSwap && !IsGloveSkin(selected)) {

            std::string gloveError;
            ProcessSuspend guard;
            if (guard.ok()) {
                for (auto& rec : g_gloveRecords) {
                    if (rec.dirty)
                        RestoreCore(rec);
                }
                g_gloveRecords.clear();
                RestoreStockGloves(gloveError);
                if (gloveError.empty())
                    g_gloveActive.store(false, std::memory_order_relaxed);
                g_useInGameGlove.store(false, std::memory_order_relaxed);
                SetStatus(gloveError.empty()
                        ? "Glove skin left behind - stock gloves on next equip"
                        : "Glove restore failed: " + gloveError);
            } else {
                SetStatus("Glove restore failed: " + guard.reason());
            }
        } else if (!wantDefault) {
            const bool pickWasGloveOnly =
                g_pendingGlove.load(std::memory_order_relaxed) &&
                g_pendingRevision.load(std::memory_order_relaxed) != 0;
            const bool gloveNeeded = !g_gloveUnsupported.load(std::memory_order_relaxed) &&
                (pickWasGloveOnly || IsGloveSkin(selected)) &&
                (!g_gloveActive.load(std::memory_order_relaxed) ||
                 g_activeGloveSkin != selected);
            const bool inGameOverride =
                g_useInGameGlove.load(std::memory_order_relaxed);

            if (gloveNeeded && !inGameOverride) {
                ConsumePendingRevision();
                std::string error;
                ProcessSuspend guard;
                if (!guard.ok())
                    error = guard.reason();
                else {
                    ApplyGlovesCore(armModel, selected, error);
                }
                if (error.empty()) {
                    g_gloveActive.store(true, std::memory_order_relaxed);
                    SetStatus("Gloves swapped (" + selected +
                        ") - your next equip shows them");
                } else {
                    if (error.find("glove parts do not match") != std::string::npos ||
                        error.find("selected glove has") != std::string::npos ||
                        error.find("stock glove joint") != std::string::npos)
                        g_gloveUnsupported.store(true, std::memory_order_relaxed);
                    SetStatus("Glove swap failed: " + error);
                }
            }
        }
    }

    {
        bool glovesArmed = false;
        for (const auto& rec : g_gloveRecords)
            if (rec.active && rec.dirty)
                glovesArmed = true;
        if (glovesArmed) {

            if (!g_gloveWatcherRun.load(std::memory_order_relaxed)) {
                if (g_gloveWatcher.joinable())
                    g_gloveWatcher.join();
                g_gloveWatcherRun.store(true, std::memory_order_relaxed);
                g_gloveWatcher = std::thread([] {
                    while (g_gloveWatcherRun.load(std::memory_order_relaxed) &&
                           Globals::running.load(std::memory_order_relaxed)) {
                        bool armed = false;
                        std::string skinName;
                        {
                            std::lock_guard<std::mutex> serial(g_swapMutex);
                            for (const auto& rec : g_gloveRecords)
                                if (rec.active && rec.dirty) {
                                    armed = true;
                                    skinName = rec.skinName;
                                    break;
                                }
                        }
                        if (!armed) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(30));
                            continue;
                        }
                        const RBX::RbxInstance arm = ResolveArmModel();
                        if (arm.Addr)
                            WeldPass(arm, skinName);
                        std::this_thread::sleep_for(std::chrono::milliseconds(3));
                    }
                });
            }
            WeldPass(ResolveArmModel());
        }
    }

    ApplySoundsCore();
    std::lock_guard<std::mutex> lock(g_stateMutex);
    std::size_t active = 0;
    for (const auto& rec : g_records)
        if (rec.active && rec.dirty)
            ++active;
    for (const auto& rec : g_gloveRecords)
        if (rec.active && rec.dirty)
            ++active;
    g_appliedGloves = std::to_string(active) + " record" +
        (active == 1 ? "" : "s") + " armed";
}

std::string GuessWeaponFromName(const std::string& liveName) {
    const std::size_t cut = liveName.rfind('_');
    if (cut == std::string::npos)
        return {};
    const std::string weapon = liveName.substr(0, cut);
    const RBX::RbxInstance root = WeaponCatalogRoot();
    if (root.Addr && root.FindChild(weapon.c_str()).Addr)
        return weapon;
    return {};
}

bool ApplySelectionFromUi() {
    std::string weapon, liveName, selected;bool editor;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        weapon = g_lastLiveWeapon;
        liveName = g_lastLiveName;
        selected = g_selected;
        editor=g_editorActive;
    }
    if ((selected.empty()&&!editor) || !EnsureDataModel())
        return false;
    if(selected.empty())selected="Default";

    const auto presetSelected=selected;
    std::string error;
    if (selected == "Default" && !editor) {
        bool touched = false;
        {
            std::lock_guard<std::mutex> serial(g_swapMutex);
            ProcessSuspend guard;
            if (!guard.ok()) {
                SetStatus("Restore deferred: could not suspend Roblox - " +
                    guard.reason());
                return false;
            }
            for (auto& rec : g_records) {
                if (!rec.dirty)
                    continue;
                RestoreCore(rec);
                rec.active = false;
                rec.dirty = false;
                touched = true;
            }
            for (auto& rec : g_gloveRecords) {
                if (!rec.dirty)
                    continue;
                RestoreCore(rec);
                rec.active = false;
                rec.dirty = false;
                touched = true;
            }
            if (guard.ok()) {
                std::string gloveError;
                const bool restored = RestoreStockGloves(gloveError);
                touched |= restored;
                if (!gloveError.empty()) {
                    SetStatus("Glove restore failed: " + gloveError);
                    return false;
                }
            }
        }
        g_gloveActive.store(false, std::memory_order_relaxed);
        g_useInGameGlove.store(false, std::memory_order_relaxed);
        g_wantDefault.store(true, std::memory_order_relaxed);
        ConsumePendingRevision();
        SetStatus(touched
                ? "Restored the original catalog - next equips are stock"
                : "Default selected - catalog already original");
        return true;
    }
    g_wantDefault.store(false, std::memory_order_relaxed);

    std::string applied;
    {
        std::lock_guard<std::mutex> serial(g_swapMutex);
        ProcessSuspend guard;
        if (!guard.ok()) {
            SetStatus("Swap deferred: could not suspend Roblox - " +
                guard.reason());
            return false;
        }

        const RBX::RbxInstance root = WeaponCatalogRoot();
        for (const std::string& candidate : WeaponNames(root)) {
            const auto selected=WeaponSelection(candidate,presetSelected);
            if(selected=="Default"){
                if(auto rec=FindRecord(candidate);rec&&rec->dirty){RestoreCore(*rec);rec->active=false;rec->dirty=false;}
                continue;
            }
            const RBX::RbxInstance folder = root.FindChild(candidate.c_str());
            if (!folder.Addr ||
                !ResolveCatalogModel(folder, candidate + "_" + selected).Addr)
                continue;
            SwapRecord* rec = FindRecord(candidate);
            if (rec && rec->active && rec->dirty &&
                rec->skinName == selected && ValidateSwapped(*rec)) {
                if (!applied.empty()) applied += ", ";
                applied += candidate;
                continue;
            }
            std::string nativeName;
            if (rec && !rec->liveName.empty())
                nativeName = rec->liveName;
            if (nativeName.empty()) {
                std::lock_guard<std::mutex> state(g_stateMutex);
                const auto it = g_nativeNames.find(candidate);
                if (it != g_nativeNames.end()) nativeName = it->second;
            }
            if (nativeName.empty() && candidate == weapon)
                nativeName = liveName;
            if (nativeName.empty())
                nativeName = candidate + "_Default";
            if (rec && rec->dirty) {
                RestoreCore(*rec);
                rec->active = false;
                rec->dirty = false;
            }
            SwapRecord fresh;
            std::string weaponError;
            if (!BuildRecord(candidate, nativeName, selected, fresh,
                             weaponError)) {
                if (weaponError == "already active") {
                    if (!applied.empty()) applied += ", ";
                    applied += candidate;
                } else {
                    error = candidate + ": " + weaponError;
                }
                continue;
            }
            if (!ApplySwapCore(fresh, weaponError)) {
                error = candidate + ": " + weaponError;
                continue;
            }
            if (rec)
                *rec = std::move(fresh);
            else
                g_records.push_back(std::move(fresh));
            if (!applied.empty()) applied += ", ";
            applied += candidate;
        }

        selected=GloveSelection(presetSelected);

        if (!g_useInGameGlove.load(std::memory_order_relaxed) &&
            !g_gloveUnsupported.load(std::memory_order_relaxed) &&
            IsGloveSkin(selected)) {
            std::string gloveError;
            ApplyGlovesCore(RBX::RbxInstance(), selected, gloveError);
            if (gloveError.empty()) {
                g_gloveActive.store(true, std::memory_order_relaxed);
                if (!applied.empty())
                    applied += " + ";
                applied += "gloves -> " + selected;
            } else if (gloveError != "catalog is missing " + selected &&
                       gloveError != "no glove model under the template ArmModel") {
                if (gloveError.find("glove parts do not match") != std::string::npos ||
                    gloveError.find("selected glove has") != std::string::npos ||
                    gloveError.find("stock glove joint") != std::string::npos)
                    g_gloveUnsupported.store(true, std::memory_order_relaxed);
                SetStatus("Glove side failed: " + gloveError);
            }
        } else if (g_gloveActive.load(std::memory_order_relaxed)) {
            std::string gloveError;
            RestoreStockGloves(gloveError);
            if (gloveError.empty()) {
                g_gloveActive.store(false, std::memory_order_relaxed);
                if (!applied.empty()) applied += " + ";
                applied += "stock gloves";
            } else {
                error = "gloves: " + gloveError;
            }
        }

        ApplySoundsCore();
        if (applied.empty() && !editor) {

            return false;
        }
    }
    if (error.empty())
        ConsumePendingRevision();
    SetStatus(error.empty()
        ? "Swapped " + applied + " - your next equip shows it" +
            (g_gloveUnsupported.load(std::memory_order_relaxed)
                ? "; special glove layout unsupported, kept stock" : "")
        : "Partly armed " + applied + "; retrying " + error);
    return true;
}

struct SoundValues {std::string id;float volume=1,speed=1;};
struct SoundPatch {std::uintptr_t sound=0,parent=0,buffer=0;std::uint64_t capacity=0;SoundValues before,after;};
std::vector<SoundPatch> g_soundPatches;
std::string g_soundStatus;
bool ReadSound(std::uintptr_t sound,SoundValues& v,std::uintptr_t& buffer,std::uint64_t& capacity){
    if(RBX::RbxInstance(sound).GetClass()!="Sound")return false;
    auto at=sound+Offsets::Sound::SoundId;
    const auto length=Rq(at+16);capacity=Rq(at+24);buffer=capacity>=16?Rq(at):at;
    if(length>capacity||length>512||capacity>4096||buffer<0x10000)return false;
    v.id=memory->read_string(at);v.volume=memory->Read<float>(sound+Offsets::Sound::Volume);v.speed=memory->Read<float>(sound+Offsets::Sound::PlaybackSpeed);
    return v.id.size()==length&&std::isfinite(v.volume)&&v.volume>=0&&v.volume<=100&&std::isfinite(v.speed)&&v.speed>=0&&v.speed<=20;
}
bool ReadDonorSound(std::uintptr_t sound,SoundValues& value){
    std::uintptr_t buffer;std::uint64_t capacity;
    if(!ReadSound(sound,value,buffer,capacity))return false;

    for(const auto& p:g_soundPatches)if(p.sound==sound&&p.buffer==buffer&&p.capacity==capacity&&
        p.parent==RBX::RbxInstance(sound).GetParent().Addr&&value.id==p.after.id){value=p.before;break;}
    return true;
}
bool WriteSound(std::uintptr_t sound,const SoundValues& desired){
    SoundValues current;std::uintptr_t buffer;std::uint64_t capacity;
    if(!ReadSound(sound,current,buffer,capacity)||desired.id.size()>capacity)return false;
    auto idAt=sound+Offsets::Sound::SoundId;
    if(current.id!=desired.id){
        if(memory->WriteRaw(buffer,desired.id.c_str(),desired.id.size()+1)!=desired.id.size()+1||!memory->Write<std::uint64_t>(idAt+16,desired.id.size()))return false;
    }
    return memory->Write<float>(sound+Offsets::Sound::Volume,desired.volume)&&memory->Write<float>(sound+Offsets::Sound::PlaybackSpeed,desired.speed);
}
void RestoreSoundsCore(){

    for(auto it=g_soundPatches.rbegin();it!=g_soundPatches.rend();++it){
        SoundValues current;std::uintptr_t buffer;std::uint64_t capacity;
        if(ReadSound(it->sound,current,buffer,capacity)&&buffer==it->buffer&&capacity==it->capacity&&RBX::RbxInstance(it->sound).GetParent().Addr==it->parent&&current.id==it->after.id){
            auto before=it->before;
            if(current.volume!=it->after.volume)before.volume=current.volume;
            if(current.speed!=it->after.speed)before.speed=current.speed;
            WriteSound(it->sound,before);
        }
    }g_soundPatches.clear();g_soundStatus.clear();
}
using SoundEntry=std::pair<std::string,std::uintptr_t>;
std::vector<SoundEntry> Sounds(RBX::RbxInstance root){
    struct Node{RBX::RbxInstance item;std::string path;int depth;};std::vector<Node> todo{{root,"",0}};
    std::unordered_set<std::uintptr_t> seen;std::vector<SoundEntry> result;
    while(!todo.empty()&&seen.size()<4096){auto n=std::move(todo.back());todo.pop_back();
        if(!n.item.Addr||n.depth>12||!seen.insert(n.item.Addr).second)continue;
        const auto cls=n.item.GetClass();if(cls=="Sound"){result.emplace_back(n.path,n.item.Addr);continue;}
        if(cls=="Script"||cls=="LocalScript"||cls=="ModuleScript")continue;
        for(auto child:n.item.GetChildList())todo.push_back({child,n.path+"/"+child.GetName(),n.depth+1});
    }return result;
}
RBX::RbxInstance OriginalCatalog(const std::string& weapon,const std::string& skin){
    auto model=ResolveCatalogModel(WeaponCatalogRoot().FindChild(weapon),weapon+"_"+skin);
    for(const auto& r:g_records)if(r.dirty&&r.weapon==weapon){
        if(r.skinName==skin)model=RBX::RbxInstance(r.live);
        else if(r.liveName==weapon+"_"+skin)model=RBX::RbxInstance(r.source);
    }return model;
}
void ApplySoundsCore(){
    SkinMix::Choices choices;bool active;
    {std::lock_guard<std::mutex> lock(g_stateMutex);choices=g_editorChoices;active=g_editorActive;}
    if(!active)return;
    static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::milliseconds(200);
    struct Change{SoundPatch patch;};std::vector<Change> changes;int missing=0,tooLong=0;
    auto collect=[&](RBX::RbxInstance target,const std::vector<SoundEntry>& donors,bool mix){
        for(const auto& item:Sounds(target)){
            if(std::any_of(g_soundPatches.begin(),g_soundPatches.end(),[&](const SoundPatch& p){return p.sound==item.second;}))continue;
            SoundPatch patch;patch.sound=item.second;patch.parent=RBX::RbxInstance(item.second).GetParent().Addr;
            if(!ReadSound(item.second,patch.before,patch.buffer,patch.capacity))continue;patch.after=patch.before;
            if(mix){
                auto match=std::find_if(donors.begin(),donors.end(),[&](const SoundEntry& donor){return donor.first==item.first;});
                if(match==donors.end())match=std::find_if(donors.begin(),donors.end(),[&](const SoundEntry& donor){return donor.first.substr(donor.first.rfind('/')+1)==item.first.substr(item.first.rfind('/')+1);});
                SoundValues donor;
                if(match!=donors.end()&&ReadDonorSound(match->second,donor))patch.after=donor;else ++missing;
                if(patch.after.id.size()>patch.capacity){patch.after=patch.before;++tooLong;}
            }
            if(choices.mute)patch.after.volume=0;
            if(patch.after.id==patch.before.id&&patch.after.volume==patch.before.volume&&patch.after.speed==patch.before.speed)continue;
            changes.push_back({std::move(patch)});
        }
    };
    const auto root=WeaponCatalogRoot();const auto live=ResolveLiveWeapons(ResolveArmModel(),WeaponNames(root));
    for(const auto& weapon:WeaponNames(root)){
        const auto donorName=weapon=="AR-15"?choices.arSounds:weapon=="Glock 19"?choices.glockSounds:std::string{};
        const auto donorWeapon=weapon=="AR-15"?choices.arSoundWeapon:choices.glockSoundWeapon;
        const auto donors=donorName.empty()?std::vector<SoundEntry>{}:Sounds(OriginalCatalog(donorWeapon,donorName));
        std::string nativeName;
        {std::lock_guard<std::mutex> lock(g_stateMutex);auto it=g_nativeNames.find(weapon);nativeName=it==g_nativeNames.end()?weapon+"_Default":it->second;}
        auto rec=FindRecord(weapon);auto target=rec&&rec->dirty?RBX::RbxInstance(rec->live):ResolveCatalogModel(root.FindChild(weapon),nativeName);
        collect(target,donors,!donorName.empty());
        for(const auto& model:live)if(model.weapon==weapon)collect(RBX::RbxInstance(model.model),donors,!donorName.empty());
    }

    if(choices.mute){auto character=Globals::localPlayer.GetModelRef();for(auto child:character.GetChildList())if(child.GetClass()=="Tool")collect(child,{},false);}
    if(!changes.empty()){
        ProcessSuspend guard;if(!guard.ok()){g_soundStatus="Sound changes deferred";return;}
        for(auto& change:changes){

            SoundValues current;std::uintptr_t buffer;std::uint64_t capacity;auto& p=change.patch;
            if(!ReadSound(p.sound,current,buffer,capacity)||buffer!=p.buffer||current.id!=p.before.id)continue;
            g_soundPatches.push_back(p);
            if(!WriteSound(p.sound,p.after)){WriteSound(p.sound,p.before);g_soundPatches.pop_back();}
        }
    }
    g_soundStatus.clear();
    if(missing)g_soundStatus="Some sound events are unavailable; original sounds retained";
    if(tooLong)g_soundStatus+=(g_soundStatus.empty()?"":" / ")+std::string("Some sound IDs exceed available space");
}

void CollectCatalog(const RBX::RbxInstance& node, const std::string& weapon,
                    std::vector<std::string>& out, int depth = 0) {
    if (!node.Addr || depth > 4)
        return;
    const std::string prefix = weapon + "_";
    for (const auto& child : node.GetChildList()) {
        const std::string name = Trim(child.GetName());
        if (child.GetClass() == "Model" && StartsWith(name, prefix) &&
            name.size() > prefix.size())
            out.push_back(name.substr(prefix.size()));
        CollectCatalog(child, weapon, out, depth + 1);
    }
}

void RefreshCatalog(const RBX::RbxInstance& root) {
    std::vector<std::string> catalog,allGloves;
    std::map<std::string,std::vector<std::string>> slotCatalog;
    for (const auto& weaponFolder : root.GetChildList()) {
        const std::string weapon = weaponFolder.GetName();
        if (!weapon.empty()){
            CollectCatalog(weaponFolder,weapon,slotCatalog[weapon]);
            auto& skins=slotCatalog[weapon];std::sort(skins.begin(),skins.end());skins.erase(std::unique(skins.begin(),skins.end()),skins.end());
            catalog.insert(catalog.end(),skins.begin(),skins.end());
        }
    }
    std::sort(catalog.begin(), catalog.end());
    catalog.erase(std::unique(catalog.begin(), catalog.end()), catalog.end());

    {
        std::unordered_set<std::string> weaponSet(catalog.begin(), catalog.end());
        std::vector<std::string> gloves;
        const RBX::RbxInstance glovesRoot = GlovesCatalogRoot();
        if (glovesRoot.Addr) {
            for (const auto& folder : glovesRoot.GetChildList()) {
                if (folder.GetClass() == "Model") {
                    const std::string name = Trim(folder.GetName());
                    if (!name.empty() && weaponSet.find(name) == weaponSet.end())
                        gloves.push_back(name);
                }
                for (const auto& model : folder.GetChildList()) {
                    if (model.GetClass() == "Model") {
                        const std::string name = Trim(model.GetName());
                        if (!name.empty()){allGloves.push_back(name);if(weaponSet.find(name)==weaponSet.end())gloves.push_back(name);}
                    }
                }
            }
        }
        std::sort(gloves.begin(), gloves.end());
        gloves.erase(std::unique(gloves.begin(), gloves.end()), gloves.end());
        catalog.insert(catalog.end(), gloves.begin(), gloves.end());
    }

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_slotCatalog=std::move(slotCatalog);std::sort(allGloves.begin(),allGloves.end());allGloves.erase(std::unique(allGloves.begin(),allGloves.end()),allGloves.end());g_gloveCatalog=std::move(allGloves);
    g_catalog = std::move(catalog);
    if (g_selected.empty() && !g_editorActive && !g_catalog.empty())
        g_selected = std::find(g_catalog.begin(), g_catalog.end(), "Default") !=
                g_catalog.end()
            ? "Default"
            : g_catalog.front();
    if (g_catalog.empty())
        g_status = "Replicated weapon skin catalog is unavailable";
}

bool ApplySelectionFromUi();

bool ReleaseRequests(std::string& error){
    const bool glovesChanged=g_gloveActive.load()||g_gloveAssembly.dirty||!g_gloveVisualSwaps.empty()||
        !g_gloveMeshChanges.empty()||!g_gloveSizeChanges.empty()||!g_gloveJointChanges.empty()||!g_gloveArmLinks.empty()||
        std::any_of(g_gloveRecords.begin(),g_gloveRecords.end(),[](const SwapRecord& r){return r.dirty;});
    const bool changed=!g_soundPatches.empty()||glovesChanged||
        std::any_of(g_records.begin(),g_records.end(),[](const SwapRecord& r){return r.dirty;});
    if(changed){
        ProcessSuspend guard;
        if(!guard.ok()){error="Could not release previous choices; try again";return false;}
        RestoreSoundsCore();
        for(auto& r:g_records)if(r.dirty)RestoreCore(r);
        for(auto& r:g_gloveRecords)if(r.dirty)RestoreCore(r);
        if(glovesChanged)RestoreStockGloves(error);
        if(!error.empty())return false;
    }
    g_records.clear();g_gloveRecords.clear();g_gloveActive.store(false);
    g_useInGameGlove.store(false);g_wantDefault.store(false);g_pendingGlove.store(false);
    g_gloveUnsupported.store(false);ConsumePendingRevision();return true;
}

void SetSelection(const std::string& skin, bool) {
    {
        std::lock_guard<std::mutex> serial(g_swapMutex);std::string error;
        if(!ReleaseRequests(error)){SetStatus(error);return;}
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_selected = skin;g_editorActive=false;g_editorChoices={};
        g_status = skin == "Default"
            ? "Restoring the original catalog children vectors..."
            : "Swapping catalog children vectors for " + skin + "...";
    }
    g_pendingGlove.store(false, std::memory_order_relaxed);
    g_gloveUnsupported.store(false, std::memory_order_relaxed);
    g_pendingRevision.fetch_add(1, std::memory_order_relaxed);

    ApplySelectionFromUi();
}

}

namespace Skins {
WeaponPreview::Snapshot CapturePreview(const std::string& weapon,const std::string& skin){
    std::unique_lock<std::mutex> lock(g_swapMutex);
    auto root=Globals::dataModel.FindChildByClass("ReplicatedStorage").FindChild("Models").FindChild("Weapons");
    auto model=ResolveCatalogModel(root.FindChild(weapon),weapon+"_"+skin);

    for(const auto& record:g_records)if(record.dirty&&record.weapon==weapon){
        if(record.skinName==skin)model=RBX::RbxInstance(record.live);
        else if(record.liveName==weapon+"_"+skin)model=RBX::RbxInstance(record.source);
    }
    const auto parts=Cheat::Visuals::MeshParser::CollectWeapon(model.Addr);
    lock.unlock();
    return WeaponPreview::CaptureRoot(model,weapon+" / "+skin,&parts);
}
std::string PreviewSelection(const std::string& weapon){return WeaponSelection(weapon,SavedSelection());}
std::string SavedSelection() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_selected;
}
void RestoreSelection(const std::string& skin) {
    if(skin.empty() || skin.size()>128) return;
    { std::lock_guard<std::mutex> lock(g_stateMutex); g_selected=skin; }
    g_pendingRevision.fetch_add(1,std::memory_order_relaxed);
}

void Tick() {
    using Clock = std::chrono::steady_clock;
    static auto nextCatalogRefresh = Clock::time_point{};
    static auto nextApply = Clock::time_point{};
    const auto now = Clock::now();

    if (g_pendingRevision.load(std::memory_order_relaxed) != 0)
        ApplySelectionFromUi();

    const RBX::RbxInstance root = WeaponCatalogRoot();
    if (!root.Addr)
        return;

    if (now >= nextCatalogRefresh) {
        nextCatalogRefresh = now + std::chrono::seconds(3);
        RefreshCatalog(root);
    }
    if (now >= nextApply) {
        nextApply = now + std::chrono::milliseconds(60);
        std::vector<std::string> weapons = WeaponNames(root);
        const RBX::RbxInstance armModel = ResolveArmModel();
        ApplySelectedSkin(armModel, weapons);
    }
}

bool ApplyEditor(const SkinMix::Choices& draft,std::string& status){
    if(!SkinMix::Valid(draft)||!EnsureDataModel()){status="Join the game before equipping settings";return false;}
    {
        std::lock_guard<std::mutex> serial(g_swapMutex);
        for(const auto& pick:std::vector<std::pair<std::string,std::string>>{{"AR-15",draft.ar},{"Glock 19",draft.glock},{draft.arSoundWeapon,draft.arSounds},{draft.glockSoundWeapon,draft.glockSounds}}){
            if(!pick.second.empty()&&pick.second!="Default"&&!OriginalCatalog(pick.first,pick.second).Addr){status="Missing catalog model: "+pick.first+" / "+pick.second;return false;}
        }
        for(const auto& gloves:{draft.arGloves,draft.glockGloves})if(gloves!="Default"&&!IsGloveSkin(gloves)){status="This selection has no glove model: "+gloves;return false;}
        status.clear();if(!ReleaseRequests(status))return false;
        std::lock_guard<std::mutex> lock(g_stateMutex);g_editorChoices=draft;g_editorActive=true;g_selected.clear();
    }
    g_gloveUnsupported.store(false);g_pendingRevision.fetch_add(1);
    const bool applied=ApplySelectionFromUi();
    status=applied?"Settings equipped; re-equip your weapon":"Settings pending; equip a weapon";
    return applied;
}
bool UnloadEditor(std::string& status){
    {
        std::lock_guard<std::mutex> serial(g_swapMutex);
        status.clear();if(!ReleaseRequests(status))return false;
        std::lock_guard<std::mutex> lock(g_stateMutex);g_editorActive=false;g_editorChoices={};g_selected="Default";
    }
    g_gloveUnsupported.store(false);g_pendingRevision.fetch_add(1);
    const bool restored=ApplySelectionFromUi();
    status=restored?"Editor unloaded":"Restore pending";return restored;
}
void RenderEditor(){
    static SkinMix::Choices draft;
    static bool initialized=false;
    static char presetName[49]="My mix";
    static std::string message;
    std::map<std::string,std::vector<std::string>> slots;std::vector<std::string> gloves;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);slots=g_slotCatalog;gloves=g_gloveCatalog;
        if(!initialized){draft=g_editorChoices;initialized=true;}
    }
    const auto offset=imGuiCustom::g_contentOffset;
    auto selector=[&](const char* id,const char* label,std::string& selected,std::vector<std::string> options,float x,float y,bool sounds=false){
        if(std::find(options.begin(),options.end(),"Default")==options.end())options.insert(options.begin(),"Default");
        if(sounds)options.insert(options.begin(),"");
        std::vector<const char*> labels;int index=0;
        for(int i=0;i<int(options.size());++i){labels.push_back(options[i].empty()?"Use skin sounds":options[i].c_str());if(options[i]==selected)index=i;}
        if(imGuiCustom::Combo(id,&index,labels.data(),int(labels.size()),ImVec2(x,y+imGuiCustom::ComboTop()),320,label))selected=options[index];
    };
    auto sounds=[&](const char* id,std::string& source,std::string& skin,float x){
        std::vector<std::pair<std::string,std::string>> options{{"",""}};std::vector<std::string> names{"Use selected skin sounds"};
        for(const auto& weapon:{std::string("AR-15"),std::string("Glock 19")}){
            auto choices=slots[weapon];if(std::find(choices.begin(),choices.end(),"Default")==choices.end())choices.insert(choices.begin(),"Default");
            for(const auto& pick:choices){options.emplace_back(weapon,pick);names.push_back(weapon+" / "+pick);}
        }
        std::vector<const char*> labels;int selected=0;
        for(int i=0;i<int(options.size());++i){labels.push_back(names[i].c_str());if(!skin.empty()&&options[i]==std::make_pair(source,skin))selected=i;}
        if(imGuiCustom::Combo(id,&selected,labels.data(),int(labels.size()),ImVec2(x,216+imGuiCustom::ComboTop()),320,"Sounds:")){
            skin=options[selected].second;if(selected)source=options[selected].first;
        }
    };
    ImGui::SetCursorPos(ImVec2(12+offset.x,51));ImGui::TextUnformatted("AR-15");
    ImGui::SetCursorPos(ImVec2(367+offset.x,51));ImGui::TextUnformatted("Glock 19");
    selector("mix_ar","Skin:",draft.ar,slots["AR-15"],12,76);
    selector("mix_glock","Skin:",draft.glock,slots["Glock 19"],367,76);
    selector("mix_ar_gloves","Gloves:",draft.arGloves,gloves,12,146);
    selector("mix_glock_gloves","Gloves:",draft.glockGloves,gloves,367,146);
    sounds("mix_ar_sound",draft.arSoundWeapon,draft.arSounds,12);
    sounds("mix_glock_sound",draft.glockSoundWeapon,draft.glockSounds,367);
    imGuiCustom::Checkbox("Mute local weapons",&draft.mute,ImVec2(12,292));
    ImGui::SetCursorPos(ImVec2(12+offset.x,332));ImGui::SetNextItemWidth(320);ImGui::InputTextWithHint("##mix_name","Preset name",presetName,sizeof(presetName));
    ImGui::SetCursorPos(ImVec2(367+offset.x,332));ImGui::SetNextItemWidth(320);
    if(ImGui::BeginCombo("##saved_mixes","Saved presets")){
        for(const auto& name:SkinMix::Presets())if(ImGui::Selectable(name.c_str())){strncpy_s(presetName,name.c_str(),_TRUNCATE);SkinMix::Load(name,draft,message);}
        ImGui::EndCombo();
    }
    ImGui::SetCursorPos(ImVec2(12+offset.x,378));if(ImGui::Button("Equip selected settings",ImVec2(220,35)))ApplyEditor(draft,message);
    ImGui::SameLine();if(ImGui::Button("Unload",ImVec2(105,35)))UnloadEditor(message);
    ImGui::SetCursorPos(ImVec2(367+offset.x,378));if(ImGui::Button("Save preset",ImVec2(155,35)))SkinMix::Save(presetName,draft,message);
    ImGui::SameLine();if(ImGui::Button("Load preset",ImVec2(155,35)))SkinMix::Load(presetName,draft,message);
    std::string soundMessage;
    {std::unique_lock<std::mutex> lock(g_swapMutex,std::try_to_lock);if(lock.owns_lock())soundMessage=g_soundStatus;}
    if(!message.empty()||!soundMessage.empty()){
        ImGui::SetCursorPos(ImVec2(12+offset.x,432));ImGui::PushTextWrapPos(700);
        if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
        if(!soundMessage.empty())ImGui::TextWrapped("%s",soundMessage.c_str());
        ImGui::PopTextWrapPos();
    }
}

void RenderMenu() {
    std::vector<std::string> catalog;
    std::string selected;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        catalog = g_catalog;
        selected = g_selected;
    }

    static char search[96]{};
    const ImVec2 offset = imGuiCustom::g_contentOffset;
    ImGui::SetCursorPos(ImVec2(18.0f + offset.x, 51.0f));
    ImGui::TextUnformatted(UiText::Tr("Click a skin to swap it in"));
    ImGui::SetCursorPos(ImVec2(18.0f + offset.x, 71.0f));
    ImGui::SetNextItemWidth(266.0f);
    ImGui::InputTextWithHint("##skin_search", UiText::Tr("Search skins..."), search, sizeof(search));
    std::string query = search;
    std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    ImGui::SetCursorPos(ImVec2(18.0f + offset.x, 96.0f));
    ImGui::BeginChild("skin_catalog", ImVec2(266.0f, 258.0f), true);
    for (const auto& skin : catalog) {
        std::string searchable = skin;
        std::transform(searchable.begin(), searchable.end(), searchable.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!query.empty() && searchable.find(query) == std::string::npos)
            continue;
        if (ImGui::Selectable(skin.c_str(), skin == selected)) {
            SetSelection(skin, false);
        }
    }
    ImGui::EndChild();

}

void Shutdown() {

    g_gloveWatcherRun.store(false, std::memory_order_relaxed);
    if (g_gloveWatcher.joinable())
        g_gloveWatcher.join();
    {
        std::lock_guard<std::mutex> serial(g_swapMutex);
        ProcessSuspend guard;
        const bool suspended = guard.ok();
        if(suspended)RestoreSoundsCore();
        for (auto& rec : g_records) {
            if (rec.active && rec.dirty) {
                if (suspended)
                    RestoreCore(rec);
            }
            rec.active = false;
            rec.dirty = false;
        }
        for (auto& rec : g_gloveRecords) {
            if (rec.active && rec.dirty) {
                if (suspended)
                    RestoreCore(rec);
            }
            rec.active = false;
            rec.dirty = false;
        }
        if (suspended) {
            std::string gloveError;
            RestoreStockGloves(gloveError);
            g_gloveActive.store(false, std::memory_order_relaxed);
        }
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_appliedSkin = "None";
    g_appliedGloves = "-";
    g_currentWeapon = "None";
    g_status = "Skin changer stopped - original catalog restored";
}

}
