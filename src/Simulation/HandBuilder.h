#pragma once

#include <array>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>
#include "AI/HandPose.h"
#include "RenderResource/PointLineGeometry.h"
#include "PxPhysicsAPI.h"

class HandBuilder
{
public:
	HandBuilder() = default;
	~HandBuilder();

	HandBuilder(const HandBuilder&) = delete;
	HandBuilder& operator=(const HandBuilder&) = delete;

public:
	void Build(const HandPoseResult& result)
	{
		BuildPhysics(result);
		BuildGeometry(result);
	}

	void BuildPhysics(const HandPoseResult& result);
	void BuildGeometry(const HandPoseResult& result);
	void DrawUI();
	void Clear();

	bool IsEmpty() const { return m_hands.empty(); }
	std::size_t GetHandCount() const { return m_hands.size(); }

	PointLineGeometry& GetGeometry() { return m_geometry; }
	const PointLineGeometry& GetGeometry() const { return m_geometry; }

public:
	static constexpr int kNumKeypoints = 21;
	static constexpr int kNumFingers = 5;
	static constexpr int kBonesPerFinger = 4;
	static constexpr int kNumBones = kNumFingers * kBonesPerFinger;

	struct Config
	{
		PointLineSpace Space = PointLineSpace::World;
		float Confidence = 0.5f;   // 置信度
		float JointRadius = 0.006f; // 关节球半径 米
		float BoneThickness = 0.003f;// 骨骼直径 米
		bool  bKinematic = true;

		// ---- 几何 ----
		float ViewportWidth = 0.0f;   // 屏幕空间下必需
		float ViewportHeight = 0.0f;   // 屏幕空间下必需
		float ScreenPointRadius = 4.0f;
		float ScreenLineThickness = 2.0f;
		float HandImageWidth = 0.6f;   // 世界空间锚点宽度（米）

		// ---- 世界变换 ----
		glm::vec3 WorldPositionOffset = glm::vec3(0.0f);  // 米
		glm::vec3 WorldRotationOffset = glm::vec3(0.0f);  // 角度（度），顺序 X→Y→Z
		glm::vec3 WorldScaleOffset = glm::vec3(1.0f);  // 无单位，默认 1
	};
	Config config;

private:
	struct HandActors
	{
		std::array<physx::PxRigidDynamic*, kNumKeypoints> Joints{};
		std::array<std::array<physx::PxRigidDynamic*, kBonesPerFinger>, kNumFingers> Bones{};
	};

	std::vector<HandActors> m_hands;
	physx::PxMaterial* m_material = nullptr;
	PointLineGeometry       m_geometry;

	// 由 config 的 World*Offset 组装：T * Rz * Ry * Rx * S，并把手的 Y/Z 轴翻到场景约定
	glm::mat4 BuildHandTransform() const;

	void EnsureMaterial(float staticFriction = 0.6f,
		float dynamicFriction = 0.6f,
		float restitution = 0.1f);
	physx::PxRigidDynamic* CreateActor(physx::PxGeometry& geometry);
	void ReleaseHandActors(HandActors& actors);
	void ApplyPose(physx::PxRigidDynamic* actor,
		const physx::PxTransform& pose,
		bool bKinematic);
};