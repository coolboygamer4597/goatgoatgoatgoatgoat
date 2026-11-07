#include "media.h"
#include "../../../../render/RetroUi.h"
#include "music_player_internal.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdint>

namespace native_music_player::detail {

struct MusicRgb {
    float r = 0.f, g = 0.f, b = 0.f;
};

struct MusicPalette {
    ImVec4 top      = ImVec4(0.075f, 0.170f, 0.220f, 1.f);
    ImVec4 bottom   = ImVec4(0.040f, 0.105f, 0.155f, 1.f);
    ImVec4 accentA  = ImVec4(0.20f, 0.53f, 0.71f, 1.f);
    ImVec4 accentB  = ImVec4(0.20f, 0.32f, 0.57f, 1.f);
    ImVec4 accentC  = ImVec4(0.48f, 0.20f, 0.50f, 1.f);
    ImVec4 progress = ImVec4(0.78f, 0.88f, 0.93f, 1.f);
};

namespace {

MusicPalette s_paletteCurrent;
MusicPalette s_paletteTarget;
bool  s_paletteReady = false;
float s_motion = 0.f;

float Luma(const MusicRgb& c) {
    return c.r * 0.2126f + c.g * 0.7152f + c.b * 0.0722f;
}

float Saturation(const MusicRgb& c) {
    float hi = std::max(c.r, std::max(c.g, c.b));
    float lo = std::min(c.r, std::min(c.g, c.b));
    return hi > 0.0001f ? (hi - lo) / hi : 0.f;
}



MusicRgb Mix(const MusicRgb& a, const MusicRgb& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t
    };
}

MusicRgb Tone(MusicRgb c, float value, float saturationScale,
              float saturationFloor) {
    float hi = std::max(c.r, std::max(c.g, c.b));
    float lo = std::min(c.r, std::min(c.g, c.b));
    float delta = hi - lo;
    float hue = 0.f;
    if (delta > 0.0001f) {
        if (hi == c.r) hue = std::fmod((c.g - c.b) / delta, 6.f);
        else if (hi == c.g) hue = (c.b - c.r) / delta + 2.f;
        else hue = (c.r - c.g) / delta + 4.f;
        hue /= 6.f;
        if (hue < 0.f) hue += 1.f;
    }
    float saturation = hi > 0.0001f ? delta / hi : 0.f;
    saturation = std::clamp(saturation * saturationScale + 0.04f,
                            saturationFloor, 0.92f);
    value = std::clamp(value, 0.f, 1.f);

    float h6 = hue * 6.f;
    int sector = (int)std::floor(h6);
    float fraction = h6 - (float)sector;
    float p = value * (1.f - saturation);
    float q = value * (1.f - saturation * fraction);
    float t = value * (1.f - saturation * (1.f - fraction));
    switch ((sector % 6 + 6) % 6) {
    case 0: return { value, t, p };
    case 1: return { q, value, p };
    case 2: return { p, value, t };
    case 3: return { p, q, value };
    case 4: return { t, p, value };
    default:return { value, p, q };
    }
}

ImVec4 ToVec4(const MusicRgb& c) {
    return ImVec4(std::clamp(c.r, 0.f, 1.f),
                  std::clamp(c.g, 0.f, 1.f),
                  std::clamp(c.b, 0.f, 1.f), 1.f);
}

ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}

ImU32 Color(ImVec4 color, float alpha = 1.f) {
    color.w = std::clamp(color.w * alpha, 0.f, 1.f);
    return ImGui::ColorConvertFloat4ToU32(color);
}

MusicPalette ExtractPalette(const uint8_t* bgra, int width, int height) {
    MusicPalette fallback;
    if (!bgra || width <= 0 || height <= 0) return fallback;

    constexpr int kBits = 5;
    constexpr int kLevels = 1 << kBits;
    constexpr int kBucketCount = kLevels * kLevels * kLevels;
    struct Bucket { float r = 0, g = 0, b = 0, weight = 0; };
    static std::vector<Bucket> buckets;
    buckets.assign(kBucketCount, Bucket{});

    const int stepX = std::max(1, width / 200);
    const int stepY = std::max(1, height / 200);
    MusicRgb overall = {};
    float overallWeight = 0.f;

    for (int y = 0; y < height; y += stepY) {
        for (int x = 0; x < width; x += stepX) {
            const uint8_t* px = bgra + ((size_t)y * width + x) * 4;
            const float alpha = px[3] / 255.f;
            if (alpha < 0.25f) continue;
            const MusicRgb c = { px[2] / 255.f, px[1] / 255.f, px[0] / 255.f };
            const float value = std::max(c.r, std::max(c.g, c.b));
            const float sat = Saturation(c);

            overall.r += c.r * alpha; overall.g += c.g * alpha;
            overall.b += c.b * alpha; overallWeight += alpha;

            if (value < 0.10f || (value > 0.96f && sat < 0.10f)) continue;

            const int ri = std::min(kLevels - 1, (int)(c.r * kLevels));
            const int gi = std::min(kLevels - 1, (int)(c.g * kLevels));
            const int bi = std::min(kLevels - 1, (int)(c.b * kLevels));
            Bucket& bucket = buckets[(size_t)(ri * kLevels + gi) * kLevels + bi];
            const float w = alpha * (0.85f + sat * 0.35f);
            bucket.r += c.r * w; bucket.g += c.g * w; bucket.b += c.b * w;
            bucket.weight += w;
        }
    }

    if (overallWeight > 0.f) {
        overall.r /= overallWeight;
        overall.g /= overallWeight;
        overall.b /= overallWeight;
    }

    struct Candidate { MusicRgb color; float weight; float score; };
    std::vector<Candidate> candidates;
    candidates.reserve(64);
    for (const Bucket& bucket : buckets) {
        if (bucket.weight <= 0.f) continue;
        const MusicRgb c = { bucket.r / bucket.weight,
                             bucket.g / bucket.weight,
                             bucket.b / bucket.weight };
        const float sat = Saturation(c);
        const float lum = Luma(c);
        const float lumWindow = std::exp(-((lum - 0.58f) * (lum - 0.58f)) / 0.10f);
        const float score = std::sqrt(bucket.weight) *
                            (0.80f + sat * 0.45f) * (0.30f + lumWindow);
        candidates.push_back({ c, bucket.weight, score });
    }
    if (candidates.empty()) return fallback;
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& x, const Candidate& y) { return x.score > y.score; });

    MusicRgb picks[3];
    int picked = 0;
    for (const Candidate& cand : candidates) {
        bool tooClose = false;
        for (int i = 0; i < picked && !tooClose; ++i) {
            const float dr = cand.color.r - picks[i].r;
            const float dg = cand.color.g - picks[i].g;
            const float db = cand.color.b - picks[i].b;
            tooClose = (dr * dr + dg * dg + db * db) < 0.055f;
        }
        if (tooClose) continue;
        picks[picked++] = cand.color;
        if (picked == 3) break;
    }
    while (picked < 3) {
        picks[picked] = picked == 0 ? overall : picks[picked - 1];
        ++picked;
    }

    const MusicRgb a = picks[0], b = picks[1], c = picks[2];
    MusicPalette result;
    result.top = ToVec4(Tone(Mix(overall, a, 0.14f), 0.35f, 0.34f, 0.06f));
    result.bottom = ToVec4(Tone(Mix(overall, Mix(a, b, 0.5f), 0.12f),
                                0.28f, 0.34f, 0.05f));
    result.accentA = ToVec4(Tone(a, 0.70f, 1.10f, 0.26f));
    result.accentB = ToVec4(Tone(b, 0.64f, 1.08f, 0.24f));
    result.accentC = ToVec4(Tone(c, 0.66f, 1.08f, 0.24f));
    result.progress = ToVec4(Tone(Mix(Mix(a, b, 0.45f), c, 0.22f),
                                  0.92f, 1.05f, 0.16f));
    return result;
}

void UpdatePalette() {
    if (!s_paletteReady) return;
    float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    float blend = 1.f - std::exp(-3.8f * dt);
    s_paletteCurrent.top = Lerp(s_paletteCurrent.top, s_paletteTarget.top, blend);
    s_paletteCurrent.bottom = Lerp(s_paletteCurrent.bottom, s_paletteTarget.bottom, blend);
    s_paletteCurrent.accentA = Lerp(s_paletteCurrent.accentA, s_paletteTarget.accentA, blend);
    s_paletteCurrent.accentB = Lerp(s_paletteCurrent.accentB, s_paletteTarget.accentB, blend);
    s_paletteCurrent.accentC = Lerp(s_paletteCurrent.accentC, s_paletteTarget.accentC, blend);
    s_paletteCurrent.progress = Lerp(s_paletteCurrent.progress, s_paletteTarget.progress, blend);
}

void DrawRoundedGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                         ImU32 topLeft, ImU32 topRight, ImU32 bottomRight,
                         ImU32 bottomLeft, float rounding) {
    const int firstVertex = dl->VtxBuffer.Size;
    dl->PathRect(min, max, rounding);
    dl->PathFillConvex(IM_COL32_WHITE);
    const float width = std::max(max.x - min.x, 0.001f);
    const float height = std::max(max.y - min.y, 0.001f);
    auto channel = [](ImU32 color, int shift) {
        return (float)((color >> shift) & 0xFF);
    };
    auto bilerp = [&](int shift, float x, float y) {
        float top = channel(topLeft, shift) +
            (channel(topRight, shift) - channel(topLeft, shift)) * x;
        float bottom = channel(bottomLeft, shift) +
            (channel(bottomRight, shift) - channel(bottomLeft, shift)) * x;
        return top + (bottom - top) * y;
    };
    for (int i = firstVertex; i < dl->VtxBuffer.Size; ++i) {
        ImDrawVert& vertex = dl->VtxBuffer[i];
        float x = std::clamp((vertex.pos.x - min.x) / width, 0.f, 1.f);
        float y = std::clamp((vertex.pos.y - min.y) / height, 0.f, 1.f);
        int coverageAlpha = (int)((vertex.col >> IM_COL32_A_SHIFT) & 0xFF);
        int gradientAlpha = (int)bilerp(IM_COL32_A_SHIFT, x, y);
        vertex.col = IM_COL32(
            (int)bilerp(IM_COL32_R_SHIFT, x, y),
            (int)bilerp(IM_COL32_G_SHIFT, x, y),
            (int)bilerp(IM_COL32_B_SHIFT, x, y),
            gradientAlpha * coverageAlpha / 255);
    }
}

void DrawBlob(ImDrawList* dl, ImVec2 center, float radius,
              ImVec4 color, float alpha) {
    constexpr int layers = 14;
    for (int i = layers; i >= 1; --i) {
        float scale = (float)i / (float)layers;
        float layerAlpha = alpha * (1.15f - scale) / (float)layers;
        dl->AddCircleFilled(center, radius * scale,
                            Color(color, layerAlpha), 48);
    }
}

}

void EnsureVisualPalette() {
    if (s_paletteReady) return;
    s_paletteCurrent = MusicPalette{};
    s_paletteTarget = s_paletteCurrent;
    s_paletteReady = true;
}

void SetPaletteFromArt(const uint8_t* bgra, int width, int height) {
    s_paletteTarget = ExtractPalette(bgra, width, height);
    if (!s_paletteReady) {
        s_paletteCurrent = s_paletteTarget;
        s_paletteReady = true;
    }
}

void SamplePaletteRegionsBGRA(const uint8_t* bgra, int width, int height,
                              int gridSide, float* outRgb) {
    if (!bgra || width <= 0 || height <= 0 || gridSide <= 0) return;
    const MusicRgb dominantColors[3] = {
        { s_paletteTarget.accentA.x, s_paletteTarget.accentA.y,
          s_paletteTarget.accentA.z },
        { s_paletteTarget.accentB.x, s_paletteTarget.accentB.y,
          s_paletteTarget.accentB.z },
        { s_paletteTarget.accentC.x, s_paletteTarget.accentC.y,
          s_paletteTarget.accentC.z }
    };
    for (int by = 0; by < gridSide; ++by) {
        for (int bx = 0; bx < gridSide; ++bx) {
            const int x0 = bx * width / gridSide;
            const int x1 = std::max(x0 + 1, (bx + 1) * width / gridSide);
            const int y0 = by * height / gridSide;
            const int y1 = std::max(y0 + 1, (by + 1) * height / gridSide);
            const int stepX = std::max(1, (x1 - x0) / 16);
            const int stepY = std::max(1, (y1 - y0) / 16);
            MusicRgb sum = {};
            float weight = 0.f;
            for (int y = y0; y < y1; y += stepY) {
                for (int x = x0; x < x1; x += stepX) {
                    const uint8_t* px = bgra + ((size_t)y * width + x) * 4u;
                    const float alpha = px[3] / 255.f;
                    if (alpha < 0.25f) continue;
                    const MusicRgb c = {
                        px[2] / 255.f, px[1] / 255.f, px[0] / 255.f
                    };
                    const float sampleWeight = alpha *
                        (0.32f + Saturation(c) * 1.70f);
                    sum.r += c.r * sampleWeight;
                    sum.g += c.g * sampleWeight;
                    sum.b += c.b * sampleWeight;
                    weight += sampleWeight;
                }
            }
            MusicRgb region = weight > 0.f
                ? MusicRgb{ sum.r / weight, sum.g / weight, sum.b / weight }
                : MusicRgb{ s_paletteTarget.top.x,
                            s_paletteTarget.top.y,
                            s_paletteTarget.top.z };
            if (weight > 0.f) {
                int nearest = 0;
                float nearestDistance = FLT_MAX;
                for (int i = 0; i < 3; ++i) {
                    const float dr = region.r - dominantColors[i].r;
                    const float dg = region.g - dominantColors[i].g;
                    const float db = region.b - dominantColors[i].b;
                    const float distance = dr * dr + dg * dg + db * db;
                    if (distance < nearestDistance) {
                        nearest = i;
                        nearestDistance = distance;
                    }
                }
                const float pull = 0.42f +
                    std::min(0.20f, Saturation(region) * 0.20f);
                region = Mix(region, dominantColors[nearest], pull);
            }
            const float sourceValue = std::max(region.r,
                std::max(region.g, region.b));
            const float sourceSaturation = Saturation(region);
            const float value = std::clamp(
                sourceValue * 0.84f + 0.025f, 0.025f, 0.68f);
            region = Tone(region, value, 1.04f,
                sourceSaturation > 0.12f
                    ? std::min(sourceSaturation, 0.36f) : 0.02f);
            float* out = outRgb + (by * gridSide + bx) * 3;
            out[0] = region.r;
            out[1] = region.g;
            out[2] = region.b;
        }
    }
}

ImU32 LyricHighlightColor() {
    return Color(Lerp(s_paletteCurrent.progress,
                      ImVec4(1.f, 1.f, 1.f, 1.f), 0.72f), 1.f);
}

void ResetPaletteState() {
    s_paletteReady = false;
    s_paletteCurrent = MusicPalette{};
    s_paletteTarget = MusicPalette{};
    s_motion = 0.f;
}

void DrawPlayerBackground(ImDrawList* dl,const ImVec2& min,const ImVec2& size,
    bool playing,bool showLyrics,bool artworkView,bool fullScreen,bool haveArt,float hover,float uiScale){
    const ImVec2 max(min.x+size.x,min.y+size.y);RetroUi::Frame(dl,min,max);
    if(artworkView&&haveArt&&AlbumArtTexture()){
        const float aspect=size.x/size.y;ImVec2 a(0,0),b(1,1);if(aspect>1){a.y=(1-1/aspect)*.5f;b.y=1-a.y;}else{a.x=(1-aspect)*.5f;b.x=1-a.x;}
        dl->AddImage(AlbumArtTexture(),ImVec2(min.x+7,min.y+7),ImVec2(max.x-7,max.y-7),a,b);
        dl->AddRectFilled(ImVec2(min.x+7,max.y-95*uiScale),ImVec2(max.x-7,max.y-7),IM_COL32(0,0,0,185));
    }else{
        RetroUi::Stars(dl,ImVec2(min.x+8,min.y+8),ImVec2(size.x-16,size.y-16),float(ImGui::GetTime()));
        const float divider=max.y-(showLyrics?86:74)*uiScale;
        dl->AddLine(ImVec2(min.x+12,divider),ImVec2(max.x-12,divider),ImGui::GetColorU32(variables::Theme::controls));
    }
    const auto accent=ImGui::GetColorU32(variables::Theme::accent);
    const float unit=(std::max)(1.f,std::round(uiScale));
    for(int i=0;i<3;++i){float h=playing?3+std::floor(3+3*std::sin(float(ImGui::GetTime())*4+i*1.5f)):3;dl->AddRectFilled(ImVec2(max.x-(14+i*4)*unit,min.y+9*unit),ImVec2(max.x-(12+i*4)*unit,min.y+(9+h)*unit),accent);}
}
}
