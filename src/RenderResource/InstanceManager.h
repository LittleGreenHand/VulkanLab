#pragma once
#include "InstanceTypes.h"
#include <unordered_map>
namespace vkglTF { struct Node; class Model; }
namespace physx { class PxRigidActor; }

class InstanceManager
{
public:
	static InstanceManager& Get();
	NodeID RegisterNode(vkglTF::Node* node);
	void UnregisterNode(NodeID id);
	vkglTF::Node* FindNode(NodeID id) const;
	const InstanceTransform* FindInstance(NodeID node, InstanceID instance) const;
	bool SetInstanceTransform(NodeID node, InstanceID instance, const InstanceTransform& transform);
	physx::PxRigidActor* GetActor(NodeID node, InstanceID instance = 0) const;
	bool Select(NodeID node, InstanceID instance = 0);
	bool SelectIndex(NodeID node, size_t index);
	static InstanceSelection DecodeGPUObject(const glm::uvec4& ids);
	bool SelectGPUObject(const glm::uvec4& ids);
	InstanceSelection GetSelection() const { return m_selection; }
	void ClearSelection() { m_selection = {}; }
	void InvalidateInstances(NodeID node);
	void RegisterActor(physx::PxRigidActor* actor, InstanceSelection object);
	void UnregisterActor(physx::PxRigidActor* actor);
	InstanceSelection FindActor(const physx::PxRigidActor* actor) const;
	bool Generate(vkglTF::Model& model, vkglTF::Node& node);
	void PollGeneration();
	void SyncPhysics();
	void BeginFrame();
	void PrepareGPU(uint32_t frame);
	void RecordDraw(uint32_t count, bool instanced, bool shadow);
	void RecordUpload(uint64_t bytes) { m_statistics.uploadedBytes += bytes; }
	const InstanceRenderStatistics& GetStatistics() const { return m_statistics; }
private:
	std::unordered_map<NodeID, vkglTF::Node*> m_nodes;
	std::unordered_map<const physx::PxRigidActor*, InstanceSelection> m_actors;
	InstanceSelection m_selection;
	InstanceRenderStatistics m_statistics;
};
