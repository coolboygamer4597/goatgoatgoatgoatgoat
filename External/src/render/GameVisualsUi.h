#pragma once
#include "../core/features/game/GameVisualsState.h"
#include "../core/features/game/DiscordPresence.h"
#include "../core/features/native/NativeChams.h"
#include "FeatureUi.h"
namespace GameVisualsUi {
inline constexpr const char* SkyNames[]={"Black hole","Ringed planet","Electric storm","Aurora","Spiral galaxy","Neon ribbons","Nebula","Dusk ocean","Synthwave sunset","Golden tempest"};
inline int section=0;
inline void Draw(){
    auto& s=GameVisuals::settings;const char* sections[]={"Environment","Sky","World","Reflections","Weather"};FeatureUi::Sections(sections,5,section);
    FeatureUi::Column left{12,95},right{365,95};
    if(section==0){
        left.Check("Ambience",s.ambient);left.Colour("ambient_indoor","Indoor ambient",s.ambientColor,false);left.Colour("ambient_outdoor","Outdoor ambient",s.outdoorColor,false);
        left.Check("Brightness",s.brightness);left.Slider("game_brightness","Brightness",s.brightnessValue,0,8);
        left.Check("Exposure",s.exposure);left.Slider("game_exposure","Exposure",s.exposureValue,-3,3,"%.2f EV");
        left.Check("Time of day",s.time);left.Slider("game_time","Hour",s.hour,0,24,"%.2f h");
        if(left.Button("Restore environment")){s.ambient=s.brightness=s.exposure=s.time=s.glow=false;}
        right.Check("Glow",s.glow);right.Slider("game_glow_intensity","Glow intensity",s.glowIntensity,0,2);right.Slider("game_glow_radius","Glow radius",s.glowRadius,1,24,"%.0f px");right.Slider("game_glow_threshold","Highlight threshold",s.glowThreshold,0,.95f);
    }else if(section==1){
        left.Check("Skybox changer",s.sky);
        if(left.Combo("game_sky_style","Sky style",s.skyStyle,SkyNames,10)){
            const float themes[10][6]={{.65f,.78f,1.f,.28f,.4f,.85f},{.85f,.74f,.5f,.45f,.35f,.65f},{.36f,.44f,.8f,.4f,.24f,.65f},{.08f,1.f,.56f,.35f,.42f,.95f},{.65f,.7f,1.f,.85f,.45f,.8f},{.42f,.5f,1.f,.95f,.3f,.7f},{.4f,.55f,1.f,.9f,.3f,.75f},{.3f,.5f,.8f,.8f,.28f,.65f},{.35f,.18f,.8f,1.f,.15f,.65f},{.8f,.47f,.12f,1.f,.8f,.4f}};
            for(int i=0;i<3;++i){s.skyPrimary[i]=themes[s.skyStyle][i];s.skySecondary[i]=themes[s.skyStyle][i+3];}
        }
        left.Check("Animate sky",s.skyAnimate);left.Slider("game_sky_speed","Animation speed",s.skySpeed,0,3,"%.2fx");left.Slider("game_sky_intensity","Sky intensity",s.skyIntensity,0,3,"%.2fx");left.Slider("game_sky_size","Feature size",s.skyScale,.4f,2.5f,"%.2fx");left.Slider("game_sky_rotation","Sky rotation",s.skyRotation,-180,180,"%.0f deg");
        right.Check("Tint sky colours",s.skyTint);right.Colour("game_sky_primary","Primary colour",s.skyPrimary,false);right.Colour("game_sky_secondary","Secondary colour",s.skySecondary,false);
    }else if(section==2){
        left.Check("World chams",s.world);left.Combo("game_world_style","Material style",s.worldStyle,Cheat::Visuals::NativeChams::WorldShaderNames(),Cheat::Visuals::NativeChams::WorldShaderNameCount());left.Colour("game_world_color","Material colour",s.worldColor);left.Slider("game_world_opacity","Opacity",s.worldOpacity,0,1);left.Slider("game_world_speed","Animation speed",s.worldSpeed,0,15,"%.2fx");left.Slider("game_world_size","Pattern size",s.worldScale,.25f,8,"%.2fx");left.Slider("game_world_distance","Draw distance",s.worldDistance,50,10000,"%.0f");
        right.Check("Blend over original",s.worldOriginal);right.Check("Material glow",s.worldGlow);right.Colour("game_world_glow_color","Glow colour",s.worldGlowColor);right.Slider("game_world_glow_strength","Glow strength",s.worldGlowStrength,0,2);
        s.worldKinds=0;
    }else if(section==3){
        left.Check("Screen reflections",s.worldReflection);left.Check("Reflect on walls",s.reflectionWalls);left.Slider("game_reflection_strength","Reflection strength",s.reflectionStrength,0,1);left.Slider("game_reflection_roughness","Reflection roughness",s.reflectionRoughness,0,1);left.Slider("game_reflection_reach","Reflection reach",s.reflectionDistance,10,200,"%.0f");
    }else{
        const char* styles[]={"Rain","Petals","Fireflies"};left.Check("Weather effects",s.weather);
        if(left.Combo("game_weather_style","Weather style",s.weatherStyle,styles,3)){const float colours[3][4]={{.65f,.78f,1.f,.35f},{1,.55f,.75f,.8f},{1,.88f,.4f,.9f}};for(int i=0;i<4;++i)s.weatherColor[i]=colours[s.weatherStyle][i];}
        left.Colour("game_weather_color","Particle colour",s.weatherColor);left.Slider("game_weather_density","Density",s.weatherDensity,0,1);left.Slider("game_weather_speed","Fall speed",s.weatherSpeed,0,3,"%.2fx");
    }
    GameVisuals::Sanitize();
}
inline void Discord(){
    auto& s=DiscordPresence::settings;FeatureUi::Column left{12,55};
    left.Check("Discord Rich Presence",s.enabled);
    left.Input("discord_details","Description",s.details,sizeof(s.details));
    left.Text(DiscordPresence::Status());
}
}
