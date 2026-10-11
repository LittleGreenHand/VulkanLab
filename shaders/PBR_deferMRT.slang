import "Binding_Global.slang";
import "Binding_Material.slang";
import "Binding_Mesh.slang";
import "MotionVector.slang";

// 0：不透明  1：遮罩  2：半透明
[SpecializationConstant] const int ALPHAMODE = 0;
struct VSOutput
{
    float4 Pos : SV_POSITION; // 顶点的裁剪空间位置，带有SV_POSITION语义，因此在片段着色器中不会被插值
    float3 WorldPos;
    float3 Normal;
    float2 UV;
    float3 Tangent;
    float3 Color;
    float4 currentClipPos;
    float4 prevClipPos;
};

struct GBuffer {
    float4 AlbedoAlpha;         // 反照率（RGB）+ Alpha（A）
    float4 Normal;              // 世界空间法线（RGB，压缩为 [0,1] 范围）
    float4 Tangent;             // 切线
    float4 Material;            // 金属度（R）+ 粗糙度（G）
    float4 WorldPos;            // 世界空间位置（XYZ）+ 预留（W）
    float4 MotionVector;        // 运动向量（XYZ）
};

[shader("vertex")]
VSOutput vertexMain(VSInput input, uint instanceIndex : SV_InstanceID) {
    VSOutput output;
    // 世界空间位置
    output.WorldPos = mul(GetModelMatrix(instanceIndex), float4(input.Pos, 1.0)).xyz;
    // 裁剪空间位置（当前帧+前一帧，用于运动向量）
    output.currentClipPos = mul(globalParams.viewProj, float4(output.WorldPos, 1.0));
    output.prevClipPos = mul(globalParams.prevViewProj, mul(GetPreviousModelMatrix(instanceIndex), float4(input.Pos, 1.0)));
    output.Pos = output.currentClipPos;
    // 纹理坐标与顶点属性
    output.UV = input.UV;
    output.Tangent = mul((float3x3)GetModelMatrix(instanceIndex), input.Tangent.xyz);
    output.Color = input.Color;
    output.Normal = GetNormal(instanceIndex, input.Normal);
    return output;
}

// --------------------------
// 片段着色器（写入 G-Buffer，无光照计算）
// --------------------------
[shader("fragment")]
GBuffer fragmentMain(VSOutput input) {
    GBuffer gbuf;

    // 1. 写入反照率颜色并处理 Alpha 遮罩（延迟渲染只保留不透明/遮罩物体，半透明需后处理）
    float4 albedoAlpha = float4(input.Color, 1.0);
    if (materialParameters.baseColorTextureEmpty == 0) {
        albedoAlpha = baseColorTexture.Sample(input.UV);
        albedoAlpha.rgb = pow(albedoAlpha.rgb, 2.2); // 伽马校正（存储线性空间）
    }
    albedoAlpha.rgb *= materialParameters.baseColorFactor.rgb;
    // 遮罩模式：Alpha 低于阈值则丢弃
    if (ALPHAMODE == 1) {
        if(albedoAlpha.a < materialParameters.alphaCutoff)
            discard;
    }
    gbuf.AlbedoAlpha = albedoAlpha;

    // 2. 写入世界空间法线、切线
    NormalInfo normalInfo = calculateNormal(input.Normal, input.Tangent, input.WorldPos, input.UV);
    gbuf.Normal = float4((normalInfo.N + 1.0) / 2.0, 1.0); // [-1,1] → [0,1]
    gbuf.Tangent = float4(normalize(normalInfo.T), 1.0);

    // 3. 写入金属度、粗糙度、AO
    float metallic = 1.0;
    float roughness = 1.0;
    float ao = 1.0;
    GetMetallicRoughness(input.UV, metallic, roughness);
    if (materialParameters.AOTextureEmpty == 0) {
        ao = AOTexture.Sample(input.UV).r;
    }
    gbuf.Material = float4(metallic, roughness, ao, materialParameters.anisotropicFactor);

    // 4. 写入世界空间位置
    gbuf.WorldPos = float4(input.WorldPos, 1.0);

    // 5. 写入运动向量（用于后期抗锯齿/运动模糊）
    gbuf.MotionVector = Calculate3DVelocity(input.currentClipPos, input.prevClipPos, globalParams.jitter);

    return gbuf;
}