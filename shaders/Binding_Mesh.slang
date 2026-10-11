import types;
struct MeshInfo {
    float4x4 model;
    float4x4 prevModel;
    float4x4 jointMatrix[64];
    float jointcount;
};
struct InstanceData {
    float4x4 model;
    float4x4 previousModel;
    float4x4 normal;
    uint4 ids; // NodeID low/high, InstanceID low/high; reserved for RGBA32_UINT picking.
};
[[vk::binding(0, LBI_MESH)]] ConstantBuffer<MeshInfo> meshInfo;
[[vk::binding(1, LBI_MESH)]] StructuredBuffer<InstanceData> instanceData;

float4x4 GetModelMatrix(uint index) { return mul(instanceData[index].model, meshInfo.model); }
float4x4 GetPreviousModelMatrix(uint index) { return mul(instanceData[index].previousModel, meshInfo.prevModel); }
uint4 GetObjectID(uint index) { return instanceData[index].ids; }

float3 TransformMeshNormal(float3 normal) {
    // inverse-transpose：GLTF 子节点允许非均匀缩放，使用列向量的余子式计算。
    float3 a = float3(meshInfo.model[0][0], meshInfo.model[1][0], meshInfo.model[2][0]);
    float3 b = float3(meshInfo.model[0][1], meshInfo.model[1][1], meshInfo.model[2][1]);
    float3 c = float3(meshInfo.model[0][2], meshInfo.model[1][2], meshInfo.model[2][2]);
    return (cross(b, c) * normal.x + cross(c, a) * normal.y + cross(a, b) * normal.z) / dot(a, cross(b, c));
}
float3 GetNormal(uint index, float3 normal) {
    return normalize(mul((float3x3)instanceData[index].normal, TransformMeshNormal(normal)));
}
