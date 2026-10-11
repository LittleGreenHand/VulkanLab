#include "InstanceManager.h"
#include "ModelInstances.h"
#include "RenderBase/VulkanglTFModel.h"
#include <atomic>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

bool InstanceTransform::IsValid() const
{
	for (int axis = 0; axis < 3; ++axis)
		if (!std::isfinite(translation[axis]) || !std::isfinite(scale[axis]) || scale[axis] < 0.0001f || scale[axis] > 10000.0f)
			return false;
	for (int axis = 0; axis < 4; ++axis)
		if (!std::isfinite(rotation[axis])) return false;
	return glm::dot(rotation, rotation) > 0.000001f;
}

glm::mat4 InstanceTransform::GetMatrix() const
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(glm::normalize(rotation)) * glm::scale(glm::mat4(1.0f), scale);
}

InstanceManager& InstanceManager::Get()
{
	static InstanceManager manager;
	return manager;
}
NodeID InstanceManager::RegisterNode(vkglTF::Node* node)
{
	static std::atomic<NodeID> next{ 1 };
	const NodeID id = next.fetch_add(1);
	m_nodes.emplace(id, node);
	return id;
}
void InstanceManager::UnregisterNode(NodeID id)
{
	if (m_selection.node == id) ClearSelection();
	m_nodes.erase(id);
	for (auto it = m_actors.begin(); it != m_actors.end();)
		if (it->second.node == id) it = m_actors.erase(it); else ++it;
}
vkglTF::Node* InstanceManager::FindNode(NodeID id) const
{
	const auto it = m_nodes.find(id);
	return it == m_nodes.end() ? nullptr : it->second;
}
const InstanceTransform* InstanceManager::FindInstance(NodeID node, InstanceID instance) const
{
	auto* found = FindNode(node);
	return found && found->instances ? found->instances->Find(instance) : nullptr;
}
bool InstanceManager::SetInstanceTransform(NodeID node, InstanceID instance, const InstanceTransform& transform)
{
	auto* found = FindNode(node);
	return found && found->instances && found->instances->SetTransform(instance, transform);
}
physx::PxRigidActor* InstanceManager::GetActor(NodeID node, InstanceID instance) const
{
	auto* found = FindNode(node);
	if (!found) return nullptr;
	if (!instance) return found->physicsComponent.physicsActor;
	return found->instances ? found->instances->GetActor(instance) : nullptr;
}
bool InstanceManager::Select(NodeID node, InstanceID instance)
{
	if (!FindNode(node) || (instance && !FindInstance(node, instance))) return false;
	m_selection = { node, instance };
	return true;
}
bool InstanceManager::SelectIndex(NodeID node, size_t index)
{
	auto* found = FindNode(node);
	return found && found->instances && index < found->instances->GetCount() && Select(node, found->instances->GetID(index));
}
InstanceSelection InstanceManager::DecodeGPUObject(const glm::uvec4& ids)
{
	return { NodeID(ids.x) | (NodeID(ids.y) << 32), InstanceID(ids.z) | (InstanceID(ids.w) << 32) };
}
bool InstanceManager::SelectGPUObject(const glm::uvec4& ids)
{
	const auto object = DecodeGPUObject(ids);
	return Select(object.node, object.instance);
}
void InstanceManager::InvalidateInstances(NodeID node)
{
	if (m_selection.node == node) m_selection.instance = 0;
}
void InstanceManager::RegisterActor(physx::PxRigidActor* actor, InstanceSelection object)
{
	if (actor) m_actors[actor] = object;
}
void InstanceManager::UnregisterActor(physx::PxRigidActor* actor) { m_actors.erase(actor); }
InstanceSelection InstanceManager::FindActor(const physx::PxRigidActor* actor) const
{
	const auto it = m_actors.find(actor);
	return it == m_actors.end() ? InstanceSelection{} : it->second;
}
bool InstanceManager::Generate(vkglTF::Model& model, vkglTF::Node& node)
{
	if (!node.instances) node.instances = std::make_unique<ModelInstances>(node);
	return node.instances->Generate(model, node.instanceSettings);
}
void InstanceManager::PollGeneration()
{
	for (const auto& [id, node] : m_nodes)
		if (node->instances) node->instances->PollGeneration();
}
void InstanceManager::SyncPhysics()
{
	for (const auto& [id, node] : m_nodes)
		if (node->instances) node->instances->SyncPhysics();
}
void InstanceManager::BeginFrame()
{
	for (const auto& [id, node] : m_nodes)
		if (node->instances) node->instances->BeginFrame();
}
void InstanceManager::PrepareGPU(uint32_t frame)
{
	m_statistics = {};
	for (const auto& [id, node] : m_nodes)
		if (node->instances) node->instances->PrepareGPU(frame);
}
void InstanceManager::RecordDraw(uint32_t count, bool instanced, bool shadow)
{
	if (shadow) ++m_statistics.shadowDraws;
	else if (instanced)
	{
		++m_statistics.instanceDraws;
		m_statistics.unbatchedDraws += count;
	}
	else ++m_statistics.ordinaryDraws;
}
