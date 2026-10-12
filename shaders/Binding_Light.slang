import types;
#define MAX_CASCADES 8
#define SHADOW_ATTENUATION_MIN 0
struct PointLightInfo {
    float4 position;
    float4 color;
    float range;
    int attenuationMode;
    int isRnder;
    float shadowBias;
};
struct DirectLightInfo {
    float4 direct;
    float4 color;
    float4x4 ViewProj[MAX_CASCADES];
    float cascadeSplits[MAX_CASCADES];
    int cascadeCount;
    int usePCF;
    int colorCascades;
    int isRnder;
    float shadowBias;
};
struct LightParameters
{
    PointLightInfo pointLights[16];
    int activePointLightCount = 1;

    DirectLightInfo directLight;
};
[[vk::binding(0, LBI_LIGHTS)]] ConstantBuffer<LightParameters> lightParameter;
[[vk::binding(1, LBI_LIGHTS)]] Sampler2DArray directLightShadowMap;
[[vk::binding(2, LBI_LIGHTS)]] SamplerCubeArray pointLightShadowCubeMap;

//计算定向光阴影
float textureProj(float4 shadowCoord, float2 offset, uint cascadeIndex, float bias)
{
    float2 uv = shadowCoord.xy + offset;
    if (shadowCoord.w <= 0.0 || shadowCoord.z < 0.0 || shadowCoord.z > 1.0 || any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float dist = directLightShadowMap.Sample(float3(uv, cascadeIndex)).r;
    return dist < shadowCoord.z - bias ? SHADOW_ATTENUATION_MIN : 1.0;
}

// 为定向光阴影计算PCF滤波
float filterPCF(float4 sc, uint cascadeIndex, float bias)
{
    int3 texDim;
    directLightShadowMap.GetDimensions(texDim.x, texDim.y, texDim.z);
    float scale = 0.75;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++)    {
        for (int y = -range; y <= range; y++)        {
            shadowFactor += textureProj(sc, float2(dx * x, dy * y), cascadeIndex, bias);
            count++;
        }
    }
    return shadowFactor / count;
}

float3 getCascadeColor(uint cascadeIndex)
{
    float3 color = float3(1.0f, 1.0f, 1.0f); // 默认白色
    switch (cascadeIndex) {
    case 0:
        color.rgb *= float3(1.0f, 0.f, 0.f); // 红色
        break;
    case 1:
        color.rgb *= float3(0.f, 1.0f, 0.f); // 绿色
        break;
    case 2:
        color.rgb *= float3(0.f, 0.f, 1.0f); // 蓝色
        break;
    case 3:
        color.rgb *= float3(1.0f, 1.0f, 0.25f); // 黄色
        break;
    case 4:
        color.rgb *= float3(1.0f, 0.25f, 1.0f); // 粉红色
        break;
    case 5:
        color.rgb *= float3(0.25f, 1.0f, 1.0f); // 青色
        break;
    case 6:
        color.rgb *= float3(1.0f, 0.75f, 0.25f); // 橙色
        break;
    case 7:
        color.rgb *= float3(0.75f, 0.25f, 1.0f); // 紫色
        break;
    }
    return color;
}
