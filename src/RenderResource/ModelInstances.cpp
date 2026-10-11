#include "ModelInstances.h"
#include "InstanceManager.h"
#include "MeshManager.h"
#include "RenderBase/VulkanDevice.h"
#include "RenderBase/RenderConfig.hpp"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include "RenderBase/VulkanInitializers.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <random>
#include <stdexcept>
#include "core/Log.h"

namespace
{
	constexpr size_t MaxInstanceCount = 100000;
	using Clock = std::chrono::steady_clock;
	struct ActorRelease { void operator()(physx::PxRigidActor* actor) const { if (actor) actor->release(); } };
	using ActorPtr = std::unique_ptr<physx::PxRigidActor, ActorRelease>;
	struct Record
	{
		InstanceSelection object;
		InstanceTransform transform;
		ActorPtr actor;
		glm::vec3 worldScale{ 1.0f };
	};
	struct BatchSource
	{
		vkglTF::Node* node = nullptr;
		glm::mat4 relative{ 1.0f };
		PhysicsMeshData collision;
	};
	struct Collision
	{
		std::shared_ptr<physx::PxTriangleMesh> triangle;
		std::shared_ptr<physx::PxConvexMesh> convex;
	};
	struct BuildResult
	{
		std::vector<BatchSource> batches;
		std::vector<Collision> collisions;
		std::vector<Record> records; // Actor 先于缓存的碰撞网格释放。
		std::vector<InstanceGPUData> gpu;
		glm::mat4 anchor{ 1.0f };
		InstanceSettings settings;
		double milliseconds = 0;
	};
	struct GPUFrame
	{
		vks::Buffer buffer{};
		std::vector<uint8_t> dirty;
	bool descriptorsReady = false;
	};
	struct GPUBatch
	{
		vks::Buffer uniform{};
		std::array<VkDescriptorSet, MaxConcurrentFrames> descriptors{};
	};
	bool FiniteMatrix(const glm::mat4& matrix)
	{
		for (int col = 0; col < 4; ++col)
			for (int row = 0; row < 4; ++row)
				if (!std::isfinite(matrix[col][row])) return false;
		return std::isfinite(glm::determinant(matrix)) && glm::determinant(matrix) != 0.0f;
	}
	bool Decompose(const glm::mat4& matrix, InstanceTransform& transform)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		if (!glm::decompose(matrix, transform.scale, transform.rotation, transform.translation, skew, perspective)) return false;
		transform.rotation = glm::normalize(transform.rotation);
		return transform.IsValid() && glm::length(skew) < 0.0001f;
	}
	bool Similarity(const glm::mat4& matrix)
	{
		InstanceTransform transform;
		return Decompose(matrix, transform) && std::abs(transform.scale.x - transform.scale.y) < 0.0001f && std::abs(transform.scale.x - transform.scale.z) < 0.0001f;
	}
	glm::uvec4 PackIDs(InstanceSelection object)
	{
		return { uint32_t(object.node), uint32_t(object.node >> 32), uint32_t(object.instance), uint32_t(object.instance >> 32) };
	}
	void SetMatrix(InstanceGPUData& gpu, const glm::mat4& matrix)
	{
		gpu.model = matrix;
		gpu.normal = glm::transpose(glm::inverse(matrix));
	}
	physx::PxTransform ToPose(const InstanceTransform& transform)
	{
		const auto q = glm::normalize(transform.rotation);
		return physx::PxTransform(physx::PxVec3(transform.translation.x, transform.translation.y, transform.translation.z), physx::PxQuat(q.x, q.y, q.z, q.w));
	}
	void NormalizeRange(InstanceRange& range, bool scale)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(range.min[axis]) || !std::isfinite(range.max[axis])) 
			{
				LOG_ERROR("[InstanceManager] Random ranges must be finite.");
			}
			if (range.min[axis] > range.max[axis]) std::swap(range.min[axis], range.max[axis]);
			if (scale)
			{
				range.min[axis] = std::clamp(range.min[axis], 0.0001f, 10000.0f);
				range.max[axis] = std::clamp(range.max[axis], 0.0001f, 10000.0f);
			}
		}
	}
	glm::vec3 Sample(const InstanceRange& range, std::mt19937& random)
	{
		glm::vec3 result;
		for (int axis = 0; axis < 3; ++axis)
		{
			// 使用引擎整数的固定映射，同一 seed/index 的结果不依赖 TBB 调度或 STL 分布实现。
			const double t = double(random()) / double(std::mt19937::max());
			result[axis] = float(double(range.min[axis]) * (1.0 - t) + double(range.max[axis]) * t);
		}
		return result;
	}
	void Collect(vkglTF::Model& model, vkglTF::Node& node, const glm::mat4& relative, InstancePhysicsType physics, std::vector<BatchSource>& batches)
	{
		if (node.mesh)
		{
			BatchSource batch;
			batch.node = &node;
			batch.relative = relative;
			if (physics != InstancePhysicsType::None)
			{
				if (!MeshManager::BuildPhysicsMeshData(model, *node.mesh, batch.collision, physics == InstancePhysicsType::Dynamic))
				{
					LOG_WARNING("[InstanceManager] Failed to build collision mesh for node %s.", node.name.c_str());
				}
				else
				{
					// 子节点的全部仿射变换烘焙到缓存网格，每实例的 Shape 复用该缓存。
					for (auto& vertex : batch.collision.vertices)
					{
						const glm::vec3 point = glm::vec3(relative * glm::vec4(vertex.x, vertex.y, vertex.z, 1.0f));
						vertex = physx::PxVec3(point.x, point.y, point.z);
					}
					if (glm::determinant(relative) < 0)
						for (size_t i = 0; i + 2 < batch.collision.indices.size(); i += 3) std::swap(batch.collision.indices[i + 1], batch.collision.indices[i + 2]);
				}
			}
			batches.push_back(std::move(batch));
		}
		for (auto* child : node.children) 
			Collect(model, *child, relative * child->GetLocalMatrix(), physics, batches);
	}
	bool Visible(const vkglTF::Node* node, const vkglTF::Node* owner)
	{
		while (node)
		{
			if (!node->visible) return false;
			if (node == owner) return true;
			node = node->parent;
		}
		return false;
	}
}

struct ModelInstances::Impl
{
	vkglTF::Node& owner;
	vks::VulkanDevice* device = nullptr;
	std::unique_ptr<BuildResult> current;
	std::future<std::unique_ptr<BuildResult>> pending;
	std::shared_ptr<std::atomic_bool> cancel;
	std::array<GPUFrame, MaxConcurrentFrames> frames;
	std::vector<GPUBatch> batches;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	std::string status;
	double milliseconds = 0;
	explicit Impl(vkglTF::Node& node) : owner(node) {}
	size_t Index(InstanceID id) const
	{
		if (!current || current->records.empty()) return SIZE_MAX;
		const auto first = current->records.front().object.instance;
		return id >= first && id - first < current->records.size() ? size_t(id - first) : SIZE_MAX;
	}
	void Mark(size_t index)
	{
		for (auto& frame : frames)
			if (index < frame.dirty.size()) frame.dirty[index] = 1;
	}
	void DestroyBatches()
	{
		if (pool) vkDestroyDescriptorPool(device->logicalDevice, pool, nullptr);
		pool = VK_NULL_HANDLE;
		for (auto& batch : batches) { batch.uniform.unmap(); batch.uniform.destroy(); }
		batches.clear();
		for (auto& frame : frames) frame.descriptorsReady = false;
	}
	void DestroyGPU()
	{
		DestroyBatches();
		for (auto& frame : frames) { frame.buffer.unmap(); frame.buffer.destroy(); frame.dirty.clear(); }
	}
	void RemoveActors()
	{
		InstanceManager::Get().InvalidateInstances(owner.nodeId);
		if (current)
			for (auto& record : current->records)
				if (record.actor) InstanceManager::Get().UnregisterActor(record.actor.get());
		current.reset(); // release() 会将 Actor 从 Scene 移除，并释放独占 Shape 的引用。
	}
	void Cancel()
	{
		if (pending.valid())
		{
			cancel->store(true);
			try { pending.get(); } catch (...) {} // 节点删除前收回任务，后台快照中的 Mesh 指针保持有效。
		}
		cancel.reset();
	}
};

ModelInstances::ModelInstances(vkglTF::Node& owner) : m_impl(std::make_unique<Impl>(owner)) {}
ModelInstances::~ModelInstances() { Clear(); }
size_t ModelInstances::GetCount() const { return m_impl->current ? m_impl->current->records.size() : 0; }
bool ModelInstances::IsGenerating() const { return m_impl->pending.valid(); }
const std::string& ModelInstances::GetStatus() const { return m_impl->status; }
double ModelInstances::GetGenerationMilliseconds() const { return m_impl->milliseconds; }
InstanceID ModelInstances::GetID(size_t index) const { return index < GetCount() ? m_impl->current->records[index].object.instance : 0; }
const InstanceTransform* ModelInstances::Find(InstanceID id) const
{
	const auto index = m_impl->Index(id);
	return index == SIZE_MAX ? nullptr : &m_impl->current->records[index].transform;
}
physx::PxRigidActor* ModelInstances::GetActor(InstanceID id) const
{
	const auto index = m_impl->Index(id);
	return index == SIZE_MAX ? nullptr : m_impl->current->records[index].actor.get();
}
const InstanceGPUData* ModelInstances::GetGPUData(InstanceID id) const
{
	const auto index = m_impl->Index(id);
	return index == SIZE_MAX ? nullptr : &m_impl->current->gpu[index];
}

bool ModelInstances::Generate(vkglTF::Model& model, const InstanceSettings& input)
{
	auto& impl = *m_impl;
	impl.Cancel();
	try
	{
		auto result = std::make_unique<BuildResult>();
		result->settings = input;
		auto& settings = result->settings;
		if (settings.count < 1 || size_t(settings.count) > MaxInstanceCount)
		{
			LOG_WARNING("[InstanceManager] Instance count must be in [1, {}], clamping.", MaxInstanceCount);
			settings.count = std::clamp(settings.count, 1, int(MaxInstanceCount));
		}
		if (model.device && uint64_t(settings.count) * sizeof(InstanceGPUData) > model.device->properties.limits.maxStorageBufferRange)
		{
			LOG_ERROR("[InstanceManager] Instance count {} exceeds maxStorageBufferRange {}, clamping.", settings.count, model.device->properties.limits.maxStorageBufferRange);
			return false;
		}
		if (settings.physics == InstancePhysicsType::Dynamic && (!std::isfinite(settings.density) || settings.density <= 0))
		{
			LOG_WARNING("[InstanceManager] Dynamic instances require positive density, clamping.");
			settings.density = std::max(settings.density, 0.0001f);
		}
		const auto start = Clock::now();
		const InstanceTransform copy{ impl.owner.translation, impl.owner.rotation, impl.owner.scale };
		if (settings.random)
		{
			NormalizeRange(settings.position, false);
			NormalizeRange(settings.rotation, false);
			NormalizeRange(settings.scale, true);
		}
		result->anchor = impl.owner.parent ? impl.owner.parent->GetWorldMatrix() : glm::mat4(1.0f);
		if (settings.physics != InstancePhysicsType::None)
		{
			if (!Similarity(result->anchor)) 
				LOG_WARNING("[InstanceManager] Anchor transform contains shear or non-uniform scale; physics may be inaccurate.");
		}

		Collect(model, impl.owner, impl.owner.matrix, settings.physics, result->batches);
		//if (result->batches.empty()) throw std::runtime_error("This node has no meshes in its subtree.");

		if (impl.current && impl.current->settings.physics == settings.physics && impl.current->batches.size() == result->batches.size())
		{
			bool same = true;
			for (size_t i = 0; i < result->batches.size(); ++i)
				same &= result->batches[i].node == impl.current->batches[i].node && result->batches[i].relative == impl.current->batches[i].relative;
			if (same) result->collisions = impl.current->collisions;
		}
		impl.device = model.device;
		impl.cancel = std::make_shared<std::atomic_bool>(false);
		const auto cancel = impl.cancel;
		const NodeID nodeID = impl.owner.nodeId;
		static std::atomic<InstanceID> next{ 1 };
		const InstanceID firstID = next.fetch_add(settings.count);
		impl.status = "Generating on OneTBB workers...";
		impl.pending = std::async(std::launch::async, [result = std::move(result), copy, cancel, nodeID, firstID, start]() mutable
		{
			auto& physics = PhysicsContext::Get();
			const auto& settings = result->settings;
			result->collisions.resize(result->batches.size());
			if (settings.physics != InstancePhysicsType::None)
			{
				tbb::parallel_for(size_t(0), result->batches.size(), [&](size_t i)
				{
					if (cancel->load()) return;
					auto& collision = result->collisions[i];
					if (collision.convex || collision.triangle) return;
					const auto& mesh = result->batches[i].collision;
					if (settings.physics == InstancePhysicsType::Dynamic)
					{
						collision.convex = { physics.CreateConvexMesh(mesh), [](physx::PxConvexMesh* p) { if (p) p->release(); } };
						if (!collision.convex) 
							LOG_WARNING("[InstanceManager] Convex mesh cooking failed; ");
					}
					else
					{
						collision.triangle = { physics.CreateTriangleMesh(mesh), [](physx::PxTriangleMesh* p) { if (p) p->release(); } };
						if (!collision.triangle) 
							LOG_WARNING("[InstanceManager] Triangle mesh cooking failed; ");
					}
				});
			}
			if (cancel->load()) return std::unique_ptr<BuildResult>{};
			result->records.resize(settings.count);
			result->gpu.resize(settings.count);
			tbb::parallel_for(tbb::blocked_range<size_t>(0, settings.count, 64), [&](const auto& range)
			{
				for (size_t i = range.begin(); i < range.end(); ++i)
				{
					if (cancel->load()) return;
					auto& record = result->records[i];
					record.object = { nodeID, firstID + i };
					record.transform = copy;
					if (settings.random)
					{
						std::seed_seq seed{ settings.seed, uint32_t(i), uint32_t(uint64_t(i) >> 32) };
						std::mt19937 random(seed);
						record.transform.translation = Sample(settings.position, random);
						record.transform.rotation = glm::quat(glm::radians(Sample(settings.rotation, random)));
						record.transform.scale = Sample(settings.scale, random);
					}
					if (!record.transform.IsValid()) 
						throw std::runtime_error("Generated transform is invalid.");
					auto& gpu = result->gpu[i];
					SetMatrix(gpu, result->anchor * record.transform.GetMatrix());
					if (!FiniteMatrix(gpu.model)) 
						throw std::runtime_error("Generated matrix is singular or non-finite.");
					gpu.previousModel = gpu.model;
					gpu.ids = PackIDs(record.object);
					if (settings.physics == InstancePhysicsType::None) 
						continue;

					InstanceTransform world;
					if (!Decompose(gpu.model, world)) 
						throw std::runtime_error("Instance physics transform contains shear or invalid scale.");
					record.worldScale = world.scale;
					const auto pose = ToPose(world);
					// 不同 Actor 在加入 Scene 前可独立并发构建；共享 cooked mesh 由 Shape 引用计数持有。
					if (settings.physics == InstancePhysicsType::Dynamic) 
						record.actor.reset(physics.GetPhysics()->createRigidDynamic(pose));
					else 
						record.actor.reset(physics.GetPhysics()->createRigidStatic(pose));
					if (!record.actor) 
						throw std::runtime_error("PhysX actor allocation failed.");
					const physx::PxMeshScale scale(physx::PxVec3(world.scale.x, world.scale.y, world.scale.z));
					for (const auto& collision : result->collisions)
					{
						physx::PxShape* shape = nullptr;
						if (collision.convex) shape = physx::PxRigidActorExt::createExclusiveShape(*record.actor, physx::PxConvexMeshGeometry(collision.convex.get(), scale), *physics.GetDefaultMaterial());
						else shape = physx::PxRigidActorExt::createExclusiveShape(*record.actor, physx::PxTriangleMeshGeometry(collision.triangle.get(), scale), *physics.GetDefaultMaterial());
						if (!shape) throw std::runtime_error("PhysX shape allocation failed.");
					}
					if (auto* dynamic = record.actor->is<physx::PxRigidDynamic>())
						if (!physx::PxRigidBodyExt::updateMassAndInertia(*dynamic, settings.density)) throw std::runtime_error("PhysX mass calculation failed.");
				}
			});
			if (cancel->load()) return std::unique_ptr<BuildResult>{};
			for (auto& batch : result->batches) batch.collision = {}; // CPU 碰撞输入快照在 cooking 后释放。
			result->milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
			return std::move(result);
		});
		return true;
	}
	catch (const std::exception& error) { impl.status = error.what(); return false; }
}

void ModelInstances::PollGeneration()
{
	auto& impl = *m_impl;
	if (!impl.pending.valid() || impl.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	try
	{
		auto result = impl.pending.get();
		if (!result) return;
		// Poll 在 simulate/fetchResults 之后运行；Scene 变更集中在主线程。
		if (impl.device) VK_CHECK_RESULT(vkDeviceWaitIdle(impl.device->logicalDevice));
		impl.DestroyBatches();
		for (auto& frame : impl.frames)
		{
			if (frame.buffer.size < result->records.size() * sizeof(InstanceGPUData)) { frame.buffer.unmap(); frame.buffer.destroy(); }
			frame.dirty.assign(result->records.size(), 1);
		}
		impl.RemoveActors();
		impl.current = std::move(result);
		std::vector<physx::PxActor*> actors;
		actors.reserve(impl.current->records.size());
		for (auto& record : impl.current->records)
			if (record.actor)
			{
				record.actor->userData = &record.object;
				actors.push_back(record.actor.get());
				InstanceManager::Get().RegisterActor(record.actor.get(), record.object);
			}
		if (!actors.empty()) PhysicsContext::Get().GetScene()->addActors(actors.data(), uint32_t(actors.size()));
		impl.milliseconds = impl.current->milliseconds;
		impl.status = "Ready";
	}
	catch (const std::exception& error) { impl.status = error.what(); }
	impl.cancel.reset();
}

void ModelInstances::Clear()
{
	auto& impl = *m_impl;
	impl.Cancel();
	if (impl.device && impl.pool) VK_CHECK_RESULT(vkDeviceWaitIdle(impl.device->logicalDevice));
	impl.DestroyGPU();
	impl.RemoveActors();
	impl.status = "Cleared";
}

bool ModelInstances::SetTransform(InstanceID id, const InstanceTransform& input)
{
	auto& impl = *m_impl;
	const auto index = impl.Index(id);
	if (index == SIZE_MAX || !input.IsValid()) { impl.status = "Transform requires finite values and positive scale (0.0001..10000)."; return false; }
	auto transform = input;
	transform.rotation = glm::normalize(transform.rotation);
	auto& record = impl.current->records[index];
	const auto matrix = impl.current->anchor * transform.GetMatrix();
	if (!FiniteMatrix(matrix)) return false;
	if (record.actor)
	{
		InstanceTransform world;
		if (!Decompose(matrix, world)) { impl.status = "Physics transform must have positive scale and no shear."; return false; }
		if (record.transform.scale != transform.scale)
		{
			PhysicsContext::Get().UpdateActorScale(record.actor.get(), world.scale);
			if (auto* dynamic = record.actor->is<physx::PxRigidDynamic>()) physx::PxRigidBodyExt::updateMassAndInertia(*dynamic, impl.current->settings.density);
		}
		record.worldScale = world.scale;
		record.actor->setGlobalPose(ToPose(world));
		if (auto* dynamic = record.actor->is<physx::PxRigidDynamic>())
		{
			dynamic->setLinearVelocity(physx::PxVec3(0));
			dynamic->setAngularVelocity(physx::PxVec3(0));
			dynamic->wakeUp();
		}
	}
	record.transform = transform;
	SetMatrix(impl.current->gpu[index], matrix);
	impl.Mark(index);
	impl.status = "Ready";
	return true;
}

void ModelInstances::SyncPhysics()
{
	auto& impl = *m_impl;
	if (!impl.current || impl.current->settings.physics != InstancePhysicsType::Dynamic) return;
	const auto inverseAnchor = glm::inverse(impl.current->anchor);
	for (size_t i = 0; i < impl.current->records.size(); ++i)
	{
		auto& record = impl.current->records[i];
		auto* dynamic = record.actor ? record.actor->is<physx::PxRigidDynamic>() : nullptr;
		if (!dynamic) continue;
		const auto pose = dynamic->getGlobalPose();
		InstanceTransform world{ glm::vec3(pose.p.x, pose.p.y, pose.p.z), glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z), record.worldScale };
		const auto matrix = world.GetMatrix();
		InstanceTransform local;
		if (Decompose(inverseAnchor * matrix, local)) record.transform = local;
		if (impl.current->gpu[i].model != matrix)
		{
			SetMatrix(impl.current->gpu[i], matrix);
			impl.Mark(i);
		}
	}
}

void ModelInstances::BeginFrame()
{
	auto& impl = *m_impl;
	if (!impl.current) return;
	for (size_t i = 0; i < impl.current->gpu.size(); ++i)
	{
		auto& gpu = impl.current->gpu[i];
		if (gpu.previousModel != gpu.model)
		{
			gpu.previousModel = gpu.model;
			impl.Mark(i);
		}
	}
}

void ModelInstances::PrepareGPU(uint32_t frameIndex)
{
	auto& impl = *m_impl;
	if (!impl.current || !impl.device || frameIndex >= MaxConcurrentFrames) return;
	const size_t count = GetCount();
	if (!impl.pool)
	{
		const uint32_t setCount = uint32_t(impl.current->batches.size()) * MaxConcurrentFrames;
		const VkDescriptorPoolSize sizes[] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setCount }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setCount } };
		VkDescriptorPoolCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		info.poolSizeCount = 2;
		info.pPoolSizes = sizes;
		info.maxSets = setCount;
		VK_CHECK_RESULT(vkCreateDescriptorPool(impl.device->logicalDevice, &info, nullptr, &impl.pool));
		impl.batches.resize(impl.current->batches.size());
		for (size_t i = 0; i < impl.batches.size(); ++i)
		{
			auto& batch = impl.batches[i];
			vkglTF::Mesh::MeshInfo uniform{};
			uniform.modelMatrix = impl.current->batches[i].relative;
			uniform.prevModelMatrix = uniform.modelMatrix;
			VK_CHECK_RESULT(impl.device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &batch.uniform, sizeof(uniform), &uniform));
			std::array<VkDescriptorSetLayout, MaxConcurrentFrames> layouts;
			layouts.fill(vkglTF::MeshDescriptorSetLayout);
			VkDescriptorSetAllocateInfo allocate{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocate.descriptorPool = impl.pool;
			allocate.descriptorSetCount = MaxConcurrentFrames;
			allocate.pSetLayouts = layouts.data();
			VK_CHECK_RESULT(vkAllocateDescriptorSets(impl.device->logicalDevice, &allocate, batch.descriptors.data()));
		}
	}
	auto& frame = impl.frames[frameIndex];
	if (!frame.buffer.buffer)
	{
		const size_t capacity = std::min(MaxInstanceCount, std::max(count, std::bit_ceil(count)));
		const size_t limit = impl.device->properties.limits.maxStorageBufferRange / sizeof(InstanceGPUData);
		VK_CHECK_RESULT(impl.device->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.buffer, std::min(capacity, limit) * sizeof(InstanceGPUData)));
		VK_CHECK_RESULT(frame.buffer.map());
		frame.dirty.assign(count, 1);
	}
	if (!frame.descriptorsReady)
	{
		for (auto& batch : impl.batches)
		{
			const auto uniformWrite = vks::initializers::writeDescriptorSet(batch.descriptors[frameIndex], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &batch.uniform.descriptor);
			const auto instanceWrite = vks::initializers::writeDescriptorSet(batch.descriptors[frameIndex], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &frame.buffer.descriptor);
			const VkWriteDescriptorSet writes[] = { uniformWrite, instanceWrite };
			vkUpdateDescriptorSets(impl.device->logicalDevice, 2, writes, 0, nullptr);
		}
		frame.descriptorsReady = true;
	}
	// 当前 frame 的 fence 已等待。合并连续脏区；单实例编辑仅上传一个 208 字节记录。
	for (size_t i = 0; i < count;)
	{
		if (!frame.dirty[i]) { ++i; continue; }
		const size_t first = i;
		while (i < count && frame.dirty[i]) frame.dirty[i++] = 0;
		const size_t bytes = (i - first) * sizeof(InstanceGPUData);
		std::memcpy(static_cast<char*>(frame.buffer.mapped) + first * sizeof(InstanceGPUData), impl.current->gpu.data() + first, bytes);
		InstanceManager::Get().RecordUpload(bytes);
	}
}

void ModelInstances::Draw(VkCommandBuffer commandBuffer, uint32_t flags, VkPipelineLayout layout, uint32_t frame, bool shadow)
{
	auto& impl = *m_impl;
	if (!impl.current || !impl.pool || !impl.frames[frame].buffer.buffer) return;
	for (size_t i = 0; i < impl.batches.size(); ++i)
	{
		const auto* source = impl.current->batches[i].node;
		if (!Visible(source, &impl.owner)) continue;
		const auto descriptor = impl.batches[i].descriptors[frame];
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, LBI_MESH, 1, &descriptor, 0, nullptr);
		for (auto* primitive : source->mesh->primitives)
		{
			auto& material = primitive->material;
			if (!shadow)
			{
				if ((flags & vkglTF::RenderFlags::RenderOpaqueNodes) && material.alphaMode != vkglTF::Material::ALPHAMODE_OPAQUE) continue;
				if ((flags & vkglTF::RenderFlags::RenderAlphaMaskedNodes) && material.alphaMode != vkglTF::Material::ALPHAMODE_MASK) continue;
				if ((flags & vkglTF::RenderFlags::RenderAlphaBlendedNodes) && material.alphaMode != vkglTF::Material::ALPHAMODE_BLEND) continue;
				if (flags & vkglTF::RenderFlags::BindMaterial)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, LBI_MATERIALS, 1, &material.descriptorSet, 0, nullptr);
			}
			vkCmdDrawIndexed(commandBuffer, primitive->indexCount, uint32_t(GetCount()), primitive->firstIndex, 0, 0);
			InstanceManager::Get().RecordDraw(uint32_t(GetCount()), true, shadow);
		}
	}
}
