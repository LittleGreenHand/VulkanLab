#pragma once
#include "InstanceTypes.h"
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
namespace vkglTF { struct Node; class Model; }
namespace physx { class PxRigidActor; }

// 一个节点的实例覆盖该节点的完整 Mesh 子树，共享 Model 的顶点、索引与材质。
class ModelInstances
{
public:
	explicit ModelInstances(vkglTF::Node& owner);
	~ModelInstances();
	ModelInstances(const ModelInstances&) = delete;
	ModelInstances& operator=(const ModelInstances&) = delete;
	bool Generate(vkglTF::Model& model, const InstanceSettings& settings);
	void PollGeneration();
	void Clear();
	bool IsGenerating() const;
	const std::string& GetStatus() const;
	double GetGenerationMilliseconds() const;
	size_t GetCount() const;
	InstanceID GetID(size_t index) const;
	const InstanceTransform* Find(InstanceID id) const;
	physx::PxRigidActor* GetActor(InstanceID id) const;
	bool SetTransform(InstanceID id, const InstanceTransform& transform);
	void SyncPhysics();
	void BeginFrame();
	void PrepareGPU(uint32_t frame);
	void Draw(VkCommandBuffer commandBuffer, uint32_t flags, VkPipelineLayout layout, uint32_t frame, bool shadow = false);
	const InstanceGPUData* GetGPUData(InstanceID id) const;
private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
