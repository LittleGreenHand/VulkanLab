struct OverlayParams
{
    float4x4 viewProjection;
    float2 extent;
    uint space;
    uint padding;
};
[[vk::push_constant]] ConstantBuffer<OverlayParams> overlay;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.position = overlay.space == 1 ? mul(overlay.viewProjection, float4(input.position, 1.0f)) : float4(input.position.xy / overlay.extent * 2.0f - 1.0f, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return input.color;
}
