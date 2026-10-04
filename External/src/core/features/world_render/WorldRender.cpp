#include "WorldRender.h"
#include "../../../sdk/offsets.h"

#include <windows.h>

#include <cstring>
#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>

namespace fce_offsets
{
    inline constexpr uint64_t VTableRva          = Offsets::FastClusterEntity::VTableRva;
    inline constexpr uintptr_t RenderQueueId     = 0x10;
    inline constexpr uintptr_t AlphaByte         = 0x14;
    inline constexpr uintptr_t TechniqueArrayPtr = 0x70;
    inline constexpr uintptr_t BasePartSubBackref = 0x130;
}

namespace technique_array_offsets
{
    inline constexpr uintptr_t Begin  = 0x00;
    inline constexpr uintptr_t End    = 0x08;
    inline constexpr uintptr_t Stride = 0x88;
}

namespace material_layer_offsets
{
    inline constexpr uintptr_t FillModeByte = 0x11;
    inline constexpr uintptr_t MatFlags     = 0x18;
    inline constexpr uintptr_t Param        = 0x1c;
    inline constexpr uintptr_t Flags2       = 0x20;
    inline constexpr uintptr_t ColorData    = 0x24;
}

namespace render_queue_ids
{
    inline constexpr uint32_t AlwaysOnTop     = 0x0d;
    inline constexpr uint32_t OnTopWithDepth  = 0x0b;
    inline constexpr uint32_t GlassTint       = 0x07;
    inline constexpr uint32_t Glass           = 0x08;
    inline constexpr uint32_t Transparent     = 0x09;
}

namespace chams_style
{
    inline constexpr int Default       = 0;
    inline constexpr int Ghost         = 1;
    inline constexpr int Wireframe     = 2;
    inline constexpr int ColoredFrame  = 3;
    inline constexpr int Colored       = 4;
    inline constexpr int SmokeNoShadow = 5;
    inline constexpr int Smoke         = 6;
    inline constexpr int Invisible     = 7;
    inline constexpr int Count         = 8;
}

namespace world_render
{
    static Settings pendingSettings{};
    static std::mutex settingsMutex;
    static thread_local Settings g_settings{};
    void set_settings(const Settings& value){std::lock_guard<std::mutex> lock(settingsMutex);pendingSettings=value;}

    std::mutex            g_player_mtx;
    std::vector<uint64_t> g_player_parts;
    std::vector<float>    g_player_anchors;

    void set_player_parts(const std::vector<uint64_t>& part_addresses,
                          const std::vector<float>& positions_xyz)
    {
        std::lock_guard<std::mutex> lk(g_player_mtx);
        g_player_parts.assign(part_addresses.begin(), part_addresses.end());
        g_player_anchors.assign(positions_xyz.begin(), positions_xyz.end());
    }

    namespace
    {
        HANDLE proc() { return (HANDLE)host_get_process_handle(); }

        inline bool looks_valid(uint64_t addr)
        {
            return addr > 0x10000ull && addr < 0x7FFFFFFEFFFFull;
        }

        bool rpm(uint64_t addr, void* out, size_t size)
        {
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return false;
            SIZE_T rd = 0;
            return ReadProcessMemory(h, (LPCVOID)addr, out, size, &rd) && rd == size;
        }

        template <typename T>
        T read_mem(uint64_t addr) { T v{}; rpm(addr, &v, sizeof(T)); return v; }

        template <typename T>
        bool write_mem(uint64_t addr, const T& v)
        {
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return false;
            SIZE_T wr = 0;
            return WriteProcessMemory(h, (LPVOID)addr, &v, sizeof(T), &wr) && wr == sizeof(T);
        }

        uint64_t g_vt = 0;
        int      g_applied_style = -1;
        std::thread       g_thread;
        std::atomic<bool> g_run{ false };

        uint64_t host_base() { return host_get_module_base(); }

        uintptr_t get_fce_vtable_rva()          { return fce_offsets::VTableRva; }
        uintptr_t get_tech_array_ptr_offset()   { return fce_offsets::TechniqueArrayPtr; }
        uintptr_t get_render_queue_id_offset()  { return fce_offsets::RenderQueueId; }
        uintptr_t get_alpha_byte_offset()       { return fce_offsets::AlphaByte; }
        uintptr_t get_tech_array_begin_offset() { return technique_array_offsets::Begin; }
        uintptr_t get_tech_array_end_offset()   { return technique_array_offsets::End; }
        uintptr_t get_tech_array_stride()       { return technique_array_offsets::Stride; }
        uintptr_t get_fillmode_offset()         { return material_layer_offsets::FillModeByte; }
        uintptr_t get_matflags_offset()         { return material_layer_offsets::MatFlags; }
        uintptr_t get_param_offset()            { return material_layer_offsets::Param; }
        uintptr_t get_flags2_offset()           { return material_layer_offsets::Flags2; }
        uintptr_t get_colordata_offset()        { return material_layer_offsets::ColorData; }

        bool EntAlive(uintptr_t ent)
        {
            if (!g_vt || !looks_valid(ent)) return false;
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return false;

            uint64_t vt = 0;
            if (!rpm(ent, &vt, 8) || vt != g_vt) return false;

            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQueryEx(h, (LPCVOID)(ent + get_render_queue_id_offset()), &mbi, sizeof(mbi)))
                return false;

            return (mbi.State == MEM_COMMIT) &&
                (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY));
        }

        bool EntKnown(uintptr_t ent)
        {
            if (!g_vt || !looks_valid(ent)) return false;
            return read_mem<uint64_t>(ent) == g_vt;
        }

        bool LayerWritable(uintptr_t layer)
        {
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return false;

            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQueryEx(h, (LPCVOID)(layer + get_fillmode_offset()), &mbi, sizeof(mbi)))
                return false;

            return (mbi.State == MEM_COMMIT) &&
                (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY));
        }

        uint64_t DiscoverFceVtable(uint64_t base)
        {
            if (!base) return 0;
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return 0;

            auto probe = [&](const uint8_t* p) -> bool {
                uint32_t qid = 0;
                std::memcpy(&qid, p + 0x10, 4);
                if (qid > 0x4000) return false;

                uint64_t arr = 0;
                std::memcpy(&arr, p + 0x70, 8);
                if (arr < 0x10000 || arr > 0x7FFFFFFEFFFFull) return false;

                uint64_t begin = 0, end = 0;
                if (!rpm(arr, &begin, 8) || !rpm(arr + 8, &end, 8)) return false;
                if (begin < 0x10000 || end <= begin) return false;
                const uint64_t bytes = end - begin;
                if (bytes > 64ull * 1024ull || (bytes % 0x88) != 0) return false;

                uint64_t back = 0;
                std::memcpy(&back, p + 0x130, 8);
                if (back < 0x10000 || back > 0x7FFFFFFEFFFFull) return false;
                return true;
                };

            std::unordered_map<uint64_t, uint32_t> counts;
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
            const uintptr_t max_addr = (uintptr_t)si.lpMaximumApplicationAddress;

            static std::vector<uint8_t> buf;
            MEMORY_BASIC_INFORMATION mbi{};

            while (addr < max_addr)
            {
                if (!VirtualQueryEx(h, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
                const bool ok = mbi.State == MEM_COMMIT && mbi.Type != MEM_IMAGE &&
                    (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE ||
                     mbi.Protect == PAGE_WRITECOPY || mbi.Protect == PAGE_EXECUTE_WRITECOPY);
                if (ok && mbi.RegionSize > 0 && mbi.RegionSize <= 512ull * 1024ull * 1024ull)
                {
                    const uintptr_t reg = (uintptr_t)mbi.BaseAddress;
                    const SIZE_T sz = mbi.RegionSize;
                    if (buf.size() < sz) buf.resize(sz);
                    SIZE_T got = 0;
                    if (ReadProcessMemory(h, (LPCVOID)reg, buf.data(), sz, &got) && got > 0x138 + 8)
                    {
                        for (SIZE_T i = 0; i + 0x138 + 8 <= got; i += 8)
                        {
                            const uint64_t maybe_vt = *reinterpret_cast<const uint64_t*>(buf.data() + i);
                            if (maybe_vt < base || maybe_vt - base > 0x20000000ull)
                                continue;
                            if (probe(buf.data() + i))
                                counts[maybe_vt]++;
                        }
                    }
                }
                const uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                if (next <= addr) break;
                addr = next;
            }

            uint64_t best = 0;
            uint32_t best_count = 0;
            for (const auto& [vt, n] : counts)
                if (n > best_count) { best_count = n; best = vt; }
            return (best_count >= 4) ? best : 0;
        }

        uint32_t StyleQueue(int style)
        {
            switch (style)
            {
            case chams_style::SmokeNoShadow: return render_queue_ids::GlassTint;
            case chams_style::Smoke:         return render_queue_ids::Glass;
            case chams_style::Invisible:     return render_queue_ids::OnTopWithDepth;
            default:                         return render_queue_ids::AlwaysOnTop;
            }
        }

        static const float k_palette[7][3] = {
            { 1.f, 0.f,   0.f   },
            { 0.f, 1.f,   0.f   },
            { 1.f, 0.50f, 0.f   },
            { 0.f, 0.50f, 1.f   },
            { 1.f, 0.f,   1.f   },
            { 0.f, 1.f,   1.f   },
            { 1.f, 1.f,   1.f   },
        };

        uint32_t ColorParam(int idx)
        {
            idx = (idx < 0) ? 0 : (idx > 6 ? 6 : idx);
            return (uint32_t)(idx + 1);
        }

        int NearestColorIdx(const float c[4])
        {
            float best = 1.0e30f;
            int   bi = 6;
            for (int i = 0; i < 7; ++i)
            {
                float dr = c[0] - k_palette[i][0];
                float dg = c[1] - k_palette[i][1];
                float db = c[2] - k_palette[i][2];
                float d = dr * dr + dg * dg + db * db;
                if (d < best) { best = d; bi = i; }
            }
            return bi;
        }

        struct LayerBackup
        {
            uint8_t  fillmode = 0;
            uint32_t matflags = 0;
            uint32_t param    = 0;
            uint32_t flags2   = 0;
            uint32_t color    = 0;
        };

        std::mutex g_mtx;
        std::unordered_map<uintptr_t, uint32_t>               g_saved;
        std::unordered_map<uintptr_t, uint8_t>                g_alpha;
        std::unordered_map<uintptr_t, LayerBackup>            g_layers;
        std::unordered_map<uintptr_t, std::vector<uintptr_t>> g_ent_layers;
        std::unordered_map<uintptr_t, int>                    g_miss;

        uint64_t g_discovered_vt = 0;
        int      g_empty_scans = 0;

        std::unordered_set<uintptr_t> g_known;

        constexpr int kEmptyBeforeDiscovery = 3;
        constexpr int kDiscoveryRetryEvery  = 60;

        void DropLayersLocked(uintptr_t ent)
        {
            auto lit = g_ent_layers.find(ent);
            if (lit == g_ent_layers.end()) return;
            for (uintptr_t layer : lit->second)
                g_layers.erase(layer);
            g_ent_layers.erase(lit);
        }

        void RestoreLayersLocked(uintptr_t ent)
        {
            auto lit = g_ent_layers.find(ent);
            if (lit == g_ent_layers.end()) return;
            const bool alive = EntAlive(ent);
            for (uintptr_t layer : lit->second)
            {
                auto bit = g_layers.find(layer);
                if (bit == g_layers.end()) continue;
                if (alive && LayerWritable(layer))
                {
                    const auto& b = bit->second;
                    write_mem<uint8_t>(layer + get_fillmode_offset(),  b.fillmode);
                    write_mem<uint32_t>(layer + get_matflags_offset(), b.matflags);
                    write_mem<uint32_t>(layer + get_param_offset(),    b.param);
                    write_mem<uint32_t>(layer + get_flags2_offset(),   b.flags2);
                    write_mem<uint32_t>(layer + get_colordata_offset(),b.color);
                }
                g_layers.erase(bit);
            }
            g_ent_layers.erase(lit);
        }

        void RestoreOne(uintptr_t ent)
        {
            if (!EntAlive(ent))
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                g_saved.erase(ent);
                g_alpha.erase(ent);
                DropLayersLocked(ent);
                return;
            }

            std::lock_guard<std::mutex> lk(g_mtx);
            uintptr_t rq_offset = get_render_queue_id_offset();
            uintptr_t alpha_offset = get_alpha_byte_offset();

            auto it = g_saved.find(ent);
            if (it != g_saved.end())
            {
                write_mem<uint32_t>(ent + rq_offset, it->second);
                g_saved.erase(it);
            }

            auto ait = g_alpha.find(ent);
            if (ait != g_alpha.end())
            {
                write_mem<uint8_t>(ent + alpha_offset, ait->second);
                g_alpha.erase(ait);
            }

            RestoreLayersLocked(ent);
        }

        void RestoreAll()
        {
            std::unordered_map<uintptr_t, uint32_t>               saved;
            std::unordered_map<uintptr_t, uint8_t>                alpha;
            std::unordered_map<uintptr_t, std::vector<uintptr_t>> ent_layers;
            std::unordered_map<uintptr_t, LayerBackup>            layers;
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                saved.swap(g_saved);
                alpha.swap(g_alpha);
                ent_layers.swap(g_ent_layers);
                layers.swap(g_layers);
                g_miss.clear();
                g_applied_style = -1;
            }

            uintptr_t rq_offset = get_render_queue_id_offset();
            uintptr_t alpha_offset = get_alpha_byte_offset();

            for (const auto& [ent, id] : saved)
            {
                if (!EntAlive(ent)) continue;
                write_mem<uint32_t>(ent + rq_offset, id);

                auto ait = alpha.find(ent);
                if (ait != alpha.end())
                    write_mem<uint8_t>(ent + alpha_offset, ait->second);

                auto lit = ent_layers.find(ent);
                if (lit == ent_layers.end()) continue;

                for (uintptr_t layer : lit->second)
                {
                    auto bit = layers.find(layer);
                    if (bit == layers.end() || !LayerWritable(layer)) continue;

                    const auto& b = bit->second;
                    write_mem<uint8_t>(layer + get_fillmode_offset(),  b.fillmode);
                    write_mem<uint32_t>(layer + get_matflags_offset(), b.matflags);
                    write_mem<uint32_t>(layer + get_param_offset(),    b.param);
                    write_mem<uint32_t>(layer + get_flags2_offset(),   b.flags2);
                    write_mem<uint32_t>(layer + get_colordata_offset(),b.color);
                }
            }
        }

        void ApplyLayers(uintptr_t ent, uint8_t fill,
                         uint32_t param, uint32_t flags2, uint32_t color)
        {
            if (!EntAlive(ent)) return;

            uintptr_t off_tech  = get_tech_array_ptr_offset();
            uintptr_t off_begin = get_tech_array_begin_offset();
            uintptr_t off_end   = get_tech_array_end_offset();
            uintptr_t stride    = get_tech_array_stride();
            if (stride == 0) stride = technique_array_offsets::Stride;

            uintptr_t off_fill = get_fillmode_offset();
            uintptr_t off_mf   = get_matflags_offset();
            uintptr_t off_param = get_param_offset();
            uintptr_t off_f2   = get_flags2_offset();
            uintptr_t off_cd   = get_colordata_offset();

            uint64_t arr = read_mem<uint64_t>(ent + off_tech);
            if (!looks_valid(arr)) return;

            uint64_t begin = read_mem<uint64_t>(arr + off_begin);
            uint64_t end   = read_mem<uint64_t>(arr + off_end);
            if (!looks_valid(begin) || end <= begin) return;
            size_t bytes = (size_t)(end - begin);
            if (bytes > 64ull * 1024ull) return;
            size_t count = bytes / stride;
            if (count == 0 || count > 256) return;

            for (size_t i = 0; i < count; ++i)
            {
                uintptr_t layer = begin + i * stride;
                if (!LayerWritable(layer)) continue;

                {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    if (!g_layers.count(layer))
                    {
                        LayerBackup b{};
                        b.fillmode = read_mem<uint8_t>(layer + off_fill);
                        b.matflags = read_mem<uint32_t>(layer + off_mf);
                        b.param    = read_mem<uint32_t>(layer + off_param);
                        b.flags2   = read_mem<uint32_t>(layer + off_f2);
                        b.color    = read_mem<uint32_t>(layer + off_cd);
                        g_layers[layer] = b;
                        g_ent_layers[ent].push_back(layer);
                    }
                }

                write_mem<uint8_t>(layer + off_fill, fill);
                write_mem<uint32_t>(layer + off_mf, 0u);
                write_mem<uint32_t>(layer + off_param, param);
                write_mem<uint32_t>(layer + off_f2, flags2);
                write_mem<uint32_t>(layer + off_cd, color);
            }
        }

        bool ApplyStyleLayers(uintptr_t ent, int style)
        {

            const float* pc = g_settings.color;
            const int di = g_settings.palette_idx;

            switch (style)
            {
            case chams_style::Ghost:
                ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
                return true;
            case chams_style::Wireframe:
                ApplyLayers(ent, 1, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
                return true;
            case chams_style::ColoredFrame:
                ApplyLayers(ent, 1, ColorParam(di), 7u, 0xFFFFFFFFu);
                return true;
            case chams_style::Colored:
                ApplyLayers(ent, 0, ColorParam(di), 15u, 0xFFFFFFFFu);
                return true;
            case chams_style::SmokeNoShadow:
                ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
                return true;
            case chams_style::Smoke:
                ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
                return true;
            case chams_style::Invisible:
                ApplyLayers(ent, 0, ColorParam(6), 0u, 0xFFFFFFFFu);
                return true;
            default: return false;
            }
        }

        int ActiveStyle()
        {
            int s = g_settings.style;
            return (s < 0) ? 0 : (s >= chams_style::Count ? chams_style::Count - 1 : s);
        }

        void ApplyEntity(uintptr_t ent)
        {
            if (!EntKnown(ent)) return;

            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return;

            uintptr_t rq_offset = get_render_queue_id_offset();
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQueryEx(h, (LPCVOID)(ent + rq_offset), &mbi, sizeof(mbi))) return;

            bool writable = (mbi.State == MEM_COMMIT) &&
                (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY));
            if (!writable) return;

            uintptr_t alpha_offset = get_alpha_byte_offset();

            {
                std::lock_guard<std::mutex> lk(g_mtx);
                if (!g_saved.count(ent))
                {
                    g_saved[ent]   = read_mem<uint32_t>(ent + rq_offset);
                    g_alpha[ent]   = read_mem<uint8_t>(ent + alpha_offset);
                }
            }

            int style = ActiveStyle();
            write_mem<uint32_t>(ent + rq_offset, StyleQueue(style));

            {
                std::lock_guard<std::mutex> lk(g_mtx);
                auto ait = g_alpha.find(ent);
                if (ait != g_alpha.end())
                    write_mem<uint8_t>(ent + alpha_offset, ait->second);
            }

            if (ApplyStyleLayers(ent, style))
                return;

            std::lock_guard<std::mutex> lk(g_mtx);
            if (g_ent_layers.count(ent))
                RestoreLayersLocked(ent);
        }

        void DropDeadLocked(uintptr_t ent)
        {
            g_saved.erase(ent);
            g_alpha.erase(ent);
            DropLayersLocked(ent);
            g_miss.erase(ent);
        }

        void RefreshKnown()
        {
            int style = ActiveStyle();
            std::vector<uintptr_t> ents;
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                ents.reserve(g_saved.size());
                for (const auto& [ent, _] : g_saved)
                    ents.push_back(ent);
            }

            const bool had_layers = (g_applied_style >= 1 && g_applied_style <= chams_style::Count - 1);
            const bool want_layers = (style >= 1 && style <= chams_style::Count - 1);
            if (had_layers && !want_layers)
            {
                for (uintptr_t ent : ents)
                {
                    if (!EntKnown(ent)) continue;
                    std::lock_guard<std::mutex> lk(g_mtx);
                    RestoreLayersLocked(ent);
                }
            }

            for (uintptr_t ent : ents)
            {
                if (!EntKnown(ent))
                {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    int& n = g_miss[ent];
                    ++n;
                    if (n >= 25)
                        DropDeadLocked(ent);
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    g_miss[ent] = 0;
                }
                ApplyEntity(ent);
            }
            g_applied_style = style;
        }

        constexpr float kChamsRadius = 7.0f;

        bool DirectRefsPlayer(const uint8_t* buf_ent,
                              const std::unordered_set<uintptr_t>& parts)
        {
            if (parts.empty()) return false;
            for (uintptr_t off = 0; off + 8 <= 0x300; off += 8)
            {
                uintptr_t q = 0;
                std::memcpy(&q, buf_ent + off, sizeof(q));
                if (q > 0x10000u && q < 0x7FFFFFFEFFFFull && parts.count(q))
                    return true;
            }
            return false;
        }

        int SpatialClassify(const uint8_t* buf_ent,
                            const std::vector<float>& anchors)
        {
            if (anchors.empty()) return -1;

            const float r2 = kChamsRadius * kChamsRadius;
            float best2 = r2;
            bool any = false;

            for (uintptr_t f0 = 0x40; f0 + 24 <= 0x300; f0 += 4)
            {
                float mn[3] = {}, mx[3] = {};
                for (int a = 0; a < 3; ++a)
                {
                    std::memcpy(&mn[a], buf_ent + f0 + (size_t)a * 8, 4);
                    std::memcpy(&mx[a], buf_ent + f0 + (size_t)a * 8 + 4, 4);
                }
                if (!std::isfinite(mn[0]) || !std::isfinite(mn[1]) || !std::isfinite(mn[2]) ||
                    !std::isfinite(mx[0]) || !std::isfinite(mx[1]) || !std::isfinite(mx[2]))
                    continue;

                bool sane = true;
                for (int a = 0; a < 3; ++a)
                {
                    const float span = mx[a] - mn[a];
                    if (span < 0.02f || span > 3000.f)
                    {
                        sane = false;
                        break;
                    }
                }
                if (!sane) continue;

                const float cx = (mn[0] + mx[0]) * 0.5f;
                const float cy = (mn[1] + mx[1]) * 0.5f;
                const float cz = (mn[2] + mx[2]) * 0.5f;

                float d2 = 1.0e30f;
                for (size_t i = 0; i + 2 < anchors.size(); i += 3)
                {
                    const float dx = cx - anchors[i];
                    const float dy = cy - anchors[i + 1];
                    const float dz = cz - anchors[i + 2];
                    const float q = dx * dx + dy * dy + dz * dz;
                    if (q < d2) d2 = q;
                }
                if (d2 < best2)
                {
                    best2 = d2;
                    any = true;
                }
            }

            if (!any) return -10;
            return (best2 <= r2) ? 1 : 0;
        }

        bool BindingRefsPlayer(const uint8_t* buf_ent, HANDLE h,
                               const std::unordered_set<uintptr_t>& parts)
        {
            if (parts.empty()) return false;
            uintptr_t bind = 0;
            std::memcpy(&bind, buf_ent + fce_offsets::BasePartSubBackref, sizeof(bind));
            if (bind <= 0x10000u || bind >= 0x7FFFFFFEFFFFull) return false;

            uint8_t bbuf[0x400]{};
            SIZE_T rd = 0;
            if (!ReadProcessMemory(h, (LPCVOID)bind, bbuf, sizeof(bbuf), &rd) || rd < 8)
                return false;
            for (SIZE_T o = 0; o + 8 <= rd; o += 8)
            {
                uintptr_t q = 0;
                std::memcpy(&q, bbuf + o, sizeof(q));
                if (q > 0x10000u && q < 0x7FFFFFFEFFFFull && parts.count(q))
                    return true;
            }
            return false;
        }

        void ScanOnce(uintptr_t vt)
        {
            g_known.clear();
            HANDLE h = proc();
            if (!h || h == INVALID_HANDLE_VALUE) return;
            g_vt = vt;

            std::unordered_set<uintptr_t> player_parts;
            std::vector<float> anchors;
            {
                std::lock_guard<std::mutex> lk(g_player_mtx);
                player_parts.insert(g_player_parts.begin(), g_player_parts.end());
                anchors = g_player_anchors;
            }

            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            uintptr_t max_va = (uintptr_t)si.lpMaximumApplicationAddress;
            uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;

            static std::vector<uint8_t> buf;
            MEMORY_BASIC_INFORMATION mbi{};

            while (addr < max_va)
            {
                if (!VirtualQueryEx(h, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
                bool readable = (mbi.Protect == PAGE_READWRITE ||
                                 mbi.Protect == PAGE_EXECUTE_READWRITE ||
                                 mbi.Protect == PAGE_WRITECOPY ||
                                 mbi.Protect == PAGE_EXECUTE_WRITECOPY);
                bool ok = mbi.State == MEM_COMMIT && mbi.Type != MEM_IMAGE && readable;
                if (ok && mbi.RegionSize > 0 && mbi.RegionSize <= 512ull * 1024ull * 1024ull)
                {
                    uintptr_t base = (uintptr_t)mbi.BaseAddress;
                    SIZE_T sz = mbi.RegionSize;
                    if (buf.size() < sz) buf.resize(sz);
                    SIZE_T got = 0;
                    if (ReadProcessMemory(h, (LPCVOID)base, buf.data(), sz, &got) && got >= 16)
                    {
                        for (SIZE_T i = 0; i + 0x300 <= got; i += 8)
                        {
                            if (*reinterpret_cast<const uintptr_t*>(buf.data() + i) != vt) continue;
                            uintptr_t node = *reinterpret_cast<const uintptr_t*>(buf.data() + i + 8);
                            if (node < 0x10000u || node >= 0x7FFFFFFEFFFFull) continue;
                            uintptr_t ent = base + i;

                            g_known.insert(ent);

                            bool is_player = DirectRefsPlayer(buf.data() + i, player_parts);
                            if (!is_player)
                            {
                                const int sc = SpatialClassify(buf.data() + i, anchors);
                                if (sc > 0) { is_player = true; }
                                else if (sc < 0)
                                {

                                    if (BindingRefsPlayer(buf.data() + i, h, player_parts))
                                        is_player = true;
                                }
                            }

                            if (is_player)
                                continue;
                            ApplyEntity(ent);
                        }
                    }
                }
                uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                if (next <= addr) break;
                addr = next;
            }

        }

        void Loop()
        {
            bool was_on = false;
            int  tick = 0;

            while (g_run.load(std::memory_order_relaxed))
            {
                {std::lock_guard<std::mutex> lock(settingsMutex);g_settings=pendingSettings;}
                bool on = g_settings.enabled && proc() != nullptr && proc() != INVALID_HANDLE_VALUE;

                if (on)
                {
                    if (!was_on)
                    {
                        std::lock_guard<std::mutex> lk(g_mtx);
                        g_saved.clear();
                        g_layers.clear();
                        g_ent_layers.clear();
                        g_miss.clear();
                        g_applied_style = -1;
                        g_empty_scans = 0;
                        was_on = true;
                        tick = 0;
                    }

                    if ((tick % 34) == 0)
                    {
                        uint64_t base = host_base();
                        if (base)
                        {

                            if (g_discovered_vt)
                                g_vt = g_discovered_vt;
                            else
                                g_vt = base + get_fce_vtable_rva();
                        }

                        RefreshKnown();

                        if (g_vt)
                        {
                            ScanOnce(g_vt);

                            if (g_known.empty() && !g_discovered_vt)
                            {
                                const int n = ++g_empty_scans;
                                const bool attempt =
                                    (n == kEmptyBeforeDiscovery) ||
                                    (n > kEmptyBeforeDiscovery &&
                                     (n - kEmptyBeforeDiscovery) % kDiscoveryRetryEvery == 0);
                                if (attempt)
                                {
                                    const uint64_t found = DiscoverFceVtable(base);
                                    if (found)
                                    {
                                        g_discovered_vt = found;
                                        if (found != g_vt)
                                        {
                                            g_vt = found;
                                            ScanOnce(g_vt);
                                        }
                                        g_empty_scans = 0;
                                    }
                                }
                            }
                            else if (!g_known.empty())
                            {
                                g_empty_scans = 0;
                            }
                        }
                    }

                    ++tick;
                }
                else if (was_on)
                {
                    RestoreAll();
                    was_on = false;
                    tick = 0;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(on ? 15 : 200));
            }

            if (was_on) RestoreAll();
        }
    }

    void apply()
    {
        if (!g_vt) {
            uint64_t base = host_base();
            if (base) g_vt = base + get_fce_vtable_rva();
        }
        if (g_vt) RefreshKnown();
    }

    void restore() { RestoreAll(); }

    void start()
    {
        if (g_run.exchange(true)) return;
        g_thread = std::thread(Loop);
    }

    void stop()
    {
        if (!g_run.exchange(false)) return;
        if (g_thread.joinable()) g_thread.join();
    }

}
