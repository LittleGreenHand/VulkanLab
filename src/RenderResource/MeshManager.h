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

	Dimensions GetSceneDimensions();
	void InitModelsSourceDebugName();

private:
	bool ApplyPhysics(vkglTF::Model& model, vkglTF::Node* node, bool isDynamic);
	bool CreatePhysicsActor(vkglTF::Model& model, vkglTF::Node* node, bool isDynamic);
	bool BuildPhysicsMeshData(const vkglTF::Model& model,const vkglTF::Mesh& mesh,PhysicsMeshData& physicsMesh,bool isDynamic);
	void UpdateSimulationResults(vkglTF::Node* node);

public:
	vkglTF::Model skybox;
	bool isModelsLoaded = false;

	std::map<BaseModels, vkglTF::Model> m_sceneTree;
};