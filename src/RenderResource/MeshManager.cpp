#include "MeshManager.h"
#include "Render/VulkanContext.h"
#include "RenderBase/VulkanDevice.h"
#include "TextureManager.h"
#include "Render/VulkanDebugUtils.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"

void MeshManager::Destroy()
{
	LOG_DEBUG("[MeshManager] Destroying mesh manager resources");
	if(isModelsLoaded)
	{
		vkDeviceWaitIdle(VulkanContext::GetVkDevice());
		m_sceneTree.clear();
		skybox.Destroy();
		vkglTF::destroyEmptyTexture();
	}
	m_physicsMeshes.clear();
	for (auto& [model, triangleMesh] : m_pxTriangleMeshes)
	{
		if (triangleMesh)
			triangleMesh->release();
	}
	for (auto& [model, staticActor] : m_pxStaticActors)
	{
		if (staticActor)
			staticActor->release();
	}
	for (auto& [model, convexMesh] : m_pxConvexMeshes)
	{
		if (convexMesh)
			convexMesh->release();
	}
	for (auto& [model, dynamicActor] : m_pxDynamicActors)
	{
		if (dynamicActor)
			dynamicActor->release();
	}
	m_pxTriangleMeshes.clear();
	m_pxStaticActors.clear();
	m_pxConvexMeshes.clear();
	m_pxDynamicActors.clear();
	isModelsLoaded = false;
	LOG_DEBUG("[MeshManager] Destroying mesh manager resources successfully");
}

void MeshManager::LoadModels()
{
	LOG_DEBUG("[MeshManager] Loading glTF models");
	vks::VulkanDevice* vulkanDevice = VulkanContext::GetVulkanDevice();
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
	m_sceneTree[M_Sponza].nodes[0]->rotation =EularToQuaternion(glm::vec3(0, 90, 0));
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

	ApplyPhysics(M_Cube, true);
	ApplyPhysics(M_Sponza, false);
}

bool MeshManager::ApplyPhysics(BaseModels key, bool isDynamic)
{
	m_sceneTree[key].isPhysics = true;
	m_sceneTree[key].isDynamic = isDynamic;

	auto& physicsMesh = m_physicsMeshes[key];
	physicsMesh.vertices.clear();
	physicsMesh.indices.clear();
	physicsMesh.vertices.reserve(m_sceneTree[key].m_vertexBuffer.size());
	physicsMesh.indices.reserve(m_sceneTree[key].m_indexBuffer.size());

	for (auto* rootNode : m_sceneTree[key].nodes)
	{
		BuildPhysicsTriangleMeshData(m_sceneTree[key], rootNode, physicsMesh);
	}

	if (isDynamic)
	{
		auto convexMesh = PhysicsContext::Get().CreateConvexMesh(physicsMesh);
		if (!convexMesh)
		{
			LOG_WARNING("[MeshManager] Failed to create ConvexMesh for model: {}", m_sceneTree[key].modelName);
			return false;
		}
		if (m_pxConvexMeshes.find(key) != m_pxConvexMeshes.end())
		{
			if (m_pxConvexMeshes[key])
				m_pxConvexMeshes[key]->release();
		}
		m_pxConvexMeshes[key] = convexMesh;

		const physx::PxTransform actorTransform(physx::PxIdentity);
		auto actor = PhysicsContext::Get().CreateDynamicActor(m_pxConvexMeshes[key], actorTransform, 1.0f);
		if (!actor)
		{
			LOG_WARNING("[MeshManager] Failed to create dynamic actor for model: {}", m_sceneTree[key].modelName);
			return false;
		}
		if (m_pxDynamicActors.find(key) != m_pxDynamicActors.end())
		{
			if (m_pxDynamicActors[key])
				m_pxDynamicActors[key]->release();
		}
		m_pxDynamicActors[key] = actor;
		PhysicsContext::Get().AddActor(actor);
	}
	else
	{
		auto triangleMesh = PhysicsContext::Get().CreateTriangleMesh(physicsMesh);
		if (!triangleMesh)
		{
			LOG_WARNING("[MeshManager] Failed to create TriangleMesh for model: {}", m_sceneTree[key].modelName);
			return false;
		}
		if (m_pxTriangleMeshes.find(key) != m_pxTriangleMeshes.end())
		{
			if (m_pxTriangleMeshes[key])
				m_pxTriangleMeshes[key]->release();
		}
		m_pxTriangleMeshes[key] = triangleMesh;

		const physx::PxTransform actorTransform(physx::PxIdentity);// Node 层级变换已经全部烘焙到顶点中
		auto actor = PhysicsContext::Get().CreateStaticActor(m_pxTriangleMeshes[key], actorTransform);
		if (!actor)
		{
			LOG_WARNING("[MeshManager] Failed to create static actor for model: {}", m_sceneTree[key].modelName);
			return false;
		}
		if (m_pxStaticActors.find(key) != m_pxStaticActors.end())
		{
			if (m_pxStaticActors[key])
				m_pxStaticActors[key]->release();
		}
		m_pxStaticActors[key] = actor;
		PhysicsContext::Get().AddActor(actor);
	}
	return true;
}

void MeshManager::BuildPhysicsTriangleMeshData(	vkglTF::Model& model, vkglTF::Node* node, PhysicsTriangleMeshData& physicsMesh)
{
	if (!node)
		return;

	glm::mat4 nodeMatrix = node->getWorldMatrix();

	if (node->mesh)
	{
		// 同一个 Node 下可以让多个 Primitive 共用已经转换过的顶点
		std::unordered_map<uint32_t, uint32_t> vertexRemap;

		for (const auto* primitive : node->mesh->primitives)
		{
			if (!primitive || primitive->indexCount == 0)
				continue;

			// 当前阶段默认 glTF Primitive 为 TRIANGLES
			// TriangleMesh 要求最终索引能够组成三角形
			if((primitive->indexCount % 3 != 0))
			{
				LOG_WARNING("[MeshManager] Primitive topology is not valid for triangle mesh");
				continue;
			}

			const uint32_t firstIndex = primitive->firstIndex;
			const uint32_t indexCount = primitive->indexCount;

			for (uint32_t i = 0; i < indexCount; ++i)
			{
				const uint32_t srcIndex =
					model.m_indexBuffer[firstIndex + i];

				auto iter = vertexRemap.find(srcIndex);

				uint32_t dstIndex;

				if (iter == vertexRemap.end())
				{
					const glm::vec3& srcPosition =
						model.m_vertexBuffer[srcIndex].pos;

					const glm::vec4 transformedPosition =
						nodeMatrix * glm::vec4(srcPosition, 1.0f);

					dstIndex =
						static_cast<uint32_t>(physicsMesh.vertices.size());

					physicsMesh.vertices.emplace_back(
						transformedPosition.x,
						transformedPosition.y,
						transformedPosition.z);

					vertexRemap.emplace(srcIndex, dstIndex);
				}
				else
				{
					dstIndex = iter->second;
				}

				physicsMesh.indices.push_back(dstIndex);
			}
		}
	}

	// 递归处理子节点
	for (auto* child : node->children)
	{
		BuildPhysicsTriangleMeshData(model, child, physicsMesh);
	}
}


void MeshManager::UpdateSimulationResults()
{
	if (PhysicsContext::Get().IsInit())
	{
		for (auto& [key, actor] : m_pxDynamicActors)
		{
			if (actor)
			{
				physx::PxTransform pose = actor->getGlobalPose();

				m_sceneTree[key].nodes[0]->rotation = glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z);
				m_sceneTree[key].nodes[0]->translation = glm::vec3(pose.p.x, pose.p.y, pose.p.z);
				m_sceneTree[key].nodes[0]->update();
			}
		}
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
