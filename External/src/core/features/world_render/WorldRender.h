#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>

namespace world_render
{

    struct Settings
    {
        bool  enabled        = false;
        int   style          = 2;
        float color[4]       = { 1.f, 1.f, 1.f, 1.f };
        int   palette_idx    = 3;
    };

    void set_settings(const Settings& settings);

    void*   host_get_process_handle();
    uint64_t host_get_module_base();

    void set_player_parts(const std::vector<uint64_t>& part_addresses,
                          const std::vector<float>&    positions_xyz);

    void start();
    void stop();

    void apply();
    void restore();
}
