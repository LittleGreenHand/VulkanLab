#pragma once
#include <map>
#include "Types.hpp"
#include "RenderBase/VulkanglTFModel.h"
#include "Simulation/PhysicsContext.h"

class MeshManager
{
public:
	static MeshManager& Get()
	{
		static MeshManager instance;
		return instance;
	}

	MeshManager() = default;
	~MeshManager() { Destroy(); }
	MeshManager(const MeshManager&) = delete;
	MeshManager& operator=(const MeshManager&) = delete;
	MeshManager(MeshManager&&) = delete;
	MeshManager& operator=(MeshManager&&) = delete;
public:
	void Destroy();
	void LoadModels();
	bool ApplyPhysics(BaseModels key, bool isDynamic = false);
	void UpdateSimulationResults();
	
	Dimensions GetSceneDimensions();//计算并获取场景包围盒
	void InitModelsSourceDebugName();

private:
	void BuildPhysicsTriangleMeshData(vkglTF::Model& model, vkglTF::Node* node, PhysicsTriangleMeshData& physicsMesh);

public:
	vkglTF::Model skybox;
	bool isModelsLoaded = false;

	std::map<BaseModels, vkglTF::Model> m_sceneTree;//存储所有glTF模型
	std::map<BaseModels, PhysicsTriangleMeshData> m_physicsMeshes;

	std::map<BaseModels, physx::PxTriangleMesh*> m_pxTriangleMeshes;
	std::map<BaseModels, physx::PxRigidStatic*> m_pxStaticActors;

	std::map<BaseModels, physx::PxConvexMesh*> m_pxConvexMeshes;
	std::map<BaseModels, physx::PxRigidDynamic*> m_pxDynamicActors;
};