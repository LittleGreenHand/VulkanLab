import "Binding_Mesh.slang";
struct VSInput
{
    float3 Pos;
};
struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 WorldPos;
};
struct PushConstants {
    float4x4 MVP;
    float4x4 Model;
    float4 LightPos;
};
[[vk::push_constant]] PushConstants PushConstant;

[shader("vertex")]
VSOutput vertexMain(VSInput input, uint instanceIndex : SV_InstanceID)
{
    VSOutput output;
    output.Pos = mul(PushConstant.MVP, mul(GetModelMatrix(instanceIndex), float4(input.Pos, 1.0)));
    output.WorldPos = mul(GetModelMatrix(instanceIndex), float4(input.Pos, 1.0));
    return output;
}

[shader("fragment")]
float fragmentMain(VSOutput input)
{
    // Store distance to light as 32 bit float value
    float3 lightVec = input.WorldPos.xyz - PushConstant.LightPos.xyz;
    return length(lightVec);
}