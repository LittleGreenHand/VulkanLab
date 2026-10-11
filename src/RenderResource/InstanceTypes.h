#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using NodeID = uint64_t;
using InstanceID = uint64_t;

struct InstanceSelection
{
	NodeID node = 0;
	InstanceID instance = 0; // 0 表示原始节点；生成的 ID 在进程生命周期内不复用。
	bool operator==(const InstanceSelection&) const = default;
};

struct InstanceTransform
{
	glm::vec3 translation{ 0.0f };
	glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f };
	glm::mat4 GetMatrix() const;
	bool IsValid() const;
};

enum class InstancePhysicsType { None, Static, Dynamic };
struct InstanceRange
{
	glm::vec3 min{ 0.0f };
	glm::vec3 max{ 0.0f };
};
struct InstanceSettings
{
	int count = 100;
	bool random = true;
	uint32_t seed = 1;
	InstanceRange position{ glm::vec3(-3.0f, 0.0f, -3.0f), glm::vec3(3.0f, 3.0f, 3.0f) };
	InstanceRange rotation{ glm::vec3(0.0f), glm::vec3(360.0f) };
	InstanceRange scale{ glm::vec3(0.1f), glm::vec3(0.1f) };
	InstancePhysicsType physics = InstancePhysicsType::None;
	float density = 1.0f;
};

// 与 Binding_Mesh.slang 的 std430 布局一致。GPU picking 可输出 ids 到 RGBA32_UINT。
struct alignas(16) InstanceGPUData
{
	glm::mat4 model{ 1.0f };
	glm::mat4 previousModel{ 1.0f };
	glm::mat4 normal{ 1.0f };
	glm::uvec4 ids{ 0 }; // NodeID low/high, InstanceID low/high
};
static_assert(sizeof(InstanceGPUData) == 208);
struct InstanceRenderStatistics
{
	uint64_t ordinaryDraws = 0;
	uint64_t instanceDraws = 0;
	uint64_t unbatchedDraws = 0;
	uint64_t shadowDraws = 0;
	uint64_t uploadedBytes = 0;
};
