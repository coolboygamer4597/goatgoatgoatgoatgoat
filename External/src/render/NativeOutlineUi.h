#pragma once
#include "FeatureUi.h"
namespace NativeOutlineUi {
inline bool settingsOpen=false;
inline void Entry(){
    FeatureUi::Column column{317,274,270};column.Check("Native outline",variables::ESP::nativeChamsOutline);column.Check("Native glow",variables::ESP::nativeOutlineGlow);
    if(column.Button("Outline settings"))settingsOpen=true;
}
inline void Draw(){
    FeatureUi::Column back{12,46};if(back.Button("Back to native chams",220))settingsOpen=false;
    FeatureUi::Column left{12,101},right{365,101};
    left.Check("Native outline",variables::ESP::nativeChamsOutline);left.Colour("native_outline_visible","Visible colour",variables::ESP::nativeOutlineColor);left.Colour("native_outline_hidden","Hidden colour",variables::ESP::nativeOutlineHiddenColor);left.Slider("native_outline_opacity","Outline opacity",variables::ESP::nativeOutlineOpacity,0,1);left.Slider("native_outline_width","Outline width",variables::ESP::nativeOutlineWidth,.5f,6,"%.1f px");const char* styles[]={"Solid","Dashed","Pulse"};left.Combo("native_outline_style","Outline style",variables::ESP::nativeOutlineStyle,styles,3);
    right.Check("Through Walls",variables::ESP::nativeChamsWalls);right.Check("Native glow",variables::ESP::nativeOutlineGlow);right.Colour("native_outline_glow_color","Glow colour",variables::ESP::nativeOutlineGlowColor);right.Slider("native_outline_glow_strength","Glow strength",variables::ESP::nativeOutlineGlowStrength,0,4);right.Slider("native_outline_glow_radius","Glow radius",variables::ESP::nativeOutlineGlowRadius,1,32,"%.0f px");right.Text("Outline and glow work independently. Through Walls also controls their hidden parts.");
}
}
