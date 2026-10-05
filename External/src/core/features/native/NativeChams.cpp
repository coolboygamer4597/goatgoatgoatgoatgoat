#define IMGUI_DEFINE_MATH_OPERATORS
#include "NativeChams.h"
#include "NativePattern.h"
#include "NativePreparation.h"
#include "NativeOcclusion.h"
#include "NativeDepthCapture.h"
#include "NativeDepthCaptureBytes.h"
#include "../mesh/shader/MeshDxShader.h"

#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include "../../cache/cache.h"
#include "../WeaponVisuals.h"
#include "../mesh/parser/MeshParser.h"
#include "../../app/app.h"
#include "../../globals/globals.h"
#include "../../variables/variables.h"
#include "../mesh/chams/MeshChams.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

namespace App {
bool PassesVisibilityChecks(std::uintptr_t character);
}

#pragma comment(lib, "d3dcompiler.lib")

#ifndef CFG_CALL_TARGET_VALID
#define CFG_CALL_TARGET_VALID 0x00000001
#endif

namespace Cheat {
namespace Visuals {
namespace NativeChams {
namespace {

constexpr int k_slot_create_buf = 3;
constexpr int k_slot_create_ps = 15;
constexpr int k_slot_create_blend = 20;
constexpr int k_slot_create_dss = 21;
constexpr int k_slot_psset = 9;
constexpr int k_slot_psget = 74;
constexpr int k_slot_omget = 89;
constexpr int k_slot_pscbget = 77;
constexpr int k_slot_pscb = 16;
constexpr int k_slot_dssset = 36;
constexpr int k_slot_dssget = 92;
constexpr int k_slot_blendset = 35;
constexpr int k_slot_blendget = 91;
constexpr int k_slot_update = 48;
constexpr int k_slot_present = 8;
constexpr int k_slot_dc_draw = 28;
constexpr int k_vt_cap = 510;

constexpr std::uint32_t k_cmd_ps = 2;
constexpr std::uint32_t k_cmd_buf = 4;
constexpr std::uint32_t k_cmd_blend = 5;
constexpr std::uint32_t k_cmd_dss = 7;

constexpr std::uint32_t k_hr_off = 0x1F0;
constexpr std::uint32_t k_draw_match_off = 0x1F4;
constexpr std::uint32_t k_draw_live_off = 0x1F8;

constexpr std::uint32_t k_draw_calls_off = 0x1FC;
constexpr std::uint32_t k_rdx_ring_off = 0x300;

constexpr std::uint32_t k_body_stage_off = 0x320;
constexpr std::uint32_t k_pending_geoms_off = 0x328;
constexpr std::uint32_t k_body_reader_off = 0x330;
constexpr std::uint32_t k_color_passes_off = 0x338;
constexpr std::uint32_t k_depth_passes_off = 0x33C;
static_assert(k_body_stage_off >= k_rdx_ring_off + 4 * sizeof(std::uint64_t));

constexpr std::uint32_t k_depth_visible_standard = 0x350;
constexpr std::uint32_t k_depth_hidden_standard = 0x358;
constexpr std::uint32_t k_depth_visible_reversed = 0x360;
constexpr std::uint32_t k_depth_hidden_reversed = 0x368;
constexpr std::uint32_t k_scene_visible_dss = 0x370;
constexpr std::uint32_t k_attachment_visible_standard = 0x378;
constexpr std::uint32_t k_attachment_visible_reversed = 0x380;
constexpr std::uint32_t k_viewport_get_fn = 0x388;
constexpr std::uint64_t k_character_attachment = 0x200;
constexpr std::uint32_t k_capture_fn = NativeDepthCapture::function;
constexpr std::uint32_t k_capture_srv_get = NativeDepthCapture::getSrv;
constexpr std::uint32_t k_capture_srv_set = NativeDepthCapture::setSrv;
constexpr std::uint32_t k_capture_dimensions = NativeDepthCapture::dimensions;
constexpr std::uint32_t k_capture_data = NativeDepthCapture::data;

constexpr std::uint32_t k_scene_open_fn = 0x800;
constexpr std::uint32_t k_scene_srv_fn = 0x808;
constexpr std::uint32_t k_scene_get_fn = 0x810;
constexpr std::uint32_t k_scene_set_fn = 0x818;
constexpr std::uint32_t k_scene_slots = 0x820;
constexpr std::uint32_t k_scene_snapshots = 0x880;
constexpr std::uint32_t k_scene_pending = 0x9A0;
constexpr std::uint32_t k_scene_index = 0x9B0;
constexpr std::uint32_t k_scene_active_srv = 0x9B8;
constexpr std::uint32_t k_scene_active_mutex = 0x9C0;
constexpr std::uint32_t k_scene_active = 0x9D0;
static_assert(k_scene_snapshots+3*sizeof(NativeWorldDepth::Snapshot)==k_scene_pending);
static_assert(k_scene_active+sizeof(NativeWorldDepth::Snapshot)<0x1000);
constexpr int k_max_targets = 4096;
constexpr int k_max_ents = 256;
constexpr int k_ent_budget = 2048;

constexpr const char* k_shader_names[] = {
    "Flat", "Lit", "Clay", "Glass", "Crystal", "Ice", "Prism", "Solar Bloom", "Core",
    "Porcelain Atlas", "Midnight Atlas", "Electric Fault", "Shadow Satin",
    "Candy Contours", "Crimson Shards", "Aurora Veil", "Mercury Drift",
    "Nebula Ink", "Lava Lace", "Plasma Weave", "Obsidian Etch",
    "Bubblegum Cells", "Quantum Knots", "Chromatic Smoke", "Prism Mosaic",
    "Silk Vortex", "Frost Veins", "Happy Doodles", "WhatsApp Drift", "Lightning Rivers", "Prismatic Rift", "Liquid Cyan"
};

constexpr float k_pattern_span = 6.0f;
constexpr float k_body_studs = 5.0f;
constexpr float k_units_per_stud_min = 0.3f;
constexpr float k_units_per_stud_max = 4.8f;

constexpr float k_image_bottom = -3.0f;
constexpr float k_image_top = 2.0f;
constexpr float k_image_half_width = 0.95f;

float pattern_seed(std::uint64_t anchor)
{
	std::uint64_t h = anchor * 0x9E3779B97F4A7C15ull;
	h ^= h >> 33;
	h *= 0xFF51AFD7ED558CCDull;
	h ^= h >> 33;
	return static_cast<float>(h & 0xFFFFull) * (1024.0f / 65536.0f);
}

constexpr char k_hlsl_head[] = R"HLSL(
cbuffer Cham : register(b13)
{
    float4 color;
    float4 glow;
    float time;
    int mode;
    float depth_scale;
    float depth_bias;

    float4 pattern;
    float4 camRight;
    float4 camUp;
    float4 camFwd;
    float4 camPos;
    float4 orgPos;
    float4 orgX;
    float4 orgY;
    float4 orgZ;

    float4 frame;

    float4 depth;
    float4 liveView[4];
    float4 drawBody[3];
    float4 bodyStatus;
    float4 worldView[4];
    float4 worldCameraRange;
    float4 worldInfo;
};
Texture2D<float> avatarFreeDepth : register(t12);

struct Out
{
    float4 c : SV_TARGET;
    float z : SV_DEPTH;
};

float3 screen_normal(float4 pos)
{
    float2 g = float2(ddx(pos.w), ddy(pos.w)) / max(abs(pos.w), 1e-6) * 480.0;
    return normalize(float3(g, 1.0));
}

float hash21(float2 p)
{

    int2 cell = int2(floor(p));
    uint h = asuint(cell.x)*0x8da6b343u ^ asuint(cell.y)*0xd8163841u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 16777216.0;
}





float3 hue_rot(float3 c, float h)
{
    float3 k = float3(0.57735, 0.57735, 0.57735);
    return c * cos(h) + cross(k, c) * sin(h) + k * dot(k, c) * (1.0 - cos(h));
}

float3 lit(float3 fill, float3 N, float amb, float k)
{
    return fill * (amb + saturate(dot(N, normalize(float3(0.45, -0.70, 0.55)))) * k);
}

float3 spec(float3 fill, float3 N, float amb, float k, float p)
{
    float3 L = normalize(float3(0.50, -0.80, 0.35));
    float3 H = normalize(L + float3(0.0, 0.0, 1.0));
    return fill * (amb + saturate(dot(N, L)) * k) + pow(saturate(dot(N, H)), p);
}

void add_glow(inout float3 c, inout float a, float mask)
{
    float w = saturate(mask) * glow.a;
    float t = w / (1.0 + w);
    c = lerp(c, glow.rgb, t);
    c += glow.rgb * t * t * 0.75;
    a = saturate(a + t * (1.0 - a) * 0.9);
}

float frag_clip_w(float4 pos)
{

    return isfinite(pos.w) && pos.w > 0.0 ? pos.w : -1.0;
}

float3 frag_world(float4 pos)
{
    float2 ndc = float2(pos.x / max(frame.x, 1.0) * 2.0 - 1.0,
                        1.0 - pos.y / max(frame.y, 1.0) * 2.0);
    float t = frag_clip_w(pos);

    if (!(t > 0.0))
        t = max(depth.w, 1e-3);
    float ax=length(liveView[0].xyz), ay=length(liveView[1].xyz), az=length(liveView[3].xyz);
    if(ax>0.0001 && ay>0.0001 && az>0.0001) {
        float3 r=liveView[0].xyz/ax, u=liveView[1].xyz/ay, f=liveView[3].xyz/az;
        float3 origin=-liveView[0].w/ax*r-liveView[1].w/ay*u-liveView[3].w/az*f;
        return origin+t*(ndc.x/ax*r+ndc.y/ay*u+f/az);
    }
    float3 view = ndc.x * (1.0 / camRight.w) * camRight.xyz
                + ndc.y * (1.0 / camUp.w) * camUp.xyz
                + (1.0 / camFwd.w) * camFwd.xyz;
    return camPos.xyz + t * view;
}

float3 frag_local(float4 pos)
{

    float3 world=frag_world(pos);
    if(bodyStatus.y>1.5)return world;
    if (bodyStatus.x > 0.5) {

        float3 x=float3(drawBody[0].x,drawBody[0].w,drawBody[1].z);
        float3 y=float3(drawBody[0].y,drawBody[1].x,drawBody[1].w);
        float3 z=float3(drawBody[0].z,drawBody[1].y,drawBody[2].x);
        float3 origin=drawBody[2].yzw;
        float3 norms=float3(dot(x,x),dot(y,y),dot(z,z));
        bool valid=all(isfinite(origin)) && all(norms>0.49) && all(norms<1.69)
            && abs(dot(x,y))<0.13 && abs(dot(x,z))<0.13 && abs(dot(y,z))<0.13;
        if (valid) {
            float3 d=world-origin;
            return float3(dot(d,x)*rsqrt(norms.x),dot(d,y)*rsqrt(norms.y),dot(d,z)*rsqrt(norms.z));
        }
    }
    float3 d = world - orgPos.xyz;
    return float3(dot(d, orgX.xyz), dot(d, orgY.xyz), dot(d, orgZ.xyz));
}

float noise3(float3 p)
{
    float3 i = floor(p), f = frac(p);
    f = f*f*(3.0-2.0*f);
    float2 q = i.xy + i.z*float2(37.0,79.0);
    float a = lerp(lerp(hash21(q),hash21(q+float2(1,0)),f.x),
                   lerp(hash21(q+float2(0,1)),hash21(q+1.0),f.x),f.y);
    q += float2(37.0,79.0);
    float b = lerp(lerp(hash21(q),hash21(q+float2(1,0)),f.x),
                   lerp(hash21(q+float2(0,1)),hash21(q+1.0),f.x),f.y);
    return lerp(a,b,f.z);
}
float cloud(float3 p)
{
    return noise3(p)*0.57 + noise3(p*2.03+17.1)*0.29 + noise3(p*4.09+31.7)*0.14;
}
float stroke(float d, float width)
{
    float aa = max(fwidth(d),0.008);
    return 1.0-smoothstep(width-aa,width+aa,abs(d));
}
float bands(float f, float width)
{
    float wave = sin(f*6.2831853);
    return stroke(wave,width);
}
float segment(float2 p,float2 a,float2 b)
{
    float2 q=p-a, v=b-a;
    return length(q-v*saturate(dot(q,v)/max(dot(v,v),0.0001)));
}
float3 palette(float f)
{
    return 0.55+0.45*cos(6.2831853*(f+float3(0.0,0.33,0.67)));
}
)HLSL";

constexpr char k_hlsl_body[] = R"HLSL(
Out ps_main(float4 pos : SV_POSITION, bool frontFace : SV_IsFrontFace)
{

    if (bodyStatus.z > 0.5 && !frontFace) discard;

    if(bodyStatus.z > 0.5 && bodyStatus.w > 0.5) {
        float sceneZ=avatarFreeDepth.Load(int3(int2(pos.xy),0));
        float fragmentZ=pos.z;

        if(bodyStatus.w < 28.0)fragmentZ=round(saturate(fragmentZ)*16777215.0)/16777215.0;
        bool wallHidden=depth.y>0.0?fragmentZ<sceneZ:fragmentZ>sceneZ;
        if(!wallHidden)discard;
    }
    float3 fill = color.rgb;

    float a = 1.0;
    float3 N = screen_normal(pos);
    float rim = saturate(1.0 - saturate(N.z));
    float rp = pow(abs(rim), 2.4);
    float edge = saturate(length(fwidth(N)) * 2.2);
    float3 N5 = normalize(round(N * 5.0) / 5.0);
    float fe5 = saturate(length(fwidth(N5)) * 3.2);
    float seed = isfinite(pattern.y) ? pattern.y : 0.0;
    float density = isfinite(pattern.x) ? max(pattern.x,0.0) : 0.0;
    float3 local = density > 0.0 ? frag_local(pos) : float3(0,0,0);
    float3 p = local * density + float3(seed,seed*0.618,seed*0.317);
    float t = time*(0.85+0.3*frac(seed*0.713))+seed*0.017;
    float2 uv = float2(p.x+p.z*0.37,p.y+p.z*0.23);
    float3 c = fill;
    float gmask = rp * 0.55 + edge * 0.75;

    switch (mode)
    {
    case 0:
        gmask = rp * 0.55 + edge * 0.8;
        break;
    case 1:
        c = lit(fill, N, 0.32, 0.68);
        gmask = pow(abs(rim), 2.0) * 0.7 + edge * 0.55;
        break;
    case 2:
        c = fill * (0.38 + saturate(N.y) * 0.35);
        gmask = edge * 0.7;
        break;
    case 3:
        c = fill * (0.18 + pow(abs(rim), 1.2) * 0.55);
        a *= saturate(0.10 + pow(abs(rim), 1.2) * 0.9);
        gmask = pow(abs(rim), 1.2) * 0.85 + edge * 0.55;
        break;
    case 4:
        c = spec(fill, N, 0.16, 0.45, 64.0);
        a *= saturate(0.18 + rim * 0.7);
        gmask = pow(abs(rim), 1.5) * 0.9 + edge * 0.6;
        break;
    case 5:
        c = fill * (0.22 + saturate(N.y * 0.4 + 0.2));
        a *= saturate(0.30 + rim * 0.55);
        gmask = pow(abs(rim), 1.35) * 0.8 + fe5 * 0.75;
        break;
    case 6:
        c = hue_rot(fill, rim * 4.0 + edge * 2.0);
        a *= saturate(0.20 + rim * 0.65);
        gmask = pow(abs(rim), 1.4) * 0.8;
        break;
    case 7:
    {
        float f=cloud(p*0.85+float3(0,-t*0.22,0));
        float heat=smoothstep(0.32,0.77,f);
        c=lerp(fill*0.12,float3(1.0,0.72,0.17),heat)*lerp(1.0,fill,0.2);
        gmask=heat*0.9;
        break;
    }
    case 8:
        c = fill * (0.12 + pow(abs(rim), 0.8) * 0.2);
        gmask = pow(abs(rim), 0.9) * 0.95 + edge * 0.4;
        break;
    case 9:
    case 10:
    {
        float f=cloud(p*0.58+float3(0,t*0.055,-t*0.025));
        float ink=bands(f*7.0,0.13);
        c=mode==9 ? lerp(float3(0.92,0.92,0.88),fill*0.055,ink)
                  : lerp(fill*0.07,fill*1.35,ink);
        gmask=mode==9 ? ink*0.06 : ink*0.75;
        break;
    }
    case 11:
    case 29:
    {
        float3 q=p+float3(sin(p.y*1.2+t*.31),cos(p.z+t*.24),sin(p.x+t*.19))*.32;
        float f=cloud(q*(mode==11?1.05:0.62));
        float bolt=stroke(f-0.5,0.022), halo=exp(-abs(f-.5)*18.0);
        c=fill*(.045+halo*.45)+lerp(fill,float3(1,1,1),.82)*bolt;
        gmask=bolt*.8+halo*.4;
        break;
    }
    case 12:
    {
        float f=p.x*.65+p.z*.4+sin(p.y*.8+t*.22)*.65+sin(p.y*1.5-t*.12)*.2;
        float fold=.5+.5*sin(f*3.7);
        c=fill*(.04+.5*pow(fold,1.8)); gmask=pow(fold,12.0)*.12;
        break;
    }
    case 13:
    {
        float f=uv.x*.47+sin(uv.y*.9+t*.16)*.48+sin(uv.y*1.6-t*.13)*.17;
        float v=.5+.5*sin(f*6.28318);
        c=lerp(float3(.95,.7,.86),float3(.32,.86,.95),smoothstep(.2,.75,v));
        c=lerp(c,float3(.98,.95,.98),bands(f,.32));
        gmask=.12;
        break;
    }
    case 14:
    case 24:
    {
        float2 q=uv*1.25, cell=floor(q), f=frac(q);
        float tri=step(f.x,f.y), h=hash21(cell+tri*29.0);
        float shimmer=.5+.5*sin(t*.55+h*6.28318);
        c=mode==14 ? float3(.12+.78*h,0.008,0.018)*(.65+.35*shimmer)
                   : palette(h+t*.025)*(.35+.65*shimmer);
        gmask=shimmer*.16;
        break;
    }
    case 15:
    {
        float f=p.x*.5+p.z*.38+sin(p.y*.55+t*.2)+cloud(p*.55)*1.2;
        float curtain=pow(.5+.5*sin(f*6.0-t*.3),3.0);
        c=fill*.06+palette(f*.15+t*.035)*curtain; gmask=curtain*.65;
        break;
    }
    case 16:
    {
        float f=cloud(p*.65+float3(t*.04,-t*.07,t*.03));
        float shine=pow(.5+.5*sin(f*17.0+t*.18),7.0);
        c=fill*(.18+f*.4+shine*.65); gmask=shine*.35;
        break;
    }
    case 17:
    case 23:
    {
        float f=cloud(p*.6+float3(t*.055,-t*.035,t*.02));
        float f2=cloud(p*.8+f*1.8-float3(0,t*.045,0));
        c=palette(f2*1.1+t*.018)*(.14+smoothstep(.25,.8,f)*.88);
        if(mode==17) c=lerp(c*.24,c,smoothstep(.38,.62,f2));
        gmask=f2*.4;
        break;
    }
    case 18:
    {
        float f=cloud(p*.95+float3(0,-t*.085,0));
        float crack=stroke(f-.51,.036);
        c=float3(.07,.013,.025)+lerp(float3(.9,.13,.025),float3(1,.82,.12),crack)*crack;
        gmask=crack; break;
    }
    case 19:
    {
        float warp=cloud(p*.6+float3(t*.09,-t*.04,0));
        float threads=sin(uv.x*5.0+warp*6.0+t*.55)*sin(uv.y*4.3-warp*4.0-t*.35);
        float arc=pow(saturate(1.0-abs(threads)),12.0);
        c=fill*(.12+.28*warp)+lerp(fill,float3(1,1,1),.6)*arc;
        gmask=arc*.8;break;
    }
    case 20:
    {
        float f=cloud(p*.7+float3(0,t*.035,0));
        float contour=stroke(sin(f*39.0+uv.x*.45),.10);
        float sweep=pow(.5+.5*sin(uv.x*.7+uv.y*1.3-t*.6),12.0);
        c=fill*(.07+contour*(.2+.7*sweep))+float3(1,1,1)*contour*sweep*.28;
        gmask=contour*sweep*.65;break;
    }
    case 21:
    {
        float f=cloud(p*.9+float3(t*.05,sin(t*.15)*.3,0));
        float bubble=stroke(f-.51,.045);
        c=lerp(float3(.19,.045,.3),float3(.98,.36,.72),smoothstep(.35,.66,f));
        c+=bubble*.4; gmask=bubble*.4; break;
    }
    case 22:
    {
        float f=sin(p.x*1.8+t*.24)*cos(p.y*1.7)+sin(p.y*1.7)*cos(p.z*1.9-t*.2)+sin(p.z*1.9)*cos(p.x*1.8);
        float knot=stroke(f,.15);
        c=fill*.08+palette(f*.2+t*.02)*knot; gmask=knot*.65; break;
    }
    case 25:
    {
        float2 q=uv*.65;
        float bend=cloud(p*.4+float3(t*.055,0,-t*.025));
        float wave=.5+.5*sin(q.x*3.1+sin(q.y*2.4-t*.25)*2.6+bend*8.0);
        float sheen=pow(wave,7.0);
        c=fill*(.15+wave*.5)+lerp(fill,float3(1,1,1),.5)*sheen*.65;
        gmask=sheen*.35;break;
    }
    case 26:
    {
        float f=cloud(p*.95+float3(t*.015,-t*.03,0));
        float veins=stroke(sin(f*24.0+sin(p.y*.8)),.09);
        float branches=stroke(sin(f*47.0+p.x*.35),.065)*.35;
        float pulse=.6+.4*sin(f*9.0-t*.65);
        c=fill*(.17+f*.3)+lerp(fill,float3(1,1,1),.65)*saturate(veins+branches)*pulse;
        gmask=veins*pulse*.5;break;
    }
    case 30:
    {
        float f=cloud(p*.5+float3(t*.04,0,-t*.035));
        float ribbon=pow(.5+.5*sin(uv.x*1.6-uv.y*1.1+f*9.0-t*.45),9.0);
        float streak=stroke(sin(uv.y*5.0+f*11.0+t*.2),.08);
        c=fill*.1+palette(f*.6+uv.y*.05+t*.025)*ribbon+fill*streak*.32;
        gmask=ribbon*.65+streak*.2;break;
    }
    case 31:
    {

        float3 q=p*.68;
        q+=float3(cloud(q+float3(t*.055,0,0)),cloud(q+float3(6,3,-t*.045)),cloud(q+float3(1,t*.04,9)))*1.8;
        float flow=cloud(q*1.7+float3(0,-t*.075,t*.025));
        float ribbon=.5+.5*sin(flow*18.0+q.y*.85-t*.32);
        float edge=pow(saturate(ribbon),3.0);
        float highlight=pow(saturate(ribbon),13.0);
        c=lerp(fill*.025,fill*1.1,edge)+float3(1,1,1)*highlight*.7;
        gmask=edge*.6+highlight*.4;break;
    }
    case 27:
    case 28:
    {

        float2 cell=floor(uv*.75), q=frac(uv*.75)-.5;
        float h=hash21(cell);
        q.y+=sin(t*.6+h*6.28)*.045;
        float disc=1.0-smoothstep(.31,.33,length(q));
        float eyes=stroke(length(q-float2(-.105,.065)),.025)+stroke(length(q-float2(.105,.065)),.025);
        if(mode==27) {
            float smile=stroke(length(q-float2(0,.045))-.18,.017)*step(q.y,-.015);
            c=lerp(fill*.06,float3(1,.82,.15),disc); c*=1.0-saturate(eyes+smile)*disc*.92;
        } else {
            float ring=stroke(length(q)-.29,.018);
            float tail=1.0-smoothstep(.016,.033,segment(q,float2(-.19,-.2),float2(-.28,-.31)));
            float phone=min(segment(q,float2(-.12,.13),float2(-.06,-.06)),segment(q,float2(-.06,-.06),float2(.13,-.12)));
            phone=min(phone,segment(q,float2(-.14,.14),float2(-.055,.14)));
            phone=min(phone,segment(q,float2(.12,-.14),float2(.12,-.05)));
            float icon=saturate(ring+tail+stroke(phone,.028));
            c=lerp(float3(.025,.29,.13),float3(1,1,1),icon);
        }
        gmask=disc*.15; break;
    }
    }

    if (mode >= 9 && mode != 10 && mode != 11 && mode != 12 && mode != 16 && mode != 20 && mode != 29)
        c *= lerp(float3(1,1,1),fill*1.5,0.25);

    if (density <= 0.0 && (mode == 7 || mode >= 9)) { c=fill*.65; gmask=0.0; }
    add_glow(c,a,gmask);
    clip(color.a - 0.00001);
    Out o;
    o.c=float4(saturate(c),saturate(a)*saturate(color.a));
    o.z=saturate(pos.z*depth_scale+depth_bias);
    return o;
}
)HLSL";
constexpr char k_hlsl_styles[] = "";

#pragma pack(push, 8)
struct RemoteState {
	std::uint32_t enabled;
	std::uint32_t cmd;
	std::uint32_t ready;
	std::uint32_t ngeom;
	std::uint64_t device;
	std::uint64_t ctx;
	std::uint64_t orig_present;
	std::uint64_t orig_draw;
	std::uint64_t vs;
	std::uint64_t ps;
	std::uint64_t il;
	std::uint64_t cb;
	std::uint64_t cb1;
	std::uint64_t blend;
	std::uint64_t raster;
	std::uint64_t dss;
	std::uint64_t last_rtv;
	std::uint64_t last_dsv;
	std::uint64_t bytecode;
	std::uint64_t bytecode_len;
	std::uint64_t out_res;
	std::uint64_t init_sys;
	std::uint64_t geoms;
	std::uint32_t entered;
	std::uint32_t draws;
	std::uint32_t creates;
	std::uint32_t busy;
	std::uint32_t stage;
	std::uint32_t il_count;
	float frame[24];
	std::uint64_t fn_create_vs;
	std::uint64_t fn_create_ps;
	std::uint64_t fn_create_il;
	std::uint64_t fn_create_buf;
	std::uint64_t fn_create_blend;
	std::uint64_t fn_create_raster;
	std::uint64_t fn_create_dss;
	std::uint64_t fn_vsset;
	std::uint64_t fn_psset;
	std::uint64_t fn_ilset;
	std::uint64_t fn_vbset;
	std::uint64_t fn_ibset;
	std::uint64_t fn_topo;
	std::uint64_t fn_omset;
	std::uint64_t fn_omget;
	std::uint64_t fn_blendset;
	std::uint64_t fn_dssset;
	std::uint64_t fn_rsset;
	std::uint64_t fn_vscb;
	std::uint64_t fn_pscb;
	std::uint64_t fn_update;
	std::uint64_t fn_pscbget;
	std::uint64_t dc;
	std::uint64_t fn_psget;
	std::uint64_t fn_dssget;
	std::uint64_t fn_blendget;
};
#pragma pack(pop)

static_assert(offsetof(RemoteState, cmd) == 0x04);
static_assert(offsetof(RemoteState, ngeom) == 0x0C);
static_assert(offsetof(RemoteState, device) == 0x10);
static_assert(offsetof(RemoteState, orig_present) == 0x20);
static_assert(offsetof(RemoteState, orig_draw) == 0x28);
static_assert(offsetof(RemoteState, bytecode) == 0x80);
static_assert(offsetof(RemoteState, out_res) == 0x90);
static_assert(offsetof(RemoteState, geoms) == 0xA0);
static_assert(offsetof(RemoteState, entered) == 0xA8);
static_assert(offsetof(RemoteState, busy) == 0xB4);
static_assert(offsetof(RemoteState, il_count) == 0xBC);
static_assert(offsetof(RemoteState, frame) == 0xC0);
static_assert(offsetof(RemoteState, fn_create_vs) == 0x120);
static_assert(offsetof(RemoteState, fn_omget) == 0x190);
static_assert(offsetof(RemoteState, fn_vscb) == 0x1B0);
static_assert(offsetof(RemoteState, fn_pscb) == 0x1B8);
static_assert(offsetof(RemoteState, fn_update) == 0x1C0);
static_assert(offsetof(RemoteState, fn_pscbget) == 0x1C8);
static_assert(offsetof(RemoteState, dc) == 0x1D0);
static_assert(offsetof(RemoteState, fn_psget) == 0x1D8);
static_assert(offsetof(RemoteState, fn_dssget) == 0x1E0);
static_assert(offsetof(RemoteState, fn_blendget) == 0x1E8);
static_assert(sizeof(RemoteState) == 0x1F0);

struct Patch {
	std::uintptr_t at = 0;
	std::uint64_t old = 0;
};

struct Target {
	std::uint64_t geom;
	NativePattern::Material mat;

	std::uint64_t anchorPrimitive;
	std::uint64_t viewMatrix;
	std::uint32_t startIndex, baseVertex, indexCount, topology;
	std::uint64_t liveBody;
    float occludedColor[4];
    std::int32_t occludedMode;
    std::uint32_t occlusion;
    std::uint64_t visibleDepthState, occludedDepthState;
    std::uint64_t weapon=0;
    std::uint64_t renderEntity=0;
};
static_assert(offsetof(Target, mat) == 8, "the draw thunk copies the cbuffer from geom + 8");
static_assert(sizeof(NativePattern::Material) % 16 == 0, "cbuffer size must stay 16-byte aligned");
static_assert(sizeof(Target) == 8 + sizeof(NativePattern::Material) + 96);

struct BodyCache {
    std::uint64_t source;
    std::uint32_t frame = 0xFFFFFFFFu;
    std::uint32_t valid = 0;
    float cframe[12]{};
};
static_assert(sizeof(BodyCache)==64);

struct Hook {
	std::vector<Target> targets;
	std::uintptr_t state = 0;
	std::uintptr_t geoms = 0;
	std::uintptr_t cave = 0;
	std::size_t cave_n = 0;
	std::uintptr_t draw_cave = 0;
	std::size_t draw_cave_n = 0;
	std::uintptr_t present_thunk = 0;
	std::uintptr_t create_stub = 0;
	std::uintptr_t draw_thunk = 0;
	std::uintptr_t device = 0;
	std::uintptr_t ctx = 0;
	std::uintptr_t dc = 0;
	std::uintptr_t dc_vt = 0;
	std::uintptr_t swap = 0;
	std::uintptr_t swap_vt = 0;
	std::uintptr_t vt_mem = 0;
	std::uintptr_t orig_present = 0;
	std::uintptr_t orig_draw = 0;
	std::uintptr_t module = 0;
	std::uintptr_t ps_dxbc = 0;
	std::size_t ps_len = 0;
	Patch patches[16]{};
	int npatch = 0;
	int stage = 0;
	int ntarget = 0;
	int slot = 0;
	int nnode = 0;
	int nent = 0;
	int ndrop = 0;
	int nbind = 0;
	int nswvt = 0;
	int ndcvt = 0;
	bool hooked = false;
	bool dc_hooked = false;

	std::uintptr_t dc_arm = 0;
	bool dc_armed = false;
	bool depth_rev = false;
	bool cmd_wait = false;
	bool draw_live = false;
	std::chrono::steady_clock::time_point first_match{};
	bool logged = false;
	bool occl_off = false;
	std::uint8_t occl = 0;
	std::uintptr_t occl_addr = 0;
	int occl_reverts = 0;
};

Hook g{};
auto g_fail = std::chrono::steady_clock::time_point{};
const char* g_why = nullptr;
int g_npick = 0;
int g_nunk = 0;
int g_nfc = 0;
int g_nbind = 0;

int g_nbody = 0;
int g_nobody = 0;
int g_nref = 0;
int g_nref_bad = 0;
bool g_cam_ok = false;
float g_density = 0.0f;

bool addr_ok(std::uintptr_t a)
{
	return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
}

bool address_is_heap(std::uintptr_t a)
{
	if (!addr_ok(a) || !memory || !memory->GetHandle())
		return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQueryEx(memory->GetHandle(), (void*)a, &mbi, sizeof(mbi)))
		return false;
	if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
		return false;
	return mbi.Type == MEM_PRIVATE;
}

void append_u64(std::vector<std::uint8_t>& c, std::uint64_t v)
{
	const auto* b = (const std::uint8_t*)&v;
	c.insert(c.end(), b, b + 8);
}

void append_call_rax(std::vector<std::uint8_t>& c)
{
	c.insert(c.end(), { 0xFF, 0xD0 });
}

std::size_t append_jcc32(std::vector<std::uint8_t>& c, std::uint8_t cc)
{
	c.push_back(0x0F);
	c.push_back((std::uint8_t)(0x80 | cc));
	c.insert(c.end(), 4, 0);
	return c.size() - 4;
}

void patch_rel32(std::vector<std::uint8_t>& c, std::size_t at, std::size_t dest)
{
	const std::int32_t rel = (std::int32_t)((std::ptrdiff_t)dest - (std::ptrdiff_t)(at + 4));
	std::memcpy(c.data() + at, &rel, 4);
}

bool mark_cfg(std::uintptr_t t)
{
	static FARPROC proc = nullptr;
	if (!proc)
	{
		const char* mods[] = {
			"kernelbase.dll", "kernel32.dll",
			"api-ms-win-core-memory-l1-1-3.dll"
		};
		for (auto* m : mods)
		{
			HMODULE h = GetModuleHandleA(m);
			if (!h)
				h = LoadLibraryA(m);
			if (!h)
				continue;
			proc = GetProcAddress(h, "SetProcessValidCallTargets");
			if (proc)
				break;
		}
	}
	if (!proc || !memory->GetHandle())
		return false;
	SYSTEM_INFO si{};
	GetSystemInfo(&si);
	const std::size_t page = si.dwPageSize ? (std::size_t)si.dwPageSize : 0x1000u;
	struct Info {
		ULONG_PTR Offset;
		ULONG Flags;
	} info{};
	info.Offset = t & (page - 1);
	info.Flags = CFG_CALL_TARGET_VALID;
	using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
	return ((Fn)proc)(
		memory->GetHandle(),
		(void*)(t & ~((std::uintptr_t)page - 1)),
		page, 1, &info) != 0;
}

bool write_exec(std::uintptr_t at, const void* data, std::size_t n)
{
	if (!addr_ok(at) || !data || !n)
		return false;
	DWORD old = 0;
	const bool changed = memory->Protect(at, n, PAGE_EXECUTE_READWRITE, &old);
	const bool ok = memory->WriteRaw(at, data, n) == n;
	if (changed)
		memory->Protect(at, n, old, nullptr);
	return ok;
}

bool exec_prot(DWORD p)
{
	const DWORD b = p & 0xFF;
	return b == PAGE_EXECUTE || b == PAGE_EXECUTE_READ ||
		b == PAGE_EXECUTE_READWRITE || b == PAGE_EXECUTE_WRITECOPY;
}

std::uintptr_t find_dll_cave(std::size_t need, std::uintptr_t ignore)
{
	static const wchar_t* pref[] = {
		L"winsta.dll", L"win32u.dll", L"uxtheme.dll", L"dwmapi.dll",
		L"msctf.dll", L"TextInputFramework.dll", L"CoreMessaging.dll",
		L"user32.dll", L"imm32.dll", L"gdi32.dll", L"ole32.dll", L"combase.dll",
	};
	for (auto* name : pref)
	{
		const std::uintptr_t mb = memory->GetModuleBase(name);
		if (!mb)
			continue;
		IMAGE_DOS_HEADER dos{};
		if (memory->ReadRaw(mb, &dos, sizeof(dos)) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE)
			continue;
		IMAGE_NT_HEADERS64 nt{};
		if (memory->ReadRaw(mb + (std::uintptr_t)dos.e_lfanew, &nt, sizeof(nt)) != sizeof(nt)
			|| nt.Signature != IMAGE_NT_SIGNATURE)
			continue;
		const std::size_t ms = nt.OptionalHeader.SizeOfImage;
		if (ms < 0x2000)
			continue;
		std::uintptr_t addr = mb + 0x1000;
		const std::uintptr_t to = mb + ms;
		MEMORY_BASIC_INFORMATION mbi{};
		while (addr < to &&
			VirtualQueryEx(memory->GetHandle(), (void*)addr, &mbi, sizeof(mbi)))
		{
			const auto rb = (std::uintptr_t)mbi.BaseAddress;
			const auto rs = (std::size_t)mbi.RegionSize;
			const std::uintptr_t next = rb + rs;
			if (next <= addr)
				break;
			const bool usable = mbi.State == MEM_COMMIT
				&& !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
				&& exec_prot(mbi.Protect);
			if (usable && rs >= need)
			{
				std::vector<std::uint8_t> buf(rs);
				if (memory->ReadRaw(rb, buf.data(), rs) == rs)
				{
					std::size_t run = 0;
					for (std::size_t i = 0; i < rs; ++i)
					{
						if (buf[i] == 0x00 || buf[i] == 0xCC)
							++run;
						else
							run = 0;
						if (run >= need)
						{
							const std::uintptr_t start = rb + i + 1 - run;
							const std::uintptr_t aligned = (start + 0x0F) & ~std::uintptr_t(0x0F);
							if (ignore && aligned == ignore)
								continue;
							if (aligned + need > rb + rs)
								continue;

							if (g.cave && aligned < g.cave + g.cave_n && g.cave < aligned + need)
								continue;
							if (g.draw_cave && aligned < g.draw_cave + g.draw_cave_n && g.draw_cave < aligned + need)
								continue;
							return aligned;
						}
					}
				}
			}
			addr = next;
		}
	}
	return 0;
}

void fail(const char* why)
{
	if (g_why == why)
		return;
	g_why = why;
	std::printf("[NativeChams] fail: %s\n", why);
}

std::uintptr_t module_base()
{
	return memory->get_module_address();
}

bool d3d11_loaded_locally()
{
	static const bool ok = GetModuleHandleW(L"d3d11.dll") != nullptr;
	return ok;
}

std::uintptr_t visual_engine()
{
	const std::uintptr_t base = module_base();
	if (!base)
		return 0;
	const std::uint64_t ve = memory->Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
	return memory->IsValid(ve) ? (std::uintptr_t)ve : 0;
}

std::uintptr_t render_view()
{
	const std::uintptr_t ve = visual_engine();
	if (!ve)
		return 0;
	const std::uint64_t rv = memory->Read<std::uint64_t>(ve + Offsets::VisualEngine::RenderView);
	return memory->IsValid(rv) ? (std::uintptr_t)rv : 0;
}

bool want_reverse()
{
	const std::uintptr_t ve = visual_engine();
	if (!ve)
		return g.depth_rev;
	float m[16]{};
	if (memory->ReadRaw(ve + Offsets::VisualEngine::ViewMatrix, m, sizeof(m)) != sizeof(m))
		return g.depth_rev;
	const float zl = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
	const float wl = std::sqrt(m[12] * m[12] + m[13] * m[13] + m[14] * m[14]);
	if (wl < 1e-4f)
		return g.depth_rev;
	return zl < wl * 0.5f;
}

struct ModRange {
	std::uintptr_t base = 0;
	std::uintptr_t end = 0;
	bool ok() const { return base && end > base; }
	bool has(std::uintptr_t p) const { return ok() && p >= base && p < end; }
};

const ModRange& remote_module_range(const wchar_t* name)
{
	struct Entry { const wchar_t* name; ModRange r; bool done; };
	static Entry s_cache[4] = {};
	static int s_n = 0;
	static DWORD cachedPid = 0;
	if (cachedPid != memory->get_process_id()) {
		cachedPid = memory->get_process_id();
		s_n = 0;
		for (auto& entry : s_cache) entry = {};
	}
	for (int i = 0; i < s_n; ++i)
		if (_wcsicmp(s_cache[i].name, name) == 0) {
			if (s_cache[i].r.ok()) return s_cache[i].r;
			s_cache[i] = s_cache[--s_n];
			break;
		}
	if (s_n >= 4)
	{
		static ModRange empty;
		return empty;
	}
	Entry& e = s_cache[s_n++];
	e.name = name;
	e.done = true;
	e.r.base = (std::uintptr_t)memory->GetModuleBase(name);
	if (e.r.base)
	{
		IMAGE_DOS_HEADER dos{};
		IMAGE_NT_HEADERS64 nt{};
		if (memory->ReadRaw(e.r.base, &dos, sizeof(dos)) == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
			memory->ReadRaw(e.r.base + (std::uintptr_t)dos.e_lfanew, &nt, sizeof(nt)) == sizeof(nt) &&
			nt.Signature == IMAGE_NT_SIGNATURE)
			e.r.end = e.r.base + nt.OptionalHeader.SizeOfImage;
		else
			e.r.base = 0;
	}
	return e.r;
}

std::uintptr_t derive_vtable_from_rtti(std::uintptr_t modBase, const char* tdName)
{
	if (!modBase || !tdName)
		return 0;

	std::uintptr_t dataVa = 0, rdataVa = 0;
	std::size_t dataSize = 0, rdataSize = 0;
	IMAGE_DOS_HEADER dos{};
	IMAGE_NT_HEADERS64 nt{};
	if (memory->ReadRaw(modBase, &dos, sizeof(dos)) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	if (memory->ReadRaw(modBase + (std::uintptr_t)dos.e_lfanew, &nt, sizeof(nt)) != sizeof(nt) || nt.Signature != IMAGE_NT_SIGNATURE)
		return 0;
	if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
        return 0;
    std::vector<IMAGE_SECTION_HEADER> sections(nt.FileHeader.NumberOfSections);
    const auto sectionAt = modBase + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader)
        + nt.FileHeader.SizeOfOptionalHeader;
    const auto sectionBytes = sections.size() * sizeof(IMAGE_SECTION_HEADER);
    if (memory->ReadRaw(sectionAt, sections.data(), sectionBytes) != sectionBytes)
        return 0;
    for (const auto& section : sections)
    {
        const auto* sec = &section;
		if (std::memcmp(sec->Name, ".data", 5) == 0 && !dataVa)
		{
			dataVa = modBase + sec->VirtualAddress;
			dataSize = sec->Misc.VirtualSize;
		}
		else if (std::memcmp(sec->Name, ".rdata", 6) == 0 && !rdataVa)
		{
			rdataVa = modBase + sec->VirtualAddress;
			rdataSize = sec->Misc.VirtualSize;
		}
	}
	if (!dataVa || !rdataVa)
		return 0;
	if (dataSize > 96ull << 20)
		dataSize = 96ull << 20;
	if (rdataSize > 96ull << 20)
		rdataSize = 96ull << 20;

	const std::size_t nameLen = std::strlen(tdName);

	std::uintptr_t td = 0;
	{
		constexpr std::size_t kChunk = 8ull << 20;
		std::vector<std::uint8_t> buf((std::min)(kChunk, dataSize));
		for (std::size_t off = 0; off < dataSize && !td; off += kChunk - nameLen - 16)
		{
			std::size_t n = (std::min)(kChunk, dataSize - off);
			if (n < nameLen + 16)
				break;
			buf.resize(n);
			if (memory->ReadRaw(dataVa + off, buf.data(), n) != n)
				break;
			for (std::size_t i = 0; i + nameLen + 1 <= n; ++i)
				if (buf[i] == tdName[0] && std::memcmp(&buf[i], tdName, nameLen) == 0 && buf[i + nameLen] == 0)
				{
					td = dataVa + off + i - 0x10;
					break;
				}
		}
	}
	if (!td)
		return 0;

	std::uintptr_t col = 0;
	const std::uint32_t tdRva = (std::uint32_t)(td - modBase);
	{
		constexpr std::size_t kChunk = 8ull << 20;
		std::vector<std::uint8_t> buf((std::min)(kChunk, rdataSize));
		for (std::size_t off = 0; off < rdataSize && !col; off += kChunk - 16)
		{
			std::size_t n = (std::min)(kChunk, rdataSize - off);
			if (n < 32)
				break;
			buf.resize(n);
			if (memory->ReadRaw(rdataVa + off, buf.data(), n) != n)
				break;
			for (std::size_t i = 0; i + 4 <= n - 4; i += 4)
			{
				std::uint32_t v;
				std::memcpy(&v, &buf[i], 4);
				if (v != tdRva)
					continue;
				const std::uintptr_t cand = rdataVa + off + i - 0x0C;
				if (cand < rdataVa)
					continue;
				std::uint32_t sig = 0, offSelf = 0;
				if (memory->ReadRaw(cand, &sig, 4) != 4 || sig != 1)
					continue;
				if (memory->ReadRaw(cand + 0x14, &offSelf, 4) != 4)
					continue;
				if (modBase + offSelf == cand && memory->Read<std::uint32_t>(cand + 4) == 0)
				{
					col = cand;
					break;
				}
			}
		}
	}
	if (!col)
		return 0;

	{
		constexpr std::size_t kChunk = 8ull << 20;
		std::vector<std::uint8_t> buf((std::min)(kChunk, rdataSize));
		for (std::size_t off = 0; off < rdataSize; off += kChunk - 16)
		{
			std::size_t n = (std::min)(kChunk, rdataSize - off);
			if (n < 16)
				break;
			buf.resize(n);
			if (memory->ReadRaw(rdataVa + off, buf.data(), n) != n)
				break;
			for (std::size_t i = 0; i + 8 <= n; i += 8)
			{
				std::uint64_t p;
				std::memcpy(&p, &buf[i], 8);
				if (p == col)
					return rdataVa + off + i + 8;
			}
		}
	}
	return 0;
}

struct WrapperVts {
	std::uintptr_t dev = 0;
	std::uintptr_t ctx = 0;
	bool ok() const { return dev && ctx; }
};

WrapperVts resolve_wrapper_vts()
{
	static WrapperVts s_vts;
	static bool s_done = false;
	static DWORD cachedPid = 0;
	if (cachedPid != memory->get_process_id()) {
		cachedPid = memory->get_process_id(); s_done = false; s_vts = {};
	}
	if (s_done)
		return s_vts;
	const ModRange& rbx = remote_module_range(L"RobloxPlayerBeta.exe");
	if (!rbx.ok())
		return s_vts;
	s_vts.dev = derive_vtable_from_rtti(rbx.base, ".?AVDeviceD3D11@Graphics@RBX@@");
	s_vts.ctx = derive_vtable_from_rtti(rbx.base, ".?AVDeviceContextD3D11@Graphics@RBX@@");

	if (!rbx.has(s_vts.dev) || !rbx.has(s_vts.ctx))
	{
		s_vts.dev = 0;
		s_vts.ctx = 0;
	}
	s_done = s_vts.ok();
	return s_vts;
}

struct ScanDiag {
	int stage = 0;
	std::uintptr_t wrap = 0;
};
ScanDiag g_diag;

struct LocalD3dRefs {
	bool tried = false;
	std::uintptr_t localD3dBase = 0;
	std::uintptr_t devVtRva = 0;
	std::uintptr_t ctxFnRva[24] = {};
};
LocalD3dRefs g_localRefs;

void ensure_local_d3d_refs()
{
	if (g_localRefs.tried)
		return;
	g_localRefs.tried = true;
	g_localRefs.localD3dBase = (std::uintptr_t)GetModuleHandleW(L"d3d11.dll");
	if (!g_localRefs.localD3dBase)
		return;
	ID3D11Device* dev = nullptr;
	ID3D11DeviceContext* ctx = nullptr;
	IDXGISwapChain* swap = nullptr;
	HWND dummy = CreateWindowExA(0, "STATIC", "", 0, 0, 0, 1, 1,
		nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
	DXGI_SWAP_CHAIN_DESC scd{};
	scd.BufferDesc.Width = 8;
	scd.BufferDesc.Height = 8;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.SampleDesc.Count = 1;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.OutputWindow = dummy;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &scd,
		&swap, &dev, nullptr, &ctx)) && dev && ctx)
	{
		const std::uintptr_t devVt = *(std::uintptr_t*)dev;
		if (devVt >= g_localRefs.localD3dBase && devVt < g_localRefs.localD3dBase + 0x400000)
			g_localRefs.devVtRva = devVt - g_localRefs.localD3dBase;
		const std::uintptr_t ctxVt = *(std::uintptr_t*)ctx;
		if (addr_ok(ctxVt))
		{
			int n = 0;
			for (int i = 0; i < 24; ++i)
			{
				const std::uintptr_t fn = ((std::uintptr_t*)ctxVt)[i];
				if (fn >= g_localRefs.localD3dBase && fn < g_localRefs.localD3dBase + 0x400000)
				{
					g_localRefs.ctxFnRva[i] = fn - g_localRefs.localD3dBase;
					++n;
				}
			}
			if (n < 8)
				for (int i = 0; i < 24; ++i)
					g_localRefs.ctxFnRva[i] = 0;
		}
	}
	if (dev) dev->Release();
	if (ctx) ctx->Release();
	if (swap) swap->Release();
	if (dummy) DestroyWindow(dummy);
}

bool find_d3d()
{
	if (g.device && memory->IsValid(g.device) && g.ctx && memory->IsValid(g.ctx) &&
		g.swap && memory->IsValid(g.swap) && g.dc && memory->IsValid(g.dc))
		return true;
	g_diag = {};
	const ModRange& d3d = remote_module_range(L"d3d11.dll");
	const ModRange& dxgi = remote_module_range(L"dxgi.dll");
	if (!d3d.ok() || !dxgi.ok())
		return false;

	const std::uintptr_t rv = render_view();
	if (!rv || !memory->IsValid(rv))
	{
		g_diag.stage = 1;
		return false;
	}
	const WrapperVts vts = resolve_wrapper_vts();
	if (!vts.ok())
	{
		g_diag.stage = 2;
		return false;
	}

	std::uintptr_t wrap = 0;
	{
		std::uint8_t hbuf[0x400]{};
		if (memory->ReadRaw(rv, hbuf, sizeof(hbuf)) == sizeof(hbuf))
		{
			for (std::size_t off = 0; off + 8 <= sizeof(hbuf); off += 8)
			{
				std::uint64_t p;
				std::memcpy(&p, hbuf + off, 8);
				if (!addr_ok((std::uintptr_t)p) || (p & 7))
					continue;
				if (memory->Read<std::uint64_t>(p) == vts.dev)
				{
					wrap = (std::uintptr_t)p;
					g_diag.wrap = wrap;
					break;
				}
			}
		}
	}
	if (!wrap)
	{
		g_diag.stage = 3;
		return false;
	}

	std::uint8_t w[0x300]{};
	if (memory->ReadRaw(wrap, w, sizeof(w)) != sizeof(w))
	{
		g_diag.stage = 3;
		return false;
	}
	auto body_ptr = [&](std::size_t off) -> std::uintptr_t
	{
		if (off + 8 > sizeof(w))
			return 0;
		std::uint64_t p;
		std::memcpy(&p, w + off, 8);
		return addr_ok((std::uintptr_t)p) && !(p & 7) ? (std::uintptr_t)p : 0;
	};
	auto has_vt_in = [&](std::uintptr_t obj, const ModRange& r) -> bool
	{
		if (!obj)
			return false;
		const std::uintptr_t vt = (std::uintptr_t)memory->Read<std::uint64_t>(obj);
		return r.has(vt);
	};

	std::uintptr_t dev = 0, swap = 0, dctx = 0;
	ensure_local_d3d_refs();
	auto is_real_device = [&](std::uintptr_t obj) -> bool
	{
		if (!obj || !g_localRefs.devVtRva)
			return false;
		const std::uintptr_t vt = (std::uintptr_t)memory->Read<std::uint64_t>(obj);
		return d3d.has(vt) && (std::uintptr_t)(vt - d3d.base) == g_localRefs.devVtRva;
	};
	const ModRange& rbxRange = remote_module_range(L"RobloxPlayerBeta.exe");
	auto is_real_context = [&](std::uintptr_t obj) -> bool
	{

		if (!obj || !addr_ok((std::uint64_t)obj))
			return false;
		const std::uintptr_t vt = (std::uintptr_t)memory->Read<std::uint64_t>(obj);
		if (!addr_ok((std::uint64_t)vt))
			return false;

		if (rbxRange.has((std::uint64_t)vt) || !address_is_heap(vt))
			return false;
		if (!g_localRefs.ctxFnRva[0])
			return true;
		int matched = 0, considered = 0;
		for (int i = 0; i < 24; ++i)
		{
			if (!g_localRefs.ctxFnRva[i])
				continue;
			++considered;
			const std::uintptr_t fn = (std::uintptr_t)memory->Read<std::uint64_t>(vt + (std::size_t)i * 8);
			if (d3d.has(fn) && (std::uintptr_t)(fn - d3d.base) == g_localRefs.ctxFnRva[i])
				++matched;
		}
		return considered >= 8 && matched >= 8;
	};

	dev = body_ptr(Offsets::DeviceD3D11Gfx::DevicePtr);
	swap = body_ptr(Offsets::DeviceD3D11Gfx::SwapChainPtr);
	dctx = body_ptr(Offsets::DeviceD3D11Gfx::ContextObj);
	if (dev && !is_real_device(dev))
		dev = 0;
	if (swap && !addr_ok((std::uintptr_t)memory->Read<std::uint64_t>(swap)))
		swap = 0;
	for (std::size_t off = 8; off + 8 <= sizeof(w); off += 8)
	{
		const std::uintptr_t p = body_ptr(off);
		if (!p)
			continue;
		if (!dev && is_real_device(p))
			dev = p;
		else if (!swap && has_vt_in(p, dxgi))
			swap = p;
		else if (!dctx && memory->Read<std::uint64_t>(p) == (std::uint64_t)vts.ctx)
			dctx = p;
	}
	if (!dev)
	{
		g_diag.stage = 4;
		return false;
	}
	if (!dctx)
	{
		g_diag.stage = 5;
		return false;
	}

	std::uintptr_t ctx = (std::uintptr_t)memory->Read<std::uint64_t>(
		dctx + Offsets::DeviceContextD3D11::RealContextPtr);
	if (!ctx || !is_real_context(ctx))
	{

		const std::uintptr_t again = (std::uintptr_t)memory->Read<std::uint64_t>(
			dctx + Offsets::DeviceContextD3D11::RealContextPtr);
		if (again && again != ctx && is_real_context(again))
			ctx = again;
		else
		{
			g_diag.stage = 6;
			return false;
		}
	}		g.device = dev;
		g.ctx = ctx;
		g.swap = swap;
		g.dc = dctx;
		return true;
	}

std::uintptr_t vt_fn(std::uintptr_t obj, int slot)
{
	const std::uintptr_t vt = (std::uintptr_t)memory->Read<std::uint64_t>(obj);
	if (!addr_ok(vt))
		return 0;
	const std::uintptr_t fn = (std::uintptr_t)memory->Read<std::uint64_t>(vt + (std::size_t)slot * 8);
	return addr_ok(fn) ? fn : 0;
}

bool compile(const char* entry, const char* model, std::vector<std::uint8_t>& out)
{
	ID3DBlob* blob = nullptr;
	ID3DBlob* err = nullptr;
	const std::string source = std::string(k_hlsl_head) + k_hlsl_body + k_hlsl_styles;
	const HRESULT hr = D3DCompile(
		source.data(), source.size(), nullptr, nullptr, nullptr,
		entry, model, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
	if (FAILED(hr) || !blob)
	{
		if (err)
		{
			std::printf("[NativeChams] compile: %s\n", (const char*)err->GetBufferPointer());
			err->Release();
		}
		return false;
	}
	if (err)
		err->Release();
	const auto* p = (const std::uint8_t*)blob->GetBufferPointer();
	out.assign(p, p + blob->GetBufferSize());
	blob->Release();
	return !out.empty();
}

NativePreparation::Shader localShader;
void start_shader(){
    localShader.Start([]{std::vector<std::uint8_t> result;if(!compile("ps_main","ps_5_0",result))result.clear();return result;});
}
bool shader_ready(){
    start_shader();
    if(localShader.Poll())return true;
    if(localShader.Status()==NativePreparation::Shader::State::Failed)fail("native shader preparation failed");
    return false;
}

std::vector<std::uint8_t> make_present_thunk(std::uintptr_t state, std::uintptr_t create)
{
	std::vector<std::uint8_t> c;
	c.insert(c.end(), { 0x51, 0x52, 0x41, 0x50, 0x41, 0x51 });
	c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x28 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xFF, 0x80, 0xA8, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x83, 0xB8, 0xB4, 0x00, 0x00, 0x00, 0x00 });
	const std::size_t j_busy = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0xC7, 0x80, 0xB4, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 });
	c.insert(c.end(),{0x83,0x38,0x00});
    std::vector<std::size_t> keepCapture;
    keepCapture.push_back(append_jcc32(c,0x05));
    c.insert(c.end(),{0x48,0x83,0xB8});c.insert(c.end(),{0x40,0x0A,0,0});c.push_back(0);
    keepCapture.push_back(append_jcc32(c,0x04));
    c.insert(c.end(),{0x48,0x83,0x78,0x70,0});keepCapture.push_back(append_jcc32(c,0x05));
    c.insert(c.end(),{0x48,0x8B,0x80});c.insert(c.end(),{0x90,0x03,0,0});
    c.insert(c.end(),{0x48,0x85,0xC0});keepCapture.push_back(append_jcc32(c,0x04));
    c.insert(c.end(),{0x48,0xB9});append_u64(c,state);
    c.insert(c.end(),{0x31,0xD2,0x45,0x31,0xC0});append_call_rax(c);
    const auto captureKept=c.size();for(auto at:keepCapture)patch_rel32(c,at,captureKept);
    c.insert(c.end(),{0x48,0xB8});append_u64(c,state);
	c.insert(c.end(), { 0x83, 0x78, 0x04, 0x00 });
	const std::size_t j_nocmd = append_jcc32(c, 0x04);
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, create);
	append_call_rax(c);
	const std::size_t nocmd = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xC7, 0x80, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });

	c.insert(c.end(), { 0x48, 0x8B, 0x90, 0x28, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0x89, 0x90, 0xA0, 0x00, 0x00, 0x00 });
	const std::size_t out = c.size();
	c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x28 });
	c.insert(c.end(), { 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59 });
	c.insert(c.end(), { 0x49, 0xBA });
	append_u64(c, state);
	c.insert(c.end(), { 0x4D, 0x8B, 0x5A, 0x20 });
	c.insert(c.end(), { 0x41, 0xFF, 0xE3 });
	patch_rel32(c, j_busy, out);
	patch_rel32(c, j_nocmd, nocmd);
	return c;
}

std::vector<std::uint8_t> make_create_stub(std::uintptr_t state)
{
	std::vector<std::uint8_t> c;

	const auto store_hr = [&]
	{
		c.insert(c.end(), { 0x48, 0xB9 });
		append_u64(c, state);
		c.insert(c.end(), { 0x89, 0x81, 0xF0, 0x01, 0x00, 0x00 });
	};
	c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x48 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x8B, 0x48, 0x04 });
	c.insert(c.end(), { 0x85, 0xC9 });
	const std::size_t j_none = append_jcc32(c, 0x04);
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });

	c.insert(c.end(), { 0x83, 0xF9, 0x02 });
	const std::size_t j_nvs = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8B, 0x90, 0x80, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x80, 0x88, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4D, 0x31, 0xC9 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x90, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x89, 0x54, 0x24, 0x20 });
	c.insert(c.end(), { 0xFF, 0x90, 0x28, 0x01, 0x00, 0x00 });
	store_hr();
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin0 = c.size() - 4;
	const std::size_t npsx = c.size();

	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x04 });
	const std::size_t j_nbuf = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x80, 0x98, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4D, 0x85, 0xC0 });
	c.insert(c.end(), { 0x74, 0x24 });
	c.insert(c.end(), { 0x4C, 0x89, 0x80, 0xD8, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0xE0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0xD8, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x88, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x38, 0x01, 0x00, 0x00 });
	store_hr();
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin3 = c.size() - 4;
	const std::size_t nbuf = c.size();

	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x05 });
	const std::size_t j_nblend = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x40, 0x01, 0x00, 0x00 });
	store_hr();
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin4 = c.size() - 4;
	const std::size_t nblend = c.size();

	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x07 });
	const std::size_t j_ndss = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x50, 0x01, 0x00, 0x00 });
	store_hr();

    c.insert(c.end(), {0xE9,0,0,0,0}); const auto j_fin_dss=c.size()-4;
    const auto sceneCommand=c.size();
    auto bytes=[&](std::initializer_list<std::uint8_t> v){c.insert(c.end(),v);};
    auto d32=[&](std::uint32_t n){for(int i=0;i<4;++i)c.push_back(std::uint8_t(n>>(8*i)));};
    bytes({0x48,0xB8});append_u64(c,state);
    bytes({0x83,0xF9,8});const auto notScene=append_jcc32(c,0x05);

    bytes({0x48,0x8B,0x48,0x10,0x48,0x8B,0x90});d32(0x200);
    bytes({0x4C,0x8D,0x80});d32(0x208);
    bytes({0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
    bytes({0xFF,0x90});d32(k_scene_open_fn);store_hr();
    bytes({0x85,0xC0});const auto openFailed=append_jcc32(c,0x08);
    bytes({0x48,0xB8});append_u64(c,state);
    bytes({0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
    bytes({0x49,0x8B,0x09,0x4D,0x8D,0x41,0x10,0x48,0x8D,0x90});d32(0x218);
    bytes({0x48,0x8B,0x01,0xFF,0x10});store_hr();
    bytes({0x85,0xC0});const auto mutexFailed=append_jcc32(c,0x08);
    bytes({0x48,0xB8});append_u64(c,state);
    bytes({0x48,0x8B,0x48,0x10,0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
    bytes({0x49,0x8B,0x11,0x49,0x83,0xC1,8,0x45,0x31,0xC0,0xFF,0x90});d32(k_scene_srv_fn);store_hr();
    bytes({0x85,0xC0});const auto srvFailed=append_jcc32(c,0x08);
    bytes({0x48,0xB8});append_u64(c,state);
    bytes({0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
    bytes({0x49,0x8B,0x51,8,0x48,0x89,0x90});d32(offsetof(RemoteState,out_res));
    bytes({0x48,0x8B,0x90});d32(0x200);
    bytes({0x49,0x89,0x51,0x18});
    bytes({0xE9,0,0,0,0});const auto sceneSucceeded=c.size()-4;
    for(auto j:{openFailed,mutexFailed,srvFailed})patch_rel32(c,j,c.size());
    for(unsigned off:{0u,8u,16u}) {
        bytes({0x48,0xB8});append_u64(c,state);
        bytes({0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
        bytes({0x49,0x8B,0x49,static_cast<std::uint8_t>(off),0x48,0x85,0xC9});
        const auto noResource=append_jcc32(c,0x04);
        bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});
        patch_rel32(c,noResource,c.size());
        bytes({0x48,0xB8});append_u64(c,state);
        bytes({0x4C,0x8B,0x88});d32(offsetof(RemoteState,init_sys));
        bytes({0x49,0xC7,0x41,static_cast<std::uint8_t>(off),0,0,0,0});
    }
	const std::size_t fin = c.size();
    for(auto j:{notScene,sceneSucceeded,j_fin_dss})patch_rel32(c,j,fin);

	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xC7, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x80, 0xB0, 0x00, 0x00, 0x00 });
	const std::size_t none = c.size();
	c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x48 });
	c.push_back(0xC3);
	patch_rel32(c, j_none, none);
	patch_rel32(c, j_nvs, npsx);
	patch_rel32(c, j_nbuf, nbuf);
	patch_rel32(c, j_nblend, nblend);
	patch_rel32(c, j_ndss, sceneCommand);
	patch_rel32(c, j_fin0, fin);
	patch_rel32(c, j_fin3, fin);
	patch_rel32(c, j_fin4, fin);
	return c;
}



std::vector<std::uint8_t> make_draw_thunk(std::uintptr_t state)
{
	std::vector<std::uint8_t> c;
	auto bytes = [&](std::initializer_list<std::uint8_t> v) { c.insert(c.end(), v); };
	auto disp32 = [&](std::uint32_t v)
	{
		const auto* b = (const std::uint8_t*)&v;
		c.insert(c.end(), b, b + 4);
	};
	auto ld = [&](std::uint8_t modrm, std::uint32_t off)
	{
		bytes({ 0x49, 0x8B, modrm });
		disp32(off);
	};
	auto ld_rax = [&](std::uint32_t off) { ld(0x86, off); };
	auto ld_rdx = [&](std::uint32_t off) { ld(0x96, off); };
	auto ctx = [&] { bytes({ 0x49, 0x8B, 0x4E, 0x18 }); };
	auto call_state = [&](std::uint32_t fn)
	{
		ld_rax(fn);
		append_call_rax(c);
	};
	auto jmp32 = [&]
	{
		c.push_back(0xE9);
		c.insert(c.end(), 4, 0);
		return c.size() - 4;
	};

	auto stage = [&](std::uint32_t n)
	{
		bytes({ 0x41, 0xC7, 0x86 });
		disp32(k_body_stage_off);
		disp32(n);
	};

    auto original_draw = [&] {
        for(std::uint32_t i=0;i<4;++i) {
            bytes({0x48,0x8B,0x84,0x24});disp32(0x180+i*8);
            bytes({0x48,0x89,0x44,0x24,static_cast<std::uint8_t>(0x20+i*8)});
        }
        bytes({0x48,0x89,0xF1,0x48,0x89,0xDA,0x4D,0x89,0xE0,0x4D,0x89,0xE9});
        ld_rax(offsetof(RemoteState,orig_draw));append_call_rax(c);
        bytes({0x48,0x89,0xC7});
    };
    auto attachment_draw = [&] {

        bytes({0xC7,0x84,0x24});disp32(0xDC);disp32(0);
        bytes({0x48,0x8B,0x44,0x24,0x58});
        bytes({0xF7,0x80});disp32(offsetof(Target,weapon));disp32(k_character_attachment);
        std::vector<std::size_t> skip;
        const auto isAttachment=append_jcc32(c,0x05);

        bytes({0x48,0x83,0xBC,0x24});disp32(0x80);bytes({0});
        skip.push_back(append_jcc32(c,0x05));
        bytes({0x83,0xBC,0x24});disp32(0x100);bytes({0});
        skip.push_back(append_jcc32(c,0x04));
        bytes({0x8B,0x88});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,depth)+4);
        bytes({0x85,0xC9});const auto standard=append_jcc32(c,0x08);
        ld_rax(k_attachment_visible_reversed);const auto selected=jmp32();
        patch_rel32(c,standard,c.size());ld_rax(k_attachment_visible_standard);
        patch_rel32(c,selected,c.size());bytes({0x48,0x89,0x84,0x24});disp32(0x118);
        const auto noAttachment=jmp32();
        patch_rel32(c,isAttachment,c.size());
        bytes({0x48,0x8B,0x80});disp32(offsetof(Target,visibleDepthState));
        bytes({0x48,0x89,0x84,0x24});disp32(0x118);
        patch_rel32(c,noAttachment,c.size());bytes({0x48,0x83,0xBC,0x24});disp32(0x118);bytes({0});
        skip.push_back(append_jcc32(c,0x04));
        bytes({0x48,0x8B,0x44,0x24,0x58});
        bytes({0x48,0x83,0xB8});disp32(offsetof(Target,visibleDepthState));bytes({0});
        skip.push_back(append_jcc32(c,0x04));
        ld_rax(k_viewport_get_fn);bytes({0x48,0x85,0xC0});
        skip.push_back(append_jcc32(c,0x04));
        bytes({0xC7,0x84,0x24});disp32(0xF8);disp32(1);
        ctx();bytes({0x48,0x8D,0x94,0x24});disp32(0xF8);
        bytes({0x4C,0x8D,0x84,0x24});disp32(0xE0);call_state(k_viewport_get_fn);
        bytes({0x83,0xBC,0x24});disp32(0xF8);bytes({1});
        skip.push_back(append_jcc32(c,0x05));
        bytes({0x48,0x8B,0x44,0x24,0x58});
        for(std::uint32_t axis=0;axis<2;++axis) {
            bytes({0xF3,0x0F,0x10,0x84,0x24});disp32(0xE8+axis*4);
            bytes({0x0F,0x2E,0x80});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,frame)+axis*4);
            skip.push_back(append_jcc32(c,0x0A));
            skip.push_back(append_jcc32(c,0x05));
        }
        ctx();bytes({0x48,0x8D,0x94,0x24});disp32(0xD0);
        bytes({0x4C,0x8D,0x84,0x24});disp32(0xD8);call_state(offsetof(RemoteState,fn_dssget));
        ctx();bytes({0x48,0x8B,0x94,0x24});disp32(0x118);
        bytes({0x44,0x8B,0x84,0x24});disp32(0xD8);call_state(offsetof(RemoteState,fn_dssset));
        bytes({0xC7,0x84,0x24});disp32(0xDC);disp32(1);
        for(auto at:skip)patch_rel32(c,at,c.size());
        original_draw();
        bytes({0x83,0xBC,0x24});disp32(0xDC);bytes({0});
        const auto unchanged=append_jcc32(c,0x04);
        ctx();bytes({0x48,0x8B,0x94,0x24});disp32(0xD0);
        bytes({0x44,0x8B,0x84,0x24});disp32(0xD8);call_state(offsetof(RemoteState,fn_dssset));
        bytes({0x48,0x8B,0x8C,0x24});disp32(0xD0);bytes({0x48,0x85,0xC9});
        const auto noState=append_jcc32(c,0x04);
        bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});
        patch_rel32(c,noState,c.size());patch_rel32(c,unchanged,c.size());
    };

	bytes({ 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56 });
	bytes({ 0x48, 0x81, 0xEC, 0x28, 0x01, 0x00, 0x00 });
	bytes({ 0x48, 0x89, 0xCE });
	bytes({ 0x48, 0x89, 0xD3 });
	bytes({ 0x4D, 0x89, 0xC4 });
	bytes({ 0x4D, 0x89, 0xCD });
	bytes({ 0x49, 0xBE });
	append_u64(c, state);

	bytes({ 0x41, 0xFF, 0x86 });
	disp32(k_draw_calls_off);
	bytes({ 0x41, 0x8B, 0x86 });
	disp32(k_draw_calls_off);
	bytes({ 0x83, 0xE0, 0x03 });
	bytes({ 0x49, 0x89, 0x94, 0xC6 });
	disp32(k_rdx_ring_off);

	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x40, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x48, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0xC7, 0x44, 0x24, 0x50, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x58, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x60, 0x00, 0x00, 0x00, 0x00 });

    bytes({0x31,0xff});
	std::vector<std::size_t> to_call;
	bytes({ 0x49, 0x83, 0x7E, 0x70, 0x00 });
	to_call.push_back(append_jcc32(c, 0x05));
	bytes({ 0x41, 0x83, 0x3E, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x41, 0x83, 0x7E, 0x08, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x49, 0x83, 0x7E, 0x38, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x41, 0x8B, 0x4E, 0x0C });
	bytes({ 0x85, 0xC9 });
	to_call.push_back(append_jcc32(c, 0x04));
	ld_rdx(offsetof(RemoteState, geoms));
	bytes({ 0x48, 0x85, 0xD2 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x31, 0xC0 });
	const std::size_t scan = c.size();

	bytes({ 0x39, 0xC8 });
	to_call.push_back(append_jcc32(c, 0x03));
	bytes({ 0x44, 0x8D, 0x14, 0x08, 0x41, 0xD1, 0xEA });
	bytes({ 0x4D, 0x69, 0xDA }); disp32(sizeof(Target));
	bytes({ 0x49, 0x01, 0xD3 });
	bytes({ 0x49, 0x83, 0x3B, 0x00 });
	const auto zeroUpper = append_jcc32(c, 0x04);
    std::vector<std::size_t> rangeLower, rangeUpper;
    bytes({0x49,0x39,0xbb});disp32(offsetof(Target,renderEntity));
    rangeLower.push_back(append_jcc32(c,0x02));rangeUpper.push_back(append_jcc32(c,0x07));
    bytes({ 0x49, 0x39, 0x1B });
    rangeLower.push_back(append_jcc32(c, 0x02));
    rangeUpper.push_back(append_jcc32(c, 0x07));
    bytes({ 0x45, 0x39, 0xAB }); disp32(offsetof(Target, startIndex));
    rangeLower.push_back(append_jcc32(c, 0x02)); rangeUpper.push_back(append_jcc32(c, 0x07));
    bytes({ 0x44, 0x8B, 0x84, 0x24 }); disp32(0x180);
    bytes({ 0x45, 0x39, 0x83 }); disp32(offsetof(Target, baseVertex));
    rangeLower.push_back(append_jcc32(c, 0x02)); rangeUpper.push_back(append_jcc32(c, 0x07));
    bytes({ 0x44, 0x8B, 0x84, 0x24 }); disp32(0x188);
    bytes({ 0x45, 0x39, 0x83 }); disp32(offsetof(Target, indexCount));
    rangeLower.push_back(append_jcc32(c, 0x02)); rangeUpper.push_back(append_jcc32(c, 0x07));
    bytes({ 0x45, 0x39, 0xA3 }); disp32(offsetof(Target, topology));
    const auto j_found = append_jcc32(c, 0x04);
    rangeLower.push_back(append_jcc32(c, 0x02));
    for (auto jump : rangeUpper) patch_rel32(c, jump, c.size());
	patch_rel32(c, zeroUpper, c.size());
	bytes({ 0x44, 0x89, 0xD1 });
	patch_rel32(c, jmp32(), scan);
	for (auto jump : rangeLower) patch_rel32(c, jump, c.size());
	bytes({ 0x44, 0x89, 0xD0, 0xFF, 0xC0 });
	patch_rel32(c, jmp32(), scan);

	patch_rel32(c, j_found, c.size());
	bytes({ 0x4C, 0x89, 0xDA });

	bytes({ 0x41, 0xFF, 0x86 });
	disp32(k_draw_match_off);
	bytes({ 0x41, 0x83, 0xBE });
	disp32(k_draw_live_off);
	bytes({ 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));

	bytes({ 0xB8, 0x01, 0x00, 0x00, 0x00, 0x49, 0x87, 0x46, 0x70 });
	bytes({ 0x48, 0x85, 0xC0 });
	to_call.push_back(append_jcc32(c, 0x05));
	bytes({ 0x48, 0x89, 0x54, 0x24, 0x58 });
    bytes({0xC7,0x84,0x24});disp32(0x100);disp32(0);
    bytes({0xC7,0x84,0x24});disp32(0x110);disp32(0);
    bytes({0x48,0x8B,0x44,0x24,0x58});
    bytes({0xC7,0x80});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)+12);disp32(0);

	ctx();
	bytes({0xBA,0x01,0,0,0});
	bytes({0x4C,0x8D,0x84,0x24}); disp32(0x80);
	bytes({0x4C,0x8D,0x8C,0x24}); disp32(0xB8);
	call_state(offsetof(RemoteState,fn_omget));

    bytes({0x48,0x8B,0x44,0x24,0x58});
    bytes({0xF7,0x80});disp32(offsetof(Target,weapon));disp32(0x1ff);
    std::vector<std::size_t> noCapture;
    noCapture.push_back(append_jcc32(c,0x05));
    bytes({0x83,0xB8});disp32(offsetof(Target,occlusion));bytes({0});
    noCapture.push_back(append_jcc32(c,0x04));
    ld_rax(k_capture_fn);bytes({0x48,0x85,0xC0});noCapture.push_back(append_jcc32(c,0x04));
    bytes({0x4C,0x89,0xF1,0x48,0x8B,0x94,0x24});disp32(0xB8);
    bytes({0x45,0x31,0xC0,0x48,0x83,0xBC,0x24});disp32(0x80);bytes({0});
    bytes({0x41,0x0F,0x95,0xC0});call_state(k_capture_fn);
    bytes({0x89,0x84,0x24});disp32(0x100);
    bytes({0xA9});disp32(0x100);const auto notCopied=append_jcc32(c,0x04);
    bytes({0x25});disp32(0xff);bytes({0xF3,0x0F,0x2A,0xC0});
    bytes({0x48,0x8B,0x44,0x24,0x58});bytes({0xF3,0x0F,0x11,0x80});
    disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)+12);
    patch_rel32(c,notCopied,c.size());for(auto at:noCapture)patch_rel32(c,at,c.size());

    bytes({0x48,0x8B,0x8C,0x24});disp32(0xB8);
    bytes({0x48,0x85,0xC9});const auto noDsv=append_jcc32(c,0x04);
    bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});
    patch_rel32(c,noDsv,c.size());
	bytes({0x48,0x8B,0x8C,0x24}); disp32(0x80);
	bytes({0x48,0x85,0xC9});
	const auto colorPass=append_jcc32(c,0x05);
	bytes({0x41,0xFF,0x86}); disp32(k_depth_passes_off);
    attachment_draw();
    bytes({0x49,0xC7,0x46,0x70,0,0,0,0});
    bytes({0x48,0x89,0xF8});
    bytes({0x48,0x81,0xC4,0x28,0x01,0,0});
    bytes({0x41,0x5E,0x41,0x5D,0x41,0x5C,0x5F,0x5E,0x5B,0xC3});
	patch_rel32(c,colorPass,c.size());
	bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});
	bytes({0x41,0xFF,0x86}); disp32(k_color_passes_off);
	stage(1);
	bytes({ 0x48, 0x8B, 0x44, 0x24, 0x58 });
	bytes({ 0x83, 0xB8 });
	disp32(offsetof(Target, mat) + offsetof(NativePattern::Material, pattern) + 8);
	bytes({ 0x00 });
	const auto skipAvatar = append_jcc32(c, 0x05);

    attachment_draw();

	patch_rel32(c, skipAvatar, c.size());
	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x40 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psget));

	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x48 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x50 });
	call_state(offsetof(RemoteState, fn_dssget));

	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x60 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x68 });
	bytes({ 0x4C, 0x8D, 0x4C, 0x24, 0x78 });
	call_state(offsetof(RemoteState, fn_blendget));

	ctx();
	bytes({ 0xBA, 0x0D, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00 });
	bytes({ 0x4C, 0x8D, 0x8C, 0x24, 0x90, 0x00, 0x00, 0x00 });
	call_state(offsetof(RemoteState, fn_pscbget));
    bytes({0x83,0xBC,0x24});disp32(0x100);bytes({0});
    const auto noSceneBinding=append_jcc32(c,0x04);
    ctx();bytes({0xBA,12,0,0,0,0x41,0xB8,1,0,0,0,0x4C,0x8D,0x8C,0x24});disp32(0x108);
    call_state(k_capture_srv_get);
    ctx();bytes({0xBA,12,0,0,0,0x41,0xB8,1,0,0,0,0x4D,0x8D,0x8E});disp32(k_capture_data+24);
    call_state(k_capture_srv_set);
    bytes({0xC7,0x84,0x24});disp32(0x110);disp32(1);
    patch_rel32(c,noSceneBinding,c.size());
	stage(2);

    bytes({0x48,0x8B,0x44,0x24,0x58});
    bytes({0xC7,0x80}); disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)); disp32(0);
    bytes({0x4C,0x8B,0x90}); disp32(offsetof(Target,liveBody));
    bytes({0x4D,0x85,0xD2});
    const auto noBodyCache=append_jcc32(c,0x04);
    bytes({0x41,0x8B,0x8E}); disp32(offsetof(RemoteState,entered));
    bytes({0x41,0x39,0x4A,0x08});
    const auto bodyCached=append_jcc32(c,0x04);
    bytes({0x41,0x89,0x4A,0x08});
    bytes({0x41,0xC7,0x42,0x0C,0,0,0,0});
    bytes({0x49,0x8B,0x86}); disp32(k_body_reader_off);
    bytes({0x48,0x85,0xC0});
    const auto noBodyReader=append_jcc32(c,0x04);
    bytes({0x4C,0x89,0x94,0x24}); disp32(0x98);
    bytes({0x48,0xC7,0xC1,0xFF,0xFF,0xFF,0xFF});
    bytes({0x49,0x8B,0x12});
    bytes({0x4D,0x8D,0x42,0x10});
    bytes({0x41,0xB9,0x30,0,0,0});
    bytes({0x48,0xC7,0x44,0x24,0x20,0,0,0,0});
    append_call_rax(c);
    bytes({0x4C,0x8B,0x94,0x24}); disp32(0x98);
    bytes({0x41,0x89,0x42,0x0C});
    patch_rel32(c,bodyCached,c.size());
    patch_rel32(c,noBodyReader,c.size());
    bytes({0x41,0x83,0x7A,0x0C,0});
    const auto bodyFailed=append_jcc32(c,0x04);
    bytes({0x48,0x8B,0x44,0x24,0x58});
    for(std::uint32_t row=0;row<3;++row) {
        bytes({0x41,0x0F,0x10,0x82}); disp32(16+row*16);
        bytes({0x0F,0x11,0x80}); disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,draw_body)+row*16);
    }
    bytes({0xC7,0x80}); disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)); disp32(0x3f800000);
    patch_rel32(c,noBodyCache,c.size());
    patch_rel32(c,bodyFailed,c.size());

	bytes({ 0x48, 0x8B, 0x44, 0x24, 0x58 });
	bytes({ 0x4C, 0x8B, 0x90 }); disp32(offsetof(Target, viewMatrix));
	bytes({ 0x4D, 0x85, 0xD2 });
	const auto noLiveCamera = append_jcc32(c, 0x04);
	for (std::uint32_t row = 0; row < 4; ++row) {
		bytes({ 0x41, 0x0F, 0x10, 0x82 }); disp32(row * 16);
		bytes({ 0x0F, 0x11, 0x80 });
		disp32(offsetof(Target, mat) + offsetof(NativePattern::Material, live_view) + row * 16);
	}
	patch_rel32(c, noLiveCamera, c.size());
    bytes({0x48,0x8B,0x44,0x24,0x58});
    bytes({0xC7,0x80});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)+8);disp32(0);

    bytes({0xC7,0x84,0x24});disp32(0xB4);disp32(0);
    ld_rax(offsetof(RemoteState,dss));
    bytes({0x48,0x89,0x84,0x24});disp32(0xC0);
    bytes({0x48,0x8B,0x44,0x24,0x58});

    bytes({0xF7,0x80});disp32(offsetof(Target,weapon));disp32(0x100);
    const auto worldDepth=append_jcc32(c,0x04);
    bytes({0x48,0x8B,0x88});disp32(offsetof(Target,visibleDepthState));
    bytes({0x48,0x85,0xC9});
    const auto viewDepthReady=append_jcc32(c,0x05);
    bytes({0x48,0x8B,0x4C,0x24,0x48});
    patch_rel32(c,viewDepthReady,c.size());
    bytes({0x48,0x89,0x8C,0x24});disp32(0xC0);
    const auto viewDepthDone=jmp32();
    patch_rel32(c,worldDepth,c.size());
    bytes({0x83,0xB8});disp32(offsetof(Target,occlusion));bytes({0});
    const auto noOcclusion=append_jcc32(c,0x04);
    bytes({0x48,0x83,0xBC,0x24});disp32(0xB8);bytes({0});
    const auto missingDepth=append_jcc32(c,0x04);
    bytes({0x48,0x83,0xB8});disp32(offsetof(Target,visibleDepthState));bytes({0});
    const auto missingVisibleState=append_jcc32(c,0x04);
    bytes({0x48,0x83,0xB8});disp32(offsetof(Target,occludedDepthState));bytes({0});
    const auto missingHiddenState=append_jcc32(c,0x04);
    bytes({0x0F,0x10,0x40,0x08});
    bytes({0x0F,0x11,0x84,0x24});disp32(0xA0);
    bytes({0x8B,0x88});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,mode));
    bytes({0x89,0x8C,0x24});disp32(0xB0);
    bytes({0x0F,0x10,0x80});disp32(offsetof(Target,occludedColor));
    bytes({0x0F,0x11,0x40,0x08});
    bytes({0x8B,0x88});disp32(offsetof(Target,occludedMode));
    bytes({0x89,0x88});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,mode));
    bytes({0xC7,0x80});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)+8);disp32(0x3f800000);
    bytes({0x48,0x8B,0x88});disp32(offsetof(Target,occludedDepthState));
    bytes({0x48,0x89,0x8C,0x24});disp32(0xC0);
    bytes({0xC7,0x84,0x24});disp32(0xB4);disp32(1);
    for(auto jump:{viewDepthDone,noOcclusion,missingDepth,missingVisibleState,missingHiddenState})patch_rel32(c,jump,c.size());
    const auto effectPass=c.size();

	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x48 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	bytes({ 0x48, 0x8B, 0x44, 0x24, 0x58 });
	bytes({ 0x48, 0x83, 0xC0, 0x08 });
	bytes({ 0x48, 0x89, 0x44, 0x24, 0x20 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x30, 0x00, 0x00, 0x00, 0x00 });
	call_state(offsetof(RemoteState, fn_update));
	stage(3);

	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x38 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psset));

	ctx();
	bytes({ 0xBA, 0x0D, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00 });
	bytes({ 0x4D, 0x8D, 0x4E, 0x48 });
	call_state(offsetof(RemoteState, fn_pscb));

	ctx();
	bytes({0x48,0x8B,0x94,0x24});disp32(0xC0);
	bytes({ 0x45, 0x31, 0xC0 });
	call_state(offsetof(RemoteState, fn_dssset));

	for (std::uint8_t i = 0; i < 4; ++i)
	{
		bytes({ 0xC7, 0x84, 0x24 });
		disp32(0x80u + i * 4u);
		bytes({ 0x00, 0x00, 0x80, 0x3F });
	}
	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x58 });
	bytes({ 0x4C, 0x8D, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF });
	call_state(offsetof(RemoteState, fn_blendset));
	stage(4);

	bytes({ 0x41, 0xFF, 0x86 });
	disp32(offsetof(RemoteState, draws));

	for (std::uint32_t index = 0; index < 4; ++index) {
		bytes({ 0x48, 0x8B, 0x84, 0x24 }); disp32(0x180 + index * 8);
		bytes({ 0x48, 0x89, 0x44, 0x24, static_cast<std::uint8_t>(0x20 + index * 8) });
	}
	bytes({ 0x48, 0x89, 0xF1 });
	bytes({ 0x48, 0x89, 0xDA });
	bytes({ 0x4D, 0x89, 0xE0 });
	bytes({ 0x4D, 0x89, 0xE9 });
	bytes({ 0x49, 0x8B, 0x46, 0x28 });
	append_call_rax(c);
	bytes({ 0x48, 0x89, 0xC7 });
	stage(5);
    bytes({0x83,0xBC,0x24});disp32(0xB4);bytes({0});
    const auto passDone=append_jcc32(c,0x04);
    bytes({0x48,0x8B,0x44,0x24,0x58});
    bytes({0x0F,0x10,0x84,0x24});disp32(0xA0);
    bytes({0x0F,0x11,0x40,0x08});
    bytes({0xC7,0x80});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,body_status)+8);disp32(0);
    bytes({0x8B,0x8C,0x24});disp32(0xB0);
    bytes({0x89,0x88});disp32(offsetof(Target,mat)+offsetof(NativePattern::Material,mode));
    bytes({0x48,0x8B,0x88});disp32(offsetof(Target,visibleDepthState));
    bytes({0x48,0x89,0x8C,0x24});disp32(0xC0);
    bytes({0xC7,0x84,0x24});disp32(0xB4);disp32(0);
    patch_rel32(c,jmp32(),effectPass);
    patch_rel32(c,passDone,c.size());

	bytes({ 0x48, 0x83, 0x7C, 0x24, 0x58, 0x00 });
	const std::size_t j_out = append_jcc32(c, 0x04);

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x40 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psset));

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x48 });
	bytes({ 0x44, 0x8B, 0x44, 0x24, 0x50 });
	call_state(offsetof(RemoteState, fn_dssset));

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x60 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x68 });
	bytes({ 0x44, 0x8B, 0x4C, 0x24, 0x78 });
	call_state(offsetof(RemoteState, fn_blendset));
	ctx();
	bytes({ 0xBA, 0x0D, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00 });
	bytes({ 0x4C, 0x8D, 0x8C, 0x24, 0x90, 0x00, 0x00, 0x00 });
	call_state(offsetof(RemoteState, fn_pscb));

    bytes({0x83,0xBC,0x24});disp32(0x110);bytes({0});
    const auto noSceneRestore=append_jcc32(c,0x04);
    ctx();bytes({0xBA,12,0,0,0,0x41,0xB8,1,0,0,0,0x4C,0x8D,0x8C,0x24});disp32(0x108);
    call_state(k_capture_srv_set);
    bytes({0x48,0x8B,0x8C,0x24});disp32(0x108);bytes({0x48,0x85,0xC9});
    const auto noSavedSrv=append_jcc32(c,0x04);bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});
    patch_rel32(c,noSavedSrv,c.size());patch_rel32(c,noSceneRestore,c.size());

	for (std::uint32_t off : { 0x40, 0x48, 0x60, 0x90 })
	{
		bytes({ 0x48, 0x8B, 0x8C, 0x24 }); disp32(off);
		bytes({ 0x48, 0x85, 0xC9 });
		bytes({ 0x74, 0x06 });
		bytes({ 0x48, 0x8B, 0x01 });
		bytes({ 0xFF, 0x50, 0x10 });
	}

	stage(6);
	bytes({ 0x49, 0xC7, 0x46, 0x70, 0x00, 0x00, 0x00, 0x00 });
	patch_rel32(c, j_out, c.size());
	bytes({ 0x48, 0x89, 0xF8 });
	bytes({ 0x48, 0x81, 0xC4, 0x28, 0x01, 0x00, 0x00 });
	bytes({ 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5B });
	c.push_back(0xC3);

	for (auto at : to_call) patch_rel32(c, at, c.size());
	bytes({ 0x48, 0x89, 0xF1, 0x48, 0x89, 0xDA, 0x4D, 0x89, 0xE0, 0x4D, 0x89, 0xE9 });
	bytes({ 0x49, 0x8B, 0x46, 0x28 });
	bytes({ 0x48, 0x81, 0xC4, 0x28, 0x01, 0x00, 0x00 });
	bytes({ 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5B, 0xFF, 0xE0 });
	return c;
}

bool add_patch(std::uintptr_t at, std::uintptr_t thunk, std::uintptr_t* orig)
{
	if (!addr_ok(at) || !addr_ok(thunk) || g.npatch >= 16)
		return false;
	const std::uint64_t old = memory->Read<std::uint64_t>(at);
	if (!old || old == thunk)
		return old == thunk;
	for (int i = 0; i < g.npatch; ++i)
	{
		if (g.patches[i].at == at)
			return true;
	}
	DWORD prot = 0;
	if (!memory->Protect(at, 8, PAGE_READWRITE, &prot))
		return false;
	const bool ok = memory->Write<std::uint64_t>(at, thunk);
	memory->Protect(at, 8, prot, nullptr);
	if (!ok)
		return false;
	g.patches[g.npatch].at = at;
	g.patches[g.npatch].old = old;
	++g.npatch;
	if (orig && !*orig)
		*orig = (std::uintptr_t)old;
	return true;
}

void unpatch()
{
	for (int i = 0; i < g.npatch; ++i)
	{
		if (!g.patches[i].at)
			continue;
		DWORD prot = 0;
		if (memory->Protect(g.patches[i].at, 8, PAGE_READWRITE, &prot))
		{
			memory->Write<std::uint64_t>(g.patches[i].at, g.patches[i].old);
			memory->Protect(g.patches[i].at, 8, prot, nullptr);
		}
		g.patches[i] = {};
	}
	g.npatch = 0;
}

int clone_vt(std::uintptr_t src, std::uintptr_t dst, int slot, std::uintptr_t thunk, std::uintptr_t* orig)
{
	if (!addr_ok(src) || !addr_ok(dst) || !addr_ok(thunk))
		return 0;
	static const int kTry[] = { 480, 384, 256, 192, 128, 96, 64, 48, 40, 32 };
	std::vector<std::uint64_t> v;
	int n = 0;
	for (const int cand : kTry)
	{
		if (cand <= slot)
			continue;
		const std::size_t bytes = (std::size_t)(cand + 1) * 8;
		v.assign((std::size_t)cand + 1, 0);
		if (memory->ReadRaw(src - 8, v.data(), bytes) == bytes)
		{
			n = cand;
			break;
		}
	}
	if (n <= slot)
		return 0;
	const std::size_t bytes = (std::size_t)(n + 1) * 8;
	*orig = (std::uintptr_t)v[(std::size_t)slot + 1];
	if (!addr_ok(*orig))
		return 0;
	v[(std::size_t)slot + 1] = thunk;
	if (memory->WriteRaw(dst - 8, v.data(), bytes) != bytes)
		return 0;

	std::vector<std::uint64_t> back(v.size());
	if (memory->ReadRaw(dst - 8, back.data(), bytes) != bytes)
		return 0;
	if ((std::uintptr_t)back[(std::size_t)slot + 1] != thunk || back[0] != v[0])
		return 0;
	return n;
}

bool patch_vtables()
{
	if (!g.present_thunk || !g.dc || !g.swap || !g.vt_mem || !g.draw_thunk)
		return false;
	std::uintptr_t dc_vt = (std::uintptr_t)memory->Read<std::uint64_t>(g.dc);
	const std::uintptr_t swap_vt = (std::uintptr_t)memory->Read<std::uint64_t>(g.swap);
	if (!addr_ok(dc_vt) || !addr_ok(swap_vt) || dc_vt == g.dc_vt || swap_vt == g.swap_vt)
		return false;

	{
		const std::uintptr_t class_vt = resolve_wrapper_vts().ctx;
		if (!class_vt) return false;
		if (dc_vt != class_vt && class_vt != g.dc_vt)
		{
			DWORD prot = 0;
			if (memory->Protect(g.dc, 8, PAGE_READWRITE, &prot))
			{
				memory->Write<std::uint64_t>(g.dc, class_vt);
				memory->Protect(g.dc, 8, prot, nullptr);
				dc_vt = class_vt;
				std::printf("[NativeChams] restored the class vtable before cloning\n");
			}
		}
	}

	g.swap_vt = g.vt_mem + 8;
	g.nswvt = clone_vt(swap_vt, g.swap_vt, k_slot_present, g.present_thunk, &g.orig_present);
	if (!g.nswvt)
		return false;
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, orig_present), g.orig_present);
	if (!add_patch(g.swap, g.swap_vt, nullptr))
		return false;

	g.ndcvt = 0;
	g.dc_hooked = false;
	g.dc_vt = g.vt_mem + 0x1000 + 8;
	g.ndcvt = clone_vt(dc_vt, g.dc_vt, k_slot_dc_draw, g.draw_thunk, &g.orig_draw);
	if (!g.ndcvt)
		return false;
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, orig_draw), g.orig_draw);

	g.dc_arm = g.dc_vt + (std::size_t)k_slot_dc_draw * 8;
	{
		DWORD prot = 0;
		if (memory->Protect(g.dc_arm, 8, PAGE_READWRITE, &prot))
		{
			memory->Write<std::uint64_t>(g.dc_arm, (std::uint64_t)g.orig_draw);
			memory->Protect(g.dc_arm, 8, prot, nullptr);
		}
	}
	g.dc_armed = false;
	if (!add_patch(g.dc, g.dc_vt, nullptr))
		return false;
	g.dc_hooked = true;
	return true;
}

bool setup_meta()
{
	if (!g.device || !g.state || !g.ctx || !g.dc)
		return false;
	if(!localShader.Poll())return false;
    const auto& ps=localShader.Bytes();
	if (g.ps_dxbc)
	{
		memory->Free(g.ps_dxbc);
		g.ps_dxbc = 0;
	}
	g.ps_dxbc = memory->Alloc(ps.size() + 16, PAGE_READWRITE);
	if (!g.ps_dxbc)
		return false;
	if (memory->WriteRaw(g.ps_dxbc, ps.data(), ps.size()) != ps.size())
		return false;
	g.ps_len = ps.size();

	if (!g.geoms)
		g.geoms = memory->Alloc((std::size_t)k_max_targets * (sizeof(Target) + sizeof(BodyCache)) * 3 + 16, PAGE_READWRITE);
	if (!g.geoms)
		return false;

	RemoteState st{};
	memory->ReadRaw(g.state, &st, sizeof(st));
	st.device = g.device;
	st.ctx = g.ctx;
	st.dc = g.dc;
	st.geoms = 0;
	st.ngeom = 0;
	st.fn_create_ps = vt_fn(g.device, k_slot_create_ps);
	st.fn_create_buf = vt_fn(g.device, k_slot_create_buf);
	st.fn_create_dss = vt_fn(g.device, k_slot_create_dss);
	st.fn_psset = vt_fn(g.ctx, k_slot_psset);
	st.fn_psget = vt_fn(g.ctx, k_slot_psget);
	st.fn_omget = vt_fn(g.ctx, k_slot_omget);
	st.fn_pscb = vt_fn(g.ctx, k_slot_pscb);
	st.fn_pscbget = vt_fn(g.ctx, k_slot_pscbget);
	st.fn_dssset = vt_fn(g.ctx, k_slot_dssset);
	st.fn_dssget = vt_fn(g.ctx, k_slot_dssget);
	st.fn_blendset = vt_fn(g.ctx, k_slot_blendset);
	st.fn_blendget = vt_fn(g.ctx, k_slot_blendget);
	st.fn_create_blend = vt_fn(g.device, k_slot_create_blend);
	st.fn_update = vt_fn(g.ctx, k_slot_update);
    memory->Write<std::uint64_t>(g.state+k_viewport_get_fn,vt_fn(g.ctx,95));
    memory->Write<std::uint64_t>(g.state+k_capture_srv_get,vt_fn(g.ctx,73));
    memory->Write<std::uint64_t>(g.state+k_capture_srv_set,vt_fn(g.ctx,8));
	if (!addr_ok((std::uintptr_t)st.fn_create_ps) || !addr_ok((std::uintptr_t)st.fn_create_dss) ||
		!addr_ok((std::uintptr_t)st.fn_create_buf) || !addr_ok((std::uintptr_t)st.fn_psset) ||
		!addr_ok(st.fn_omget) || !addr_ok((std::uintptr_t)st.fn_psget) || !addr_ok((std::uintptr_t)st.fn_pscb) || !addr_ok(st.fn_pscbget) ||
		!addr_ok((std::uintptr_t)st.fn_dssset) || !addr_ok((std::uintptr_t)st.fn_dssget) ||
		!addr_ok((std::uintptr_t)st.fn_blendset) || !addr_ok((std::uintptr_t)st.fn_blendget) ||
		!addr_ok((std::uintptr_t)st.fn_create_blend) || !addr_ok((std::uintptr_t)st.fn_update))
		return false;

	st.busy = 0;
	st.cmd = 0;
	st.ready = 0;
	st.last_rtv = 0;
	memory->WriteRaw(g.state, &st, sizeof(st));

	memory->Write<std::uint32_t>(g.state + k_draw_match_off, 0);
	memory->Write<std::uint32_t>(g.state + k_draw_live_off, 0);
	memory->Write<std::uint32_t>(g.state + k_draw_calls_off, 0);
	memory->Write<std::uint32_t>(g.state + k_color_passes_off, 0);
	memory->Write<std::uint32_t>(g.state + k_depth_passes_off, 0);
	for (std::uint32_t i = 0; i < 4; ++i)
		memory->Write<std::uint64_t>(g.state + k_rdx_ring_off + i * 8, 0);
	memory->Write<std::uint32_t>(g.state + k_body_stage_off, 0);
	memory->Write<std::uint64_t>(g.state + k_pending_geoms_off, 0);
    const HMODULE localKernel = GetModuleHandleW(L"KernelBase.dll");
    const auto reader = reinterpret_cast<std::uintptr_t>(GetProcAddress(localKernel,"ReadProcessMemory"));
    const auto& remoteKernel = remote_module_range(L"KernelBase.dll");
    const auto readerRva = reader - reinterpret_cast<std::uintptr_t>(localKernel);
    std::uint64_t remoteReader = 0;
    if (localKernel && reader && remoteKernel.has(remoteKernel.base + readerRva + 15)) {
        const auto candidate=remoteKernel.base+readerRva;
        unsigned char signature[16]{};
        if (memory->ReadRaw(candidate,signature,sizeof(signature))==sizeof(signature) &&
            std::memcmp(signature,reinterpret_cast<const void*>(reader),sizeof(signature))==0)
            remoteReader=candidate;
    }
    memory->Write<std::uint64_t>(g.state+k_body_reader_off,remoteReader);

	g.draw_live = false;
	g.first_match = {};
	g.cmd_wait = false;
	return true;
}

void issue_cmd(std::uint32_t cmd, std::uintptr_t code, std::size_t len, std::uintptr_t init)
{
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, out_res), 0);
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, bytecode), code);
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, bytecode_len), len);
	memory->Write<std::uint64_t>(g.state + offsetof(RemoteState, init_sys), init);
	memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, cmd), cmd);
	g.cmd_wait = true;
}

void issue_desc_cmd(std::uint32_t cmd, const void* desc, std::size_t n, std::uintptr_t init)
{
	memory->WriteRaw(g.state + 0x200, desc, n);
	issue_cmd(cmd, 0, 0, init);
}

std::uint64_t take_out()
{
	const std::uint32_t cmd = memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, cmd));
	if (cmd)
		return 0;
	g.cmd_wait = false;
	return memory->Read<std::uint64_t>(g.state + offsetof(RemoteState, out_res));
}

void store_res(std::uint64_t off, std::uint64_t v)
{
	memory->Write<std::uint64_t>(g.state + off, v);
}



bool pump_creates()
{
	if (!g.state)
		return false;
	if (g.cmd_wait)
	{
		const std::uint64_t out = take_out();
		if (g.cmd_wait)
			return false;
		if (!out)
		{
			fail("create");
			g.stage = 0;
			return false;
		}
		switch (g.stage)
		{
		case 1: store_res(offsetof(RemoteState, ps), out); g.stage = 2; break;
		case 2:
			store_res(offsetof(RemoteState, dss), out);
			g.stage = 3;
			break;
		case 3: store_res(offsetof(RemoteState, cb), out); g.stage = 4; break;
		case 4: store_res(offsetof(RemoteState, blend), out); g.stage = 5; break;
        case 5: store_res(k_depth_visible_standard,out); g.stage=6; break;
        case 6: store_res(k_depth_hidden_standard,out); g.stage=7; break;
        case 7: store_res(k_depth_visible_reversed,out); g.stage=8; break;
        case 8: store_res(k_depth_hidden_reversed,out); g.stage=9; break;
        case 9: store_res(k_attachment_visible_standard,out); g.stage=10; break;
        case 10:
            store_res(k_attachment_visible_reversed,out);
			g.stage = 11;
			memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, ready), 1);
			break;
		default:
			break;
		}
	}
	if (g.cmd_wait)
		return g.stage >= 11;

	if (g.stage == 0)
	{
		g.stage = 1;
		issue_cmd(k_cmd_ps, g.ps_dxbc, g.ps_len, 0);
		return false;
	}
	if (g.stage == 2)
	{
		D3D11_DEPTH_STENCIL_DESC d{};
		d.DepthEnable = TRUE;

		d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		d.DepthFunc = D3D11_COMPARISON_ALWAYS;
		d.StencilEnable = FALSE;
		issue_desc_cmd(k_cmd_dss, &d, sizeof(d), 0);
		return false;
	}
    if(g.stage>=5 && g.stage<=8) {
        D3D11_DEPTH_STENCIL_DESC d{};
        d.DepthEnable=TRUE; d.DepthWriteMask=(g.stage==5 || g.stage==7)?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        const D3D11_COMPARISON_FUNC funcs[]={D3D11_COMPARISON_LESS_EQUAL,D3D11_COMPARISON_GREATER,
            D3D11_COMPARISON_GREATER_EQUAL,D3D11_COMPARISON_LESS};
        d.DepthFunc=funcs[g.stage-5];
        issue_desc_cmd(k_cmd_dss,&d,sizeof(d),0);return false;
    }

    if(g.stage==9 || g.stage==10) {
        D3D11_DEPTH_STENCIL_DESC d{};
        d.DepthEnable=TRUE;d.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc=g.stage==9?D3D11_COMPARISON_LESS_EQUAL:D3D11_COMPARISON_GREATER_EQUAL;
        issue_desc_cmd(k_cmd_dss,&d,sizeof(d),0);return false;
    }

	if (g.stage == 3)
	{
		D3D11_BUFFER_DESC d{};
		d.ByteWidth = (UINT)sizeof(NativePattern::Material);
		d.Usage = D3D11_USAGE_DEFAULT;
		d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		issue_desc_cmd(k_cmd_buf, &d, sizeof(d), 0);
		return false;
	}
	if (g.stage == 4)
	{
		D3D11_BLEND_DESC d{};
		auto& rt = d.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		issue_desc_cmd(k_cmd_blend, &d, sizeof(d), 0);
		return false;
	}
	return g.stage >= 11;
}

bool Active()
{

	return variables::ESP::nativeChams || WeaponVisuals::Active();
}

bool is_vt(std::uint64_t obj, std::uint64_t rva)
{
	if (!addr_ok((std::uintptr_t)obj) || (obj & 7) || !g.module)
		return false;
	const auto vt = memory->Read<std::uint64_t>(obj);

	const char* expected = nullptr;
	std::uint32_t subobject = 0;
	if (rva == Offsets::GeometryD3D11::VTableRva) expected = ".?AVGeometryD3D11@Graphics@RBX@@";
	else if (rva == Offsets::FastClusterBinding::VTableRva) expected = ".?AVFastClusterBinding@Graphics@RBX@@";
	else if (rva == Offsets::FastClusterEntity::VTableRva) expected = ".?AVFastClusterEntity@Graphics@RBX@@";
	else if (rva == Offsets::FastCluster::VTableRva) expected = ".?AVFastCluster@Graphics@RBX@@";
	else if (rva == Offsets::FastCluster::VTableRvaSub) {
		expected = ".?AVFastCluster@Graphics@RBX@@";
		subobject = static_cast<std::uint32_t>(Offsets::FastCluster::BindingSubobject);
	}
	if (!expected) return vt == g.module + rva;
	struct Type { std::string name; std::uint32_t offset = 0; };
	static std::unordered_map<std::uint64_t, Type> types;
	static DWORD pid = 0;
	if (pid != memory->get_process_id()) { types.clear(); pid = memory->get_process_id(); }
	auto found = types.find(vt);
	if (found == types.end()) {
		const auto& module = remote_module_range(L"RobloxPlayerBeta.exe");
		if (!module.has(vt) || vt < module.base + 8) return false;
		const auto col = memory->Read<std::uint64_t>(vt - 8);
		std::uint32_t header[6]{};
		if (!module.has(col) || memory->ReadRaw(col, header, sizeof(header)) != sizeof(header) ||
			header[0] != 1 || module.base + header[5] != col) return false;
		const auto nameAt = module.base + header[3] + 0x10;
		char name[96]{};
		if (!module.has(nameAt + sizeof(name)) || memory->ReadRaw(nameAt, name, sizeof(name)-1) != sizeof(name)-1)
			return false;
		found = types.emplace(vt, Type{name, header[1]}).first;
	}
	return found->second.name == expected && found->second.offset == subobject;
}

struct GeometryRange {
    std::uint64_t geom;
    std::uint64_t renderEntity=0;
    std::uint32_t topology, startIndex, baseVertex, indexCount, vertexCount;
    auto key() const { return std::make_tuple(renderEntity, geom, startIndex, baseVertex, indexCount, topology); }
};
void add_geom(std::uint64_t entity, std::size_t offset, std::vector<GeometryRange>& out,std::uint64_t owner=0)
{

    struct Descriptor { std::uint64_t geom, control; std::uint32_t words[5]; } d{};
    if (memory->ReadRaw(entity + offset, &d, sizeof(d)) != sizeof(d) ||
        !is_vt(d.geom, Offsets::GeometryD3D11::VTableRva)) return;
    if (d.words[0] > 6 || !d.words[3] || d.words[3] > 10000000u) return;
    GeometryRange range{d.geom,owner,d.words[0],d.words[1],d.words[2],d.words[3],d.words[4]};
    if (std::none_of(out.begin(),out.end(),[&](const GeometryRange& r){return r.key()==range.key();}))
        out.push_back(range);
}

bool typed_object(std::uint64_t object,const char* expected){
    const auto& module=remote_module_range(L"RobloxPlayerBeta.exe");auto vt=memory->Read<std::uint64_t>(object);
    if(!module.has(vt)||vt<module.base+8)return false;auto col=memory->Read<std::uint64_t>(vt-8);std::uint32_t hdr[6]{};
    if(!module.has(col)||memory->ReadRaw(col,hdr,sizeof(hdr))!=sizeof(hdr)||hdr[0]!=1||module.base+hdr[5]!=col)return false;
    char name[96]{};auto at=module.base+hdr[3]+16;
    return module.has(at+sizeof(name))&&memory->ReadRaw(at,name,sizeof(name)-1)==sizeof(name)-1&&std::strcmp(name,expected)==0;
}
void collect_fces(std::uint64_t part, std::vector<GeometryRange>& out,
    std::vector<std::uint64_t>& visitedClusters,bool allowInstanced=true)
{
	static std::uintptr_t clusterField = Offsets::BasePart::ClusterNode;
	std::uint64_t node =
		memory->Read<std::uint64_t>(part + clusterField);

    if(typed_object(node,".?AVInstancedBinding2@Graphics@RBX@@") || typed_object(node,".?AVInstancedBindingSpecialMesh@Graphics@RBX@@")){
        if(!allowInstanced)return;
        auto entity=memory->Read<std::uint64_t>(node+0x70);
        if(!typed_object(entity,".?AVInstancedEntity2@Graphics@RBX@@"))return;
        auto resource=memory->Read<std::uint64_t>(entity+0x98);
        if(!typed_object(resource,".?AUMeshResource@Graphics@RBX@@"))return;
        for(std::size_t offset=0xD0;offset<0x300;offset+=0x28)add_geom(resource,offset,out);
        return;
    }
	for (const auto seen : visitedClusters)
		if (node == seen || node == seen + Offsets::FastCluster::BindingSubobject) return;
	auto isCluster = [&](std::uint64_t p) {
		return is_vt(p, Offsets::FastClusterBinding::VTableRva) ||
			is_vt(p, Offsets::FastCluster::VTableRva) || is_vt(p, Offsets::FastCluster::VTableRvaSub);
	};
	if (!isCluster(node)) {

		std::uint64_t fields[11]{};
		if (memory->ReadRaw(part + 0x160, fields, sizeof(fields)) != sizeof(fields)) return;
		for (std::size_t i = 0; i < 11; ++i) {
			if (isCluster(fields[i])) { node = fields[i]; clusterField = 0x160 + i*8; break; }
		}
	}
	if (!addr_ok((std::uintptr_t)node))
		return;
	if (is_vt(node, Offsets::FastClusterBinding::VTableRva))
	{
		++g_nbind;
		node = memory->Read<std::uint64_t>(node + Offsets::FastClusterBinding::Owner);
	}
	if (!addr_ok((std::uintptr_t)node))
		return;

	std::uint64_t base = node;
	if (is_vt(node, Offsets::FastCluster::VTableRvaSub))
		base = node - Offsets::FastCluster::BindingSubobject;
	else if (!is_vt(node, Offsets::FastCluster::VTableRva))
	{
		++g_nunk;
		return;
	}
	if (std::find(visitedClusters.begin(), visitedClusters.end(), base) != visitedClusters.end())
		return;
	visitedClusters.push_back(base);
	++g_nfc;

    const std::size_t listOffsets[] = {Offsets::FastCluster::EntityBegin, 0x138};
    std::vector<std::uint64_t> seenEntities;
    for (const auto listOffset : listOffsets) {
        std::uint64_t bounds[2]{};
        if (memory->ReadRaw(base+listOffset,bounds,sizeof(bounds))!=sizeof(bounds)) continue;
        const auto begin=bounds[0], end=bounds[1];
        if (!addr_ok(begin) || (begin&7) || end<=begin || ((end-begin)&7)) continue;
        if (end-begin > std::uint64_t(k_max_ents)*8) { ++g.ndrop; continue; }
        const auto n=static_cast<std::size_t>((end-begin)/8);
        std::uint64_t entities[k_max_ents]{};
        if (memory->ReadRaw(begin,entities,n*8)!=n*8) continue;
        std::uint64_t confirm[2]{};
        if (memory->ReadRaw(base+listOffset,confirm,sizeof(confirm))!=sizeof(confirm) ||
            bounds[0]!=confirm[0] || bounds[1]!=confirm[1]) continue;
        for (std::size_t i=0;i<n;++i) {
            const auto ent=entities[i];
            if (!addr_ok(ent) || std::find(seenEntities.begin(),seenEntities.end(),ent)!=seenEntities.end() ||
                !is_vt(ent,Offsets::FastClusterEntity::VTableRva)) continue;
            seenEntities.push_back(ent);
            add_geom(ent,Offsets::FastClusterEntity::MaterialPtr,out);
            add_geom(ent,Offsets::FastClusterEntity::DecalMaterialPtr,out);
        }
    }
}

void fill_geoms(bool refreshTargets)
{
	if (!g.state || !g.geoms || g.stage < 11)
		return;

	auto off = [&]
	{
		memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 0);
		memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), 0);
		g.ntarget = 0;
	};

	if (!Active())
	{
		off();
		return;
	}

	const float dscale = 1.0f;
	const float dbias = 0.0f;
	static auto previous = std::chrono::steady_clock::now();
	static double phase = 0.0, weaponPhase=0.0, armPhase=0.0, glovePhase=0.0;
	const auto frameTime = std::chrono::steady_clock::now();
	const float speed = std::isfinite(variables::ESP::nativeChamsAnimationSpeed)
		? std::clamp(variables::ESP::nativeChamsAnimationSpeed, 0.0f, 15.0f) : 1.0f;

	phase += std::chrono::duration<double>(frameTime - previous).count() * speed;
	weaponPhase += std::chrono::duration<double>(frameTime-previous).count()*std::clamp(variables::Weapons::speed,0.f,15.f);
    const double elapsed=std::chrono::duration<double>(frameTime-previous).count();
    armPhase+=elapsed*std::clamp(variables::Weapons::arms.speed,0.f,15.f);
    glovePhase+=elapsed*std::clamp(variables::Weapons::gloves.speed,0.f,15.f);
	previous = frameTime;
	const float now_s = static_cast<float>(phase);

	int mode = variables::ESP::nativeChamsStyle;
	if (mode < 0) mode = 0;
	if (mode >= ShaderNameCount()) mode = ShaderNameCount() - 1;

	std::uint64_t local_addr = (std::uint64_t)Globals::localPlayer.GetModelRef().Addr;
	if (!local_addr)
		local_addr = (std::uint64_t)Globals::localPlayer.FindChildByClass("Character").Addr;
	const std::uint64_t local_player_addr = (std::uint64_t)Globals::localPlayer.Addr;

	auto& targets = g.targets;
	if (refreshTargets || targets.empty()) {
	targets.clear();
	targets.reserve(64);
	std::vector<GeometryRange> taken;
	taken.reserve(128);

	g_npick = 0;
	g_nunk = 0;
	g_nfc = 0;
	g_nbind = 0;
	g.ndrop = 0;

	auto push_character = [&](std::uint64_t character, int kind=0,bool firstPerson=false)
	{
		if (!addr_ok((std::uintptr_t)character))
			return;
		std::vector<std::uint64_t> visitedClusters;
		const PlayerCache::LimbAddrs emptyLimbs{};
        const auto& limbs = kind?emptyLimbs:PlayerCache::GetLimbs((std::uintptr_t)character);
		std::vector<std::uintptr_t> parts = {
			limbs.head, limbs.torso, limbs.lArm, limbs.rArm,
			limbs.lLeg, limbs.rLeg, limbs.upperTorso, limbs.lowerTorso,
			limbs.lUpperArm, limbs.lLowerArm, limbs.lHand,
			limbs.rUpperArm, limbs.rLowerArm, limbs.rHand,
			limbs.lUpperLeg, limbs.lLowerLeg, limbs.lFoot,
			limbs.rUpperLeg, limbs.rLowerLeg, limbs.rFoot};
        if(kind)parts={character};
        const auto bodyPartCount=parts.size();
        if(!kind) {
            struct Node { RBX::RbxInstance item; unsigned depth; };
            std::vector<Node> todo;
            for (const auto& child : RBX::RbxInstance(character).GetChildList()) {
                const auto cls=child.GetClass(),name=child.GetName();

                const bool cosmeticGun=cls=="Model" && (name.rfind("SAR-15",0)==0 || name.rfind("SGlock",0)==0);
                if(cls=="Accessory" || cls=="Hat" || cls=="Tool" || cosmeticGun)todo.push_back({child,0});
            }
            std::unordered_map<std::uintptr_t,bool> seen;
            while(!todo.empty() && seen.size()<256) {
                auto node=todo.back();todo.pop_back();
                if(!node.item.Addr || node.depth>8 || !seen.emplace(node.item.Addr,true).second)continue;
                const auto cls=node.item.GetClass();
                if(cls=="Part" || cls=="MeshPart" || cls=="UnionOperation" || cls=="WedgePart") {
                    const auto alpha=memory->Read<float>(node.item.Addr+Offsets::BasePart::Transparency);
                    if(std::isfinite(alpha) && alpha>=0 && alpha<.999f && node.item.GetPrimitivePtr())parts.push_back(node.item.Addr);
                }
                if(cls=="Script" || cls=="LocalScript" || cls=="ModuleScript" || cls=="ParticleEmitter" || cls=="Beam" || cls=="Trail")continue;
                for(auto child:node.item.GetChildList())todo.push_back({child,node.depth+1});
            }
        }

		std::uint64_t anchorPrimitive = kind?0:RBX::RbxInstance(limbs.hrp).GetPrimitivePtr();
		if (!anchorPrimitive)
			for (const std::uintptr_t part : parts)
				if (part && (anchorPrimitive = RBX::RbxInstance(part).GetPrimitivePtr()) != 0)
					break;
		for (std::size_t partIndex=0;partIndex<parts.size();++partIndex) {
            const auto part=parts[partIndex];
            const bool attachment=!kind && partIndex>=bodyPartCount;
			if (!part || targets.size() >= (std::size_t)k_max_targets)
				continue;
			std::vector<GeometryRange> geoms;
			collect_fces(part, geoms, visitedClusters,!attachment);
			std::uint64_t area = 0;
			for (const auto& range : geoms)
			{

                const auto seed = kind ? pattern_seed(character) : pattern_seed(character ^ (++area * 0x9E3779B97F4A7C15ull));
                if (std::any_of(taken.begin(), taken.end(), [&](const GeometryRange& r) { return r.key() == range.key(); }))
                    continue;
                taken.push_back(range);
				if (targets.size() >= (std::size_t)k_max_targets)
					break;
				Target t{};t.weapon=kind|(firstPerson?0x100:0)|(attachment?k_character_attachment:0);
                t.geom = range.geom;t.renderEntity=range.renderEntity;
                t.startIndex = range.startIndex; t.baseVertex = range.baseVertex;
                t.indexCount = range.indexCount; t.topology = range.topology;
				t.mat.pattern[1] = seed;

                t.anchorPrimitive = anchorPrimitive;
				targets.push_back(t);
			}
		}
	};

    for(const auto& surface:WeaponVisuals::surfaces)
        if(surface.firstPerson)push_character(surface.part,surface.kind,true);
	if(variables::ESP::nativeChams)for (const auto& player : PlayerCache::players)
	{

		const bool is_local = (local_addr && (std::uint64_t)player.characterAddr == local_addr) ||
			(local_player_addr && (std::uint64_t)player.playerAddr == local_player_addr);
		if (is_local || PlayerRules::Esp(player.userId) || !App::PassesChamChecks(player.isValid, player.characterAddr, player.teamAddr, player.health))
			continue;
		if(!App::WithinVisualRange(player.characterAddr,variables::ESP::nativeDistance,variables::ESP::nativeUnlimited))continue;
		++g_npick;
		push_character(player.characterAddr);
	}

	if (variables::ESP::nativeChams && local_addr && variables::ESP::meshChamsLocal &&
		App::PassesVisibilityChecks((std::uintptr_t)local_addr))
	{
		++g_npick;
		push_character(local_addr);
	}

	std::sort(targets.begin(), targets.end(), [](const Target& a, const Target& b) { return std::tie(a.renderEntity,a.geom,a.startIndex,a.baseVertex,a.indexCount,a.topology) < std::tie(b.renderEntity,b.geom,b.startIndex,b.baseVertex,b.indexCount,b.topology); });
	}

	const auto dimensions = memory->Read<RBX::Vec2>(
		Globals::renderEngine.Addr + Offsets::VisualEngine::Dimensions);
    memory->WriteRaw(g.state+k_capture_dimensions,&dimensions,sizeof(dimensions));

	auto view = Globals::renderEngine.GetViewMat();
	auto cam_raw = NativePattern::CameraFrom(view.data, dimensions.X, dimensions.Y);
	for (int attempt = 0; attempt < 3 && !cam_raw.valid; ++attempt)
	{
		view = Globals::renderEngine.GetViewMat();
		cam_raw = NativePattern::CameraFrom(view.data, dimensions.X, dimensions.Y);
	}
	g_nbody = 0;
	g_nobody = 0;
	g_nref = 0;
	g_nref_bad = 0;

    const NativePattern::Camera cam = cam_raw;
    const RBX::Mat4 ref_view = view;
	g_cam_ok = cam_raw.valid;

	const float size = std::isfinite(variables::ESP::nativeChamsPatternSize)
		? std::clamp(variables::ESP::nativeChamsPatternSize, 0.25f, 4.0f) : 1.0f;
	const float density = std::clamp(k_pattern_span / (k_body_studs * size),
		k_units_per_stud_min, k_units_per_stud_max);
	g_density = density;
	struct Body {
		bool ok = false;
		float org[4]{};
		float x[4]{1, 0, 0, 0};
		float y[4]{0, 1, 0, 0};
		float z[4]{0, 0, 1, 0};
	};

	std::unordered_map<std::uint64_t, Body> bodies;

    const bool classify=cam.valid;
    const bool reverse=cam.bz>0.0f;
    const auto visibleDepth=classify?memory->Read<std::uint64_t>(g.state+(reverse?k_depth_visible_reversed:k_depth_visible_standard)):0;
    const auto hiddenDepth=classify?memory->Read<std::uint64_t>(g.state+(reverse?k_depth_hidden_reversed:k_depth_hidden_standard)):0;
	for (auto& t : targets) {
		auto& mat = t.mat;
		t.viewMatrix = cam_raw.valid ? Globals::renderEngine.Addr + Offsets::VisualEngine::ViewMatrix : 0;
		if (cam.valid) std::memcpy(mat.live_view, ref_view.data, sizeof(mat.live_view));
        else std::memset(mat.live_view, 0, sizeof(mat.live_view));
		for (int i = 0; i < 3; ++i) {
			mat.cam_right[i] = cam.right[i];
			mat.cam_up[i] = cam.up[i];
			mat.cam_fwd[i] = cam.fwd[i];
			mat.cam_pos[i] = cam.pos[i];
		}
		mat.cam_right[3] = cam.ax;
		mat.cam_up[3] = cam.ay;
		mat.cam_fwd[3] = cam.az;
		mat.cam_pos[3] = 0.0f;
		mat.frame[0] = dimensions.X;
		mat.frame[1] = dimensions.Y;

		mat.frame[2] = k_image_bottom;
		mat.frame[3] = k_image_top;
		mat.depth[0] = cam.azz;
		mat.depth[1] = cam.bz;
		mat.depth[2] = cam.az;

		mat.depth[3] = 0.0f;
		mat.pattern[0] = 0.0f;
        mat.body_status[0] = 0.0f;mat.body_status[1]=0;mat.body_status[2]=0;mat.world_depth={};

		mat.pattern[2] = variables::ESP::nativeChamsOnly ? 1.0f : 0.0f;
		mat.pattern[3] = k_image_half_width;
		mat.org_pos[0] = mat.org_pos[1] = mat.org_pos[2] = mat.org_pos[3] = 0.0f;
		for (int i = 0; i < 4; ++i) {
			mat.org_x[i] = i == 0 ? 1.0f : 0.0f;
			mat.org_y[i] = i == 1 ? 1.0f : 0.0f;
			mat.org_z[i] = i == 2 ? 1.0f : 0.0f;
		}
		if (cam.valid) {
			auto it = bodies.find(t.anchorPrimitive);
			if (it == bodies.end()) {
				Body b{};
				if (addr_ok((std::uintptr_t)t.anchorPrimitive)) {

					const auto cfg = memory->Read<RBX::CFrame>(
						t.anchorPrimitive + Offsets::Primitive::Rotation);
					b.ok = NativePattern::BodyFrame(cfg.data, b.org, b.x, b.y, b.z);
				}
				it = bodies.emplace(t.anchorPrimitive, b).first;
			}
			if (it->second.ok) {
				++g_nbody;
				std::memcpy(mat.org_pos, it->second.org, sizeof(mat.org_pos));
				std::memcpy(mat.org_x, it->second.x, sizeof(mat.org_x));
				std::memcpy(mat.org_y, it->second.y, sizeof(mat.org_y));
				std::memcpy(mat.org_z, it->second.z, sizeof(mat.org_z));
				mat.pattern[0] = density;

				const float ref = it->second.org[0] * ref_view.data[12] +
					it->second.org[1] * ref_view.data[13] +
					it->second.org[2] * ref_view.data[14] + ref_view.data[15];
				mat.depth[3] = std::isfinite(ref) && ref > 0.0f ? ref : 0.0f;
				if (mat.depth[3] > 0.0f)
					++g_nref;
				else
					++g_nref_bad;
			} else {
				++g_nobody;
			}
		}
		else {

			++g_nobody;
		}
		for (int i = 0; i < 3; ++i) {
			mat.color[i] = variables::ESP::nativeChamsColor[i];
			mat.glow[i] = variables::ESP::nativeChamsGlowColor[i];
		}
		mat.color[3] = std::clamp(variables::ESP::nativeChamsOpacity, 0.0f, 1.0f) *
			std::clamp(variables::ESP::nativeChamsColor[3], 0.0f, 1.0f);
		mat.glow[3] = variables::ESP::nativeChamsGlow ? variables::ESP::nativeChamsGlowStrength : 0.0f;
        t.occlusion=classify && visibleDepth && hiddenDepth;
        t.visibleDepthState=visibleDepth;t.occludedDepthState=hiddenDepth;

        if(t.weapon&k_character_attachment)
            t.visibleDepthState=memory->Read<std::uint64_t>(g.state+(reverse?k_attachment_visible_reversed:k_attachment_visible_standard));
        for(int i=0;i<4;++i)t.occludedColor[i]=std::clamp(variables::ESP::nativeChamsOccludedColor[i],0.0f,1.0f);
        t.occludedColor[3]*=std::clamp(variables::ESP::nativeChamsOpacity,0.0f,1.0f);
        t.occludedMode=std::clamp(variables::ESP::nativeChamsOccludedStyle,0,ShaderNameCount()-1);
		mat.mode = mode;
		mat.time = now_s;
		mat.depth_scale = dscale;
		mat.depth_bias = dbias;
        const auto kind=t.weapon&0xff;
        const bool firstPerson=(t.weapon&0x100)!=0;
        if(kind>=1&&kind<=3){
            auto& limb=kind==3?variables::Weapons::gloves:variables::Weapons::arms;
            const bool gun=kind==1;
            const float opacity=std::clamp(gun?variables::Weapons::opacity:limb.opacity,0.f,1.f);
            std::memcpy(mat.color,gun?variables::Weapons::color:limb.color,16);mat.color[3]*=opacity;
            std::memcpy(mat.glow,gun?variables::Weapons::glowColor:limb.glowColor,16);
            mat.glow[3]=(gun?variables::Weapons::glow:limb.glow)?(gun?variables::Weapons::glowStrength:limb.glowStrength):0;
            mat.mode=std::clamp(gun?variables::Weapons::nativeStyle:limb.style,0,ShaderNameCount()-1);
            mat.time=static_cast<float>(gun?weaponPhase:kind==2?armPhase:glovePhase);

            const float surfaceScale=gun?std::clamp(variables::Weapons::scale,.01f,100.f)*.1f:(std::max)(.1f,limb.scale);
            mat.pattern[0]=density/surfaceScale;
            mat.pattern[2]=(gun&&variables::Weapons::showOriginal)?0.f:1.f;
            mat.body_status[1]=firstPerson?1.f:0.f;

            if(firstPerson)t.occlusion=0;
            if(firstPerson&&variables::Weapons::hide){mat.pattern[2]=1;mat.color[3]=0;mat.glow[3]=0;}
            std::memcpy(t.occludedColor,variables::ESP::nativeChamsOccludedColor,16);t.occludedColor[3]*=opacity;
            t.occludedMode=std::clamp(variables::ESP::nativeChamsOccludedStyle,0,ShaderNameCount()-1);
        }
        if(!variables::ESP::nativeChamsOcclusion){std::memcpy(t.occludedColor,mat.color,16);t.occludedMode=mat.mode;}

	}

	g.ntarget = (int)targets.size();
	if (targets.empty())
	{
		off();
		return;
	}

	const auto active = memory->Read<std::uint64_t>(g.state + offsetof(RemoteState, geoms));
	const auto pending = memory->Read<std::uint64_t>(g.state + k_pending_geoms_off);

    static std::uintptr_t tableAllocation = 0;
    static std::array<std::size_t, 3> tableUsed{};
    if (tableAllocation != g.geoms) { tableAllocation = g.geoms; tableUsed.fill(k_max_targets); }
    int writeSlot = -1;
	std::uintptr_t dst = 0;
	for (int slot = 0; slot < 3; ++slot) {
		const auto candidate = g.geoms + slot * k_max_targets * (sizeof(Target) + sizeof(BodyCache));
		if (candidate != active && candidate != pending) { dst = candidate; writeSlot = slot; break; }
	}
	if (!dst) return;

	std::vector<Target> snapshot(std::max(tableUsed[writeSlot], targets.size()));
	std::copy(targets.begin(), targets.end(), snapshot.begin());
    std::unordered_map<std::uint64_t,std::size_t> bodySlots;
    std::vector<BodyCache> drawBodies;
    for (std::size_t i=0;i<targets.size();++i) {
        const auto primitive=targets[i].anchorPrimitive;
        if(!primitive){snapshot[i].liveBody=0;continue;}
        auto inserted=bodySlots.emplace(primitive,drawBodies.size());
        if(inserted.second) { BodyCache body{}; body.source=primitive+Offsets::Primitive::Rotation; drawBodies.push_back(body); }
        snapshot[i].liveBody=dst+k_max_targets*sizeof(Target)+inserted.first->second*sizeof(BodyCache);
    }
    const auto bodyBytes=drawBodies.size()*sizeof(BodyCache);
    if (memory->WriteRaw(dst+k_max_targets*sizeof(Target),drawBodies.data(),bodyBytes)!=bodyBytes) { off(); return; }

	if (memory->WriteRaw(dst, snapshot.data(), snapshot.size() * sizeof(Target)) != snapshot.size() * sizeof(Target)) {
		off();
		return;
	}
	tableUsed[writeSlot] = targets.size();
	memory->Write<std::uint64_t>(g.state + k_pending_geoms_off, dst);
	memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), k_max_targets);
	memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 1);
}

bool install()
{
	if (g.hooked)
		return true;
	const auto now = std::chrono::steady_clock::now();
	if (g_fail.time_since_epoch().count() != 0 &&
		now - g_fail < std::chrono::seconds(2))
		return false;
    if(!shader_ready())return false;
    struct Attempt {
        bool success=false;
        ~Attempt(){g_fail=success?std::chrono::steady_clock::time_point{}:std::chrono::steady_clock::now();}
    } attempt;
	const std::uintptr_t base = module_base();
	if (!base || !memory->GetHandle() || !memory->IsConnected())
	{
		g_fail = now;
		return false;
	}
	if (!g.state)
		g.state = memory->Alloc(0x1000, PAGE_READWRITE);
	if (!g.state)
	{
		fail("alloc");
		g_fail = now;
		return false;
	}
	if (!find_d3d())
	{
		static const char* kStage[] = { "ok", "no renderview", "no vtables (RTTI)",
			"no DeviceD3D11 instance", "no real device", "no DeviceContextD3D11",
			"ctx fingerprint mismatch" };
		fail(g_diag.stage >= 0 && g_diag.stage < 7 ? kStage[g_diag.stage] : "no device");
		std::printf("[NativeChams] scan failed: stage=%d (%s), wrap=0x%llX\n",
			g_diag.stage,
			g_diag.stage >= 0 && g_diag.stage < 7 ? kStage[g_diag.stage] : "?",
			(unsigned long long)g_diag.wrap);
		g_fail = now;
		return false;
	}
	if (!d3d11_loaded_locally())
	{

		fail("d3d11 local");
		g_fail = now;
		return false;
	}
	if (!setup_meta())
	{
		fail("gpu meta");
		g_fail = now;
		return false;
	}

	auto cr = make_create_stub(g.state);
	auto dr = make_draw_thunk(g.state);
	auto ps = make_present_thunk(g.state, 0x10000);
	if (cr.empty() || dr.empty() || ps.empty())
	{
		fail("thunk");
		g_fail = now;
		return false;
	}
	const auto align16 = [](std::size_t n) { return (n + 15u) & ~std::size_t(15); };
	const std::size_t nps = align16(ps.size());
	const std::size_t ncr = align16(cr.size());
	const std::size_t ndr = align16(dr.size());

	const std::size_t nall = nps + ncr + align16(sizeof(k_native_depth_capture_code));
	const std::uintptr_t cave = find_dll_cave(nall, 0);
	if (!cave)
	{
		fail("cave");
		g_fail = now;
		return false;
	}
	g.cave = cave;
	g.cave_n = nall;
	const std::uintptr_t drawCave = find_dll_cave(ndr, 0);
	if (!drawCave)
	{
		fail("draw cave");
		g.cave = 0;
		return false;
	}
	g.draw_cave = drawCave;
	g.draw_cave_n = ndr;
	g.present_thunk = cave;
	g.create_stub = cave + nps;
	g.draw_thunk = drawCave;
	if (!g.vt_mem)
		g.vt_mem = memory->Alloc(0x2000, PAGE_READWRITE);
	if (!g.vt_mem)
	{
		fail("alloc vt");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	ps = make_present_thunk(g.state, g.create_stub);
	if (ps.size() > nps)
	{
		fail("thunk jmp");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	if (!write_exec(g.create_stub, cr.data(), cr.size())
		|| !write_exec(g.draw_thunk, dr.data(), dr.size())
        || !write_exec(cave+nps+ncr,k_native_depth_capture_code,sizeof(k_native_depth_capture_code))
		|| !write_exec(g.present_thunk, ps.data(), ps.size()))
	{
		fail("write stub");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	FlushInstructionCache(memory->GetHandle(), (void*)g.cave, nall);
	FlushInstructionCache(memory->GetHandle(), (void*)g.draw_cave, ndr);
	mark_cfg(g.present_thunk);
	mark_cfg(g.create_stub);
	mark_cfg(g.draw_thunk);
    mark_cfg(cave+nps+ncr);
    memory->Write<std::uint64_t>(g.state+k_capture_fn,cave+nps+ncr);
	if (!patch_vtables())
	{
		fail("vt");
		unpatch();
		g.cave = 0;
		g_fail = now;
		return false;
	}
    attempt.success=true;
	g.module = base;
	g.hooked = true;
	g.stage = 0;
	if (!g.logged)
	{
		g.logged = true;
		std::printf("[NativeChams] on stage=%s patches=%d cave=0x%llX swapvt=%d dcvt=%d\n",
			g.dc_hooked ? "present+draw" : "present only",
			g.npatch, (unsigned long long)g.cave, g.nswvt, g.ndcvt);
	}
	return true;
}

const char* g_occl_note = nullptr;

void occl_note(const char* msg)
{
	if (g_occl_note == msg)
		return;
	g_occl_note = msg;
	std::printf("[NativeChams] %s\n", msg);
}

uintptr_t occl_flag()
{
	const std::uintptr_t base = module_base();
	if (!base || !memory || !memory->GetHandle())
		return 0;
	const auto& module = remote_module_range(L"RobloxPlayerBeta.exe");
    if(!module.ok() || module.base!=base)return 0;
    const auto addr=NativeOcclusion::resolve(base,module.end-module.base,[](std::uint64_t at,void* out,std::size_t n){
        return memory->ReadRaw(at,out,n)==n;
    });
    if(!addr)return 0;
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQueryEx(memory->GetHandle(), (void*)addr, &mbi, sizeof(mbi)))
		return 0;
	if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
		return 0;
	const DWORD p = mbi.Protect & 0xFF;
	const bool writable = p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
		p == PAGE_EXECUTE_READWRITE;
	const bool exec = p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ ||
		p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
	if (!writable || exec)
		return 0;
	std::uint8_t v = 0xFF;
	if (memory->ReadRaw(addr, &v, 1) != 1 || v > 1)
		return 0;
	return addr;
}

void apply_occlusion(bool off)
{
	if (!off)
	{
		if (!g.occl_off)
			return;
		if (g.occl_addr)
			memory->Write<std::uint8_t>(g.occl_addr, g.occl);
		g.occl_addr = 0;
		g.occl_off = false;
		occl_note("occlusion culling restored");
		return;
	}
	if (g.occl_off)
	{

		if (g.occl_addr && memory->Read<std::uint8_t>(g.occl_addr) != 0)
		{
			++g.occl_reverts;
			memory->Write<std::uint8_t>(g.occl_addr, 0);
			if (g.occl_reverts == 60)
				occl_note("client changed the verified culling flag repeatedly while Native was active");
		}
		return;
	}
	const std::uintptr_t addr = occl_flag();
	if (!addr)
	{
		occl_note("occlusion flag not found (stale offset) - hidden targets stay hidden");
		return;
	}
	const std::uint8_t saved = memory->Read<std::uint8_t>(addr);
	memory->Write<std::uint8_t>(addr, 0);
	g.occl_addr = addr;
	g.occl = saved;
	g.occl_reverts = 0;
	g.occl_off = true;
	occl_note(memory->Read<std::uint8_t>(addr) == 0
		? "verified character occlusion-culling flag disabled"
		: "occlusion flag is not writable - hidden targets stay hidden");
}

void teardown()
{
	apply_occlusion(false);
	if (g.state)
		memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 0);
	if (!g.hooked)
		return;

    if(g.state && memory->IsConnected()) {
        const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(50);
        while(memory->Read<std::uint64_t>(g.state+k_capture_data) && std::chrono::steady_clock::now()<until)
            Sleep(1);
    }
	unpatch();
	g.hooked = false;
	g.dc_hooked = false;
	g.dc_armed = false;
	g.dc_arm = 0;
}

}

void Prepare(){
    start_shader();

    if(memory->IsConnected())find_d3d();
}
const char* StartupStatus(){
    using State=NativePreparation::Shader::State;
    if(localShader.Status()==State::Preparing)return "Preparing native graphics...";
    if(localShader.Status()==State::Failed)return "Native graphics preparation failed";
    if(g.hooked)return g.stage>=11?"Ready":"Starting native renderer...";
    return g_why?g_why:"Ready to enable";
}

void Tick()
{
	if (!memory->IsConnected())
	{
		teardown();
		g.device = 0;
		g.ctx = 0;
		g.swap = 0;
		g.dc = 0;
		g.orig_present = 0;
		g.orig_draw = 0;
		return;
	}

	const std::uintptr_t base = module_base();
	if (g.module && base && g.module != base)
	{

		teardown();
		if (g.state) { memory->Free(g.state); g.state = 0; }
		if (g.geoms) { memory->Free(g.geoms); g.geoms = 0; }
		if (g.vt_mem) { memory->Free(g.vt_mem); g.vt_mem = 0; }
		if (g.ps_dxbc) { memory->Free(g.ps_dxbc); g.ps_dxbc = 0; }
		g = {};
		g_why = nullptr;
	}

	if (!Active())
	{
		if (g.hooked)
		{
			teardown();
			g.stage = 0;
			g.logged = false;
		}
		if (g.state)
		{
			memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 0);
			memory->Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), 0);
		}
		g.ntarget = 0;
		g.targets.clear();
		apply_occlusion(false);
		return;
	}
	const bool rev = want_reverse();
	if (!g.hooked)
	{
		g.depth_rev = rev;
		if (!install())
			return;
		std::printf("[NativeChams] depth=%s\n", rev ? "reverse" : "standard");
	}

	if (g.dc_hooked && !g.dc_armed && g.dc_arm && g.state &&
		memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, ready)))
	{
		DWORD prot = 0;
		if (memory->Protect(g.dc_arm, 8, PAGE_READWRITE, &prot))
		{
			memory->Write<std::uint64_t>(g.dc_arm, (std::uint64_t)g.draw_thunk);
			memory->Protect(g.dc_arm, 8, prot, nullptr);
			g.dc_armed = true;
			std::printf("[NativeChams] resources ready -> draw hook armed\n");
		}
	}

	if (g.dc_armed && !g.draw_live && g.state)
	{
		auto& first_match = g.first_match;
		const auto now_live = std::chrono::steady_clock::now();
		const std::uint32_t hits =
			memory->Read<std::uint32_t>(g.state + k_draw_match_off);
		if (hits && first_match.time_since_epoch().count() == 0)
			first_match = now_live;
		if (hits >= 400 &&
			(first_match.time_since_epoch().count() == 0 ||
				now_live - first_match >= std::chrono::milliseconds(1500)))
		{
			memory->Write<std::uint32_t>(g.state + k_draw_live_off, 1);
			g.draw_live = true;
			std::printf("[NativeChams] geometry matches observed (%u); enabling render-state path\n", hits);
		}
	}

	if (g.stage >= 11 && g.depth_rev != rev)
	{
		g.depth_rev = rev;

	}
	if(g.stage<11)pump_creates();

	static auto last_fill = std::chrono::steady_clock::time_point{};
	static auto last_material = std::chrono::steady_clock::time_point{};
	const auto now_fill = std::chrono::steady_clock::now();
	if (last_material.time_since_epoch().count() == 0 || now_fill - last_material >= std::chrono::milliseconds(16))
	{
		const bool refresh = last_fill.time_since_epoch().count() == 0 || now_fill - last_fill >= std::chrono::milliseconds(60);
		last_material = now_fill;
		if (refresh) last_fill = now_fill;
		fill_geoms(refresh);
	}

	apply_occlusion(((variables::ESP::nativeChams && variables::ESP::nativeChamsWalls) || (variables::Weapons::native && variables::ESP::nativeChamsWalls)) && g.ntarget > 0);
	static auto last_stat = std::chrono::steady_clock::time_point{};
	static int last_stage = -1;
	if (g.stage == 11 && last_stage != 11)
	{
		last_stage = 11;
		std::printf("[NativeChams] ready (drawing)\n");
	}
	const auto now = std::chrono::steady_clock::now();
	if (last_stat.time_since_epoch().count() == 0 ||
		now - last_stat >= std::chrono::seconds(5))
	{
		last_stat = now;
		std::printf("[NativeChams] st=%d rdy=%u armed=%d live=%d match=%u calls=%u draws=%u geom=%d pick=%d unk=%d\n",
			g.stage, Ready(), g.dc_armed ? 1 : 0, g.draw_live ? 1 : 0, Matched(),
			Calls(), Draws(), g.ntarget, g_npick, g_nunk);
		std::printf("[NativeChams] walk: clusters=%d bindings=%d unk=%d dropped=%d\n",
			g_nfc, g_nbind, g_nunk, g.ndrop);
		std::printf("[NativeChams] map: cam=%s body=%d/%d ref=%d/%d density=%.1f/stud\n",
			g_cam_ok ? "ok" : "invalid", g_nbody, g_nbody + g_nobody,
			g_nref, g_nref + g_nref_bad, (double)g_density);
	}
}

bool WorldDepthNeeded() { return false; }
void SubmitWorldDepth(unsigned, const NativeWorldDepth::Snapshot&) {}

void Stop()
{
	teardown();
}

bool Hooked()
{
	return g.hooked;
}

bool DrawHooked()
{
	return g.dc_hooked;
}

const char* Why()
{
	if (g.hooked)
		return nullptr;
	return g_why ? g_why : "off";
}

std::uint32_t Stage()
{
	return (std::uint32_t)g.stage;
}

std::uint32_t Ready()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, ready));
}

std::uint32_t Draws()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, draws));
}

std::uint32_t Items()
{
	if (!g.state)
		return 0;
	return static_cast<std::uint32_t>(g.ntarget);
}

std::uint32_t Matched()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + k_draw_match_off);
}

std::uint32_t Calls()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + k_draw_calls_off);
}

bool DrawLive()
{
	return g.draw_live;
}

std::uint32_t Entered()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, entered));
}

std::uint32_t Creates()
{
	if (!g.state)
		return 0;
	return memory->Read<std::uint32_t>(g.state + offsetof(RemoteState, creates));
}

const char* MapStatus()
{
	static char buf[160];
	std::snprintf(buf, sizeof(buf),
		"cam %s | body %d/%d | depth ref %d/%d | %.0f u/stud",
		g_cam_ok ? "ok" : "held", g_nbody, g_nbody + g_nobody,
		g_nref, g_nref + g_nref_bad, (double)g_density);
	return buf;
}

const char* OcclusionNote()
{

	return g_occl_note ? g_occl_note : "";
}

const char* const* ShaderNames()
{
	return k_shader_names;
}

int ShaderNameCount()
{
	return static_cast<int>(sizeof(k_shader_names) / sizeof(k_shader_names[0]));
}

std::string PreviewShaderSource()
{
    std::string source = std::string(k_hlsl_head) + k_hlsl_body + k_hlsl_styles;

    const auto start = source.find("float3 frag_local(float4 pos)");
    const auto end = source.find("float noise3", start);
    if (start == std::string::npos || end == std::string::npos) return {};
    source.replace(start, end-start,
        "static float3 previewLocal;\nfloat3 frag_local(float4 pos){return previewLocal;}\n");
    const auto normalStart=source.find("float3 screen_normal(float4 pos)");
    const auto normalEnd=source.find("float hash21",normalStart);
    source.replace(normalStart,normalEnd-normalStart,"float3 screen_normal(float4 pos){return normalize(float3(-ddx(pos.z)*512.0,-ddy(pos.z)*512.0,1));}\n");
    source += R"PREVIEW(
struct PreviewInput { float4 pos:SV_POSITION; float3 local:TEXCOORD0; float4 tint:COLOR0; float seed:TEXCOORD1; };
Out preview_ps(PreviewInput input) {
    previewLocal=input.local;
    Out o=ps_main(input.pos,true);

    float3 background=float3(.035,.04,.055);
    float3 underlay=pattern.w>.5 ? input.tint.rgb : background;

    o.c=float4(lerp(underlay,pow(saturate(o.c.rgb),1.0/2.2),o.c.a),1);
    return o;
}
 )PREVIEW";

    const std::string seedLine="float seed = isfinite(pattern.y) ? pattern.y : 0.0;";
    source.insert(0,"static float previewSeed;\n");
    const auto actual=source.find(seedLine);
    if(actual!=std::string::npos)source.replace(actual,seedLine.size(),"float seed = previewSeed;");
    const auto entry=source.find("previewLocal=input.local;");
    source.insert(entry,"previewSeed=input.seed;\n");

    const auto clipAt=source.find("clip(color.a - 0.00001);");
    if(clipAt!=std::string::npos)source.erase(clipAt,std::string("clip(color.a - 0.00001);").size());
    return source;
}

}
}
}
