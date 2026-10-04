#pragma once

#include "NativeWorldDepth.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace NativePattern {

struct Material {
    float color[4];
    float glow[4];
    float time;
    int mode;
    float depth_scale;
    float depth_bias;

    float pattern[4];
    float cam_right[4];
    float cam_up[4];
    float cam_fwd[4];
    float cam_pos[4];
    float org_pos[4];
    float org_x[4];
    float org_y[4];
    float org_z[4];
    float frame[4];

    float depth[4];

    float live_view[16];
    float draw_body[12];
    float body_status[4];
    NativeWorldDepth::Snapshot world_depth;
};
static_assert(sizeof(Material) == 448, "cbuffer size changed - update the thunk and the test");
static_assert(offsetof(Material, world_depth) == 352, "world snapshot packing changed");
static_assert(offsetof(Material, live_view) == 224, "live camera packing changed");
static_assert(offsetof(Material, pattern) == 48, "cbuffer packing changed");
static_assert(offsetof(Material, cam_right) == 64, "cbuffer packing changed");
static_assert(offsetof(Material, frame) == 192, "cbuffer packing changed");
static_assert(offsetof(Material, depth) == 208, "cbuffer packing changed");

struct Camera {
    float right[3]{1, 0, 0};
    float up[3]{0, 1, 0};
    float fwd[3]{0, 0, 1};
    float pos[3]{0, 0, 0};
    float ax{1.0f};
    float ay{1.0f};
    float az{1.0f};

    float azz{0.0f};
    float bz{1.0f};
    float width{0.0f};
    float height{0.0f};
    bool valid{false};
};

inline float Dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline float Len3(const float* a) {
    return std::sqrt(Dot3(a, a));
}

inline Camera CameraFrom(const float* m, float width, float height) {
    Camera c{};
    if (!m)
        return c;
    if (!(width >= 64.0f && width <= 16384.0f && height >= 64.0f && height <= 16384.0f))
        return c;
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]))
            return c;
    const float ax = Len3(m + 0);
    const float ay = Len3(m + 4);
    const float az = Len3(m + 12);
    if (!(ax > 1e-6f) || !(ay > 1e-6f) || !(az > 1e-6f))
        return c;
    for (int i = 0; i < 3; ++i) {
        c.right[i] = m[i] / ax;
        c.up[i] = m[4 + i] / ay;
        c.fwd[i] = m[12 + i] / az;
    }

    constexpr float k_ortho_tol = 5e-3f;
    if (std::abs(Dot3(c.right, c.up)) > k_ortho_tol ||
        std::abs(Dot3(c.right, c.fwd)) > k_ortho_tol ||
        std::abs(Dot3(c.up, c.fwd)) > k_ortho_tol)
        return c;
    for (int i = 0; i < 3; ++i)
        c.pos[i] = -m[3] / ax * c.right[i] - m[7] / ay * c.up[i] - m[15] / az * c.fwd[i];

    const float zrow[3]{m[8], m[9], m[10]};
    const float alpha = Len3(zrow);
    if (!(alpha > 1e-9f))
        return c;
    const float align = Dot3(zrow, c.fwd) / alpha;
    if (std::abs(align) < 0.99f)
        return c;
    const float sign = align < 0.0f ? -1.0f : 1.0f;
    const float a_z = alpha * sign / az;
    const float b_z = alpha * sign * Dot3(c.pos, c.fwd) + m[11];
    if (!std::isfinite(a_z) || !std::isfinite(b_z) || std::abs(b_z) < 1e-6f)
        return c;
    c.azz = a_z;
    c.bz = b_z;
    const float w = Dot3(c.pos, m + 12) + m[15];
    const float px = Dot3(c.pos, m + 0) + m[3];
    const float py = Dot3(c.pos, m + 4) + m[7];
    const float tol = 2e-3f * (1.0f + std::abs(m[3]) + std::abs(m[7]) + std::abs(m[15]));
    if (!(std::abs(w) <= tol) || !(std::abs(px) <= tol) || !(std::abs(py) <= tol))
        return c;
    c.ax = ax;
    c.ay = ay;
    c.az = az;
    c.width = width;
    c.height = height;
    c.valid = true;
    return c;
}

inline bool BodyFrame(const float* cframe, float org[4], float x[4], float y[4], float z[4]) {
    if (!cframe)
        return false;
    org[0] = cframe[9];
    org[1] = cframe[10];
    org[2] = cframe[11];
    org[3] = 0.0f;
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(org[i]))
            return false;
    }
    const float rx[3]{cframe[0], cframe[3], cframe[6]};
    const float uy[3]{cframe[1], cframe[4], cframe[7]};
    const float fz[3]{cframe[2], cframe[5], cframe[8]};
    const float nx = Len3(rx), ny = Len3(uy), nz = Len3(fz);

    const bool ok = nx > 0.7f && nx < 1.3f && ny > 0.7f && ny < 1.3f && nz > 0.7f && nz < 1.3f &&
        std::abs(Dot3(rx, uy)) / (nx * ny) < 0.1f &&
        std::abs(Dot3(rx, fz)) / (nx * nz) < 0.1f &&
        std::abs(Dot3(uy, fz)) / (ny * nz) < 0.1f;
    const float basis[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float* src[3]{ok ? rx : basis[0], ok ? uy : basis[1], ok ? fz : basis[2]};
    const float len[3]{ok ? nx : 1.0f, ok ? ny : 1.0f, ok ? nz : 1.0f};
    for (int i = 0; i < 3; ++i) {
        x[i] = src[0][i] / len[0];
        y[i] = src[1][i] / len[1];
        z[i] = src[2][i] / len[2];
    }
    x[3] = y[3] = z[3] = 0.0f;

    if (org[0] == 0.0f && org[1] == 0.0f && org[2] == 0.0f)
        return false;
    return true;
}

}
