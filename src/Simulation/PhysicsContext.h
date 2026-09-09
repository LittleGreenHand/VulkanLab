#pragma once
#include "PxPhysicsAPI.h"
#include <vector>

struct PhysicsTriangleMeshData
{
	std::vector<physx::PxVec3> vertices;
	std::vector<physx::PxU32> indices;
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
	physx::PxTriangleMesh* CreateTriangleMesh(const PhysicsTriangleMeshData& meshData);
	physx::PxRigidStatic* CreateStaticActor(physx::PxTriangleMesh* triangleMesh, const physx::PxTransform& transform);
	physx::PxConvexMesh* CreateConvexMesh(const PhysicsTriangleMeshData& meshData);
	physx::PxRigidDynamic* CreateDynamicActor(physx::PxConvexMesh* convexMesh, const physx::PxTransform& transform, float density);

	void AddActor(physx::PxRigidActor* actor) { if (m_scene && actor) m_scene->addActor(*actor); }

public:
	physx::PxPhysics* GetPhysics() const { return m_physics; }
	physx::PxScene* GetScene() const { return m_scene; }
	physx::PxMaterial* GetDefaultMaterial() const { return m_defaultMaterial; }
	const physx::PxCookingParams& GetCookingParams() const { return m_cookingParams; }
	bool IsInit() const { return m_isInitialized; }

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