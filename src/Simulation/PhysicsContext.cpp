#include "PhysicsContext.h"
#include "Core/Log.h"
#include <thread>
#include <string>

PhysicsContext::PhysicsContext()
{

}

bool PhysicsContext::Init()
{
	LOG_INFO("[PhysX] PhysX Version: {}.{}.{}", PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);
	m_isInitialized = false;

	//Foundation
	{
		m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback);
		if (!m_foundation)
		{
			LOG_ERROR("[PhysX] Create PxFoundation failed!");
			return false;
		}
	}

	// PhysX SDK实例
	{
		m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, m_toleranceScale, true);
		if (!m_physics)
		{
			LOG_ERROR("[PhysX] Create PxPhysics failed!");
			return false;
		}

		if (!PxInitExtensions(*m_physics, nullptr))
		{
			LOG_ERROR("[PhysX] PxInitExtensions failed!");
			return false;
		}

		m_cookingParams = physx::PxCookingParams(m_toleranceScale);
		LOG_INFO("[PhysX] Default Simulation Tolerances: Length={}, Speed={}", m_toleranceScale.length, m_toleranceScale.speed);
	}

	// CPU Dispatcher
	{
		auto threadCount = std::thread::hardware_concurrency() - 2;
		threadCount = threadCount > 0 ? threadCount : 1;
		m_dispatcher = physx::PxDefaultCpuDispatcherCreate(threadCount);
		if (!m_dispatcher)
		{
			LOG_ERROR("[PhysX] Create CpuDispatcher failed!");
			return false;
		}
		LOG_INFO("[PhysX] CPU Dispatcher created with {} threads.", threadCount);
	}

	// PhysX Scene
	{
		physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
		sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
		sceneDesc.cpuDispatcher = m_dispatcher;
		sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
		sceneDesc.solverType = physx::PxSolverType::eTGS;
		m_scene = m_physics->createScene(sceneDesc);
		if (!m_scene)
		{
			LOG_ERROR("[PhysX] Create PxScene failed!");
			return false;
		}
		LOG_INFO("[PhysX] Default gravity is ({},{},{})", sceneDesc.gravity.x, sceneDesc.gravity.y, sceneDesc.gravity.z);
		LOG_INFO("[PhysX] Default SolverType is {}", sceneDesc.solverType == physx::PxSolverType::eTGS ? "Temporal Gauss-Seidel solver" : "Projected Gauss-Seidel iterative solver");
	}

	// 默认材质
	{
		m_defaultMaterial = m_physics->createMaterial(0.5f, 0.5f, 0.6f);
		if (!m_defaultMaterial)
		{
			LOG_ERROR("[PhysX] Create PxMaterial failed!");
			return false;
		}
		LOG_INFO("[PhysX] Default Material DynamicFriction: {}, StaticFriction: {}, Restitution: {}", m_defaultMaterial->getDynamicFriction(), m_defaultMaterial->getStaticFriction(), m_defaultMaterial->getRestitution());
	}

	m_isInitialized = true;
	return true;
}

void PhysicsContext::Destroy()
{
	if (!m_isInitialized)
		return;

	m_isInitialized = false;
	if (m_scene)
	{
		m_scene->release();
		m_scene = nullptr;
	}
	if (m_defaultMaterial)
	{
		m_defaultMaterial->release();
		m_defaultMaterial = nullptr;
	}
	if (m_dispatcher)
	{
		m_dispatcher->release();
		m_dispatcher = nullptr;
	}
	PxCloseExtensions();
	if (m_physics)
	{
		m_physics->release();
		m_physics = nullptr;
	}
	if (m_foundation)
	{
		m_foundation->release();
		m_foundation = nullptr;
	}
}

void PhysicsContext::Simulate(float deltaTime)
{
	if (!m_isInitialized || !isSimulationEnabled)
		return;
	mAccumulator += deltaTime;

	while (mAccumulator >= mFixedDeltaTime)
	{
		m_scene->simulate(mFixedDeltaTime);
		m_scene->fetchResults(true);

		mAccumulator -= mFixedDeltaTime;
	}
}

physx::PxTriangleMesh* PhysicsContext::CreateTriangleMesh(const PhysicsTriangleMeshData& meshData)
{
	if (meshData.vertices.empty() || meshData.indices.empty())
	{
		LOG_ERROR("[PhysX] CreateTriangleMesh failed! Vertices or Indices is empty.");
		return nullptr;
	}

	if (meshData.indices.size() % 3 != 0)
	{
		LOG_ERROR("[PhysX] CreateTriangleMesh failed! Indices count is not a multiple of 3.");
		return nullptr;
	}

	physx::PxTriangleMeshDesc meshDesc;
	meshDesc.points.count = static_cast<physx::PxU32>(meshData.vertices.size());
	meshDesc.points.stride = sizeof(physx::PxVec3);
	meshDesc.points.data = meshData.vertices.data();
	meshDesc.triangles.count = static_cast<physx::PxU32>(meshData.indices.size() / 3);
	meshDesc.triangles.stride = sizeof(physx::PxU32) * 3;
	meshDesc.triangles.data = meshData.indices.data();

	LOG_DEBUG("[PhysX] CreateTriangleMesh: {} vertices with {} stride, {} triangles with {} stride", meshDesc.points.count, meshDesc.points.stride, meshDesc.triangles.count, meshDesc.triangles.stride);

	if (!meshDesc.isValid())
	{
		LOG_ERROR("[PhysX] CreateTriangleMesh failed! MeshDesc is invalid.");
		return nullptr;
	}

	physx::PxTriangleMeshCookingResult::Enum result;
	physx::PxTriangleMesh* triangleMesh = PxCreateTriangleMesh(GetCookingParams(), meshDesc, m_physics->getPhysicsInsertionCallback(), &result);
	LOG_DEBUG("[PhysX] TriangleMesh cooking result: {}", std::to_string(result));
	return triangleMesh;
}

physx::PxRigidStatic* PhysicsContext::CreateStaticActor(physx::PxTriangleMesh* triangleMesh, const physx::PxTransform& transform)
{
	physx::PxRigidStatic* actor = m_physics->createRigidStatic(transform);
	if (!actor)
	{
		LOG_ERROR("[PhysX] Create PxRigidStatic failed!");
		return nullptr;
	}
	
	physx::PxTriangleMeshGeometry geometry(triangleMesh);// 创建 Geometry	
	physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *m_defaultMaterial);// 创建 Shape 并 Attach 到 Actor
	if (!shape)
	{
		LOG_ERROR("[PhysX] Create PxShape failed!");
		actor->release();
		return nullptr;
	}	
	return actor;
}

physx::PxConvexMesh* PhysicsContext::CreateConvexMesh(const PhysicsTriangleMeshData& meshData)
{
	if (meshData.vertices.empty())
	{
		LOG_ERROR("[PhysX] CreateConvexMesh Failed! Convex mesh vertices are empty!");
		return nullptr;
	}

	physx::PxConvexMeshDesc convexDesc;
	convexDesc.points.count = static_cast<physx::PxU32>(meshData.vertices.size());
	convexDesc.points.stride = sizeof(physx::PxVec3);
	convexDesc.points.data = meshData.vertices.data();	
	convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;// 让 PhysX 根据输入点计算 Convex Hull
	LOG_DEBUG("[PhysX] CreateConvexMesh: {} vertices with {} stride", convexDesc.points.count, convexDesc.points.stride);

	physx::PxConvexMeshCookingResult::Enum result;
	physx::PxConvexMesh* convexMesh = PxCreateConvexMesh(m_cookingParams, convexDesc, m_physics->getPhysicsInsertionCallback(), &result);

	if (!convexMesh)
	{
		LOG_ERROR("[PhysX] Create PxConvexMesh failed! PxConvexMeshCookingResult: {}", std::to_string(result));
		return nullptr;
	}
	LOG_DEBUG("[PhysX] ConvexMesh cooking result: {}", std::to_string(result));
	return convexMesh;
}

physx::PxRigidDynamic* PhysicsContext::CreateDynamicActor(physx::PxConvexMesh* convexMesh, const physx::PxTransform& transform, float density)
{
	physx::PxRigidDynamic* actor = m_physics->createRigidDynamic(transform);
	if (!actor)
	{
		LOG_ERROR("[PhysX] Failed to create rigid dynamic actor!");
		return nullptr;
	}

	physx::PxConvexMeshGeometry geometry(convexMesh);
	physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *m_defaultMaterial);
	if (!shape)
	{
		LOG_ERROR("[PhysX] Failed to create shape for dynamic actor!");
		actor->release();
		return nullptr;
	}

	physx::PxRigidBodyExt::updateMassAndInertia(*actor,	density);
	return actor;
}