import "Binding_Mesh.slang";
struct PushConstants {
    float4x4 MVP;
};
[[vk::push_constant]] PushConstants pushConstants;

[shader("vertex")]
float4 vertexMain(float3 Pos, uint instanceIndex : SV_InstanceID) : SV_POSITION
{
    return mul(pushConstants.MVP, mul(GetModelMatrix(instanceIndex), float4(Pos, 1.0)));
}

[shader("fragment")]
float4 fragmentMain()
{
	return float4(1.0, 0.0, 0.0, 1.0);
}