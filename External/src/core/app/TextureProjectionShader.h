#pragma once
namespace TextureProjectionShader {
inline constexpr char Coordinates[]=R"HLSL(
float2 projected_axis_uv(float3 surfacePoint,float3 normal,float3 minimum,float3 inverseSpan,int axis)
{
    int face=axis*2+(normal[axis]<0?1:0);
    float3 q=(surfacePoint-minimum)*inverseSpan;
    float2 uv=axis==0?q.zy:axis==1?q.zx:q.yx;
    return(float2(face%3,face/3)+.004+saturate(uv)*.992)/float2(3,2);
}
float3 projected_weights(float3 normal)
{
    float3 weights=pow(abs(normal),8);
    float total=weights.x+weights.y+weights.z;
    return total>1e-8?weights/total:float3(0,0,1);
}
float2 projected_uv(float3 surfacePoint,float3 normal,float3 minimum,float3 inverseSpan)
{
    float3 a=abs(normal);
    int axis=a.x>=a.y&&a.x>=a.z?0:a.y>=a.z?1:2;
    int face=axis*2+(normal[axis]<0?1:0);
    float3 q=(surfacePoint-minimum)*inverseSpan;
    float2 uv=axis==0?q.zy:axis==1?q.zx:q.yx;
    return(float2(face%3,face/3)+.004+saturate(uv)*.992)/float2(3,2);
}
)HLSL";
}
