#pragma once
#include "PxPhysicsAPI.h"
#include "Types.hpp"
#include <vector>
#include <glm/gtc/quaternion.hpp>

struct PhysicsMeshData
{
	std::vector<physx::PxVec3> vertices;
	std::vector<physx::PxU32> indices;
	MeshTopology topology = MeshTopology::TriangleList;
};

class PhysicsContext
{
public:
	static PhysicsContext& Get()
	{
		static PhysicsContext instance;
		return instance;
	}

private:
	PhysicsContext();
	~PhysicsContext() { Destroy(); }
	PhysicsContext(const PhysicsContext&) = delete;
	PhysicsContext& operator=(const PhysicsContext&) = delete;
	PhysicsContext(PhysicsContext&&) = delete;
	PhysicsContext& operator=(PhysicsContext&&) = delete;

public:
	bool Init();
	void Destroy();
	void Simulate(float deltaTime);

public:
	void AddActor(physx::PxRigidActor* actor) { if (m_scene && actor) m_scene->addActor(*actor); }
	physx::PxTriangleMesh* CreateTriangleMesh(const PhysicsMeshData& meshData);
	physx::PxRigidStatic* CreateStaticActor(physx::PxTriangleMesh* triangleMesh, const physx::PxTransform& transform, physx::PxMeshScale scale);
	physx::PxConvexMesh* CreateConvexMesh(const PhysicsMeshData& meshData);
	physx::PxRigidDynamic* CreateDynamicActor(physx::PxConvexMesh* convexMesh, const physx::PxTransform& transform, physx::PxMeshScale scale, float density);
	void UpdateActorTransform(physx::PxRigidActor* actor, const glm::vec3& translation, const glm::quat& rotation);
	void UpdateActorScale(physx::PxRigidActor* actor, const glm::vec3& scale);

public:
	physx::PxPhysics* GetPhysics() const { return m_physics; }
	physx::PxScene* GetScene() const { return m_scene; }
	physx::PxMaterial* GetDefaultMaterial() const { return m_defaultMaterial; }
	const physx::PxCookingParams& GetCookingParams() const { return m_cookingParams; }
	bool IsInit() const { return m_isInitialized; }
	bool IsSimulationEnabled() const { return isSimulationEnabled; }

private:
	physx::PxDefaultAllocator m_allocator;
	physx::PxDefaultErrorCallback m_errorCallback;

	physx::PxFoundation* m_foundation = nullptr;
	physx::PxPhysics* m_physics = nullptr;
	physx::PxTolerancesScale m_toleranceScale;
	physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
	physx::PxScene* m_scene = nullptr;
	physx::PxMaterial* m_defaultMaterial = nullptr;
	physx::PxCookingParams	m_cookingParams{ m_toleranceScale };

	bool m_isInitialized = false;
	float mAccumulator = 0.0f;
	float mFixedDeltaTime = 1.0f / 60.0f;

public:
	bool isSimulationEnabled = false;
};