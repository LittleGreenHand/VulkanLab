module types;

public struct VSOutput
{
    public float4 Pos : SV_POSITION;
    public float2 UV;
};

public struct PBR_VSOutput
{
    public float4 Pos : SV_POSITION; // 顶点的裁剪空间位置，带有SV_POSITION语义，因此在片段着色器中不会被插值
    public float3 WorldPos;
    public float3 Normal;
    public float2 UV;
    public float3 Tangent;
    public float3 Color;
    public float4 currentClipPos;
    public float4 prevClipPos;
};

//以下定义对应于C++端RenderConfig.hpp中的枚举DescriptorSetBindIndex
public static const uint LBI_GLOBAL = 0;
public static const uint LBI_IBL = 1;
public static const uint LBI_LIGHTS = 2;
public static const uint LBI_MATERIALS = 3;
public static const uint LBI_MESH = 4;
public static const uint LBI_CUSTOM = 5;
