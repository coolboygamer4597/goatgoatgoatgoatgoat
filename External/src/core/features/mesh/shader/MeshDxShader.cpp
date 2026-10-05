#include "MeshDxShader.h"
#include "core/features/mesh/cache/MeshCache.h"
#include "core/features/mesh/occlusion/MeshOcclusion.h"
#include "sdk/MeshBridge.h"
#include "core/globals/globals.h"
#include "core/variables/variables.h"
#include "core/cache/cache.h"
#include "core/features/native/NativeChams.h"
#include "core/features/native/NativePattern.h"

using namespace Mesh;

#include <d3dcompiler.h>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace Cheat {
namespace Visuals {
namespace MeshDxShader {
namespace {

constexpr char k_hlsl[] =
R"HLSL(
cbuffer Constants : register(b0)
{
    row_major float4x4 view;
    row_major float4x4 world;
    float3 camera;
    float  time;
    float4 base_color;
    float4 fresnel_color;
    float4 visible_color;
    float4 occluded_color;
    float4 occluded_fresnel;
    int    mode;
    float  fresnel_power;
    int    occlusion_enabled;
    int    occluded_mode;
    float4 outline_color;
    float  outline_fade;
    int    outline_style;
    int    outline_enabled;
    float  glow_strength;
    float  chams_opacity;
    float3 cb_padding;
};

Texture2D<float> world_depth : register(t0);
Texture2D<float> cham_depth  : register(t1);

struct VSIn
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD0;
};

struct PSIn
{
    float4 pos    : SV_POSITION;
    float3 wpos   : TEXCOORD0;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD1;
    float3 lpos   : TEXCOORD2;
};

PSIn vs_main(VSIn i)
{
    PSIn o;
    float4 wp = mul(world, float4(i.pos, 1.0));
    o.pos     = mul(view, wp);

    float dist = length(wp.xyz - camera);
    o.pos.z   = saturate(dist / max(cb_padding.x, 1.0)) * o.pos.w;

    o.wpos = wp.xyz;
    o.lpos = i.pos / max(cb_padding.y,0.01);

    o.normal = mul((float3x3)world, i.normal);
    o.uv = i.uv;
    return o;
}

float ps_depth(PSIn i) : SV_DEPTH
{

    return saturate(length(i.wpos - camera) / max(cb_padding.x, 1.0));
}

float4 ps_main(PSIn i) : SV_TARGET
{
    float3 V = normalize(camera - i.wpos);

    float3 dn = cross(ddy(i.wpos), ddx(i.wpos));
    float3 face_n = normalize(dn);
    if (dot(face_n, V) < 0.0)
        face_n = -face_n;
    float facing = saturate(dot(face_n, V));

    float3 N    = face_n;
    float  ndv  = facing;
    float  fres = pow(saturate(1.0 - ndv), max(0.1, fresnel_power));
    float3 L    = normalize(float3(0.45, 0.85, 0.30));
    float  ndl  = saturate(dot(N, L));
    float  lambert = ndl * 0.65 + 0.35;
    float3 H    = normalize(L + V);
    float  ndh  = saturate(dot(N, H));

    float3 env  = pow(saturate(N * 0.5 + 0.5), 1.4);

    float4 bc = base_color;
    float4 fc = fresnel_color;
    int    m  = mode;
    bool   pixel_occluded = false;

    if (occlusion_enabled != 0)
    {

        float cham_d = saturate(length(i.wpos - camera) / max(cb_padding.x, 1.0));
        int2 sp = int2(i.pos.xy);
        uint depth_w, depth_h;
        world_depth.GetDimensions(depth_w, depth_h);
        int2 sample_pos = clamp(
            sp,
            int2(0, 0),
            int2((int)depth_w - 1, (int)depth_h - 1));
        float wall_d = world_depth.Load(int3(sample_pos, 0));

        const float contact_tolerance = 0.001 / max(cb_padding.x,1.0);
        bool is_occluded = wall_d < 0.9999 &&
                           cham_d + contact_tolerance >= wall_d;
        if (is_occluded)
        {
            pixel_occluded = true;
            bc = occluded_color;
            fc = occluded_fresnel;
            m  = occluded_mode;
        }
        else
        {
            bc = visible_color;
            fc = fresnel_color;
            m  = mode;
        }
    }

    float4 result = bc;

    if (m == 0)
    {
        result.rgb = bc.rgb;
        result.a   = 1.0;
    }
    else if (m == 1)
    {
        float  spec   = pow(ndv, 14.0);
        result.rgb    = env * bc.rgb + spec.xxx * 0.6 + fc.rgb * fres;
        result.a      = 1.0;
    }
    else if (m == 2)
    {
        float3 rainbow = 0.5 + 0.5 * cos(time * 2.0 + i.lpos * 0.35 + float3(0.0, 2.0, 4.0));
        result.rgb     = rainbow * lambert;
        result.a       = 1.0;
    }
    else if (m == 3)
    {
        float3 iri = 0.5 + 0.5 * cos(ndv * 6.2831 + float3(0.0, 2.1, 4.2));
        float spec = pow(ndh, 48.0) * 0.7;
        result.rgb = bc.rgb * lambert + iri * fres * 0.85 + fc.rgb * fres * 0.35 + spec.xxx;
        result.a   = 1.0;
    }
    else if (m == 4)
    {
        float wrap = saturate(ndl * 0.55 + 0.45);
        float spec = pow(ndh, 128.0) * 1.1;
        float rim  = pow(saturate(1.0 - ndv), 2.5) * 0.4;
        result.rgb = bc.rgb * wrap + spec.xxx + fc.rgb * rim + env * fc.rgb * 0.12;
        result.a   = 1.0;
    }
    else if (m == 5)
    {
        float3 holo = 0.5 + 0.5 * cos(time * 3.0 + i.lpos.y * 2.0 + ndv * 8.0 + float3(0.0, 2.0, 4.0));
        float spec = pow(ndh, 80.0) * 0.9;
        result.rgb = bc.rgb * lambert * 0.55 + holo * (fres * 0.9 + 0.15) + fc.rgb * fres * 0.25 + spec.xxx;
        result.a   = 1.0;
    }
    else if (m == 6)
    {
        float t = time * 0.45;

        float3 p = i.lpos * 0.07;
        float n1 = sin(p.x * 1.3 + p.y * 0.9 + t);
        float n2 = sin(p.y * 1.1 - p.z * 1.0 + t * 0.85 + 2.1);
        float n3 = sin(p.z * 1.2 + p.x * 0.8 - t * 0.7 + 4.2);

        float w = sin(n1 * 1.4 + n2) * 0.55;
        float u = saturate(0.5 + 0.5 * (n1 + w));
        float v = saturate(0.5 + 0.5 * (n2 - w * 0.6));
        float s = saturate(0.5 + 0.5 * (n3 + n1 * 0.35));

        float3 cA = float3(0.95, 0.35, 0.70);
        float3 cB = float3(0.45, 0.55, 1.00);
        float3 cC = float3(0.40, 0.95, 0.85);
        float3 cD = float3(1.00, 0.70, 0.35);
        float3 cE = float3(0.70, 0.40, 0.95);

        float3 col = lerp(cA, cB, u);
        col = lerp(col, cC, v * 0.85);
        col = lerp(col, cD, s * 0.70);
        col = lerp(col, cE, (1.0 - u) * v * 0.55);
        col += fres * 0.08;

        result.rgb = saturate(col);
        result.a   = 1.0;
    }
    else if (m == 7)
    {
        result.rgb = bc.rgb;
        result.a   = 1.0;
    }
    else if (m == 8)
    {
        float f = saturate(fres * 1.45 + 0.08);
        float3 tint = lerp(bc.rgb * 0.42, float3(0.85, 0.92, 1.0), 0.35);
        result.rgb = lerp(tint, saturate(bc.rgb + 0.35), f) + pow(ndh, 72.0) * 0.45;
        result.a   = saturate(0.22 + f * 0.62) * max(bc.a, 0.35);
    }
    else if (m == 9)
    {
        float2 wp = float2(i.lpos.x + i.lpos.y, i.lpos.y + i.lpos.z) * 2.8
                  + float2(time * 1.35, time * 0.95);

        wp += i.uv * 6.0;
        float2 r = float2(wp.x + wp.y, wp.x - wp.y) * 0.7071;
        float2 cell = abs(frac(r) - 0.5);
        float d = min(cell.x, cell.y);
        float rope_w = 1.0 - smoothstep(0.0, 0.07, d);
        float core = 1.0 - smoothstep(0.0, 0.028, d);
        float3 copper = float3(1.00, 0.42, 0.10);
        float3 hi     = float3(1.00, 0.92, 0.55);
        float3 dark   = float3(0.06, 0.03, 0.02);
        float3 rope = lerp(copper, hi, core);
        result.rgb = lerp(dark, rope, rope_w) * (0.85 + fres * 0.35);
        result.a   = saturate(0.25 + rope_w * 0.85);
    }
    else if (m == 10)
    {
        float3 p = i.lpos * 0.9;
        float t = time;

        float wA = sin(p.x * 3.4 + p.y * 2.1 - t * 2.4);
        float wB = cos(p.y * 3.8 + p.z * 2.6 + t * 1.9);
        float wC = sin(dot(p, float3(2.2, 1.6, 2.8)) + t * 2.8);
        float flow = wA + wB * 0.85 + wC * 0.55;
        float ripple = 0.5 + 0.5 * sin(flow * 3.0 + ndv * 8.0 + t * 1.5);
        float streak = pow(saturate(0.5 + 0.5 * sin(p.y * 14.0 - t * 6.0 + flow * 2.0)), 6.0);

        float3 dark = float3(0.16, 0.18, 0.20);
        float3 mid  = float3(0.48, 0.50, 0.53);
        float3 hi   = float3(0.92, 0.94, 0.97);
        float3 iri  = 0.5 + 0.5 * cos(flow * 2.4 + t * 1.2 + ndv * 10.0 + float3(0.0, 2.1, 4.2));

        float3 metal = lerp(dark, mid, saturate(0.45 + lambert * 0.4 + flow * 0.08));
        metal = lerp(metal, hi, pow(ndh, 36.0) * 0.95 + pow(saturate(ndv), 8.0) * 0.4);
        metal += streak * hi * 0.55;
        metal += iri * fres * (0.18 + ripple * 0.28);

        float3 envW = pow(saturate(N * 0.5 + 0.5 + float3(flow, -flow, ripple) * 0.08), 1.2);
        metal += envW * 0.22 * (0.5 + ripple * 0.5);
        result.rgb = saturate(metal);
        result.a   = 1.0;
    })HLSL"
R"HLSL(
    else if (m == 11)
    {
        float f = saturate(fres * 1.2 + 0.05);
        float3 tint = lerp(bc.rgb * 0.55, float3(0.78, 0.88, 1.0), 0.4);
        result.rgb = lerp(tint, saturate(bc.rgb + 0.25), f * 0.85) + pow(ndh, 90.0) * 0.35;
        result.a   = saturate(0.12 + f * 0.48) * max(bc.a, 0.25);
    }
    else if (m == 12)
    {
        float f = saturate(fres * 1.6 + 0.1);
        float3 ice = lerp(float3(0.55, 0.75, 0.95), float3(0.9, 0.97, 1.0), f);
        ice = lerp(ice, bc.rgb, 0.35);
        result.rgb = ice + pow(ndh, 64.0) * 0.55;
        result.a   = saturate(0.2 + f * 0.55) * max(bc.a, 0.3);
    }
    else if (m == 13)
    {
        float pulse = 0.55 + 0.45 * sin(time * 2.4);
        float rim = pow(saturate(1.0 - ndv), 1.8);
        result.rgb = bc.rgb * (0.4 + rim * 0.6) + fc.rgb * fres * 0.3;
        result.a   = saturate((0.14 + rim * 0.5) * pulse) * max(bc.a, 0.28);
    }
    else if (m == 14)
    {
        float3 p = i.lpos * 0.12;
        float t = time * 0.7;
        float3 band = 0.5 + 0.5 * cos(t + p.y * 3.0 + p.x * 1.2 + float3(0.0, 2.1, 4.2));
        float veil = 0.35 + 0.45 * sin(p.y * 2.0 + t * 1.3);
        result.rgb = lerp(bc.rgb * 0.5, band, 0.65) * (0.55 + fres * 0.55);
        result.a   = saturate(0.16 + veil * 0.4 + fres * 0.25) * max(bc.a, 0.25);
    }
    else if (m == 15)
    {
        float2 q = i.uv * 8.0 + float2(time * 0.6, time * 0.35);
        float2 cell = frac(q) - 0.5;
        float d = length(cell);
        float bubble = 1.0 - smoothstep(0.18, 0.42, d);
        float ring = smoothstep(0.28, 0.34, d) * (1.0 - smoothstep(0.34, 0.42, d));
        float3 iri = 0.5 + 0.5 * cos(d * 18.0 + time * 2.0 + float3(0.0, 2.0, 4.0));
        result.rgb = lerp(bc.rgb * 0.45, iri, saturate(bubble * 0.7 + ring)) + fres * 0.2;
        result.a   = saturate(0.12 + bubble * 0.35 + ring * 0.55 + fres * 0.2) * max(bc.a, 0.25);
    }
    else if (m == 16)
    {
        float3 p = i.lpos * 1.4;
        float wob = sin(p.x * 4.0 + time * 3.2) * cos(p.y * 3.5 - time * 2.6);
        float blob = 0.5 + 0.5 * wob;
        float3 gel = lerp(bc.rgb * 0.55, bc.rgb + float3(0.15, 0.25, 0.2), blob);
        gel += pow(ndh, 40.0) * 0.4;
        result.rgb = saturate(gel);
        result.a   = saturate(0.22 + blob * 0.28 + fres * 0.35) * max(bc.a, 0.3);
    }
    else if (m == 17)
    {
        float3 p = i.lpos * 1.1;
        float t = time;
        float wA = sin(p.x * 3.2 + p.y * 2.0 - t * 2.2);
        float wB = cos(p.y * 3.6 + p.z * 2.4 + t * 1.7);
        float flow = wA + wB * 0.8;
        float ripple = 0.5 + 0.5 * sin(flow * 2.8 + ndv * 7.0 + t * 1.4);
        float streak = pow(saturate(0.5 + 0.5 * sin(p.y * 12.0 - t * 5.5 + flow)), 5.5);
        float3 dark = float3(0.22, 0.24, 0.28);
        float3 mid  = float3(0.55, 0.58, 0.62);
        float3 hi   = float3(0.95, 0.97, 1.0);
        float3 iri  = 0.5 + 0.5 * cos(flow * 2.0 + t + ndv * 9.0 + float3(0.0, 2.1, 4.2));
        float3 merc = lerp(dark, mid, saturate(0.4 + lambert * 0.35 + flow * 0.07));
        merc = lerp(merc, hi, pow(ndh, 42.0) * 0.9 + streak * 0.5);
        merc = lerp(merc, bc.rgb, 0.28);
        merc += iri * fres * (0.22 + ripple * 0.25);
        result.rgb = saturate(merc);
        result.a   = saturate(0.2 + fres * 0.45 + ripple * 0.12) * max(bc.a, 0.3);
    }
    else if (m == 18)
    {
        float3 p = i.lpos * 0.55;
        float t = time * 1.15;
        float c1 = sin(p.x * 4.0 + p.z * 3.0 + t * 2.0);
        float c2 = cos(p.y * 3.5 - p.x * 2.5 + t * 1.6);
        float cau = saturate(0.55 + 0.45 * (c1 * c2));
        float spark = pow(saturate(cau), 8.0);
        float3 water = lerp(float3(0.15, 0.45, 0.75), float3(0.55, 0.85, 1.0), cau);
        water = lerp(water, bc.rgb, 0.4);
        water += spark * float3(0.85, 0.95, 1.0) * 0.55;
        water += pow(ndh, 70.0) * 0.4;
        result.rgb = saturate(water);
        result.a   = saturate(0.16 + fres * 0.5 + cau * 0.15) * max(bc.a, 0.28);
    }
    else if (m == 19)
    {
        float3 p = i.lpos * 0.2;
        float t = time * 0.55;
        float wave = sin(p.x * 2.0 + t) * cos(p.z * 1.7 - t * 0.8);
        float depth = saturate(0.35 + wave * 0.2 + fres * 0.45);
        float3 deep = float3(0.02, 0.12, 0.28);
        float3 shallow = float3(0.12, 0.45, 0.7);
        float3 col = lerp(deep, shallow, depth);
        col = lerp(col, bc.rgb * 0.7, 0.35);
        col += pow(ndh, 90.0) * 0.35;
        result.rgb = saturate(col);
        result.a   = saturate(0.22 + depth * 0.35 + fres * 0.25) * max(bc.a, 0.32);
    }
    else if (m == 20)
    {
        float3 p = i.lpos * 1.6;
        float t = time * 1.8;
        float drip = sin(p.y * 6.0 - t * 4.0 + sin(p.x * 3.0 + t) * 1.5);
        float blob = saturate(0.5 + 0.5 * drip);
        float bead = pow(saturate(abs(drip)), 3.0);
        float3 silv = lerp(float3(0.35, 0.38, 0.42), float3(0.92, 0.94, 0.98), blob);
        silv = lerp(silv, bc.rgb, 0.25);
        silv += bead * 0.65 + pow(ndh, 28.0) * 0.85;
        silv += fres * float3(0.7, 0.85, 1.0) * 0.35;
        result.rgb = saturate(silv);
        result.a   = saturate(0.18 + blob * 0.35 + fres * 0.4) * max(bc.a, 0.28);
    }
    else if (m == 21)
    {
        float2 q = float2(i.lpos.x, i.lpos.z) * 0.9;
        float t = time * 2.2;
        float d0 = length(q + float2(sin(t * 0.3), cos(t * 0.25)) * 0.4);
        float d1 = length(q - float2(0.7, -0.3));
        float rings = sin(d0 * 14.0 - t * 5.0) * 0.5 + sin(d1 * 11.0 - t * 3.8) * 0.35;
        rings = saturate(0.5 + rings * 0.5);
        float crest = pow(rings, 4.0);
        float3 col = lerp(bc.rgb * 0.45, float3(0.4, 0.75, 1.0), rings);
        col += crest * 0.55 + fres * 0.25;
        result.rgb = saturate(col);
        result.a   = saturate(0.14 + rings * 0.35 + fres * 0.4) * max(bc.a, 0.26);
    }
    else if (m == 22)
    {
        float3 p = i.lpos * 0.85;
        float t = time * 0.9;
        float flow = sin(p.x * 2.5 + t) + cos(p.y * 3.0 - t * 1.2);
        float3 film = 0.5 + 0.5 * cos(flow * 3.5 + ndv * 12.0 + t * 1.5 + float3(0.0, 2.2, 4.4));
        float wet = saturate(0.4 + fres * 0.7);
        float3 col = lerp(bc.rgb * 0.4, film, 0.75);
        col += pow(ndh, 55.0) * 0.45;
        result.rgb = saturate(col);
        result.a   = saturate(0.15 + wet * 0.45 + abs(flow) * 0.05) * max(bc.a, 0.28);
    }
    else if (m == 23)
    {
        float3 p = i.wpos * 0.6;
        float t = time * 1.4;
        float w = sin(p.x * 2.0 + t) + sin(p.y * 2.3 - t * 1.1)
                + sin(p.z * 1.7 + t * 0.9) + sin((p.x + p.y + p.z) * 1.3 + t * 1.3);
        float s = 0.5 + 0.5 * sin(w * 1.5);
        float3 col = lerp(float3(0.20, 0.05, 0.55), float3(0.95, 0.20, 0.75), s);
        col = lerp(col, float3(0.20, 0.85, 0.95), pow(saturate(s * s), 2.0) * 0.7);
        result.rgb = saturate(col * (0.75 + 0.45 * lambert) + fc.rgb * fres * 0.30);
        result.a = 1.0;
    }
    else if (m == 24)
    {
        float spec = pow(ndh, 90.0) * 1.2;
        float flake = pow(saturate(dot(N, normalize(float3(0.3, 0.8, 0.5)))), 24.0);
        float3 gold = lerp(float3(0.45, 0.28, 0.05), float3(1.00, 0.85, 0.35), 0.35 + 0.65 * lambert);
        result.rgb = saturate(gold + flake * 0.35 + spec.xxx * float3(1.0, 0.9, 0.6) + fc.rgb * fres * 0.25);
        result.a = 1.0;
    }
    else if (m == 25)
    {
        float3 p = i.wpos * 2.2;
        float t = time * 1.8;
        float n = sin(p.x * 2.5 + t) * sin(p.y * 3.1 - t * 1.2) * sin(p.z * 2.7 + t * 0.8);
        float bubble = smoothstep(0.55, 0.95, 0.5 + 0.5 * n);
        float3 toxic = lerp(float3(0.10, 0.55, 0.05), float3(0.55, 1.00, 0.10), 0.4 + 0.6 * bubble);
        result.rgb = saturate(toxic * (0.6 + 0.5 * lambert) + fc.rgb * fres * 0.35 + bubble * 0.25);
        result.a = 1.0;
    }
    else if (m == 26)
    {
        float3 p = i.wpos * 1.5;
        float t = time * 0.35;
        float neb = 0.5 + 0.5 * sin(p.x * 1.8 + t) * cos(p.y * 2.2 - t * 1.3) * sin(p.z * 1.6 + t * 0.7);
        float3 col = lerp(float3(0.03, 0.02, 0.10), float3(0.35, 0.10, 0.60), neb);
        col = lerp(col, float3(0.10, 0.30, 0.75), pow(saturate(1.0 - neb), 2.0) * 0.6);
        float3 g = floor(i.wpos * 24.0);
        float star = step(0.985, frac(sin(dot(g, float3(12.9898, 78.233, 37.719))) * 43758.5453));
        col += star * (0.5 + 0.5 * sin(time * 6.0 + g.x * 3.7 + g.y * 5.1 + g.z * 2.3)) * float3(0.9, 0.95, 1.0);
        result.rgb = saturate(col + fc.rgb * fres * 0.35);
        result.a = 1.0;
    }
    else if (m == 27)
    {
        float2 mc = float2(i.wpos.x + i.wpos.z, i.wpos.y) * 3.0;
        mc.y += time * 3.5;
        float2 cellid = floor(mc);
        float rnd = frac(sin(dot(cellid, float2(12.9898, 78.233))) * 43758.5453);
        float bright = pow(1.0 - frac(mc.y + rnd * 7.0), 3.0);
        float flicker = 0.8 + 0.2 * sin(time * 20.0 + cellid.x * 9.0);
        float3 col = lerp(float3(0.0, 0.18, 0.04), float3(0.35, 1.0, 0.45), bright) * flicker;
        result.rgb = saturate(col * (0.7 + 0.5 * lambert) + fc.rgb * fres * 0.2);
        result.a = 1.0;
    }
    else if (m == 28)
    {
        float3 p = i.lpos;
        float t = time;
        float bend = sin(p.y * 2.7 - t * 2.2) * 0.28
                   + sin(p.z * 3.3 + t * 1.4) * 0.11;
        float vertical_phase = (p.x + bend) * 4.8 - t * 1.35;
        float vertical_line = 1.0 - smoothstep(0.025, 0.095,
            abs(frac(vertical_phase) - 0.5));
        float rising_phase = frac(p.y * 0.42 - t * 0.62);
        float rising_glow = exp(-pow((rising_phase - 0.52) * 5.5, 2.0));
        float crossing = saturate(vertical_line * (0.35 + rising_glow * 1.15));
        float rim_light = pow(saturate(1.0 - ndv), 2.2);
        float3 deep_blue = float3(0.015, 0.055, 0.16);
        float3 electric_blue = float3(0.08, 0.48, 1.00);
        float3 crest = float3(0.42, 0.86, 1.00);
        result.rgb = lerp(deep_blue, electric_blue, crossing);
        result.rgb += crest * (rising_glow * vertical_line * 0.72 + rim_light * 0.22);
        result.a = saturate(0.12 + vertical_line * 0.42 +
                            rising_glow * vertical_line * 0.32 + rim_light * 0.10);
    }
    else if (m == 29)
    {
        float3 p = i.lpos;
        float n1 = sin(p.y * 4.1 + sin(p.x * 2.3) - time * 0.82);
        float n2 = sin(p.y * 7.3 - p.z * 2.1 + time * 0.58 + n1);
        float band = pow(saturate(0.5 + 0.5 * n2), 5.0);
        float3 col = lerp(float3(0.008, 0.015, 0.075),
                          float3(0.08, 0.38, 0.95), band);
        col += float3(0.25, 0.05, 0.55) * pow(saturate(0.5 + 0.5 * n1), 8.0);
        result.rgb = saturate(col + fc.rgb * fres * 0.22);
        result.a = saturate(0.18 + band * 0.48 + fres * 0.12);
    }
    else if (m == 30)
    {
        float3 p = i.lpos;
        float2 cell = floor(float2(p.x + p.z, p.y) * 8.0);
        float rnd = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
        float rise = frac(p.y * 0.48 - time * (0.24 + rnd * 0.23) + rnd);
        float spark = pow(saturate(1.0 - abs(rise - 0.5) * 8.0), 5.0) * step(0.72, rnd);
        float smoke = 0.5 + 0.5 * sin(p.x * 3.0 + p.z * 2.2 + time * 0.18);
        result.rgb = saturate(lerp(float3(0.05, 0.008, 0.002),
            float3(0.75, 0.11, 0.01), smoke * 0.55) +
            spark * float3(1.0, 0.62, 0.10) + fc.rgb * fres * 0.12);
        result.a = saturate(0.16 + smoke * 0.18 + spark * 0.58 + fres * 0.08);
    }
    else if (m == 31)
    {
        float3 p = i.lpos * 1.8;
        float a = sin(p.x * 1.9 + time * 0.38 + sin(p.y * 2.7));
        float b = cos(p.y * 2.2 - time * 0.31 + sin(p.z * 3.1 + a));
        float cloud = saturate(0.48 + 0.28 * a + 0.30 * b);
        float3 col = lerp(float3(0.018, 0.008, 0.07),
                          float3(0.48, 0.06, 0.58), cloud);
        col = lerp(col, float3(0.04, 0.62, 0.85), pow(1.0 - cloud, 3.0) * 0.55);
        float3 sid = floor(p * 18.0);
        float star = step(0.992, frac(sin(dot(sid, float3(17.13, 43.71, 91.07))) * 46321.19));
        col += star * (0.55 + 0.45 * sin(time * 1.1 + sid.y)) * 0.8;
        result.rgb = saturate(col + fc.rgb * fres * 0.2);
        result.a = saturate(0.20 + cloud * 0.23 + star * 0.45 + fres * 0.1);
    }
    else if (m == 32)
    {
        float3 p = i.lpos;
        float warp = sin(p.x * 2.4 + p.z * 1.7 + time * 0.44) * 0.34;
        float flow = 0.5 + 0.5 * sin((p.y + warp) * 7.0 - time * 0.76);
        float vein = pow(saturate(flow), 7.0);
        float crystal = pow(saturate(dot(N, normalize(float3(-0.25, 0.85, 0.45)))), 10.0);
        float3 col = lerp(float3(0.015, 0.12, 0.22),
                          float3(0.28, 0.88, 1.0), vein);
        col += float3(0.72, 0.93, 1.0) * crystal * 0.35;
        result.rgb = saturate(col + fc.rgb * fres * 0.3);
        result.a = saturate(0.15 + vein * 0.42 + crystal * 0.18 + fres * 0.13);
    }
    else if (m == 33)
    {
        float3 p = i.lpos;
        float branch = sin(p.y * 4.0 + sin(p.x * 5.1 + time * 0.56) * 1.5 +
                           sin(p.z * 6.3 - time * 0.43));
        float bolt = pow(saturate(1.0 - abs(branch) * 5.5), 3.0);
        float pulse = 0.45 + 0.55 * pow(saturate(sin(time * 1.20 + p.y * 1.2)), 4.0);
        float haze = 0.5 + 0.5 * sin(p.x * 2.1 - p.z * 2.7 + time * 0.30);
        float3 col = float3(0.006, 0.004, 0.025) +
                     float3(0.22, 0.08, 0.72) * haze * 0.28 +
                     float3(0.45, 0.72, 1.0) * bolt * pulse;
        result.rgb = saturate(col + fc.rgb * fres * 0.24);
        result.a = saturate(0.13 + haze * 0.12 + bolt * pulse * 0.64 + fres * 0.12);
    }
)HLSL"
R"HLSL(
    else if (m == 34)
    {
        float3 p = i.lpos;
        float line1 = abs(sin((p.y + sin(p.x * 4.2 + time * 0.75) * 0.32) * 8.0));
        float line2 = abs(sin((p.y + sin(p.z * 5.3 - time * 0.58) * 0.25 + 0.37) * 10.0));
        float ink = pow(1.0 - min(line1, line2), 8.0);
        float hue = frac(p.x * 0.18 + p.z * 0.13 + time * 0.11);
        float3 rainbow = saturate(abs(frac(hue + float3(0.0, 0.67, 0.33)) * 6.0 - 3.0) - 1.0);
        result.rgb = saturate(float3(0.025, 0.015, 0.08) + rainbow * ink + fc.rgb * fres * 0.18);
        result.a = saturate(0.14 + ink * 0.67 + fres * 0.10);
    }
    else if (m == 35)
    {
        float3 p = i.lpos;
        float trunk = abs(p.x + sin(p.y * 5.0 + time * 0.85) * 0.13 +
            sin(p.y * 13.0 - time * 0.52) * 0.045);
        float fork1 = abs(p.x - 0.22 - sin(p.y * 7.0 - time * 0.63) * 0.11);
        float fork2 = abs(p.z + 0.18 + sin(p.y * 8.5 + time * 0.71) * 0.10);
        float bolt = exp(-trunk * 42.0) + exp(-fork1 * 55.0) * step(0.1, p.y) +
                     exp(-fork2 * 55.0) * step(-0.15, p.y);
        float flash = 0.72 + 0.28 * sin(time * 5.2 + p.y * 11.0);
        result.rgb = saturate(float3(0.008, 0.012, 0.055) +
            float3(0.36, 0.70, 1.0) * bolt * flash + fc.rgb * fres * 0.20);
        result.a = saturate(0.12 + bolt * 0.75 + fres * 0.12);
    }
    else if (m == 36)
    {
        float3 p = i.lpos * 2.0;
        float lava = sin(p.x * 2.4 + time * 0.52) + cos(p.y * 3.1 - time * 0.37) +
                     sin(p.z * 2.8 + p.y + time * 0.43);
        float hot = smoothstep(0.35, 1.75, lava);
        float crust = smoothstep(-1.4, 0.25, lava);
        float3 col = lerp(float3(0.025, 0.006, 0.002), float3(0.62, 0.04, 0.005), crust);
        col = lerp(col, float3(1.0, 0.68, 0.08), hot);
        result.rgb = saturate(col + fc.rgb * fres * 0.13);
        result.a = saturate(0.30 + hot * 0.52 + fres * 0.08);
    }
    else if (m == 37)
    {
        float3 p = i.lpos * 7.0;
        float2 q = frac(float2(p.x + p.z, p.y) + float2(time * 0.18, -time * 0.30));
        float grid = max(1.0 - smoothstep(0.025, 0.10, abs(q.x - 0.5)),
                         1.0 - smoothstep(0.025, 0.10, abs(q.y - 0.5)));
        float2 cell = floor(float2(p.x + p.z, p.y));
        float gate = step(0.38, frac(sin(dot(cell, float2(19.17, 73.41))) * 45131.7));
        float current = grid * gate * (0.55 + 0.45 * sin(time * 2.4 + cell.x));
        result.rgb = saturate(float3(0.002, 0.035, 0.055) +
            float3(0.0, 0.82, 1.0) * current + fc.rgb * fres * 0.18);
        result.a = saturate(0.14 + current * 0.62 + fres * 0.10);
    }
    else if (m == 38)
    {
        float3 p = i.lpos;
        float ribbon = 0.5 + 0.5 * sin(p.y * 9.0 + sin(p.x * 3.0 + time * 0.62) * 2.2 - time * 0.88);
        float hue = frac(p.y * 0.24 + p.x * 0.13 + time * 0.09);
        float3 prism = saturate(abs(frac(hue + float3(0.0, 0.67, 0.33)) * 6.0 - 3.0) - 1.0);
        result.rgb = saturate(lerp(float3(0.025, 0.02, 0.06), prism, pow(ribbon, 3.0)) +
            fc.rgb * fres * 0.28);
        result.a = saturate(0.16 + pow(ribbon, 3.0) * 0.52 + fres * 0.13);
    }
    else if (m == 39)
    {
        float3 p = i.lpos * 2.1;
        float smoke = sin(p.x * 1.7 + time * 0.33 + sin(p.y * 2.2)) *
                      cos(p.z * 2.4 - time * 0.28 + sin(p.x * 1.3));
        smoke = smoothstep(-0.35, 0.82, smoke);
        float wisp = pow(saturate(0.5 + 0.5 * sin(p.y * 5.0 - time * 0.74 + smoke * 2.0)), 5.0);
        result.rgb = saturate(lerp(float3(0.01, 0.025, 0.035),
            float3(0.42, 0.78, 0.76), smoke * 0.65 + wisp * 0.25) + fc.rgb * fres * 0.16);
        result.a = saturate(0.10 + smoke * 0.30 + wisp * 0.22 + fres * 0.11);
    }
    else if (m == 40)
    {
        float3 p = i.lpos;
        float radius = length(float2(p.x, p.y));
        float wave = abs(frac(radius * 4.5 - time * 0.62) - 0.5);
        float ring = 1.0 - smoothstep(0.035, 0.13, wave);
        float sweep = 0.5 + 0.5 * sin(atan2(p.y, p.x) * 3.0 - time * 1.1);
        result.rgb = saturate(float3(0.005, 0.045, 0.028) +
            float3(0.05, 1.0, 0.42) * ring * (0.55 + sweep * 0.45) + fc.rgb * fres * 0.15);
        result.a = saturate(0.13 + ring * 0.61 + fres * 0.10);
    }
    else if (m == 41)
    {
        float3 p = i.lpos;
        float petals = 0.5 + 0.5 * sin(atan2(p.z, p.x) * 6.0 + length(p.xz) * 12.0 - time * 0.72);
        float bloom = pow(petals, 4.0) * (0.55 + 0.45 * sin(p.y * 5.0 + time * 0.66));
        float3 col = lerp(float3(0.09, 0.006, 0.035), float3(1.0, 0.18, 0.52), bloom);
        result.rgb = saturate(col + float3(1.0, 0.55, 0.72) * fres * 0.28);
        result.a = saturate(0.20 + bloom * 0.47 + fres * 0.12);
    }
    else if (m == 42)
    {
        float3 p = i.lpos;
        float row = floor((p.y + time * 0.17) * 18.0);
        float rnd = frac(sin(row * 91.73) * 43758.54);
        float shift = step(0.78, rnd) * sin(time * 7.0 + row) * 0.35;
        float scan = step(0.78, frac((p.y - time * 0.55) * 23.0));
        float block = step(0.62, frac(sin(dot(floor(float2(p.x + shift, p.z) * 8.0),
            float2(12.99, 78.23))) * 43758.5));
        result.rgb = saturate(float3(0.015, 0.02, 0.04) +
            float3(0.05, 0.85, 1.0) * scan + float3(0.92, 0.03, 0.48) * block * scan +
            fc.rgb * fres * 0.14);
        result.a = saturate(0.17 + scan * 0.47 + block * scan * 0.20 + fres * 0.09);
    }
    else if (m == 43)
    {
        float3 p = i.lpos * 3.2;
        float c1 = sin(p.x * 2.1 + time * 0.55 + sin(p.z * 2.7));
        float c2 = cos(p.z * 2.4 - time * 0.46 + sin(p.y * 2.0));
        float caustic = pow(saturate(1.0 - abs(c1 + c2) * 0.48), 7.0);
        float depth = 0.5 + 0.5 * sin(p.y * 0.8 - time * 0.24);
        result.rgb = saturate(lerp(float3(0.0, 0.07, 0.18), float3(0.0, 0.38, 0.67), depth) +
            float3(0.34, 0.92, 1.0) * caustic + fc.rgb * fres * 0.20);
        result.a = saturate(0.18 + caustic * 0.52 + depth * 0.12 + fres * 0.10);
    }
    else if (m == 44)
    {
        float3 p = i.lpos * 5.0;
        float cells = sin(p.x + sin(p.y * 1.7)) * cos(p.z * 1.3 - sin(p.x));
        float crack = pow(saturate(1.0 - abs(cells) * 7.0), 2.0);
        float flow = 0.68 + 0.32 * sin(time * 1.35 + p.y * 2.0);
        result.rgb = saturate(float3(0.018, 0.006, 0.003) +
            float3(1.0, 0.24, 0.015) * crack * flow +
            float3(1.0, 0.75, 0.12) * crack * crack + fc.rgb * fres * 0.10);
        result.a = saturate(0.24 + crack * 0.62 + fres * 0.07);
    }
    else if (m == 45)
    {
        float3 p = i.lpos;
        float curtain = sin(p.y * 4.5 + sin(p.x * 3.2 + time * 0.46) * 1.8 - time * 0.51);
        float edge = pow(saturate(0.5 + 0.5 * curtain), 4.0);
        float blend = 0.5 + 0.5 * sin(p.z * 2.0 - time * 0.31);
        float3 col = lerp(float3(0.02, 0.72, 0.42), float3(0.34, 0.16, 0.95), blend);
        result.rgb = saturate(float3(0.006, 0.018, 0.035) + col * edge + fc.rgb * fres * 0.24);
        result.a = saturate(0.12 + edge * 0.54 + fres * 0.12);
    }
    else if (m == 46)
    {
        float3 p = i.lpos * 2.4;
        float curl = sin(p.x * 2.8 + sin(p.y * 4.1 - time * 0.53)) +
                     cos(p.z * 3.3 - sin(p.x * 2.0 + time * 0.41));
        float ink = smoothstep(-0.35, 0.48, curl);
        float edgeInk = pow(saturate(1.0 - abs(curl) * 1.7), 5.0);
        result.rgb = saturate(lerp(float3(0.01, 0.01, 0.012), float3(0.72, 0.74, 0.78), ink) +
            edgeInk * 0.35 + fc.rgb * fres * 0.13);
        result.a = saturate(0.18 + ink * 0.26 + edgeInk * 0.38 + fres * 0.09);
    }
    else if (m == 47)
    {
        float3 p = i.lpos;
        float2 uv = float2(p.x + p.z * 0.37, p.y) * 8.0;
        float2 id = floor(uv);
        float rnd = frac(sin(dot(id, float2(27.61, 57.17))) * 46321.7);
        float trail = frac(uv.y - time * (0.72 + rnd * 0.55) + rnd * 5.0);
        float xdist = abs(frac(uv.x + rnd) - 0.5);
        float comet = exp(-xdist * 35.0) * pow(saturate(1.0 - trail), 5.0) * step(0.58, rnd);
        result.rgb = saturate(float3(0.005, 0.012, 0.055) +
            float3(0.30, 0.65, 1.0) * comet + float3(0.82, 0.93, 1.0) * comet * comet +
            fc.rgb * fres * 0.18);
        result.a = saturate(0.12 + comet * 0.73 + fres * 0.10);
    }
    else if (m == 48)
    {
        float3 p = i.lpos * 2.0;
        float cloud = sin(p.x * 1.8 + time * 0.37) * cos(p.y * 2.3 - time * 0.29) +
                      sin(p.z * 2.7 + p.x - time * 0.33);
        float path = abs(sin(p.y * 4.7 + sin(p.x * 5.4 + time * 0.69) * 1.3 +
                             sin(p.z * 7.1 - time * 0.47)));
        float lightning = pow(saturate(1.0 - path * 7.5), 3.0);
        float flicker = 0.55 + 0.45 * pow(saturate(sin(time * 2.7 + p.x * 3.0)), 6.0);
        float3 col = lerp(float3(0.012, 0.014, 0.055), float3(0.16, 0.08, 0.34),
                          saturate(cloud * 0.35 + 0.5));
        col += float3(0.52, 0.72, 1.0) * lightning * flicker;
        result.rgb = saturate(col + fc.rgb * fres * 0.20);
        result.a = saturate(0.16 + saturate(cloud * 0.2 + 0.2) + lightning * 0.56 + fres * 0.10);
    }
)HLSL"
R"HLSL(
    else if (m == 49)
    {
        float3 R = reflect(-V, N);
        float horizon = pow(saturate(1.0 - abs(R.y)), 2.2);
        float spec = pow(ndh, 120.0) * 1.8;
        float strata = 0.5 + 0.5 * sin(i.lpos.y * 11.0 + i.lpos.x * 3.0 - time * 0.42);
        float3 reflection = lerp(float3(0.005, 0.008, 0.018), float3(0.08, 0.22, 0.42), horizon);
        result.rgb = saturate(reflection + float3(0.28, 0.08, 0.55) * fres * strata + spec.xxx);
        result.a = 1.0;
    }
    else if (m == 50)
    {
        float3 p = i.lpos * 3.0;
        float warp = sin(p.x * 1.7 + time * 0.38) + sin(p.z * 2.3 - time * 0.31);
        float vein = pow(saturate(1.0 - abs(sin(p.y * 3.8 + warp * 1.7)) * 4.5), 2.0);
        float vein2 = pow(saturate(1.0 - abs(cos(p.x * 4.2 - p.z * 2.1 + time * 0.47)) * 6.0), 2.0);
        float3 stone = lerp(float3(0.025, 0.015, 0.06), float3(0.13, 0.04, 0.20), lambert);
        result.rgb = saturate(stone + float3(0.08, 0.72, 1.0) * vein +
            float3(1.0, 0.08, 0.62) * vein2 + fc.rgb * fres * 0.35);
        result.a = 0.96;
    }
    else if (m == 51)
    {
        float edge = pow(saturate(1.0 - ndv), 1.3);
        float facet = pow(saturate(dot(N, normalize(float3(-0.35, 0.72, 0.60)))), 16.0);
        float spectral = frac(edge * 2.4 + i.lpos.y * 0.16 + time * 0.12);
        float3 prism = saturate(abs(frac(spectral + float3(0.0, 0.67, 0.33)) * 6.0 - 3.0) - 1.0);
        float glint = pow(ndh, 180.0) * (1.0 + sin(time * 2.0) * 0.25);
        result.rgb = saturate(float3(0.20, 0.36, 0.48) * lambert + prism * edge * 0.85 +
            facet * 0.42 + glint.xxx);
        result.a = 0.88;
    }
    else if (m == 52)
    {
        float2 uv = i.lpos.xy * 18.0;
        float weaveA = step(0.5, frac(uv.x + floor(uv.y) * 0.5));
        float weaveB = step(0.5, frac(uv.y + floor(uv.x) * 0.5));
        float weave = abs(weaveA - weaveB);
        float rail = pow(saturate(1.0 - abs(frac((i.lpos.y - time * 0.46) * 3.0) - 0.5) * 8.0), 3.0);
        float spec = pow(ndh, 72.0);
        result.rgb = saturate(lerp(float3(0.018, 0.020, 0.024), float3(0.10, 0.12, 0.15), weave) +
            float3(0.0, 0.68, 1.0) * rail + spec.xxx * 0.72 + fc.rgb * fres * 0.18);
        result.a = 1.0;
    }
    else if (m == 53)
    {
        float3 p = i.lpos * 2.6;
        float flow = sin(p.x * 2.1 + time * 0.55 + sin(p.y * 2.8)) +
                     cos(p.z * 2.6 - time * 0.43 + sin(p.x * 1.9));
        float crest = smoothstep(0.35, 1.35, flow);
        float trough = smoothstep(-0.2, -1.45, flow);
        float3 col = lerp(float3(0.015, 0.025, 0.07), float3(0.02, 0.72, 1.0), crest);
        col += float3(0.85, 0.06, 1.0) * trough * 0.72 + pow(ndh, 82.0);
        result.rgb = saturate(col + fc.rgb * fres * 0.32);
        result.a = 0.94;
    }
    else if (m == 54)
    {
        float3 p = i.lpos * 7.0;
        float gx = pow(saturate(1.0 - abs(frac(p.x + sin(p.y + time * 0.62) * 0.22) - 0.5) * 10.0), 3.0);
        float gy = pow(saturate(1.0 - abs(frac(p.y + sin(p.z - time * 0.51) * 0.22) - 0.5) * 10.0), 3.0);
        float gz = pow(saturate(1.0 - abs(frac(p.z + sin(p.x + time * 0.43) * 0.22) - 0.5) * 10.0), 3.0);
        float lattice = saturate(gx + gy + gz);
        float3 col = lerp(float3(0.018, 0.01, 0.07), float3(0.16, 0.48, 1.0), lattice);
        result.rgb = saturate(col + float3(0.86, 0.12, 1.0) * gx * gy + fc.rgb * fres * 0.28);
        result.a = 0.95;
    }
    else if (m == 55)
    {
        float3 p = i.lpos;
        float scan = pow(saturate(1.0 - abs(frac((p.y - time * 0.78) * 7.0) - 0.5) * 9.0), 3.0);
        float glitch = step(0.86, frac(sin(floor(p.y * 24.0 + time * 5.0) * 71.3) * 41731.0));
        float grid = step(0.92, frac((p.x + p.z) * 16.0));
        float3 col = float3(0.015, 0.20, 0.28) + float3(0.0, 0.78, 1.0) * scan +
            float3(0.58, 0.14, 1.0) * glitch + grid * 0.22;
        result.rgb = saturate(col * (0.72 + lambert * 0.35) + fc.rgb * fres * 0.25);
        result.a = 0.90;
    }
    else if (m == 56)
    {
        float3 p = i.lpos * 2.5;
        float curl = sin(p.x * 2.3 + time * 0.64 + sin(p.y * 3.5)) +
                     cos(p.z * 3.0 - time * 0.51 + sin(p.x * 2.0));
        float flame = smoothstep(-0.35, 1.25, curl + p.y * 0.18);
        float lick = pow(saturate(0.5 + 0.5 * sin(p.y * 8.0 - time * 1.4 + curl)), 5.0);
        float3 col = lerp(float3(0.08, 0.008, 0.002), float3(0.92, 0.10, 0.005), flame);
        col = lerp(col, float3(1.0, 0.78, 0.10), lick * flame);
        result.rgb = saturate(col + fc.rgb * fres * 0.15);
        result.a = 1.0;
    }
    else if (m == 57)
    {
        float3 p = i.lpos * 4.0;
        float fracture = abs(sin(p.x * 2.7 + sin(p.y * 3.1)) * cos(p.z * 3.3 - sin(p.x * 2.2)));
        float crack = pow(saturate(1.0 - fracture * 8.0), 2.0);
        float reflection = pow(saturate(1.0 - ndv), 1.8);
        float spec = pow(ndh, 110.0);
        result.rgb = saturate(float3(0.008, 0.025, 0.045) * (0.65 + lambert * 0.4) +
            float3(0.12, 0.68, 1.0) * crack + float3(0.28, 0.52, 0.72) * reflection + spec.xxx);
        result.a = 0.98;
    }
    else if (m == 58)
    {
        float3 p = i.lpos * 2.0;
        float nebula = 0.5 + 0.5 * sin(p.x * 1.7 + time * 0.28) * cos(p.y * 2.1 - time * 0.23);
        float path1 = abs(sin(p.y * 5.2 + sin(p.x * 6.1 + time * 0.72) * 1.4 + p.z));
        float path2 = abs(cos(p.x * 5.7 - sin(p.z * 6.8 - time * 0.61) * 1.2 + p.y));
        float bolt = pow(saturate(1.0 - min(path1, path2) * 8.0), 2.0);
        float3 col = lerp(float3(0.012, 0.005, 0.06), float3(0.20, 0.04, 0.38), nebula);
        result.rgb = saturate(col + float3(0.42, 0.72, 1.0) * bolt +
            float3(0.90, 0.35, 1.0) * bolt * bolt + fc.rgb * fres * 0.28);
        result.a = 0.96;
    }
    else if (m == 59)
    {
        float3 p = i.lpos;
        float a = pow(saturate(1.0 - abs(frac((p.x + p.y - time * 0.44) * 5.0) - 0.5) * 11.0), 3.0);
        float b = pow(saturate(1.0 - abs(frac((p.z - p.y + time * 0.52) * 6.0) - 0.5) * 11.0), 3.0);
        float crossing = a * b;
        result.rgb = saturate(float3(0.018, 0.012, 0.045) + float3(1.0, 0.04, 0.42) * a +
            float3(0.0, 0.72, 1.0) * b + crossing * 0.75 + fc.rgb * fres * 0.22);
        result.a = 0.94;
    }
    else if (m == 60)
    {
        float3 p = i.lpos * 3.0;
        float cells = sin(p.x * 2.1 + sin(p.y * 3.0 + time * 0.51)) *
                      cos(p.z * 2.6 - sin(p.x * 2.7 - time * 0.44));
        float vein = pow(saturate(1.0 - abs(cells) * 7.0), 2.0);
        float pulse = 0.58 + 0.42 * sin(time * 2.1 - p.y * 2.4);
        result.rgb = saturate(float3(0.012, 0.055, 0.022) +
            float3(0.18, 1.0, 0.36) * vein * pulse + float3(0.0, 0.56, 0.76) * vein * vein +
            fc.rgb * fres * 0.18);
        result.a = 0.95;
    }
    else if (m == 61)
    {
        float3 R = reflect(-V, N);
        float3 chrome = pow(saturate(R * 0.5 + 0.5), 1.35);
        float pulse = 0.5 + 0.5 * sin(i.lpos.y * 7.0 - time * 0.92);
        float band = pow(pulse, 6.0);
        float spec = pow(ndh, 96.0);
        result.rgb = saturate(chrome * 0.62 + float3(0.02, 0.48, 1.0) * band +
            float3(0.72, 0.12, 1.0) * fres * 0.44 + spec.xxx);
        result.a = 1.0;
    }
    else if (m == 62)
    {
        float3 p = i.lpos * 4.4;
        float plates = sin(p.x * 1.8 + sin(p.y * 2.5)) * cos(p.z * 2.2 - sin(p.x * 1.7));
        float seam = pow(saturate(1.0 - abs(plates) * 9.0), 2.0);
        float heat = 0.72 + 0.28 * sin(time * 1.35 + p.y * 1.8);
        float3 armor = lerp(float3(0.025, 0.012, 0.008), float3(0.12, 0.035, 0.012), lambert);
        result.rgb = saturate(armor + float3(1.0, 0.20, 0.01) * seam * heat +
            float3(1.0, 0.78, 0.08) * seam * seam + pow(ndh, 68.0) * 0.38);
        result.a = 1.0;
    }
    else if (m == 63)
    {
        float3 p = i.lpos * 2.3;
        float angle = atan2(p.z, p.x);
        float radius = length(p.xz);
        float spiral = abs(sin(angle * 5.0 + radius * 12.0 - time * 1.05 + sin(p.y * 3.0)));
        float rift = pow(saturate(1.0 - spiral * 5.0), 2.0);
        float core = pow(saturate(1.0 - radius * 0.38), 3.0);
        float3 col = float3(0.012, 0.006, 0.06) + float3(0.56, 0.06, 0.92) * rift +
                     float3(0.08, 0.78, 1.0) * rift * core + float3(0.92, 0.95, 1.0) * rift * rift;
        result.rgb = saturate(col + fc.rgb * fres * 0.27);
        result.a = 0.97;
    }

    if (pixel_occluded && m >= 28)
        result.a = max(result.a, 0.82);
    result.a *= saturate(chams_opacity);
    return result;
}

)HLSL"
R"HLSL(

struct FSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

FSIn vs_fs(uint id : SV_VertexID)
{
    FSIn o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv=uv;
    return o;
}

float ps_publish(FSIn i) : SV_TARGET {
    uint w,h;world_depth.GetDimensions(w,h);
    int2 pixel=clamp(int2(i.uv*float2(w,h)),int2(0,0),int2(w-1,h-1));
    return world_depth.Load(int3(pixel,0));
}

bool OutlineSolid(int2 sp)
{
    uint tw, th;
    cham_depth.GetDimensions(tw, th);
    if (sp.x < 0 || sp.y < 0 || sp.x >= (int)tw || sp.y >= (int)th)
        return false;
    float d = cham_depth.Load(int3(sp, 0));
    return d <= 0.9995;
}

float4 ps_outline(FSIn i) : SV_TARGET
{
    if (outline_enabled == 0)
        return float4(0, 0, 0, 0);

    int2 sp = int2(i.pos.xy);
    if (OutlineSolid(sp))
        return float4(0, 0, 0, 0);

    int radius = (int)(2.0 + saturate(outline_fade / 3.0) * 6.0);
    if (radius < 2) radius = 2;
    if (radius > 8) radius = 8;

    bool near = false;
    [unroll] for (int k = 0; k < 8; ++k)
    {
        float ang = (float)k * 0.78539816;
        float2 dir = float2(cos(ang), sin(ang));
        int2 n1 = sp + int2(round(dir.x), round(dir.y));
        if (OutlineSolid(n1)) { near = true; break; }
        int mid = max(radius / 2, 2);
        int2 n2 = sp + int2(round(dir.x * (float)mid), round(dir.y * (float)mid));
        if (OutlineSolid(n2)) { near = true; break; }
        int2 n3 = sp + int2(round(dir.x * (float)radius), round(dir.y * (float)radius));
        if (OutlineSolid(n3)) { near = true; break; }
    }
    if (!near)
        return float4(0, 0, 0, 0);

    float nearest = 1e5;
    [unroll] for (int k = 0; k < 8; ++k)
    {
        float ang = (float)k * 0.78539816;
        float2 dir = float2(cos(ang), sin(ang));

        [loop] for (int r = 1; r <= radius; r += 1)
        {
            int2 np = sp + int2(round(dir.x * (float)r), round(dir.y * (float)r));
            if (!OutlineSolid(np))
                continue;
            float dist = length(float2(np - sp));
            if (dist < nearest)
                nearest = dist;
            break;
        }
    }

    if (nearest > (float)radius + 0.5)
        return float4(0, 0, 0, 0);

    float t = saturate(nearest / (float)radius);
    float core = exp(-nearest * nearest * 0.35);
    float mid  = exp(-t * t * 2.2);
    float tail = pow(saturate(1.0 - t), 2.8);
    float fall = core * 0.55 + mid * 0.40 + tail * 0.35;

    float3 rgb = outline_color.rgb;
    float a = outline_color.a * fall * 0.90;
    float anim = 1.0;

    if (outline_style == 0)
    {
        float breath = 0.85 + 0.15 * sin(time * 1.55);
        anim = breath;
        rgb *= 0.92 + 0.14 * core;
    }
    else if (outline_style == 1)
    {
        float wave = 0.5 + 0.5 * sin(nearest * 0.55 - time * 4.0);
        float pulse = 0.55 + 0.45 * (0.5 + 0.5 * sin(time * 2.6));
        anim = lerp(0.55, 1.15, wave) * pulse;
        fall = core * 0.50 + mid * (0.35 + 0.30 * wave) + tail * 0.40;
        a = outline_color.a * fall * 0.92;
        rgb *= 0.85 + 0.35 * wave;
    }
    else if (outline_style == 2)
    {
        float2 c = float2(sp) + 0.5;
        float band = sin(c.x * 0.065 + c.y * 0.048 - time * 3.2);
        float flow = 0.55 + 0.45 * (0.5 + 0.5 * band);
        anim = flow;
        rgb = lerp(rgb, saturate(rgb * 1.35 + 0.10), saturate(band * 0.40 + 0.25));
        a = outline_color.a * fall * (0.55 + 0.50 * flow);
    }
    else
    {
        float ang = atan2((float)sp.y, (float)sp.x);
        float swirl = 0.5 + 0.5 * sin(ang * 3.0 + time * 2.5 + nearest * 0.25);
        float rim = exp(-nearest * nearest * 0.18);
        anim = 0.60 + 0.40 * swirl;
        fall = rim * 0.75 + mid * 0.35 + tail * 0.40;
        a = outline_color.a * fall * (0.65 + 0.45 * swirl);
        rgb = lerp(rgb, saturate(rgb + float3(0.20, 0.30, 0.50) * swirl), 0.35);
        rgb *= 0.90 + 0.40 * rim;
    }

    a *= anim;
    a *= saturate(1.10 - t * 0.75);
    if (a < 0.008)
        return float4(0, 0, 0, 0);
    return float4(rgb, saturate(a));
}
)HLSL";

struct CBData
{
	float view[16];
	float world[16];
	float camera[3];
	float time;
	float base_color[4];
	float fresnel_color[4];
	float visible_color[4];
	float occluded_color[4];
	float occluded_fresnel[4];
	int   mode;
	float fresnel_power;
	int   occlusion_enabled;
	int   occluded_mode;
	float outline_color[4];
	float outline_fade;
	int   outline_style;
	int   outline_enabled;
	float glow_strength;
	float chams_opacity;
	float cb_padding[3];
};
static_assert(sizeof(CBData) % 16 == 0, "CBData");

struct GpuMesh
{
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	UINT          index_count = 0;
	float         bound_radius = 0.7071f;
	std::uint64_t last_use = 0;
};

struct DrawItem
{
	const GpuMesh* mesh = nullptr;
	Matrix4x4      world{};
};

constexpr float kDepthWindow = 5000.0f;

constexpr float kOccMinReach = 180.0f;
constexpr float kOccSlack = 8.0f;
constexpr std::size_t kOccMaxCount = 4096;

float g_occ_reach_last = 0.0f;
int   g_occ_count_last = 0;
int   g_occ_far_last = 0;
int   g_occ_targets_last = 0;
int   g_occ_skipped_last = 0;

float ItemRadius(const DrawItem& item)
{
	if (!item.mesh)
		return 0.0f;
	const Matrix4x4& w = item.world;
	const float cx = std::sqrt(w.m[0][0] * w.m[0][0] + w.m[1][0] * w.m[1][0] + w.m[2][0] * w.m[2][0]);
	const float cy = std::sqrt(w.m[0][1] * w.m[0][1] + w.m[1][1] * w.m[1][1] + w.m[2][1] * w.m[2][1]);
	const float cz = std::sqrt(w.m[0][2] * w.m[0][2] + w.m[1][2] * w.m[1][2] + w.m[2][2] * w.m[2][2]);
	const float scale = (std::max)(cx, (std::max)(cy, cz));
	return item.mesh->bound_radius * scale;
}

ID3D11Device*            g_device = nullptr;
ID3D11DeviceContext*     g_context = nullptr;
ID3D11VertexShader*      g_vs = nullptr;
ID3D11VertexShader*      g_vs_fs = nullptr;
ID3D11PixelShader*       g_ps = nullptr;
ID3D11PixelShader*       g_ps_depth = nullptr;
ID3D11PixelShader*       g_ps_publish = nullptr;
NativeWorldDepth::Exchange g_native_depth;
bool g_native_depth_failed=false;
ID3D11PixelShader*       g_ps_outline = nullptr;
ID3D11InputLayout*       g_layout = nullptr;
ID3D11Buffer*            g_cb = nullptr;
ID3D11BlendState*        g_blend = nullptr;
ID3D11BlendState*        g_blend_no_color = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;
ID3D11DepthStencilState* g_ds_off = nullptr;
ID3D11RasterizerState*   g_raster = nullptr;
ID3D11RasterizerState*   g_raster_wire = nullptr;
ID3D11Texture2D*         g_depth_tex = nullptr;
ID3D11DepthStencilView*  g_dsv = nullptr;
ID3D11ShaderResourceView* g_cham_srv = nullptr;
ID3D11Texture2D*         g_world_depth_tex = nullptr;
ID3D11DepthStencilView*  g_world_dsv = nullptr;
ID3D11ShaderResourceView* g_world_srv = nullptr;

GpuMesh g_unit_cube{};
GpuMesh g_unit_cyl{};
GpuMesh g_unit_wedge{};
std::unordered_map<std::string, GpuMesh> g_uploaded;
std::uint64_t g_upload_tick = 0;

constexpr std::size_t kMeshUploadCap = 512;
constexpr std::size_t kMeshUploadKeep = 384;

bool CreateUnitCylinder();
bool CreateUnitWedge();
std::vector<DrawItem> g_queue;
std::vector<DrawItem> g_world_occluders;

CBData   g_cbdata{};
unsigned g_width = 0;
unsigned g_height = 0;
bool     g_frame_valid = false;
Vector3  g_camera{};

const char* k_mode_names[] = {
	"flat", "chrome", "rainbow", "pearl", "glossy", "holographic",
	"fade", "wireframe", "glass", "ropes", "liquid metal",
	"soft glass", "ice", "ghost pulse", "aurora soft", "bubble", "jelly",
	"mercury soft", "water glass", "deep ocean", "quicksilver", "ripple", "oil slick",
	"plasma", "gold", "toxic", "galaxy", "matrix", "blue wave scan",
	"midnight currents", "ember drift", "nebula veil", "arctic flow", "void lightning",
	"squiggle carnival", "forked lightning", "lava bloom", "cyan circuitry", "prism ribbons",
	"ghost smoke", "pulse rings", "cherry pulse", "digital glitch", "ocean caustics",
	"molten cracks", "aurora ribbons", "monochrome ink", "comet rain", "storm plasma"
	,"obsidian ray glaze", "neon marble", "diamond dispersion", "carbon neon", "liquid neon armor",
	"plasma lattice", "solid hologram", "firestorm armor", "black ice armor", "cosmic lightning armor",
	"laser weave", "bioelectric veins", "chrome pulse", "magma armor", "quantum rift"
};

bool ModeUsesFillColor(int m)
{

	if (m == 0 || m == 7 || m == 8)
	{
		return true;
	}

	if (m >= 11 && m <= 22)
	{
		return true;
	}

	return false;
}

void ReleaseMesh(GpuMesh& mesh)
{
	if (mesh.vb) { mesh.vb->Release(); mesh.vb = nullptr; }
	if (mesh.ib) { mesh.ib->Release(); mesh.ib = nullptr; }
	mesh.index_count = 0;
}

GpuMesh Upload(const MeshVertex* verts, std::size_t vcount,
               const std::uint32_t* indices, std::size_t icount)
{
	GpuMesh out{};
	if (!g_device || !verts || !indices || vcount == 0 || icount == 0)
		return out;

	D3D11_BUFFER_DESC vbd{};
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = (UINT)(vcount * sizeof(MeshVertex));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vinit{ verts, 0, 0 };
	if (FAILED(g_device->CreateBuffer(&vbd, &vinit, &out.vb)))
		return out;

	D3D11_BUFFER_DESC ibd{};
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = (UINT)(icount * sizeof(std::uint32_t));
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA iinit{ indices, 0, 0 };
	if (FAILED(g_device->CreateBuffer(&ibd, &iinit, &out.ib)))
	{
		out.vb->Release();
		out.vb = nullptr;
		return out;
	}
	out.index_count = (UINT)icount;
	float bound = 0.0f;
	for (std::size_t i = 0; i < vcount; ++i)
	{
		const float lengthSquared =
			verts[i].pos[0] * verts[i].pos[0] +
			verts[i].pos[1] * verts[i].pos[1] +
			verts[i].pos[2] * verts[i].pos[2];
		if (lengthSquared > bound)
			bound = lengthSquared;
	}
	out.bound_radius = bound > 0.0f ? std::sqrt(bound) : 0.7071f;
	return out;
}

bool CreateDepth(unsigned w, unsigned h)
{
	if (g_world_srv) { g_world_srv->Release(); g_world_srv = nullptr; }
	if (g_world_dsv) { g_world_dsv->Release(); g_world_dsv = nullptr; }
	if (g_world_depth_tex) { g_world_depth_tex->Release(); g_world_depth_tex = nullptr; }
	if (g_cham_srv) { g_cham_srv->Release(); g_cham_srv = nullptr; }
	if (g_dsv) { g_dsv->Release(); g_dsv = nullptr; }
	if (g_depth_tex) { g_depth_tex->Release(); g_depth_tex = nullptr; }

	auto make_depth_srv = [&](ID3D11Texture2D** tex, ID3D11DepthStencilView** dsv,
	                          ID3D11ShaderResourceView** srv) -> bool {
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(g_device->CreateTexture2D(&td, nullptr, tex)))
			return false;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
		dsvd.Format = DXGI_FORMAT_D32_FLOAT;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		if (FAILED(g_device->CreateDepthStencilView(*tex, &dsvd, dsv)))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
		srvd.Format = DXGI_FORMAT_R32_FLOAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;
		if (FAILED(g_device->CreateShaderResourceView(*tex, &srvd, srv)))
			return false;
		return true;
	};

	if (!make_depth_srv(&g_depth_tex, &g_dsv, &g_cham_srv))
		return false;
	if (!make_depth_srv(&g_world_depth_tex, &g_world_dsv, &g_world_srv))
		return false;

	g_width = w;
	g_height = h;
	return true;
}

Matrix4x4 MakeBoxWorld(const Vector3& pos, const Matrix4x4& rot, const Vector3& sz)
{
	Matrix4x4 w{};
	w.m[0][0] = rot.m[0][0] * sz.x;
	w.m[0][1] = rot.m[0][1] * sz.y;
	w.m[0][2] = rot.m[0][2] * sz.z;
	w.m[0][3] = pos.x;
	w.m[1][0] = rot.m[1][0] * sz.x;
	w.m[1][1] = rot.m[1][1] * sz.y;
	w.m[1][2] = rot.m[1][2] * sz.z;
	w.m[1][3] = pos.y;
	w.m[2][0] = rot.m[2][0] * sz.x;
	w.m[2][1] = rot.m[2][1] * sz.y;
	w.m[2][2] = rot.m[2][2] * sz.z;
	w.m[2][3] = pos.z;
	w.m[3][0] = 0.f;
	w.m[3][1] = 0.f;
	w.m[3][2] = 0.f;
	w.m[3][3] = 1.f;
	return w;
}

const GpuMesh* Fetch(const std::string& mesh_id)
{
	if (mesh_id.empty())
		return nullptr;

	if (auto it = g_uploaded.find(mesh_id); it != g_uploaded.end())
	{
		if (!it->second.vb)
			return nullptr;
		it->second.last_use = ++g_upload_tick;
		return &it->second;
	}

	const auto mesh = MeshCache::Get().FindShared(mesh_id);
	if (!mesh || mesh->vertices.empty() || mesh->faces.empty())
		return nullptr;

	const std::size_t vcount = mesh->vertices.size();
	if (vcount > 50000)
		return nullptr;

	std::size_t fac_count = mesh->faces.size();
	if (fac_count > 20000)
		fac_count = 20000;

	std::vector<std::uint32_t> indices;
	indices.reserve(fac_count * 3);
	for (std::size_t i = 0; i < fac_count; ++i)
	{
		const auto& f = mesh->faces[i];
		if (f.indices[0] >= vcount || f.indices[1] >= vcount || f.indices[2] >= vcount)
			continue;
		indices.push_back(f.indices[0]);
		indices.push_back(f.indices[1]);
		indices.push_back(f.indices[2]);
	}
	if (indices.empty())
		return nullptr;

	if (g_uploaded.size() >= kMeshUploadCap)
	{
		while (g_uploaded.size() > kMeshUploadKeep)
		{
			auto oldest = g_uploaded.begin();
			for (auto it = g_uploaded.begin(); it != g_uploaded.end(); ++it)
			{
				if (it->second.last_use < oldest->second.last_use)
					oldest = it;
			}
			ReleaseMesh(oldest->second);
			g_uploaded.erase(oldest);
		}
	}

	GpuMesh gpu = Upload(mesh->vertices.data(), vcount, indices.data(), indices.size());
	gpu.last_use = ++g_upload_tick;
	auto [it, _] = g_uploaded.emplace(mesh_id, gpu);
	return it->second.vb ? &it->second : nullptr;
	return it->second.vb ? &it->second : nullptr;
}

static std::unordered_map<std::string, std::pair<Vector3, Vector3>> g_occ_fit;

static bool BuildOccluderWorld(const std::string& mesh_id,
    const Vector3& pos, const Matrix4x4& rot, const Vector3& size,
    Matrix4x4& out, const GpuMesh** gpuOut)
{
    *gpuOut = &g_unit_cube;
    if (mesh_id.empty())
        return false;
    const GpuMesh* m = Fetch(mesh_id);
    if (!m)
        return false;
    Vector3 center{}, extent{};
    if (auto it = g_occ_fit.find(mesh_id); it != g_occ_fit.end())
    {
        center = it->second.first;
        extent = it->second.second;
    }
    else
    {
        const auto mesh = MeshCache::Get().FindShared(mesh_id);
        if (!mesh || mesh->vertices.empty())
            return false;
        Vector3 mn{ 1e30f, 1e30f, 1e30f }, mx{ -1e30f, -1e30f, -1e30f };
        for (const auto& v : mesh->vertices)
        {
            mn.x = (std::min)(mn.x, v.pos[0]);
            mn.y = (std::min)(mn.y, v.pos[1]);
            mn.z = (std::min)(mn.z, v.pos[2]);
            mx.x = (std::max)(mx.x, v.pos[0]);
            mx.y = (std::max)(mx.y, v.pos[1]);
            mx.z = (std::max)(mx.z, v.pos[2]);
        }
        center = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
        extent = {
            (std::max)(mx.x - mn.x, 1e-4f),
            (std::max)(mx.y - mn.y, 1e-4f),
            (std::max)(mx.z - mn.z, 1e-4f) };
        g_occ_fit.emplace(mesh_id, std::make_pair(center, extent));
    }
    const Vector3 s{
        extent.x < 1e-4f ? 1.0f : size.x / extent.x,
        extent.y < 1e-4f ? 1.0f : size.y / extent.y,
        extent.z < 1e-4f ? 1.0f : size.z / extent.z };
    const float cx = center.x * s.x, cy = center.y * s.y, cz = center.z * s.z;
    Matrix4x4 w{};
    w.m[0][0] = rot.m[0][0] * s.x; w.m[0][1] = rot.m[0][1] * s.y; w.m[0][2] = rot.m[0][2] * s.z;
    w.m[0][3] = pos.x - (rot.m[0][0] * cx + rot.m[0][1] * cy + rot.m[0][2] * cz);
    w.m[1][0] = rot.m[1][0] * s.x; w.m[1][1] = rot.m[1][1] * s.y; w.m[1][2] = rot.m[1][2] * s.z;
    w.m[1][3] = pos.y - (rot.m[1][0] * cx + rot.m[1][1] * cy + rot.m[1][2] * cz);
    w.m[2][0] = rot.m[2][0] * s.x; w.m[2][1] = rot.m[2][1] * s.y; w.m[2][2] = rot.m[2][2] * s.z;
    w.m[2][3] = pos.z - (rot.m[2][0] * cx + rot.m[2][1] * cy + rot.m[2][2] * cz);
    w.m[3][0] = 0.f; w.m[3][1] = 0.f; w.m[3][2] = 0.f; w.m[3][3] = 1.f;
    out = w;
    *gpuOut = m;
    return true;
}

void EnsureDepthFromRtv(ID3D11RenderTargetView* rtv)
{
	if (g_dsv && g_world_dsv)
		return;
	unsigned w = g_width, h = g_height;
	if ((!w || !h) && rtv)
	{
		ID3D11Resource* res = nullptr;
		rtv->GetResource(&res);
		if (res)
		{
			ID3D11Texture2D* tex = nullptr;
			if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex)) && tex)
			{
				D3D11_TEXTURE2D_DESC td{};
				tex->GetDesc(&td);
				w = td.Width;
				h = td.Height;
				tex->Release();
			}
			res->Release();
		}
	}
	if (w && h)
		CreateDepth(w, h);
}

bool CreateUnitCube()
{
	const MeshVertex verts[24] = {
		{ { 0.5f, -0.5f, -0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f,  0.5f, -0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f,  0.5f,  0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f, -0.5f,  0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f,  0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f,  0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, 0.5f,  0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, 0.5f,  0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f,  0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
	};
	const std::uint32_t idx[36] = {
		0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
	};
	g_unit_cube = Upload(verts, 24, idx, 36);
	CreateUnitCylinder();
	CreateUnitWedge();
	return g_unit_cube.vb != nullptr;
}

bool CreateUnitCylinder()
{
	constexpr int K = 16;
	std::vector<MeshVertex> verts;
	std::vector<std::uint32_t> idx;
	verts.reserve(K * 4 + 2);
	for (int i = 0; i < K; ++i)
	{
		const float a0 = (float)i / K * 6.2831853f;
		const float a1 = (float)(i + 1) / K * 6.2831853f;
		const float c0 = std::cos(a0), s0 = std::sin(a0);
		const float c1 = std::cos(a1), s1 = std::sin(a1);
		const float nx0 = c0, nz0 = s0, nx1 = c1, nz1 = s1;

		verts.push_back({ { c0 * 0.5f, -0.5f, s0 * 0.5f }, { nx0, 0, nz0 }, { 0, 0 }, 0, 0 });
		verts.push_back({ { c0 * 0.5f, 0.5f, s0 * 0.5f }, { nx0, 0, nz0 }, { 0, 0 }, 0, 0 });
		verts.push_back({ { c1 * 0.5f, 0.5f, s1 * 0.5f }, { nx1, 0, nz1 }, { 0, 0 }, 0, 0 });
		verts.push_back({ { c1 * 0.5f, -0.5f, s1 * 0.5f }, { nx1, 0, nz1 }, { 0, 0 }, 0, 0 });
		const std::uint32_t b = (std::uint32_t)verts.size() - 4;
		idx.push_back(b); idx.push_back(b + 1); idx.push_back(b + 2);
		idx.push_back(b); idx.push_back(b + 2); idx.push_back(b + 3);
	}
	g_unit_cyl = Upload(verts.data(), (int)verts.size(), idx.data(), (int)idx.size());
	return g_unit_cyl.vb != nullptr;
}

bool CreateUnitWedge()
{

	const MeshVertex verts[6] = {
		{ { -0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f,  0.5f, -0.5f }, { 0, 1, 0 },  { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, -0.5f }, { 0, 1, 0 },  { 0, 0 }, 0, 0 },
	};

	const std::uint32_t idx[24] = {
		0, 2, 1,   0, 3, 2,
		0, 1, 4,   0, 4, 5,
		3, 2, 4,   3, 4, 5,
		0, 3, 5,
		1, 2, 4,
	};
	g_unit_wedge = Upload(verts, 6, idx, 24);
	return g_unit_wedge.vb != nullptr;
}

void Issue(const GpuMesh& mesh, const Matrix4x4& world)
{
	std::memcpy(g_cbdata.world, &world, sizeof(world));

	D3D11_MAPPED_SUBRESOURCE ms{};
	if (FAILED(g_context->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		return;
	std::memcpy(ms.pData, &g_cbdata, sizeof(g_cbdata));
	g_context->Unmap(g_cb, 0);

	const UINT stride = sizeof(MeshVertex);
	const UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &mesh.vb, &stride, &offset);
	g_context->IASetIndexBuffer(mesh.ib, DXGI_FORMAT_R32_UINT, 0);
	g_context->DrawIndexed(mesh.index_count, 0, 0);
}

}

bool Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
	if (!device || !context)
		return false;

	g_device = device;
	g_context = context;

	ID3DBlob* vs_blob = nullptr;
	ID3DBlob* ps_blob = nullptr;
	ID3DBlob* err = nullptr;

	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "vs_main", "vs_4_0", 0, 0, &vs_blob, &err)))
	{
		if (err) err->Release();
		return false;
	}
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_main", "ps_4_0", 0, 0, &ps_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		return false;
	}

	if (FAILED(g_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
	                                       nullptr, &g_vs)) ||
	    FAILED(g_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
	                                      nullptr, &g_ps)))
	{
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}

	ID3DBlob* ps_depth_blob = nullptr;
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_depth", "ps_4_0", 0, 0, &ps_depth_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(g_device->CreatePixelShader(ps_depth_blob->GetBufferPointer(),
	                                      ps_depth_blob->GetBufferSize(), nullptr, &g_ps_depth)))
	{
		ps_depth_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	ps_depth_blob->Release();
    ID3DBlob* publish=nullptr;
    if(FAILED(D3DCompile(k_hlsl,sizeof(k_hlsl)-1,"world_depth",nullptr,nullptr,
        "ps_publish","ps_4_0",0,0,&publish,&err))) {if(err)err->Release();return false;}
    const auto publishResult=g_device->CreatePixelShader(publish->GetBufferPointer(),publish->GetBufferSize(),nullptr,&g_ps_publish);
    publish->Release();if(FAILED(publishResult))return false;

	ID3DBlob* vs_fs_blob = nullptr;
	ID3DBlob* ps_outline_blob = nullptr;
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "vs_fs", "vs_4_0", 0, 0, &vs_fs_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_outline", "ps_4_0", 0, 0, &ps_outline_blob, &err)))
	{
		if (err) err->Release();
		vs_fs_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(g_device->CreateVertexShader(vs_fs_blob->GetBufferPointer(),
	                                       vs_fs_blob->GetBufferSize(), nullptr, &g_vs_fs)) ||
	    FAILED(g_device->CreatePixelShader(ps_outline_blob->GetBufferPointer(),
	                                      ps_outline_blob->GetBufferSize(), nullptr, &g_ps_outline)))
	{
		vs_fs_blob->Release();
		ps_outline_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	vs_fs_blob->Release();
	ps_outline_blob->Release();

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(g_device->CreateInputLayout(layout, _countof(layout),
	                                      vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
	                                      &g_layout)))
	{
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	vs_blob->Release();
	ps_blob->Release();

	D3D11_BUFFER_DESC cbd{};
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(CBData);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(g_device->CreateBuffer(&cbd, nullptr, &g_cb)))
		return false;

	D3D11_BLEND_DESC bd{};
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(g_device->CreateBlendState(&bd, &g_blend)))
		return false;

	D3D11_BLEND_DESC bd_no{};
	bd_no.RenderTarget[0].BlendEnable = FALSE;
	bd_no.RenderTarget[0].RenderTargetWriteMask = 0;
	if (FAILED(g_device->CreateBlendState(&bd_no, &g_blend_no_color)))
		return false;

	D3D11_DEPTH_STENCIL_DESC dsd{};
	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(g_device->CreateDepthStencilState(&dsd, &g_ds)))
		return false;

	D3D11_DEPTH_STENCIL_DESC dsd_off{};
	dsd_off.DepthEnable = FALSE;
	dsd_off.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsd_off.DepthFunc = D3D11_COMPARISON_ALWAYS;
	if (FAILED(g_device->CreateDepthStencilState(&dsd_off, &g_ds_off)))
		return false;

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	if (FAILED(g_device->CreateRasterizerState(&rd, &g_raster)))
		return false;

	D3D11_RASTERIZER_DESC rdw = rd;
	rdw.FillMode = D3D11_FILL_WIREFRAME;
	rdw.AntialiasedLineEnable = TRUE;
	if (FAILED(g_device->CreateRasterizerState(&rdw, &g_raster_wire)))
		return false;

	return CreateUnitCube();
}

void Shutdown()
{
    g_native_depth.Reset();g_native_depth_failed=false;
    if(g_ps_publish){g_ps_publish->Release();g_ps_publish=nullptr;}
	g_queue.clear();
	g_world_occluders.clear();
	g_frame_valid = false;
	for (auto& [_, mesh] : g_uploaded)
		ReleaseMesh(mesh);
	g_uploaded.clear();
	ReleaseMesh(g_unit_cube);
	ReleaseMesh(g_unit_cyl);
	ReleaseMesh(g_unit_wedge);

	if (g_world_srv) { g_world_srv->Release(); g_world_srv = nullptr; }
	if (g_world_dsv) { g_world_dsv->Release(); g_world_dsv = nullptr; }
	if (g_world_depth_tex) { g_world_depth_tex->Release(); g_world_depth_tex = nullptr; }
	if (g_cham_srv) { g_cham_srv->Release(); g_cham_srv = nullptr; }
	if (g_dsv) { g_dsv->Release(); g_dsv = nullptr; }
	if (g_depth_tex) { g_depth_tex->Release(); g_depth_tex = nullptr; }
	if (g_raster_wire) { g_raster_wire->Release(); g_raster_wire = nullptr; }
	if (g_raster) { g_raster->Release(); g_raster = nullptr; }
	if (g_ds_off) { g_ds_off->Release(); g_ds_off = nullptr; }
	if (g_ds) { g_ds->Release(); g_ds = nullptr; }
	if (g_blend_no_color) { g_blend_no_color->Release(); g_blend_no_color = nullptr; }
	if (g_blend) { g_blend->Release(); g_blend = nullptr; }
	if (g_cb) { g_cb->Release(); g_cb = nullptr; }
	if (g_layout) { g_layout->Release(); g_layout = nullptr; }
	if (g_ps_outline) { g_ps_outline->Release(); g_ps_outline = nullptr; }
	if (g_ps_depth) { g_ps_depth->Release(); g_ps_depth = nullptr; }
	if (g_ps) { g_ps->Release(); g_ps = nullptr; }
	if (g_vs_fs) { g_vs_fs->Release(); g_vs_fs = nullptr; }
	if (g_vs) { g_vs->Release(); g_vs = nullptr; }
	g_device = nullptr;
	g_context = nullptr;
	g_width = g_height = 0;
}

void Resize(unsigned width, unsigned height)
{
	if (!g_device || width == 0 || height == 0)
		return;
	if (width == g_width && height == g_height && g_dsv)
		return;
	CreateDepth(width, height);
}

void BeginFrame(const Matrix4x4& view, const Vector3& camera, float time)
{
	g_queue.clear();
	g_world_occluders.clear();
	g_camera = camera;
	g_frame_valid = (g_vs && g_ps && g_ps_depth && g_vs_fs && g_ps_outline && g_cb);

	std::memcpy(g_cbdata.view, &view, sizeof(view));
	g_cbdata.camera[0] = camera.x;
	g_cbdata.camera[1] = camera.y;
	g_cbdata.camera[2] = camera.z;
	g_cbdata.time = time;

	g_cbdata.mode = variables::ESP::meshChamsDxMode;
	if (g_cbdata.mode < 0) g_cbdata.mode = 0;
	if (g_cbdata.mode > 63) g_cbdata.mode = 63;
	g_cbdata.occluded_mode = variables::ESP::meshChamsOccludedDxMode;
	if (g_cbdata.occluded_mode < 0) g_cbdata.occluded_mode = 0;
	if (g_cbdata.occluded_mode > 63) g_cbdata.occluded_mode = 63;

	static const float k_white[4] = { 1.f, 1.f, 1.f, 1.f };
	const float* fill = ModeUsesFillColor(g_cbdata.mode) ? variables::ESP::chamsFillColor : k_white;
	std::memcpy(g_cbdata.base_color, fill, sizeof(g_cbdata.base_color));
	std::memcpy(g_cbdata.fresnel_color, fill, sizeof(g_cbdata.fresnel_color));
	std::memcpy(g_cbdata.visible_color, fill, sizeof(g_cbdata.visible_color));

	std::memcpy(g_cbdata.occluded_color, variables::ESP::meshChamsOccludedColor,
	            sizeof(g_cbdata.occluded_color));
	std::memcpy(g_cbdata.occluded_fresnel, variables::ESP::meshChamsOccludedColor,
	            sizeof(g_cbdata.occluded_fresnel));
	g_cbdata.fresnel_power = 2.5f;
	g_cbdata.occlusion_enabled = variables::ESP::meshChams && variables::ESP::meshChamsOcclusion ? 1 : 0;

	std::memcpy(g_cbdata.outline_color, variables::ESP::meshChamsOutlineColor, sizeof(g_cbdata.outline_color));
	g_cbdata.outline_fade = variables::ESP::meshChamsOutlineFade;
	if (g_cbdata.outline_fade < 0.35f) g_cbdata.outline_fade = 0.35f;

	if (g_cbdata.outline_fade > 2.f) g_cbdata.outline_fade = 2.f;
	g_cbdata.outline_style = variables::ESP::meshChamsOutlineStyle;
	if (g_cbdata.outline_style < 0) g_cbdata.outline_style = 0;
	if (g_cbdata.outline_style > 3) g_cbdata.outline_style = 3;
	g_cbdata.outline_enabled = variables::ESP::meshChams && variables::ESP::meshChamsOutline ? 1 : 0;
	g_cbdata.glow_strength = 0.f;
	g_cbdata.chams_opacity = (std::max)(0.0f, (std::min)(1.0f, variables::ESP::meshChamsOpacity));
	g_cbdata.cb_padding[0]=5000;g_cbdata.cb_padding[1]=1;g_cbdata.cb_padding[2]=0;
}

void QueueMesh(const std::string& mesh_id, const Matrix4x4& world)
{
	if (!g_frame_valid)
		return;
	const GpuMesh* mesh = Fetch(mesh_id);
	if (!mesh)
		return;
	g_queue.push_back({ mesh, world });
}

void QueueBox(const Matrix4x4& world)
{
	if (!g_frame_valid || !g_unit_cube.vb)
		return;
	g_queue.push_back({ &g_unit_cube, world });
}

std::uintptr_t NativeDepthHandle(unsigned slot) {
    return slot<3?reinterpret_cast<std::uintptr_t>(g_native_depth.slots[slot].handle):0;
}

void Flush(ID3D11RenderTargetView* rtv)
{
    const bool nativeDepth=NativeChams::WorldDepthNeeded();
    if(nativeDepth && !variables::ESP::meshChams)MeshCache::Get().Refresh(false);

	if (!g_frame_valid || !rtv || !g_context || (g_queue.empty() && !nativeDepth))
	{
		g_queue.clear();
		g_world_occluders.clear();
		g_frame_valid = false;
		return;
	}

	if (Globals::renderEngine.Addr)
	{
		const RBX::Mat4 live = Globals::renderEngine.GetViewMat();
		std::memcpy(g_cbdata.view, live.data, sizeof(live.data));
        const auto camera=NativePattern::CameraFrom(live.data,float(g_width),float(g_height));
        if(camera.valid) {
            std::memcpy(g_cbdata.camera,camera.pos,12);
            g_camera={camera.pos[0],camera.pos[1],camera.pos[2]};
        }

	}

    g_cbdata.cb_padding[0]=5000;
    for(const auto& item:g_queue){
        float dx=item.world.m[0][3]-g_cbdata.camera[0],dy=item.world.m[1][3]-g_cbdata.camera[1],dz=item.world.m[2][3]-g_cbdata.camera[2];
        g_cbdata.cb_padding[0]=(std::max)(g_cbdata.cb_padding[0],std::sqrt(dx*dx+dy*dy+dz*dz)+ItemRadius(item)+32);
    }
	EnsureDepthFromRtv(rtv);
	if (!g_dsv)
	{
		g_queue.clear();
		g_world_occluders.clear();
		g_frame_valid = false;
		return;
	}

	const bool want_occ = (nativeDepth || g_cbdata.occlusion_enabled != 0) && g_world_dsv && g_world_srv && g_unit_cube.vb;
	if (!want_occ)
	{
		g_occ_reach_last = 0.0f;
		g_occ_count_last = 0;
		g_occ_far_last = 0;
		g_occ_targets_last = 0;
		g_occ_skipped_last = 0;
	}
	if (want_occ)
	{

		g_world_occluders.clear();
		g_world_occluders.reserve(kOccMaxCount);
		std::vector<MeshOcclusion::Relevance> relevances;
		relevances.reserve(g_queue.size());
		float reach = kOccMinReach;
		for (const DrawItem& item : g_queue)
		{
			const Matrix4x4& w = item.world;
			const Vector3 centre{ w.m[0][3], w.m[1][3], w.m[2][3] };
			const float radius = ItemRadius(item) + kOccSlack;
			bool merged = false;
			for (MeshOcclusion::Relevance& known : relevances)
			{
				const Vector3 d = known.to - centre;
				const float gap = known.radius + radius;
				const float gapSquared = d.x * d.x + d.y * d.y + d.z * d.z;
				if (gapSquared > gap * gap)
					continue;

				const float separation = std::sqrt(gapSquared) + radius;
				if (separation > known.radius)
					known.radius = separation;
				merged = true;
				break;
			}
			if (!merged)
				relevances.push_back({ g_camera, centre, radius });
			const Vector3 fromCamera = centre - g_camera;
			const float distance =
				std::sqrt(fromCamera.x * fromCamera.x + fromCamera.y * fromCamera.y + fromCamera.z * fromCamera.z);
			if (distance + radius > reach)
				reach = distance + radius;
		}

        if(nativeDepth)for(const auto& player:PlayerCache::players) {
            if(!player.isValid || !player.characterAddr)continue;
            const auto& p=player.position;
            if(!std::isfinite(p.X)||!std::isfinite(p.Y)||!std::isfinite(p.Z))continue;
            const Vector3 centre{p.X,p.Y,p.Z};const Vector3 d=centre-g_camera;
            const float distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
            if(!variables::ESP::nativeUnlimited && distance>variables::ESP::nativeDistance+10)continue;
            relevances.push_back({g_camera,centre,10.f});
            reach=(std::max)(reach,distance+12.f);
            g_cbdata.cb_padding[0]=(std::max)(g_cbdata.cb_padding[0],reach+32.f);
        }
		if (reach > g_cbdata.cb_padding[0])
			reach = g_cbdata.cb_padding[0];
		g_occ_reach_last = reach;
		g_occ_targets_last = (int)relevances.size();
		g_occ_far_last = 0;
		g_occ_skipped_last = 0;
		MeshOcclusion::VisitOccluders(
			g_camera, reach, kOccMaxCount, relevances,
			[&](const Vector3& pos, const Matrix4x4& rot, const Vector3& size,
			   const std::string& mesh_id, std::uint8_t shape, bool can_query, bool rejected) {
				if (rejected || !can_query)
					return;
				{
					const Vector3 d = pos - g_camera;
					if (d.x * d.x + d.y * d.y + d.z * d.z > kOccMinReach * kOccMinReach)
						++g_occ_far_last;
				}
				Matrix4x4 w{};
				const GpuMesh* m = nullptr;
				if (BuildOccluderWorld(mesh_id, pos, rot, size, w, &m))
					g_world_occluders.push_back({ m, w });
				else if (shape == 1 && g_unit_cyl.vb)
					g_world_occluders.push_back({ &g_unit_cyl, MakeBoxWorld(pos, rot, size) });
				else if (shape == 2 && g_unit_wedge.vb)
					g_world_occluders.push_back({ &g_unit_wedge, MakeBoxWorld(pos, rot, size) });
				else if (mesh_id.empty())
					g_world_occluders.push_back({ &g_unit_cube, MakeBoxWorld(pos, rot, size) });
				else
				{

					++g_occ_skipped_last;
				}
			});
		g_occ_count_last = (int)g_world_occluders.size();
	}

	D3D11_VIEWPORT vp{
		0.f, 0.f, (float)g_width, (float)g_height, 0.f, 1.f
	};
	g_context->RSSetViewports(1, &vp);
	g_context->RSSetState(g_raster);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->IASetInputLayout(g_layout);
	g_context->VSSetShader(g_vs, nullptr, 0);
	g_context->VSSetConstantBuffers(0, 1, &g_cb);
	g_context->PSSetConstantBuffers(0, 1, &g_cb);
	g_context->OMSetDepthStencilState(g_ds, 0);

	const FLOAT bf[4]{ 0, 0, 0, 0 };

	if (want_occ)
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);

		g_context->ClearDepthStencilView(g_world_dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
		ID3D11RenderTargetView* null_rtv[1]{ nullptr };
		g_context->OMSetRenderTargets(1, null_rtv, g_world_dsv);
		g_context->OMSetBlendState(g_blend_no_color, bf, 0xFFFFFFFFu);
		g_context->PSSetShader(g_ps_depth, nullptr, 0);

		for (const auto& item : g_world_occluders)
			Issue(*item.mesh, item.world);

		g_context->OMSetRenderTargets(1, null_rtv, nullptr);
	}
	else if (g_cbdata.occlusion_enabled != 0)
	{

		g_cbdata.occlusion_enabled = 0;
	}

    if(nativeDepth && want_occ && g_ps_publish) {
        if(!g_native_depth.width && !g_native_depth_failed && !g_native_depth.Init(g_device,g_width,g_height)) {
            g_native_depth_failed=true;std::printf("[NativeChams] shared world-depth texture creation failed\n");
        }
        const int slot=g_native_depth.AcquireWrite();
        if(slot>=0) {
            auto& shared=g_native_depth.slots[slot];
            D3D11_VIEWPORT sharedVp{0,0,float(g_native_depth.width),float(g_native_depth.height),0,1};
            g_context->RSSetViewports(1,&sharedVp);
            auto* sharedRtv=shared.rtv.Get();g_context->OMSetRenderTargets(1,&sharedRtv,nullptr);
            g_context->OMSetBlendState(nullptr,bf,0xFFFFFFFFu);
            g_context->OMSetDepthStencilState(g_ds_off,0);
            g_context->IASetInputLayout(nullptr);
            g_context->VSSetShader(g_vs_fs,nullptr,0);
            g_context->PSSetShader(g_ps_publish,nullptr,0);
            g_context->PSSetShaderResources(0,1,&g_world_srv);
            g_context->Draw(3,0);
            ID3D11ShaderResourceView* clear=nullptr;g_context->PSSetShaderResources(0,1,&clear);
            g_context->OMSetRenderTargets(0,nullptr,nullptr);
            NativeWorldDepth::Snapshot snapshot{};
            std::memcpy(snapshot.view,g_cbdata.view,64);std::memcpy(snapshot.camera_range,g_cbdata.camera,12);
            snapshot.camera_range[3]=g_cbdata.cb_padding[0];
            snapshot.info[0]=1;snapshot.info[1]=float(g_native_depth.width);snapshot.info[2]=float(g_native_depth.height);

            NativeChams::SubmitWorldDepth(unsigned(slot),snapshot);
            shared.mutex->ReleaseSync(0);
            g_context->RSSetViewports(1,&vp);
            g_context->IASetInputLayout(g_layout);g_context->VSSetShader(g_vs,nullptr,0);
            g_context->OMSetDepthStencilState(g_ds,0);
        }
    }
    if(g_queue.empty()) {
        g_context->OMSetRenderTargets(1,&rtv,nullptr);
        g_world_occluders.clear();g_frame_valid=false;return;
    }

	g_context->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	g_context->OMSetRenderTargets(1, &rtv, g_dsv);
	g_context->OMSetBlendState(g_blend, bf, 0xFFFFFFFFu);
	g_context->PSSetShader(g_ps, nullptr, 0);
	{
		const bool wire =
			g_cbdata.mode == 7 ||
			(g_cbdata.occlusion_enabled != 0 && g_cbdata.occluded_mode == 7);
		g_context->RSSetState((wire && g_raster_wire) ? g_raster_wire : g_raster);
	}

	if (g_cbdata.occlusion_enabled != 0 && g_world_srv)
	{
		ID3D11ShaderResourceView* srvs[1]{ g_world_srv };
		g_context->PSSetShaderResources(0, 1, srvs);
	}
	else
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);
	}

    for(const auto& item:g_queue){

        if(item.mesh)Issue(*item.mesh,item.world);
    }

	const bool want_outline =
		g_cbdata.outline_enabled != 0 && g_cham_srv && g_vs_fs && g_ps_outline;
	if (want_outline)
	{
		g_context->RSSetState(g_raster);
		ID3D11RenderTargetView* null_rtv[1]{ nullptr };
		g_context->OMSetRenderTargets(1, null_rtv, nullptr);

		ID3D11ShaderResourceView* srvs[2]{
			(g_cbdata.occlusion_enabled != 0) ? g_world_srv : g_cham_srv,
			g_cham_srv
		};

		if (g_cbdata.occlusion_enabled == 0)
			srvs[0] = g_cham_srv;
		g_context->PSSetShaderResources(0, 2, srvs);

		g_context->OMSetRenderTargets(1, &rtv, nullptr);
		g_context->OMSetDepthStencilState(g_ds_off, 0);
		g_context->OMSetBlendState(g_blend, bf, 0xFFFFFFFFu);
		g_context->VSSetShader(g_vs_fs, nullptr, 0);
		g_context->PSSetShader(g_ps_outline, nullptr, 0);
		g_context->IASetInputLayout(nullptr);
		g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D11_MAPPED_SUBRESOURCE ms{};
		if (SUCCEEDED(g_context->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		{
			std::memcpy(ms.pData, &g_cbdata, sizeof(g_cbdata));
			g_context->Unmap(g_cb, 0);
		}
		g_context->Draw(3, 0);

		ID3D11ShaderResourceView* null2[2]{ nullptr, nullptr };
		g_context->PSSetShaderResources(0, 2, null2);
		g_context->VSSetShader(g_vs, nullptr, 0);
		g_context->IASetInputLayout(g_layout);
		g_context->OMSetDepthStencilState(g_ds, 0);
	}
	else
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);
		g_context->OMSetRenderTargets(1, &rtv, nullptr);
	}

	g_queue.clear();
	g_world_occluders.clear();
	g_frame_valid = false;
}

bool IsFrameValid()
{
	return g_frame_valid;
}

const char* OcclusionStatus()
{
	static char buf[192];

	if (!variables::ESP::meshChamsOcclusion)
		std::snprintf(buf, sizeof(buf), "visible check: off");
	else if (g_occ_count_last == 0)
		std::snprintf(buf, sizeof(buf), "visible check: on, no occluders gathered yet");
	else
		std::snprintf(buf, sizeof(buf),
			"visible check: %d occluders out to %.0f studs (%d past 180), %d targets, %d skipped (no drawable geometry)",
			g_occ_count_last, g_occ_reach_last, g_occ_far_last, g_occ_targets_last, g_occ_skipped_last);
	return buf;
}

const char* const* ModeNames()
{
	return k_mode_names;
}

int ModeNameCount()
{
	return (int)_countof(k_mode_names);
}

}
}
}
