#pragma once
#define GOAT_CUSTOM_MODEL_PRESENTATION_HLSL \
"float3 ModelPaintEncode(float3 c){return lerp(c*12.92,1.055*pow(max(c,0),1/2.4)-.055,step(.0031308,c));}\n" \
"float3 ModelPaintDecode(float3 c){return lerp(c/12.92,pow((c+.055)/1.055,2.4),step(.04045,c));}\n" \
"float3 ModelPaintDisplay(float3 linearColor,float exposure,float glow){float3 c=ModelPaintEncode(saturate(linearColor));if(glow>.001||exposure>=.99)return c;float limit=clamp(exposure,.5,1),knee=limit*.8,peak=max(c.r,max(c.g,c.b));if(peak<=knee)return c;float shoulder=knee+(limit-knee)*(1-exp(-(peak-knee)/max(limit-knee,.001)));return c*(shoulder/max(peak,.001));}\n" \
"float3 ModelPaintLinear(float3 c,float exposure,float glow){return ModelPaintDecode(ModelPaintDisplay(c,exposure,glow));}\n"
