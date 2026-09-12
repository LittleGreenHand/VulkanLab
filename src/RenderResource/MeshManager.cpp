#include "MeshManager.h"
#include "Render/VulkanContext.h"
#include "RenderBase/VulkanDevice.h"
#include "TextureManager.h"
#include "Render/VulkanDebugUtils.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>

void MeshManager::Destroy()
{
	if (isModelsLoaded)
	{
		vkDeviceWaitIdle(VulkanContext::GetVkDevice());
		m_sceneTree.clear();
		skybox.Destroy();
		vkglTF::destroyEmptyTexture();
	}
	isModelsLoaded = false;
	LOG_DEBUG("[MeshManager] Destroying mesh manager resources successfully");
}

void MeshManager::LoadModels()
{
	LOG_DEBUG("[MeshManager] Loading glTF models");
	vks::VulkanDevice* vulkanDevice = VulkanContext::GetVulkanDevice();
	//uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
	uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;

	m_sceneTree[M_Cube].loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	m_sceneTree[M_Cube].nodes[0]->clearTransform();
	m_sceneTree[M_Cube].nodes[0]->scale = (glm::vec3(0.01, 0.01, 0.01));
	m_sceneTree[M_Cube].nodes[0]->translation = (glm::vec3(0, -0, -1));
	//m_sceneTree[M_Cube].nodes[0]->visible = false;
	m_sceneTree[M_Cube].nodes[0]->update();

	m_sceneTree[M_Cerberus].loadFromFile(getAssetPath() + "models/cerberus/cerberus.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.setBaseColorTexture(&TextureManager::Get().textures.albedoMap);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.setNormalTexture(&TextureManager::Get().textures.normalMap);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.setAOTexture(&TextureManager::Get().textures.aoMap);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.setMetallicTexture(&TextureManager::Get().textures.metallicMap);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.setRoughnessTexture(&TextureManager::Get().textures.roughnessMap);
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.updateDescriptorSet();
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.materialParameters.metallicFactor = 1;
	m_sceneTree[M_Cerberus].linearNodes[0]->mesh->primitives[0]->material.materialParameters.roughnessFactor = 1;
	m_sceneTree[M_Cerberus].nodes[0]->clearTransform();
	m_sceneTree[M_Cerberus].nodes[0]->rotation = EularToQuaternion(glm::vec3(-90, 90, 0));
	m_sceneTree[M_Cerberus].nodes[0]->translation = (glm::vec3(0.2, -0.15, -2.5));
	m_sceneTree[M_Cerberus].nodes[0]->scale = (glm::vec3(0.2, 0.2, 0.2));
	m_sceneTree[M_Cerberus].nodes[0]->visible = false;
	m_sceneTree[M_Cerberus].nodes[0]->update();

	m_sceneTree[M_Sponza].loadFromFile(getAssetPath() + "models/sponza/sponza.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	m_sceneTree[M_Sponza].nodes[0]->clearTransform();
	m_sceneTree[M_Sponza].nodes[0]->rotation = EularToQuaternion(glm::vec3(0, 90, 0));
	//m_sceneTree[M_Sponza].nodes[0]->scale = (glm::vec3(0.1, 0.1, 0.1));
	m_sceneTree[M_Sponza].nodes[0]->translation = (glm::vec3(0, -1, 0));
	m_sceneTree[M_Sponza].nodes[0]->update();

	m_sceneTree[M_Sphere].loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	m_sceneTree[M_Sphere].nodes[0]->clearTransform();
	m_sceneTree[M_Sphere].nodes[0]->scale = (glm::vec3(0.1, 0.1, 0.1));
	m_sceneTree[M_Sphere].nodes[0]->translation = (glm::vec3(0, -0, -2));
	m_sceneTree[M_Sphere].nodes[0]->visible = true;
	m_sceneTree[M_Sphere].materials[0].materialParameters.baseColorFactor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	m_sceneTree[M_Sphere].materials[0].materialParameters.metallicFactor = 0.0f;
	m_sceneTree[M_Sphere].materials[0].materialParameters.roughnessFactor = 0.5f;
	m_sceneTree[M_Sphere].materials[0].alphaMode = vkglTF::Material::ALPHAMODE_BLEND;
	m_sceneTree[M_Sphere].nodes[0]->update();

	m_sceneTree[M_Axis].loadFromFile(getAssetPath() + "models/axis.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	m_sceneTree[M_Axis].nodes[0]->clearTransform();
	m_sceneTree[M_Axis].nodes[0]->scale = (glm::vec3(0.1, 0.1, 0.1));
	m_sceneTree[M_Axis].nodes[0]->translation = (glm::vec3(0, -0.15, -1));
	m_sceneTree[M_Axis].nodes[0]->visible = false;
	m_sceneTree[M_Axis].nodes[0]->update();

	//models[M_Terrain].loadFromFile(getAssetPath() + "models/Terrain.gltf", vulkanDevice, queue, glTFLoadingFlags);

	skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, VulkanContext::GetGraphicsQueue(), glTFLoadingFlags);
	InitModelsSourceDebugName();
	isModelsLoaded = true;
	LOG_DEBUG("[MeshManager] glTF models loaded successfully");

	if (PhysicsContext::Get().IsInit())
	{
		ApplyPhysics(M_Sphere, true);
		ApplyPhysics(M_Sponza, false);
	}
}
bool MeshManager::ApplyPhysics(BaseModels key, bool isDynamic)
{
	if (!PhysicsContext::Get().IsInit())
	{
		LOG_WARNING("[MeshManager] PhysX is not initialized.");
		return false;
	}

	auto iter = m_sceneTree.find(key);
	if (iter == m_sceneTree.end())
	{
		LOG_WARNING("[MeshManager] Model not found.");
		return false;
	}

	auto& model = iter->second;
	bool result = true;
	for (auto* rootNode : model.nodes)
	{
		if (!ApplyPhysics(model, rootNode, isDynamic))
		{
			result = false;
		}
	}
	return result;
}
bool MeshManager::ApplyPhysics(vkglTF::Model& model, vkglTF::Node* node, bool isDynamic)
{
	if (!node)
		return true;

	bool result = true;
	if (node->mesh)
	{
		if (!CreatePhysicsActor(model, node, isDynamic))
		{
			result = false;
		}
	}

	for (auto* child : node->children)
	{
		if (!ApplyPhysics(model, child, isDynamic))
		{
			result = false;
		}
	}

	return result;
}

bool MeshManager::BuildPhysicsMeshData(const vkglTF::Model& model, const vkglTF::Mesh& mesh, PhysicsMeshData& physicsMesh, bool isDynamic)
{
	physicsMesh.vertices.clear();
	physicsMesh.indices.clear();		
	physicsMesh.topology = MeshTopology::TriangleList;// 一个 Mesh 最终合成为一个 PxTriangleMesh，所以统一转换成 TriangleList
	for (const auto* primitive : mesh.primitives)
	{
		if (!primitive || primitive->vertexCount == 0)
			continue;

		// PxConvexMesh 只需要点集
		if (isDynamic)
		{
			auto vertices = model.GetPrimitiveVertices(*primitive);
			for (const auto& vertex : vertices)
			{
				physicsMesh.vertices.emplace_back(vertex.pos.x, vertex.pos.y, vertex.pos.z); 
			}
			continue;
		}

		auto vertices = model.GetPrimitiveVertices(*primitive);
		auto indices = model.GetPrimitiveIndices(*primitive);
		if (vertices.empty() || indices.empty())
			continue;

		const uint32_t baseVertex = static_cast<uint32_t>(physicsMesh.vertices.size());
		for (const auto& vertex : vertices)
		{
			physicsMesh.vertices.emplace_back(vertex.pos.x, vertex.pos.y, vertex.pos.z);
		}

		auto GetLocalIndex =
			[&](uint32_t globalIndex) -> uint32_t
			{
				return baseVertex +
					(globalIndex - primitive->firstVertex);
			};

		auto AddTriangle =
			[&](uint32_t i0, uint32_t i1, uint32_t i2)
			{
				// 跳过退化三角形
				if (i0 == i1 || i1 == i2 || i0 == i2)
					return;

				physicsMesh.indices.push_back(i0);
				physicsMesh.indices.push_back(i1);
				physicsMesh.indices.push_back(i2);
			};

		switch (primitive->topology)
		{
		case MeshTopology::TriangleList:
		{
			if (indices.size() % 3 != 0)
			{
				LOG_WARNING("[MeshManager] Invalid TriangleList index count in mesh: {}", mesh.name);
				return false;
			}

			for (size_t i = 0; i < indices.size(); i += 3)
			{
				AddTriangle(GetLocalIndex(indices[i]), GetLocalIndex(indices[i + 1]), GetLocalIndex(indices[i + 2]));
			}
			break;
		}

		case MeshTopology::TriangleStrip:
		{
			if (indices.size() < 3)
				continue;

			for (size_t i = 0; i + 2 < indices.size(); ++i)
			{
				uint32_t i0 = GetLocalIndex(indices[i]);
				uint32_t i1 = GetLocalIndex(indices[i + 1]);
				uint32_t i2 = GetLocalIndex(indices[i + 2]);

				if (i & 1)
					std::swap(i0, i1);

				AddTriangle(i0, i1, i2);
			}
			break;
		}

		case MeshTopology::TriangleFan:
		{
			if (indices.size() < 3)
				continue;

			const uint32_t center = GetLocalIndex(indices[0]);

			for (size_t i = 1; i + 1 < indices.size(); ++i)
			{
				AddTriangle(center, GetLocalIndex(indices[i]), GetLocalIndex(indices[i + 1]));
			}
			break;
		}

		case MeshTopology::Points:
		case MeshTopology::Lines:
		case MeshTopology::LineLoop:
		case MeshTopology::LineStrip:
		{
			LOG_WARNING(
				"[MeshManager] Unsupported topology for PxTriangleMesh, mesh: {}",
				mesh.name);
			break;
		}
		}
	}

	if (physicsMesh.vertices.empty())
		return false;

	if (!isDynamic && physicsMesh.indices.empty())
		return false;

	return true;
}

bool MeshManager::CreatePhysicsActor(vkglTF::Model& model, vkglTF::Node* node,	bool isDynamic)
{
	if (!node || !node->mesh)
		return false;

	auto* mesh = node->mesh;
	if (node->physicsComponent.physicsActor)
	{
		node->physicsComponent.physicsActor->release();
		node->physicsComponent.physicsActor = nullptr;
	}
	if (mesh->physicsMesh.triangleMesh)
	{
		mesh->physicsMesh.triangleMesh->release();
		mesh->physicsMesh.triangleMesh = nullptr;
	}

	if (mesh->physicsMesh.convexMesh)
	{
		mesh->physicsMesh.convexMesh->release();
		mesh->physicsMesh.convexMesh = nullptr;
	}

	PhysicsMeshData physicsMesh;
	if (!BuildPhysicsMeshData(model, *mesh,	physicsMesh, isDynamic))
	{
		LOG_WARNING("[MeshManager] Failed to build physics mesh data for mesh: {}",	mesh->name);
		return false;
	}

	glm::vec3 worldScale;
	glm::quat worldRotation;
	glm::vec3 worldTranslation;
	glm::vec3 skew;
	glm::vec4 perspective;
	if (!glm::decompose(node->GetWorldMatrix(), worldScale, worldRotation, worldTranslation, skew, perspective))
	{
		LOG_WARNING("[MeshManager] Failed to decompose Node transform: {}",	node->name);
		return false;
	}

	worldRotation = glm::normalize(worldRotation);
	const physx::PxTransform actorTransform = ToPxTransform(worldTranslation, worldRotation);
	const physx::PxMeshScale meshScale(	ToPxVec3(worldScale));
	if (isDynamic)
	{
		auto* convexMesh = PhysicsContext::Get().CreateConvexMesh(physicsMesh);
		if (!convexMesh)
		{
			LOG_WARNING("[MeshManager] Failed to create PxConvexMesh: {}", mesh->name);
			return false;
		}

		mesh->physicsMesh.convexMesh = convexMesh;
		auto* actor = PhysicsContext::Get().CreateDynamicActor(convexMesh, actorTransform, meshScale, 1.0f);

		if (!actor)
		{
			convexMesh->release();
			mesh->physicsMesh.convexMesh = nullptr;
			return false;
		}
		node->physicsComponent.physicsActor = actor;
	}
	else
	{
		auto* triangleMesh = PhysicsContext::Get().CreateTriangleMesh(physicsMesh);
		if (!triangleMesh)
		{
			LOG_WARNING("[MeshManager] Failed to create PxTriangleMesh: {}", mesh->name);
			return false;
		}
		mesh->physicsMesh.triangleMesh = triangleMesh;
		auto* actor = PhysicsContext::Get().CreateStaticActor(triangleMesh, actorTransform, meshScale);
		if (!actor)
		{
			triangleMesh->release();
			mesh->physicsMesh.triangleMesh = nullptr;
			return false;
		}
		node->physicsComponent.physicsActor = actor;
	}

	node->physicsComponent.isPhysics = true;
	node->physicsComponent.isDynamic = isDynamic;

	PhysicsContext::Get().AddActor(node->physicsComponent.physicsActor);
	return true;
}

void MeshManager::UpdateSimulationResults()
{
	if (!PhysicsContext::Get().IsInit())
		return;

	for (auto& [key, model] : m_sceneTree)
	{
		for (auto* rootNode : model.nodes)
		{
			UpdateSimulationResults(rootNode);
			rootNode->update();
		}
	}
}

void MeshManager::UpdateSimulationResults(vkglTF::Node* node)
{
	if (!node)
		return;

	if (node->physicsComponent.isPhysics && node->physicsComponent.isDynamic && node->physicsComponent.physicsActor)
	{
		auto* dynamicActor = node->physicsComponent.physicsActor->is<physx::PxRigidDynamic>();
		if (dynamicActor)
		{
			const physx::PxTransform pose = dynamicActor->getGlobalPose();

			glm::vec3 worldScale;
			glm::quat oldWorldRotation;
			glm::vec3 oldWorldTranslation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(node->GetWorldMatrix(), worldScale, oldWorldRotation, oldWorldTranslation, skew, perspective);
			glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pose.p.x, pose.p.y, pose.p.z)) * glm::mat4(glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z)) * glm::scale(glm::mat4(1.0f), worldScale);

			glm::mat4 localMatrix = worldMatrix;
			if (node->parent)
			{
				localMatrix = glm::inverse(node->parent->GetWorldMatrix()) * worldMatrix;
			}

			// GetLocalMatrix() 最后还会乘 matrix，因此求回 TRS 时把 matrix 去掉。
			localMatrix *= glm::inverse(node->matrix);

			glm::vec3 localScale;
			glm::quat localRotation;
			glm::vec3 localTranslation;
			if (glm::decompose(localMatrix, localScale, localRotation, localTranslation, skew, perspective))
			{
				node->translation = localTranslation;
				node->rotation = glm::normalize(localRotation);
				node->scale = localScale;
			}
		}
	}

	for (auto* child : node->children)
	{
		UpdateSimulationResults(child);
	}
}

Dimensions MeshManager::GetSceneDimensions()
{
	Dimensions dimension;
	dimension.min = glm::vec3(FLT_MAX);
	dimension.max = glm::vec3(-FLT_MAX);
	for (auto& [key, model] : m_sceneTree)
	{
		model.getSceneDimensions();
		if (dimension.min.x > model.dimensions.min.x) { dimension.min.x = model.dimensions.min.x; }
		if (dimension.min.y > model.dimensions.min.y) { dimension.min.y = model.dimensions.min.y; }
		if (dimension.min.z > model.dimensions.min.z) { dimension.min.z = model.dimensions.min.z; }
		if (dimension.max.x < model.dimensions.max.x) { dimension.max.x = model.dimensions.max.x; }
		if (dimension.max.y < model.dimensions.max.y) { dimension.max.y = model.dimensions.max.y; }
		if (dimension.max.z < model.dimensions.max.z) { dimension.max.z = model.dimensions.max.z; }
	}
	dimension.size = dimension.max - dimension.min;
	dimension.center = (dimension.min + dimension.max) / 2.0f;
	dimension.radius = glm::distance(dimension.min, dimension.max) / 2.0f;
	return dimension;
}

void MeshManager::InitModelsSourceDebugName()
{
	for (auto& [key, model] : m_sceneTree)
	{
		for (int i = 0; i < model.materials.size(); i++)
		{
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)model.materials[i].descriptorSet, model.modelName + "_Material_" + std::to_string(i) + "DescriptorSet");
		}
		for (int i = 0; i < model.linearNodes.size(); i++)
		{
			if (model.linearNodes[i]->mesh)
			{
				VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)model.linearNodes[i]->mesh->uniformBuffer.descriptorSet, model.linearNodes[i]->name + "_MeshDescriptorSet");
			}
		}
	}
}
