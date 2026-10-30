#pragma once
#include <Windows.h>
#include <array>
#include <string>
#include "../../../ext/imgui/imgui.h"

namespace variables {
inline bool menuOpen = true;
inline int menuKey = VK_INSERT;
inline int selectedTab = 0;

namespace Aimbot {
inline bool recoilEnabled=false;
inline bool recoilHorizontal=true;
inline bool glockAutomatic=false;
inline int glockClickDelay=120;
inline bool recoilActive=false;
inline float recoilStrength=1.f;
inline float recoilPull=400.f;
inline float recoilGlockStrength=.5f;
inline float recoilGlockPull=60.f;
inline bool silentEnabled = false;
inline bool silentActive = false;
inline int silentMethod = 1;
inline int silentKey = 1;
inline bool silentToggleKey = false;
inline bool silentShowFOV = false;
inline float silentFovRadius = 120.0f;
inline ImVec4 silentFovColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline bool silentNearestPoint = false;
inline bool deadCheck = true;
inline bool teamCheck = true;
inline bool invisibleCheck = true;
inline int aimTarget = 0;

}

namespace ESP {
inline bool meshChams = false;
inline bool meshChamsLocal = false;
inline float meshChamsOpacity = 0.65f;
inline bool meshChamsOcclusion = true;

inline bool meshChamsUnionWalls = true;
inline bool meshTeamCheck = true;
inline bool meshDeadCheck = true;
inline bool meshInvisibleCheck = true;
inline bool meshTransparencyCheck = true;
inline float meshTransparencyMin = 0.50f;
inline float meshTransparencyMax = 0.99f;
inline float chamsFillColor[4] = {0.20f, 0.70f, 1.00f, 0.72f};
inline float meshChamsOccludedColor[4] = {1.00f, 0.20f, 0.25f, 0.58f};
inline bool meshChamsOutline = true;
inline float meshChamsOutlineColor[4] = {0.92f, 0.92f, 0.95f, 1.00f};
inline float meshChamsOutlineFade = 0.75f;
inline int meshChamsOutlineStyle = 0;
inline int meshChamsDxMode = 0;
inline int meshChamsOccludedDxMode = 0;

inline bool visualKeybindEnabled = false;
inline int visualKeybindKey = 0;
inline int visualKeybindMode = 0;
inline int visualKeybindTarget = 0;
inline bool nativeChams = false;
inline bool nativeUnlimited=true,meshUnlimited=true;
inline float nativeDistance=2000,meshDistance=2000;

inline int nativeChamsStyle = 0;
inline float nativeChamsOpacity = 0.85f;
inline bool nativeChamsOnly = false;
inline bool nativeChamsOcclusion = false;
inline int nativeChamsOccludedStyle = 0;
inline float nativeChamsOccludedColor[4] = {1.0f,0.20f,0.25f,0.85f};
inline float nativeChamsAnimationSpeed = 10.0f;
inline bool nativePreview = true;

inline float nativeChamsPatternSize = 1.0f;
inline float nativeChamsColor[4] = {0.20f, 0.70f, 1.00f, 0.85f};
inline bool nativeChamsGlow = false;
inline float nativeChamsGlowColor[4] = {1.00f, 0.45f, 0.10f, 0.60f};
inline float nativeChamsGlowStrength = 0.55f;

inline bool nativeChamsWalls = true;

}

namespace Weapons {
inline bool preview = true;
inline constexpr int customTextureCount=231;
inline int texturePreset=-1,glockTexturePreset=-1;
inline std::array<bool,customTextureCount> textureCleanSurface=[] {std::array<bool,customTextureCount> values{};values.fill(true);return values;}();
inline bool modelCleanup=true;
inline bool removeX5Outline=true;
inline bool textureShading=true;
inline float textureShadingIntensity=1;
inline float textureShadowIntensity=0;
inline bool textureModelGlow=false;
inline float textureModelGlowStrength=1;
struct LimbMaterial {
    bool enabled=false,glow=false;
    int style=0;
    float opacity=1,speed=1,scale=1,glowStrength=.4f;
    float color[4]={.5f,.75f,1,1},glowColor[4]={1,.75f,.2f,1};
};
inline LimbMaterial arms,gloves;
inline bool hide=false,native=false,viewmodel=true,ar15=true,glock=true;
inline bool showOriginal=false;
inline bool glow=false;
inline float opacity=.9f,speed=10,scale=1,glowStrength=.4f;
inline int nativeStyle=0;
inline float color[4]={.5f,.75f,1,1},glowColor[4]={1,.75f,.2f,1};
inline bool separateWeapons=false,weaponMaterialsInitialized=false;
inline LimbMaterial arMaterial,glockMaterial,arArms,glockArms,arGloves,glockGloves;
inline bool separateArms=false,separateGloves=false,armMaterialsInitialized=false,gloveMaterialsInitialized=false;
inline LimbMaterial SharedMaterial(){
    LimbMaterial m;m.enabled=native;m.glow=glow;m.style=nativeStyle;m.opacity=opacity;m.speed=speed;m.scale=scale;m.glowStrength=glowStrength;
    for(int i=0;i<4;++i){m.color[i]=color[i];m.glowColor[i]=glowColor[i];}return m;
}
inline LimbMaterial MaterialForWeapon(bool isGlock){return separateWeapons?(isGlock?glockMaterial:arMaterial):SharedMaterial();}
inline void SeparateWeapons(bool enabled){if(enabled&&!weaponMaterialsInitialized){arMaterial=glockMaterial=SharedMaterial();weaponMaterialsInitialized=true;}separateWeapons=enabled;}
inline int& TextureForWeapon(bool isGlock){return isGlock?glockTexturePreset:texturePreset;}
inline bool AnyNative(){return separateWeapons?(arMaterial.enabled||glockMaterial.enabled):native;}
inline bool AnyLimb(bool glove){return glove?(separateGloves?(arGloves.enabled||glockGloves.enabled):gloves.enabled):(separateArms?(arArms.enabled||glockArms.enabled):arms.enabled);}
inline LimbMaterial MaterialForLimb(bool isGlock,bool glove){return glove?(separateGloves?(isGlock?glockGloves:arGloves):gloves):(separateArms?(isGlock?glockArms:arArms):arms);}
inline void SeparateLimbs(bool glove,bool enabled){auto& initialized=glove?gloveMaterialsInitialized:armMaterialsInitialized;if(enabled&&!initialized){if(glove)arGloves=glockGloves=gloves;else arArms=glockArms=arms;initialized=true;}(glove?separateGloves:separateArms)=enabled;}
inline LimbMaterial& LimbProfile(bool isGlock,bool glove){return glove?(separateGloves?(isGlock?glockGloves:arGloves):gloves):(separateArms?(isGlock?glockArms:arArms):arms);}
inline void SetSharedMaterial(const LimbMaterial& m){native=m.enabled;glow=m.glow;nativeStyle=m.style;opacity=m.opacity;speed=m.speed;scale=m.scale;glowStrength=m.glowStrength;for(int i=0;i<4;++i){color[i]=m.color[i];glowColor[i]=m.glowColor[i];}}

}

namespace Misc {
inline bool streamProof = true;
inline bool nvidiaStreamproof = false;
inline bool showInTaskbar = false;
inline bool moderatorAlerts = true;
inline bool spectatorCounter = false;
inline float spectatorPanelX = 20, spectatorPanelY = 80;
inline bool spotifyPlayer = false;
inline int streamKey = 0;
inline int streamKeyMode = 0;
inline bool keybinds = false;
inline bool performancePanel = false;
inline float keybindPanelX = 20, keybindPanelY = 140;
inline float performancePanelX = 20, performancePanelY = 300;
inline int spotifyKey = 0;
inline int spotifyKeyMode = 0;
inline int fpsLimit = 1000;
}

namespace Theme {
inline int preset = 0;
inline int sadblobStyle = 0;
inline std::string customMp4Path;
inline ImVec4 background = ImVec4(0.031f,0.031f,0.047f,1);
inline ImVec4 panels = ImVec4(0.04f,0.04f,0.06f,1);
inline ImVec4 controls = ImVec4(0.12f,0.12f,0.16f,1);
inline ImVec4 accent = ImVec4(1.0f,0.87f,0.2f,1);
inline ImVec4 text = ImVec4(0.92f,0.92f,0.9f,1);
inline ImVec4 textBright = ImVec4(1,1,1,1);
}
}
