#include "../keys/visual_keybind.h"
#include "../keys/keys.h"
#include "app.h"
#include "../net/game_info.h"
#include "../net/performance.h"
#include "../functions/settings/preferences.h"
#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "../../memory/memory.h"
#include "../../sdk/offsets.h"
#include "../../sdk/sdk.h"
#include "../cache/cache.h"
#include "../globals/globals.h"
#include "../tp_handler/tp_handler.h"
#include "../functions/aim/aim.h"
#include "../functions/aim/recoil.h"
#include "../functions/aim/raycast_silent.h"
#include "../functions/skins/skins.h"
#include "../../render/render.h"
#include "../../render/OverlayLifecycle.h"
#include "../../render/menu/library.h"
#include "../features/mesh/chams/MeshChams.h"
#include "../features/mesh/occlusion/MeshOcclusion.h"
#include "../features/mesh/parser/MeshParser.h"
#include "../features/mesh/shader/MeshDxShader.h"
#include "../features/native/NativeChams.h"
#include "../features/WeaponVisuals.h"
#include "../../render/MenuMotion.h"
#include "../features/TargetLabels.h"

#pragma comment(lib, "winmm.lib")

namespace {
constexpr const char* kProc = "RobloxPlayerBeta.exe";
constexpr const wchar_t* kTitle = L"Roblox";

Mesh::Matrix4x4 ToMeshMatrix(const RBX::Mat4& src) {
    Mesh::Matrix4x4 out;
    std::memcpy(out.m, src.data, sizeof(src.data));
    return out;
}

bool PassesMeshVisibilityChecks(std::uintptr_t character) {
    if (!variables::ESP::meshInvisibleCheck && !variables::ESP::meshTransparencyCheck)
        return true;
    struct VisibilityState {
        ULONGLONG checkedAt = 0;
        int bodyParts = 0;
        int visibleParts = 0;
        std::vector<float> transparencies;
    };

    static std::unordered_map<std::uintptr_t, VisibilityState> cache;
    if (cache.size() > 256)
        cache.clear();
    auto& state = cache[character];
    const ULONGLONG now = GetTickCount64();
    if (now - state.checkedAt >= 100) {
        state.checkedAt = now;
        state.bodyParts = 0;
        state.visibleParts = 0;
        state.transparencies.clear();
        const auto& limbs = PlayerCache::GetLimbs(character);
        for (const auto address : {limbs.head, limbs.torso, limbs.lArm, limbs.rArm,
             limbs.lLeg, limbs.rLeg, limbs.upperTorso, limbs.lowerTorso,
             limbs.lUpperArm, limbs.lLowerArm, limbs.lHand, limbs.rUpperArm,
             limbs.rLowerArm, limbs.rHand, limbs.lUpperLeg, limbs.lLowerLeg,
             limbs.lFoot, limbs.rUpperLeg, limbs.rLowerLeg, limbs.rFoot}) {
            if (!address)
                continue;
            ++state.bodyParts;
            const float transparency = memory->read<float>(address + Offsets::BasePart::Transparency);
            const float fade = Cheat::Visuals::MeshOcclusion::PartFadeValue(address);
            if (std::isfinite(transparency))
                state.transparencies.push_back(transparency);

            if (std::isfinite(fade) && fade > (std::isfinite(transparency) ? transparency : 0.0f))
                state.transparencies.push_back(fade);
            if (std::isfinite(transparency) && transparency < 0.999f)
                ++state.visibleParts;
            else if (std::isfinite(fade) && fade < 0.999f)
                ++state.visibleParts;
        }
    }
    bool inTransparencyRange = false;
    const float lo = (std::min)(variables::ESP::meshTransparencyMin, variables::ESP::meshTransparencyMax);
    const float hi = (std::max)(variables::ESP::meshTransparencyMin, variables::ESP::meshTransparencyMax);
    for (float transparency : state.transparencies) {
        if (transparency >= lo && transparency <= hi) {
            inTransparencyRange = true;
            break;
        }
    }
    if (variables::ESP::meshInvisibleCheck && state.bodyParts > 0 && state.visibleParts == 0)
        return false;
    if (variables::ESP::meshTransparencyCheck && inTransparencyRange)
        return false;
    return true;
}

Mesh::Vector2 GetRobloxViewport(const ImVec2& overlaySize) {
    if (Globals::camera.Addr) {
        const uintptr_t candidates[] = {Offsets::Camera::ViewportSize, 0x2C8, Offsets::Camera::Viewport};
        float bestScore = FLT_MAX;
        Mesh::Vector2 best;
        const float overlayAspect = overlaySize.y > 1.0f ? overlaySize.x / overlaySize.y : 0.0f;
        for (uintptr_t offset : candidates) {
            const RBX::Vec2 value = memory->read<RBX::Vec2>(Globals::camera.Addr + offset);
            if (!std::isfinite(value.X) || !std::isfinite(value.Y) ||
                value.X < 64.0f || value.Y < 64.0f || value.X > 16384.0f || value.Y > 16384.0f)
                continue;
            const float aspect = value.X / value.Y;
            const float score = overlayAspect > 0.0f ? std::fabs(aspect - overlayAspect) : 0.0f;
            if (score < bestScore) {
                bestScore = score;
                best = Mesh::Vector2(value.X, value.Y);
            }
        }
        if (best.x > 1.0f && best.y > 1.0f)
            return best;
    }
    return Mesh::Vector2((std::max)(overlaySize.x, 1.0f), (std::max)(overlaySize.y, 1.0f));
}

void RenderMeshVisuals(ImDrawList* drawList, const RBX::Mat4& rbView, const ImVec2& overlaySize) {
    if (!variables::ESP::meshChams && !Cheat::Visuals::NativeChams::WorldDepthNeeded())
        return;
    const Mesh::Matrix4x4 view = ToMeshMatrix(rbView);
    const Mesh::Vector2 viewport = GetRobloxViewport(overlaySize);
    const float width = (std::max)(overlaySize.x, 1.0f);
    const float height = (std::max)(overlaySize.y, 1.0f);
    const float scaleX = width / viewport.x;
    const float scaleY = height / viewport.y;
    const RBX::Vec3 cp = Globals::camera.GetCameraCFrame().GetPosition();
    static const auto animationStart = std::chrono::steady_clock::now();
    const float animationTime = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - animationStart).count();
    Cheat::Visuals::MeshDxShader::BeginFrame(view, Mesh::Vector3(cp.X, cp.Y, cp.Z),
        animationTime);
    ImVec4 fallbackFill = *reinterpret_cast<const ImVec4*>(variables::ESP::chamsFillColor);
    fallbackFill.w *= (std::max)(0.0f, (std::min)(1.0f, variables::ESP::meshChamsOpacity));
    const ImU32 fill = ImGui::ColorConvertFloat4ToU32(fallbackFill);
    if(variables::ESP::meshChams) for (const auto& player : PlayerCache::players) {
        if(!App::WithinVisualRange(player.characterAddr,variables::ESP::meshDistance,variables::ESP::meshUnlimited))continue;
        if (PlayerRules::Esp(player.userId) || !App::PassesChamChecks(player.isValid, player.characterAddr, player.teamAddr, player.health))
            continue;
        Cheat::Visuals::MeshChams::Draw(drawList, player.characterAddr, view,
            viewport, scaleX, scaleY, fill);
    }

    if (variables::ESP::meshChams && variables::ESP::meshChamsLocal) {
        const auto localChar = Globals::localPlayer.GetModelRef();
        if (localChar.Addr && PassesMeshVisibilityChecks(localChar.Addr))
            Cheat::Visuals::MeshChams::Draw(drawList, localChar.Addr, view,
                viewport, scaleX, scaleY, fill);
    }

}

#pragma pack(push, 1)
struct RttiObjectLocator {
    std::uint32_t signature;
    std::uint32_t offset;
    std::uint32_t constructorOffset;
    std::uint32_t typeDescriptor;
    std::uint32_t classHierarchy;
    std::uint32_t self;
};
struct RttiClassHierarchy {
    std::uint32_t signature;
    std::uint32_t attributes;
    std::uint32_t baseClassCount;
    std::uint32_t baseClassArray;
};
struct RttiBaseClass {
    std::uint32_t typeDescriptor;
    std::uint32_t containedBases;
    std::int32_t memberDisplacement;
    std::int32_t vbtableDisplacement;
    std::int32_t displacementInVbtable;
    std::uint32_t attributes;
    std::uint32_t classHierarchy;
};
#pragma pack(pop)

bool get_section(std::uintptr_t base, const char* name, std::uintptr_t& start, std::size_t& size) {
    IMAGE_DOS_HEADER dos{};
    if (!memory->read_raw(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    IMAGE_NT_HEADERS64 nt{};
    if (!memory->read_raw(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
        return false;
    const auto sections = base + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (std::uint16_t index = 0; index < nt.FileHeader.NumberOfSections; ++index) {
        IMAGE_SECTION_HEADER section{};
        if (!memory->read_raw(sections + index * sizeof(section), &section, sizeof(section)))
            return false;
        if (std::memcmp(section.Name, name, std::strlen(name)) == 0) {
            start = base + section.VirtualAddress;
            size = section.Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

bool is_task_scheduler(std::uintptr_t object, std::uintptr_t moduleBase) {
    const auto vtable = memory->read<std::uintptr_t>(object);
    const auto locatorAddress = vtable >= sizeof(std::uintptr_t)
        ? memory->read<std::uintptr_t>(vtable - sizeof(std::uintptr_t)) : 0;
    if (!locatorAddress)
        return false;
    RttiObjectLocator locator{};
    if (!memory->read_raw(locatorAddress, &locator, sizeof(locator)) || locator.signature != 1 || !locator.self || !locator.typeDescriptor)
        return false;
    if (locatorAddress - locator.self != moduleBase)
        return false;
    const auto matchesType = [moduleBase](std::uint32_t descriptor) {
        char typeName[128]{};
        return descriptor &&
            memory->read_raw(moduleBase + descriptor + sizeof(std::uintptr_t) * 2, typeName, sizeof(typeName) - 1) &&
            std::strcmp(typeName, ".?AVTaskScheduler@RBX@@") == 0;
    };
    if (matchesType(locator.typeDescriptor))
        return true;
    if (!locator.classHierarchy)
        return false;
    RttiClassHierarchy hierarchy{};
    if (!memory->read_raw(moduleBase + locator.classHierarchy, &hierarchy, sizeof(hierarchy)) ||
        !hierarchy.baseClassArray || !hierarchy.baseClassCount || hierarchy.baseClassCount > 32)
        return false;
    for (std::uint32_t index = 0; index < hierarchy.baseClassCount; ++index) {
        std::uint32_t descriptorRva = 0;
        if (!memory->read_raw(moduleBase + hierarchy.baseClassArray + index * sizeof(descriptorRva), &descriptorRva, sizeof(descriptorRva)))
            continue;
        RttiBaseClass descriptor{};
        if (descriptorRva && memory->read_raw(moduleBase + descriptorRva, &descriptor, sizeof(descriptor)) &&
            matchesType(descriptor.typeDescriptor))
            return true;
    }
    return false;
}

std::uintptr_t find_frame_value_offset(std::uintptr_t scheduler) {
    constexpr double expected = 1.0 / 60.0;
    for (std::uintptr_t offset = 0; offset < 0x1000; offset += 4) {
        double value = 0.0;
        if (memory->read_raw(scheduler + offset, &value, sizeof(value)) &&
            std::isfinite(value) && std::abs(value - expected) < 0.0000001)
            return offset;
    }
    return 0;
}

std::uintptr_t find_task_scheduler(std::uintptr_t& frameValueOffset) {
    const auto base = memory->get_module_address();
    const auto configured = memory->read<std::uintptr_t>(base + Offsets::TaskScheduler::Pointer);
    double configuredValue = 0.0;

    if (configured && memory->read_raw(configured + Offsets::TaskScheduler::MaxFPS, &configuredValue, sizeof(configuredValue)) &&
        std::isfinite(configuredValue) && std::abs(configuredValue - 1.0 / 60.0) < 0.0000001) {
        frameValueOffset = Offsets::TaskScheduler::MaxFPS;
        return configured;
    }
    for (const char* sectionName : {".data", ".rdata"}) {
        std::uintptr_t sectionStart = 0;
        std::size_t sectionSize = 0;
        if (!get_section(base, sectionName, sectionStart, sectionSize))
            continue;
        std::vector<std::uintptr_t> pointers(sectionSize / sizeof(std::uintptr_t));
        if (!memory->read_raw(sectionStart, pointers.data(), pointers.size() * sizeof(std::uintptr_t)))
            continue;
        for (const auto candidate : pointers) {
            if (!candidate || !is_task_scheduler(candidate, base))
                continue;
            const auto offset = find_frame_value_offset(candidate);
            if (offset) {
                frameValueOffset = offset;
                return candidate;
            }
        }
    }
    return 0;
}

bool valid_frame_location(std::uintptr_t scheduler, std::uintptr_t offset) {
    if (!scheduler || !offset)
        return false;
    double value = 0.0;
    return memory->read_raw(scheduler + offset, &value, sizeof(value)) && std::isfinite(value) &&
        ((value > 0.0 && value <= 1.0) || (value >= 10.0 && value <= 100000.0));
}

std::uintptr_t find_task_scheduler_verified(std::uintptr_t& frameValueOffset) {

    const auto base = memory->get_module_address();
    const auto configured = memory->read<std::uintptr_t>(base + Offsets::TaskScheduler::Pointer);
    double v = 0.0;
    if (configured &&
        memory->read_raw(configured + Offsets::TaskScheduler::MaxFPS, &v, sizeof(v)) &&
        std::isfinite(v) && ((v > 0.0 && v <= 1.0) || (v >= 10.0 && v <= 100000.0))) {
        frameValueOffset = Offsets::TaskScheduler::MaxFPS;
        return configured;
    }
    std::uintptr_t offset = 0;
    const auto scanned = find_task_scheduler(offset);
    if (scanned) {
        frameValueOffset = offset;
        return scanned;
    }
    return 0;
}

void apply_game_fps_limit() {
    static std::uintptr_t scheduler = 0;
    static std::uintptr_t frameValueOffset = 0;
    static int failedStickChecks = 0;
    static auto nextResolve = std::chrono::steady_clock::time_point{};
    static bool reported = false;
    static auto nextApply = std::chrono::steady_clock::time_point{};
    const auto nowApply = std::chrono::steady_clock::now();

    const int limit = variables::Misc::fpsLimit > 0 ? variables::Misc::fpsLimit : 1000;

    if (nowApply < nextApply)
        return;
    nextApply = nowApply + std::chrono::milliseconds(250);

    if (!valid_frame_location(scheduler, frameValueOffset)) {
        const auto now = std::chrono::steady_clock::now();
        if (now < nextResolve)
            return;

        nextResolve = now + std::chrono::seconds(5);
        scheduler = find_task_scheduler_verified(frameValueOffset);
        failedStickChecks = 0;
        reported = false;
        if (!scheduler)
            return;
    }

    const auto address = scheduler + frameValueOffset;
    double current = 0.0;
    if (!memory->read_raw(address, &current, sizeof(current)) || !std::isfinite(current))
        return;

    double target = 0.0;
    if (current > 0.0 && current <= 1.0)
        target = 1.0 / static_cast<double>(limit);
    else if (current >= 10.0 && current <= 100000.0)
        target = static_cast<double>(limit);
    else
        return;

    const bool alreadySet = std::abs(current - target) < 0.0000001;
    if (!alreadySet &&
        !memory->write_raw(address, &target, sizeof(target))) {
        scheduler = 0;
        frameValueOffset = 0;
        return;
    }

    double back = 0.0;
    if (memory->read_raw(address, &back, sizeof(back)) && std::isfinite(back) &&
        std::abs(back - target) > 0.0000001) {
        if (++failedStickChecks >= 3) {
            scheduler = 0;
            frameValueOffset = 0;
            failedStickChecks = 0;
        }
        return;
    }
    failedStickChecks = 0;
    if (!reported) {
        std::cout << "[+] game FPS limit set to " << limit << "\n";
        reported = true;
    }
}
}

namespace App {
bool WithinVisualRange(std::uintptr_t character,float limit,bool unlimited){
    if(unlimited)return true;
    const auto& limbs=PlayerCache::GetLimbs(character);
    auto part=RBX::RbxInstance(limbs.hrp?limbs.hrp:limbs.head);
    if(!part.Addr)return false;
    const auto p=part.GetPos(),c=Globals::camera.GetCameraCFrame().GetPosition();
    const float x=p.X-c.X,y=p.Y-c.Y,z=p.Z-c.Z;
    return std::isfinite(limit)&&limit>=0 && x*x+y*y+z*z<=limit*limit;
}
bool PassesVisibilityChecks(std::uintptr_t character) {
    return PassesMeshVisibilityChecks(character);
}
bool PassesChamChecks(bool valid, std::uintptr_t character, std::uintptr_t team, float health) {
    if (!valid || !character)
        return false;

    if (variables::ESP::meshTeamCheck && team && PlayerCache::localPlayerTeam &&
        team == PlayerCache::localPlayerTeam)
        return false;
    if (variables::ESP::meshTeamCheck && team && !PlayerCache::localPlayerTeamName.empty()) {
        const std::string name = RBX::RbxInstance(team).GetName();
        if (!name.empty() && name == PlayerCache::localPlayerTeamName)
            return false;
    }
    if (variables::ESP::meshDeadCheck && (!std::isfinite(health) || health <= 0.0f))
        return false;
    return PassesVisibilityChecks(character);
}
}

namespace App {
namespace {
std::wstring startupError;

struct PointerRead {
    std::uintptr_t value = 0;
    DWORD error = 0;
    SIZE_T bytes = 0;
    bool complete = false;
};

PointerRead ReadPointer(std::uintptr_t address) {
    PointerRead result;
    const BOOL readOk = ReadProcessMemory(memory->GetHandle(),
        reinterpret_cast<LPCVOID>(address), &result.value,
        sizeof(result.value), &result.bytes);
    result.complete = readOk != FALSE && result.bytes == sizeof(result.value);
    if (!result.complete)
        result.error = readOk ? ERROR_PARTIAL_COPY : GetLastError();
    return result;
}

std::wstring PointerStatus(const PointerRead& read) {
    if (!read.complete)
        return L"read failed (Windows error " + std::to_wstring(read.error) +
            L", " + std::to_wstring(read.bytes) + L"/8 bytes)";
    if (!read.value) return L"address read successfully but contained zero";
    if (!memory->IsValid(read.value)) return L"pointer refers to inaccessible memory";
    return L"valid";
}

std::wstring HexAddress(std::uintptr_t value) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"0x%llX", static_cast<unsigned long long>(value));
    return buffer;
}

std::wstring RunningClientVersion() {
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(memory->GetHandle(), 0, path.data(), &length))
        return {};
    path.resize(length);
    const auto start = path.rfind(L"version-");
    if (start == std::wstring::npos)
        return {};
    const auto end = path.find_first_of(L"\\/", start);
    return path.substr(start, end - start);
}
}

std::wstring StartupError() { return startupError; }

bool game_open() {
    return FindWindowW(nullptr, kTitle) != nullptr;
}

bool init() {
    startupError.clear();
    if (!memory->find_process_id(kProc)) {
        startupError = L"RobloxPlayerBeta.exe was not found. Check that the desktop Roblox client is running.";
        return false;
    }
    if (!memory->attach_to_process(kProc)) {
        startupError = L"Could not open the Roblox process (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }
    const std::wstring runningVersion = RunningClientVersion();
    const std::wstring expectedVersion(Offsets::ClientVersion.begin(), Offsets::ClientVersion.end());
    if (!runningVersion.empty() && runningVersion != expectedVersion) {
        startupError = L"Roblox version mismatch. This build expects " + expectedVersion +
            L", but this PC has " + runningVersion + L". The offsets need updating for that client.";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!memory->find_module_address(kProc) &&
           memory->IsConnected() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto base = memory->get_module_address();
    if (!base) {
        startupError = L"The Roblox module was not available after 10 seconds. The client may still be starting or may have closed.";
        return false;
    }
    std::uintptr_t fake = 0, dm = 0, ve = 0;
    PointerRead fakeRead, alternateFakeRead, dmRead, veRead;
    while (memory->IsConnected()) {
        veRead = ReadPointer(base + Offsets::VisualEngine::Pointer);
        ve = veRead.complete ? veRead.value : 0;
        fakeRead = ReadPointer(base + Offsets::FakeDataModel::Pointer);
        fake = fakeRead.complete && memory->IsValid(fakeRead.value) ? fakeRead.value : 0;
        alternateFakeRead = {};
        if (!fake && ve && memory->IsValid(ve)) {

            alternateFakeRead = ReadPointer(ve + Offsets::VisualEngine::FakeDataModel);
            if (alternateFakeRead.complete && memory->IsValid(alternateFakeRead.value))
                fake = alternateFakeRead.value;
        }
        dmRead = fake && memory->IsValid(fake)
            ? ReadPointer(fake + Offsets::FakeDataModel::RealDataModel) : PointerRead{};
        dm = dmRead.complete ? dmRead.value : 0;
        if (fake && dm && ve && memory->IsValid(fake) && memory->IsValid(dm) && memory->IsValid(ve))
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!fake || !dm || !ve || !memory->IsValid(fake) || !memory->IsValid(dm) || !memory->IsValid(ve)) {
        if (!memory->IsConnected()) {
            startupError = L"Roblox closed while the external was waiting for game data.";
            return false;
        }
        const wchar_t* stage = !fake || !memory->IsValid(fake) ? L"FakeDataModel" :
            !dm || !memory->IsValid(dm) ? L"DataModel" : L"VisualEngine";
        const PointerRead& failed = stage == std::wstring(L"FakeDataModel") ? fakeRead :
            stage == std::wstring(L"DataModel") ? dmRead : veRead;
        const std::uintptr_t slot = stage == std::wstring(L"FakeDataModel")
            ? base + Offsets::FakeDataModel::Pointer :
            stage == std::wstring(L"DataModel") ? fake + Offsets::FakeDataModel::RealDataModel :
            base + Offsets::VisualEngine::Pointer;
        DWORD windowPid = 0;
        if (const HWND gameWindow = FindWindowW(nullptr, kTitle))
            GetWindowThreadProcessId(gameWindow, &windowPid);
        std::wstring probe = PointerStatus(failed) + L" at " + HexAddress(slot) + L".";
        if (stage == std::wstring(L"FakeDataModel")) {
            probe += L"\nVisualEngine: " + PointerStatus(veRead) + L".";
            if (ve && memory->IsValid(ve))
                probe += L"\nRender-engine path: " + PointerStatus(alternateFakeRead) + L".";
        }
        startupError = L"Roblox startup stopped at " + std::wstring(stage) + L".\n" +
            probe + L"\n" +
            L"Selected Roblox PID: " + std::to_wstring(memory->get_process_id()) +
            L"; game window PID: " + std::to_wstring(windowPid) +
            L". Client: " + (runningVersion.empty() ? L"version folder unknown" : runningVersion) + L".";
        return false;
    }
    Globals::dataModel = RBX::RbxInstance{dm};
    Globals::renderEngine = RBX::RenderEngine{ve};
    Globals::workspace = Globals::dataModel.FindChildByClass("Workspace");
    Globals::players = Globals::dataModel.FindChildByClass("Players");
    Globals::camera = Globals::workspace.FindChildByClass("Camera");
    const auto local = memory->read<std::uintptr_t>(Globals::players.Addr + Offsets::Player::LocalPlayer);
    Globals::localPlayer = RBX::RbxInstance{local};
    return true;
}

std::int32_t Run() {
    if (!Globals::dataModel.Addr && !init())
        return 1;
    Cheat::Visuals::NativeChams::Prepare();
    OverlayWindow overlay;
    if (!overlay.Initialize()) {
        startupError = L"The overlay could not initialize. Check the graphics driver and DirectX 11 support.";
        return -1;
    }
    std::cout << "[+] overlay initialized\n[*] press insert to toggle menu\n\n";
    timeBeginPeriod(1);
    std::thread tpThread(Core::tp_handler::thread);

    std::thread skinsThread([] {
        while (Globals::running.load()) {
            { Performance::WorkerScope timing; Skins::Tick(); }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    while (memory->IsConnected() && Globals::running) {

        if (!OverlayLifecycle::PumpMessages() || !Globals::running) break;
        const auto frameStart = std::chrono::steady_clock::now();
        MenuMotion::Update(variables::menuOpen);
        Preferences::Tick();
        Performance::Sample(variables::Misc::performancePanel);
        static auto nextGameInfo=std::chrono::steady_clock::time_point{};
        if(frameStart>=nextGameInfo){
            nextGameInfo=frameStart+std::chrono::seconds(1);
            GameInfo::Tick(memory->read<std::uint64_t>(Globals::dataModel.Addr+Offsets::DataModel::PlaceId));
        }
        if (!game_open())
            break;
        static int previousMenuKey = variables::menuKey;
        static bool menuKeyWasDown = false;
        const bool menuKeyDown = Keys::IsKeyPressed(variables::menuKey);
        if (previousMenuKey != variables::menuKey) {
            previousMenuKey = variables::menuKey;
            menuKeyWasDown = menuKeyDown;
        }
        if(menuKeyDown && !menuKeyWasDown && !imGuiCustom::KeyCapturing() &&
            (!variables::menuOpen || !ImGui::GetIO().WantTextInput)){
            variables::menuOpen=!variables::menuOpen;
            ImGui::ClearActiveID();
        }
        menuKeyWasDown = menuKeyDown;

        Recoil::Tick();
        apply_game_fps_limit();
        if (!Globals::renderEngine.Addr || !Globals::players.Addr || !Globals::localPlayer.Addr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        static auto lastPlayerScan = std::chrono::steady_clock::time_point{};
        const auto nowScan = std::chrono::steady_clock::now();
        if (nowScan - lastPlayerScan >= std::chrono::milliseconds(33)) {
            lastPlayerScan = nowScan;
            Performance::Scope timing(Performance::PlayerCache);
            PlayerCache::updateplayers();
        }

    static VisualKeybind::State visualKeyState;
    DWORD foregroundPid=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    visualKeyState.Update(variables::ESP::visualKeybindEnabled,
        variables::ESP::visualKeybindTarget,variables::ESP::visualKeybindMode,
        variables::ESP::visualKeybindKey,Keys::IsKeyPressed(variables::ESP::visualKeybindKey),
        !variables::menuOpen && foregroundPid==memory->get_process_id(),
        variables::ESP::nativeChams,variables::ESP::meshChams);
    WeaponVisuals::Refresh();
    { Performance::Scope timing(Performance::Native);
      if (variables::ESP::nativeChams || WeaponVisuals::Active()) Cheat::Visuals::NativeChams::Tick();
      else Cheat::Visuals::NativeChams::Stop();
    }

        {
            const auto vm = Globals::renderEngine.GetViewMat();
            Performance::Scope timing(Performance::SilentAim);
            Aimbot::RunAimbot(vm);
        }

        const bool overlayNeeded =
            variables::menuOpen || MenuMotion::Visible() || Keys::SpotifyOn()
            || Keys::KeybindsOn() || variables::Misc::performancePanel
            || (variables::Aimbot::silentEnabled && variables::Aimbot::silentShowFOV)
            || variables::ESP::meshChams
            || Cheat::Visuals::NativeChams::WorldDepthNeeded() || TargetLabels::Active();
        static bool overlayWasNeeded = true;
        if (!overlayNeeded) {
            if (overlayWasNeeded) {

                overlay.BeginFrame();
                overlay.RenderMenu();

                if (overlay.EndFrame()) overlayWasNeeded = false;
            }

            overlay.UpdateWindowStyle(false);

            DrainOverlayMouseEvents();
            Sleep(8);
            continue;
        }
        overlayWasNeeded = true;

        DrainOverlayMouseEvents();
        overlay.BeginFrame();
        { Performance::Scope timing(Performance::Menu); overlay.RenderMenu(); }
        if (!Globals::running.load())
            break;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        { Performance::Scope timing(Performance::Overlay); overlay.render(dl); }
        if (variables::ESP::meshChams || Cheat::Visuals::NativeChams::WorldDepthNeeded()) {
            const auto meshView = Globals::renderEngine.GetViewMat();
            const ImVec2 overlaySize = overlay.GetClientSize();
            Performance::Scope timing(Performance::Mesh);
            RenderMeshVisuals(dl, meshView, overlaySize);
        }

        overlay.EndFrame();

        {
            using clock = std::chrono::steady_clock;
            const long long periodUs = variables::menuOpen ? 8333 : 16666;
            static clock::time_point nextFrame = clock::now();
            const auto nowFrame = clock::now();
            if (nowFrame < nextFrame) {
                const auto remainMs = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - nowFrame).count();
                if (remainMs > 1)
                    Sleep(static_cast<DWORD>(remainMs - 1));
            }
            nextFrame += std::chrono::microseconds(periodUs);
            if (nextFrame < nowFrame)
                nextFrame = nowFrame + std::chrono::microseconds(periodUs);
        }
    }
    Recoil::Stop();
    UiAssets::Shutdown();
    Cheat::Features::RaycastSilent::SetActive(false);
    Cheat::Features::RaycastSilent::Remove();
    Cheat::Visuals::NativeChams::Stop();
    timeEndPeriod(1);
    Preferences::Save();
    Globals::running = false;
    Cheat::Visuals::MeshParser::Shutdown();
    if (skinsThread.joinable())
        skinsThread.join();
    Cheat::Features::RaycastSilent::Remove();
    Skins::Shutdown();
    if (tpThread.joinable())
        tpThread.join();
    overlay.Cleanup();
    return 0;
}
}
