#include <cmath>
#include "MeshParser.h"
#include "sdk/MeshBridge.h"
#include "core/globals/globals.h"

#include <cctype>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace Cheat {
namespace Visuals {
namespace MeshParser {
namespace {

class DiscoveryWorker {
    using Clock = std::chrono::steady_clock;
    struct Record {
        std::shared_ptr<const std::vector<Entry>> parts;
        Clock::time_point due{};
        Clock::time_point used{};
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::unordered_map<std::uint64_t, Record> records;
    std::thread worker;
    bool stopping = false;

    void Run() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!stopping && Globals::running.load()) {
            const auto now = Clock::now();
            std::uint64_t character = 0;
            auto oldest = (Clock::time_point::max)();
            for (auto it = records.begin(); it != records.end();) {
                if (now - it->second.used > std::chrono::seconds(5)) {
                    it = records.erase(it);
                    continue;
                }
                if (it->second.due <= now && it->second.due < oldest) {
                    character = it->first;
                    oldest = it->second.due;
                }
                ++it;
            }
            if (!character) {
                wake.wait_for(lock, std::chrono::milliseconds(10));
                continue;
            }
            lock.unlock();
            std::shared_ptr<const std::vector<Entry>> result;
            try {
                result = std::make_shared<const std::vector<Entry>>(((character&1)?CollectWeapon(character&~std::uint64_t(1)):CollectDrawable(character)));
            } catch (...) {

            }
            lock.lock();
            if (auto it = records.find(character); it != records.end()) {
                if (result) it->second.parts = std::move(result);
                it->second.due = Clock::now() + std::chrono::milliseconds(250);
            }
        }
    }
public:
    std::shared_ptr<const std::vector<Entry>> Request(std::uint64_t character) {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) return {};
        if (!worker.joinable()) worker = std::thread([this] { Run(); });
        auto& record = records[character];
        record.used = Clock::now();
        if (!record.parts) wake.notify_one();
        return record.parts;
    }
    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        wake.notify_one();
        if (worker.joinable()) worker.join();
    }
    ~DiscoveryWorker() { Stop(); }
};

DiscoveryWorker& Discovery() {
    static DiscoveryWorker worker;
    return worker;
}

bool IsBasePartClass(const std::string& cls)
{
	return cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
	       cls == "NegateOperation" || cls == "IntersectOperation" ||
	       cls == "TrussPart" || cls == "WedgePart" || cls == "CornerWedgePart" ||
	       cls == "Seat" || cls == "VehicleSeat" || cls == "SpawnLocation";
}

bool IsSkipClass(const std::string& cls)
{
	return cls == "Humanoid" || cls == "Script" || cls == "LocalScript" ||
	       cls == "ModuleScript" || cls == "Sound" || cls == "Animation" ||
	       cls == "Animator" || cls == "BindableEvent" || cls == "BindableFunction" ||
	       cls == "RemoteEvent" || cls == "RemoteFunction" || cls == "Attachment" ||
	       cls == "Motor6D" || cls == "Weld" || cls == "WeldConstraint" ||
	       cls == "ManualWeld" || cls == "Snap" || cls == "BodyColors" ||
	       cls == "Shirt" || cls == "Pants" || cls == "ShirtGraphic" ||
	       cls == "BodyGyro" || cls == "BodyVelocity" || cls == "BodyForce" ||
	       cls == "Highlight" || cls == "BillboardGui" || cls == "SurfaceGui" ||
	       cls == "ProximityPrompt" || cls == "ClickDetector" ||
	       cls == "WrapTarget" || cls == "WrapLayer" || cls == "SurfaceAppearance" ||
	       cls == "NoCollisionConstraint";
}

bool NameHas(const std::string& s, const char* needle)
{
	if (s.empty() || !needle)
		return false;
	std::string a = s;
	std::string b = needle;
	for (char& c : a) c = (char)std::tolower((unsigned char)c);
	for (char& c : b) c = (char)std::tolower((unsigned char)c);
	return a.find(b) != std::string::npos;
}

bool IsSkipPartName(const std::string& name)
{
	if (name.empty())
		return false;
	if (name == "HumanoidRootPart" || name == "CollisionCapsule")
		return true;
	return NameHas(name, "collision") || NameHas(name, "hitbox") ||
	       NameHas(name, "capsule") || NameHas(name, "nocol");
}

bool IsSkipContainerName(const std::string& name)
{
	return NameHas(name, "weapon") || NameHas(name, "gun") ||
	       NameHas(name, "viewmodel") || NameHas(name, "firstperson") ||
	       NameHas(name, "viewarms") || NameHas(name, "fakearm");
}

bool IsWeaponPartName(const std::string& name)
{
	if (name.empty())
		return false;
	return NameHas(name, "scope") || NameHas(name, "flashlight") ||
	       NameHas(name, "torch") || NameHas(name, "laser") ||
	       NameHas(name, "rail") || NameHas(name, "sight") ||
	       NameHas(name, "optic") || NameHas(name, "magazine") ||
	       NameHas(name, "mag ") || NameHas(name, "silencer") ||
	       NameHas(name, "suppressor") || NameHas(name, "muzzle") ||
	       NameHas(name, "grip") || NameHas(name, "stock") ||
	       NameHas(name, "barrel") || NameHas(name, "bolt") ||
	       NameHas(name, "trigger") || NameHas(name, "attachment");
}

bool IsAccessoryClass(const std::string& cls)
{
	return cls == "Accessory" || cls == "Hat" || cls == "Accoutrement";
}

Kind Classify(const std::string& part_name, const std::string& container, bool under_acc)
{
	if (under_acc)
	{
		if (NameHas(part_name, "face") || NameHas(container, "face"))
			return Kind::Face;
		if (NameHas(container, "hair") || NameHas(part_name, "hair") ||
		    NameHas(container, "ponytail") || NameHas(container, "bun") ||
		    NameHas(container, "beetle") || NameHas(container, "ringo"))
			return Kind::Hair;
		return Kind::Accessory;
	}

	static const char* k_body[] = {
		"Head", "Torso", "UpperTorso", "LowerTorso",
		"LeftUpperArm", "LeftLowerArm", "LeftHand", "Left Arm",
		"RightUpperArm", "RightLowerArm", "RightHand", "Right Arm",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot", "Left Leg",
		"RightUpperLeg", "RightLowerLeg", "RightFoot", "Right Leg",
		"HumanoidRootPart"
	};
	for (const char* n : k_body)
	{
		if (part_name == n)
			return Kind::Body;
	}
	return Kind::Other;
}

std::string ReadContentString(std::uint64_t addr)
{
	if (!g_Memory.IsValid(addr))
		return {};
	std::string s = g_Memory.ReadString(addr);
	if (!s.empty() && s != "Unknown")
		return s;
	std::uint64_t p = g_Memory.Read<std::uint64_t>(addr);
	if (!g_Memory.IsValid(p))
		return {};
	s = g_Memory.ReadString(p);
	if (s == "Unknown")
		return {};
	return s;
}

std::string ReadSpecialMeshId(std::uint64_t sm)
{
	return ReadContentString(sm + Offsets::SpecialMesh::MeshId);
}

std::string ReadCharacterMeshId(std::uint64_t cm)
{
	return ReadContentString(cm + Offsets::CharacterMesh::MeshId);
}

std::string ReadMeshPartId(std::uint64_t part)
{
	std::string s = MeshPart(part).GetMeshId();
	if (s == "Unknown")
		return {};
	return s;
}

void PushPart(
	std::uint64_t character,
	const Instance& part,
	const std::string& container,
	bool under_acc,
	std::unordered_set<std::uint64_t>& seen,
	std::vector<Entry>& out)
{
	if (!g_Memory.IsValid(part.address) || seen.count(part.address))
		return;
	seen.insert(part.address);

	Entry e{};
	e.part = part.address;
	e.character = character;
	e.name = part.GetName();
	e.class_name = part.GetClassName();
	e.container = container;
	e.kind = Classify(e.name, container, under_acc);

	if (e.class_name == "MeshPart")
		e.mesh_id = ReadMeshPartId(part.address);

	for (const auto& c : part.GetChildren())
	{
		const std::string cc = c.GetClassName();
		if (cc == "SpecialMesh" || cc == "FileMesh" || cc == "CylinderMesh" || cc == "BlockMesh")
		{
			e.special_mesh = c.address;
			if (e.mesh_id.empty())
				e.mesh_id = ReadSpecialMeshId(c.address);
			if (e.kind == Kind::Other)
				e.kind = Kind::Special;
			break;
		}
	}

	out.push_back(std::move(e));
}

void Walk(
	std::uint64_t character,
	const Instance& node,
	int depth,
	const std::string& container,
	bool under_acc,
	std::unordered_set<std::uint64_t>& seen,
	std::vector<Entry>& out)
{
	if (!Globals::running.load() || depth > 12 || !g_Memory.IsValid(node.address))
		return;

	const std::string cls = node.GetClassName();
	if (IsSkipClass(cls))
		return;

	if (cls == "Tool")
		return;

	if (IsAccessoryClass(cls))
	{
		const std::string acc = node.GetName();

		for (const auto& c : node.GetChildren())
			Walk(character, c, depth + 1, acc, true, seen, out);
		return;
	}

	const bool under_tool = under_acc ||
		NameHas(node.GetName(), "weapon") || NameHas(node.GetName(), "gun") ||
		NameHas(node.GetName(), "tool") || NameHas(node.GetName(), "flashlight") ||
		NameHas(node.GetName(), "scope");

	if (!under_acc && (cls == "Model" || cls == "Folder"))
	{
		const std::string nm = node.GetName();

		if (IsSkipContainerName(nm))
			return;

		bool maybe_acc = NameHas(nm, "accessory") || NameHas(nm, "hat") ||
			NameHas(nm, "hair") || NameHas(nm, "layer") || NameHas(nm, "mesh") ||
			NameHas(nm, "armor") || NameHas(nm, "clothing") || NameHas(nm, "gear");
		for (const auto& c : node.GetChildren())
			Walk(character, c, depth + 1, maybe_acc ? nm : container, under_tool || maybe_acc, seen, out);
		return;
	}
	if (cls == "CharacterMesh")
	{
		Entry e{};
		e.character = character;
		e.name = node.GetName();
		e.class_name = cls;
		e.container = container;
		e.kind = Kind::CharacterMesh;
		e.mesh_id = ReadCharacterMeshId(node.address);
		out.push_back(std::move(e));
		return;
	}

	if (IsBasePartClass(cls))
	{
		const std::string nm = node.GetName();		if (IsSkipPartName(nm))
			return;
		if (!under_acc && IsWeaponPartName(nm))
			return;

		const bool acc = under_acc || (!under_tool && nm == "Handle");
		if (acc || !under_tool)
		{

			PushPart(character, node, container, acc, seen, out);
		}
		for (const auto& c : node.GetChildren())
			Walk(character, c, depth + 1, container, acc || under_tool, seen, out);
		return;
	}

	for (const auto& c : node.GetChildren())
		Walk(character, c, depth + 1, container, under_acc, seen, out);
}

}

const char* KindName(Kind k)
{
	switch (k)
	{
	case Kind::Body: return "body";
	case Kind::Accessory: return "accessory";
	case Kind::Face: return "face";
	case Kind::Hair: return "hair";
	case Kind::CharacterMesh: return "charmesh";
	case Kind::Special: return "special";
	default: return "other";
	}
}

std::vector<Entry> Collect(std::uint64_t character)
{
	std::vector<Entry> out;
	if (!g_Memory.IsValid(character))
		return out;

	out.reserve(48);
	std::unordered_set<std::uint64_t> seen;
	Walk(character, Instance(character), 0, {}, false, seen, out);
	return out;
}

std::vector<Entry> CollectDrawable(std::uint64_t character)
{
	std::vector<Entry> all = Collect(character);
	std::vector<Entry> out;
	out.reserve(all.size());
	for (auto& e : all)
	{
		if (!e.part || !g_Memory.IsValid(e.part))
			continue;
		if (IsSkipPartName(e.name))
			continue;
		{
			const bool is_acc = e.kind == Kind::Accessory || e.kind == Kind::Hair || e.kind == Kind::Face;
			if (!is_acc && IsWeaponPartName(e.name))
				continue;
		}

		if (e.kind == Kind::Other)
			continue;
		if (e.kind == Kind::Special)
			continue;

		out.push_back(std::move(e));
	}
	return out;
}

std::vector<Entry> CollectWeapon(std::uint64_t root){
    std::vector<Entry> out;
    std::vector<std::pair<Instance,int>> pending{{Instance(root),0}};
    std::unordered_set<std::uint64_t> seen,walked;
    while(!pending.empty() && out.size()<256 && walked.size()<1024){
        auto [node,depth]=pending.back();pending.pop_back();
        if(depth>12||!g_Memory.IsValid(node.address)||!walked.insert(node.address).second)continue;
        const auto cls=node.GetClassName(),name=node.GetName();
        if(IsSkipClass(cls)||IsSkipPartName(name))continue;
        if(IsBasePartClass(cls)){
            const float alpha=g_Memory.Read<float>(node.address+Offsets::BasePart::Transparency);
            if(std::isfinite(alpha)&&alpha<.999f)PushPart(root,node,"weapon",false,seen,out);
        }
        for(const auto& child:node.GetChildren())pending.push_back({child,depth+1});
    }
    return out;
}
std::shared_ptr<const std::vector<Entry>> RequestWeaponDrawable(std::uint64_t root){

    return Discovery().Request(root|1);
}

std::vector<Entry> CollectForBounds(std::uint64_t character)
{
	std::vector<Entry> all = Collect(character);
	std::vector<Entry> out;
	out.reserve(all.size());
	for (auto& e : all)
	{
		if (!e.part || !g_Memory.IsValid(e.part))
			continue;
		if (IsSkipPartName(e.name))
			continue;
		{
			const bool is_acc = e.kind == Kind::Accessory || e.kind == Kind::Hair || e.kind == Kind::Face;
			if (!is_acc && IsWeaponPartName(e.name))
				continue;
		}
		out.push_back(std::move(e));
	}
	return out;
}

std::shared_ptr<const std::vector<Entry>> RequestDrawable(std::uint64_t character) {
    return Discovery().Request(character);
}

void Shutdown() { Discovery().Stop(); }

}
}
}
