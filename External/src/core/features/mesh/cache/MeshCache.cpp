#include "MeshCache.h"
#include "sdk/MeshBridge.h"
#include "core/globals/globals.h"

#include <windows.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace {

std::uint64_t FindMeshContentProvider()
{
	std::uint64_t dm = Globals::dataModel.Addr;
	if (!g_Memory.IsValid(dm))
		return 0;

	for (const auto& c : Globals::dataModel.GetChildList())
	{
		if (c.GetClass() == "MeshContentProvider")
			return c.Addr;
	}
	return 0;
}

bool LooksLikeMeshData(std::uint64_t md, int& vtx_count, int& fac_count,
                       std::uint64_t& vtx_start, std::uint64_t& fac_start)
{
	if (!g_Memory.IsValid(md))
		return false;
	vtx_start = g_Memory.Read<std::uint64_t>(md + Offsets::MeshData::VertexStart);
	std::uint64_t vtx_end = g_Memory.Read<std::uint64_t>(md + Offsets::MeshData::VertexEnd);
	fac_start = g_Memory.Read<std::uint64_t>(md + Offsets::MeshData::FaceStart);
	std::uint64_t fac_end = g_Memory.Read<std::uint64_t>(md + Offsets::MeshData::FaceEnd);

	vtx_count = (vtx_end > vtx_start) ? (int)((vtx_end - vtx_start) / sizeof(MeshVertex)) : 0;
	fac_count = (fac_end > fac_start) ? (int)((fac_end - fac_start) / sizeof(MeshFace)) : 0;

	return vtx_count > 0 && vtx_count < 50000 &&
	       fac_count > 0 && fac_count < 100000 &&
	       g_Memory.IsValid(vtx_start) && g_Memory.IsValid(fac_start);
}

void CollectKeys(const std::string& raw, const std::string& cleaned, std::vector<std::string>& keys)
{
	auto add = [&](const std::string& k) {
		if (k.empty())
			return;
		for (const auto& e : keys)
			if (e == k)
				return;
		keys.push_back(k);
	};
	add(cleaned);
	add(raw);
	if (raw.rfind("rbxasset://", 0) == 0)
		add(raw.substr(11));
	if (raw.rfind("rbxassetid://", 0) == 0)
		add(raw.substr(13));
}

}

std::string CleanAssetId(const std::string& raw)
{
	if (raw.empty() || raw == "Unknown")
		return {};
	if (raw.rfind("rbxassetid://", 0) == 0)
		return raw.substr(13);
	const auto q = raw.find("?id=");
	if (q != std::string::npos && q + 4 < raw.size())
	{
		std::string id = raw.substr(q + 4);
		const auto end = id.find_first_of("& \n\r\t");
		if (end != std::string::npos)
			id.resize(end);
		return id;
	}
	const auto pos = raw.find_last_of("=/");
	if (pos != std::string::npos && pos + 1 < raw.size())
		return raw.substr(pos + 1);
	return raw;
}

MeshCache& MeshCache::Get()
{
	static MeshCache inst;
	return inst;
}

void MeshCache::Refresh(bool force, bool fullTraversal, const std::vector<std::string>& wanted)
{

    static std::mutex refreshMutex;
    std::unique_lock<std::mutex> refreshLock(refreshMutex, std::defer_lock);
    if (fullTraversal) refreshLock.lock();
    else if (!refreshLock.try_lock()) return;
	const ULONGLONG now = GetTickCount64();
	if (last_refresh_ && !fullTraversal)
	{
		const ULONGLONG gap = force ? 250ull : 1000ull;
		if ((now - last_refresh_) < gap)
			return;
	}

	std::uint64_t mcp = FindMeshContentProvider();
	if (!g_Memory.IsValid(mcp))
		return;

	static const uintptr_t k_cache_offs[] = {
		Offsets::MeshContentProvider::LRUHolder,
		Offsets::MeshContentProvider::LruHolder,
		Offsets::MeshContentProvider::Cache,
		0xc8, 0xd8, 0xf0, 0xe0, 0xe8
	};

	std::uint64_t cache_obj = 0;
	std::uint64_t lru = 0;
	std::uint64_t sentinel = 0;
	std::uint64_t node = 0;

	for (uintptr_t off : k_cache_offs)
	{
		std::uint64_t c = g_Memory.Read<std::uint64_t>(mcp + off);
		if (!g_Memory.IsValid(c))
			continue;
		std::uint64_t l = g_Memory.Read<std::uint64_t>(c + Offsets::MeshContentProvider::LRUCache);
		if (!g_Memory.IsValid(l))
			continue;
		std::uint64_t s = g_Memory.Read<std::uint64_t>(
			l + Offsets::MemEnforcedLRUCache::Head);
		if (!g_Memory.IsValid(s))
			continue;
		std::uint64_t n = g_Memory.Read<std::uint64_t>(s);
		if (!g_Memory.IsValid(n) || n == s)
			continue;

		int readable = 0;
		std::uint64_t look = n;
		for (int i = 0; i < 5 && g_Memory.IsValid(look) && look != s; ++i)
		{
			const std::string id = g_Memory.ReadString(look + Offsets::LRUNode::AssetID);
			if (!id.empty() && id != "Unknown")
				++readable;
			look = g_Memory.Read<std::uint64_t>(look + Offsets::LRUNode::Next);
		}
		if (!readable)
			continue;
		cache_obj = c;
		lru = l;
		sentinel = s;
		node = n;
		break;
	}
	if (!cache_obj || !node)
		return;

	last_refresh_ = now;

	std::unordered_map<std::string, std::shared_ptr<CachedMesh>> temp;
	int max_nodes = fullTraversal ? 12000 : 400;
	int consecutive_cached = 0;

	while (g_Memory.IsValid(node) && node != sentinel && max_nodes-- > 0)
	{
		std::string raw_id = g_Memory.ReadString(node + Offsets::LRUNode::AssetID);
		std::string id = CleanAssetId(raw_id);
		if (!wanted.empty() && std::none_of(wanted.begin(), wanted.end(), [&](const auto& key){return CleanAssetId(key)==id;})) {
			node = g_Memory.Read<std::uint64_t>(node + Offsets::LRUNode::Next);
			continue;
		}

		if (!id.empty() && temp.find(id) == temp.end())
		{
			bool already_cached = false;
			std::shared_ptr<CachedMesh> existing;
			{
				std::lock_guard<std::mutex> lk(mtx_);
				if (auto cache_it = cache_.find(id); cache_it != cache_.end())
				{
					already_cached = true;
					existing = cache_it->second;
				}
			}

			if (already_cached)
			{
				std::vector<std::string> keys;
				CollectKeys(raw_id, id, keys);
				for (const auto& k : keys)
					temp.emplace(k, existing);
				if (++consecutive_cached > 15 && !fullTraversal) break;
				node = g_Memory.Read<std::uint64_t>(node + Offsets::LRUNode::Next);
				continue;
			}
			consecutive_cached = 0;

			std::uint64_t pointer = g_Memory.Read<std::uint64_t>(
				node + Offsets::LRUNode::CachedItem);

			std::uint64_t mesh_data = 0;
			int vtx_count = 0, fac_count = 0;
			std::uint64_t vtx_start = 0, fac_start = 0;

			if (g_Memory.IsValid(pointer))
			{
				mesh_data = g_Memory.Read<std::uint64_t>(
					pointer + Offsets::CachedItem::FileMeshData);
			}
			if (!LooksLikeMeshData(mesh_data, vtx_count, fac_count, vtx_start, fac_start))
			{
				const std::uint64_t legacy = g_Memory.Read<std::uint64_t>(
					node + Offsets::MeshContentProvider::ToMeshData);
				const std::uint64_t legacy_md = g_Memory.IsValid(legacy)
					? g_Memory.Read<std::uint64_t>(legacy + Offsets::MeshContentProvider::MeshData)
					: 0;
				if (LooksLikeMeshData(legacy_md, vtx_count, fac_count, vtx_start, fac_start))
					mesh_data = legacy_md;
				else if (LooksLikeMeshData(pointer, vtx_count, fac_count, vtx_start, fac_start))
					mesh_data = pointer;
				else
					mesh_data = 0;
			}

			if (mesh_data && vtx_count > 0)
			{
				auto mesh = std::make_shared<CachedMesh>();
				mesh->asset_id = id;
				mesh->vertices.resize((std::size_t)vtx_count);
				mesh->faces.resize((std::size_t)fac_count);

				const SIZE_T vb = (SIZE_T)vtx_count * sizeof(MeshVertex);
				const SIZE_T fb = (SIZE_T)fac_count * sizeof(MeshFace);
				if (g_Memory.IsValid(vtx_start) &&
                    g_Memory.IsValid(fac_start))
				{
                    g_Memory.ReadRaw(vtx_start, mesh->vertices.data(), vb);
                    g_Memory.ReadRaw(fac_start, mesh->faces.data(), fb);
                    if (!std::isnan(mesh->vertices[0].pos[0])) {
                        std::vector<std::string> keys;
                        CollectKeys(raw_id, id, keys);
                        for (const auto& k : keys)
                            temp.emplace(k, mesh);
                    }
				}
			}
		}

		node = g_Memory.Read<std::uint64_t>(node + Offsets::LRUNode::Next);
	}

	std::lock_guard<std::mutex> lk(mtx_);
	for (auto& kv : temp)
		cache_[kv.first] = std::move(kv.second);
}

std::shared_ptr<const CachedMesh> MeshCache::FindShared(const std::string& asset_id) const
{
	if (asset_id.empty() || asset_id == "Unknown")
		return nullptr;

	std::vector<std::string> keys;
	const std::string cleaned = CleanAssetId(asset_id);
	CollectKeys(asset_id, cleaned, keys);

	std::lock_guard<std::mutex> lk(mtx_);
	for (const auto& k : keys)
	{
		auto it = cache_.find(k);
		if (it == cache_.end() || !it->second)
			continue;
		return it->second;
	}
	return nullptr;
}

bool MeshCache::Find(const std::string& asset_id, CachedMesh& out) const
{
	auto p = FindShared(asset_id);
	if (!p)
		return false;
	out = *p;
	return true;
}

std::size_t MeshCache::Count() const
{
	std::lock_guard<std::mutex> lk(mtx_);
	return cache_.size();
}

}
}
