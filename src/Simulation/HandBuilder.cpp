#include "HandBuilder.h"
#include "PhysicsContext.h"
#include "Core/Log.h"

#include <cmath>
#include <algorithm>
#include "imgui.h"
#include "glm/gtc/matrix_transform.inl"

namespace
{
	struct BoneLink { int Parent; int Child; };

	constexpr BoneLink kBoneLinks[HandBuilder::kNumBones] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 4},        // 拇指
		{0, 5}, {5, 6}, {6, 7}, {7, 8},        // 食指
		{0, 9}, {9, 10}, {10, 11}, {11, 12},   // 中指
		{0, 13}, {13, 14}, {14, 15}, {15, 16}, // 无名指
		{0, 17}, {17, 18}, {18, 19}, {19, 20}  // 小指
	};

	inline bool IsFinite3(const float* v)
	{
		return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
	}

	inline physx::PxQuat RotationBetween(const physx::PxVec3& from, const physx::PxVec3& to)
	{
		const float cosTheta = from.dot(to);
		if (cosTheta > 1.0f - 1e-6f)  return physx::PxQuat(physx::PxIdentity);
		if (cosTheta < -1.0f + 1e-6f)
		{
			const physx::PxVec3 axis =
				(std::abs(from.x) < 0.9f
					? physx::PxVec3(1, 0, 0).cross(from)
					: physx::PxVec3(0, 1, 0).cross(from)).getNormalized();
			return physx::PxQuat(physx::PxPi, axis);
		}
		const physx::PxVec3 axis = from.cross(to);
		const float s = std::sqrt((1.0f + cosTheta) * 2.0f);
		return physx::PxQuat(axis.x / s, axis.y / s, axis.z / s, s * 0.5f).getNormalized();
	}
}

HandBuilder::~HandBuilder() { Clear(); }

glm::mat4 HandBuilder::BuildHandTransform() const
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), config.WorldPositionOffset);

	transform = glm::rotate(transform, glm::radians(config.WorldRotationOffset.z), glm::vec3(0, 0, 1));
	transform = glm::rotate(transform, glm::radians(config.WorldRotationOffset.y), glm::vec3(0, 1, 0));
	transform = glm::rotate(transform, glm::radians(config.WorldRotationOffset.x), glm::vec3(1, 0, 0));

	// 手部坐标约定：X 向右、Y 向下、Z 远离相机；场景 Y 向上、Z 朝观察者。
	const glm::vec3 s = config.WorldScaleOffset;
	transform = glm::scale(transform, glm::vec3(s.x, -s.y, -s.z));

	return transform;
}

// ---------------- 物理 ----------------

void HandBuilder::EnsureMaterial(float sf, float df, float re)
{
	if (m_material) return;
	auto* physics = PhysicsContext::Get().GetPhysics();
	if (!physics) return;
	m_material = physics->createMaterial(sf, df, re);
}

physx::PxRigidDynamic* HandBuilder::CreateActor(physx::PxGeometry& geometry)
{
	auto& ctx = PhysicsContext::Get();
	auto* physics = ctx.GetPhysics();
	auto* scene = ctx.GetScene();
	if (!physics || !scene || !m_material) return nullptr;

	auto* actor = physics->createRigidDynamic(physx::PxTransform(physx::PxIdentity));
	if (!actor) return nullptr;

	if (!physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *m_material))
	{
		actor->release();
		return nullptr;
	}

	actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
	actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
	scene->addActor(*actor);
	return actor;
}

void HandBuilder::ReleaseHandActors(HandActors& actors)
{
	for (auto*& j : actors.Joints)
		if (j) { j->release(); j = nullptr; }
	for (auto& finger : actors.Bones)
		for (auto*& b : finger)
			if (b) { b->release(); b = nullptr; }
}

void HandBuilder::Clear()
{
	for (auto& hand : m_hands)
		ReleaseHandActors(hand);
	m_hands.clear();

	if (m_material) { m_material->release(); m_material = nullptr; }
	m_geometry.Clear();
}

void HandBuilder::ApplyPose(physx::PxRigidDynamic* actor, const physx::PxTransform& pose, bool bKinematic)
{
	if (!actor) return;
	if (bKinematic) actor->setKinematicTarget(pose);
	else            actor->setGlobalPose(pose);
}

void HandBuilder::BuildPhysics(const HandPoseResult& result)
{
	auto& ctx = PhysicsContext::Get();
	if (!ctx.IsInit()) return;
	if (config.Space != PointLineSpace::World) return;
	EnsureMaterial();
	if (!m_material) return;

	if (result.ImageWidth <= 0 || result.ImageHeight <= 0) return;

	const glm::mat4 handTransform = BuildHandTransform();

	const int numHands = static_cast<int>(result.Hands.size());

	while (static_cast<int>(m_hands.size()) > numHands)
	{
		ReleaseHandActors(m_hands.back());
		m_hands.pop_back();
	}
	while (static_cast<int>(m_hands.size()) < numHands)
		m_hands.emplace_back();

	for (int h = 0; h < numHands; ++h)
	{
		HandActors& actors = m_hands[h];
		const HandPose& hand = result.Hands[h];

		if (!hand.HasWorldKeypoints)
		{
			ReleaseHandActors(actors);
			continue;
		}

		const auto& wrist = hand.Keypoints[0];
		const auto& p0 = hand.WorldKeypoints[0];
		if (!std::isfinite(wrist.X) || !std::isfinite(wrist.Y) || !IsFinite3(p0.data()))
		{
			ReleaseHandActors(actors);
			continue;
		}

		const glm::vec3 wristWorld(p0[0], p0[1], p0[2]);
		const glm::vec3 anchor(
			(wrist.X / result.ImageWidth - 0.5f) * config.HandImageWidth,
			(wrist.Y / result.ImageHeight - 0.5f) * config.HandImageWidth
			* result.ImageHeight / result.ImageWidth,
			0.0f);

		std::array<glm::vec3, kNumKeypoints> positions{};
		std::array<bool, kNumKeypoints> visible{};

		for (int i = 0; i < kNumKeypoints; ++i)
		{
			const float* wp = hand.WorldKeypoints[i].data();
			const HandKeypoint& kp = hand.Keypoints[i];

			visible[i] = std::isfinite(kp.Confidence) && kp.Confidence >= config.Confidence && IsFinite3(wp);

			if (visible[i])
			{
				const glm::vec3 worldPoint(wp[0], wp[1], wp[2]);
				const glm::vec4 p = handTransform
					* glm::vec4(worldPoint - wristWorld + anchor, 1.0f);
				positions[i] = glm::vec3(p);
			}
		}

		// 关节球
		for (int i = 0; i < kNumKeypoints; ++i)
		{
			if (!visible[i] || actors.Joints[i]) continue;
			physx::PxSphereGeometry geom(config.JointRadius);
			actors.Joints[i] = CreateActor(geom);
		}

		// 骨骼胶囊
		for (int b = 0; b < kNumBones; ++b)
		{
			const int p = kBoneLinks[b].Parent;
			const int c = kBoneLinks[b].Child;
			if (!visible[p] || !visible[c]) continue;

			const int finger = b / kBonesPerFinger;
			const int joint = b % kBonesPerFinger;
			if (actors.Bones[finger][joint]) continue;

			const float len = glm::length(positions[c] - positions[p]);
			const float radius = config.BoneThickness * 0.5f;
			const float halfHeight = std::max(1e-4f, len * 0.5f - radius);

			physx::PxCapsuleGeometry geom(radius, halfHeight);
			actors.Bones[finger][joint] = CreateActor(geom);
		}

		// 姿态更新
		{
			for (int i = 0; i < kNumKeypoints; ++i)
			{
				if (!actors.Joints[i]) continue;
				const auto& q = positions[i];
				ApplyPose(actors.Joints[i],
					physx::PxTransform(physx::PxVec3(q.x, q.y, q.z)),
					config.bKinematic);
			}
			for (int b = 0; b < kNumBones; ++b)
			{
				const int p = kBoneLinks[b].Parent;
				const int c = kBoneLinks[b].Child;
				const int finger = b / kBonesPerFinger;
				const int joint = b % kBonesPerFinger;

				physx::PxRigidDynamic* actor = actors.Bones[finger][joint];
				if (!actor || !visible[p] || !visible[c]) continue;

				const glm::vec3& s = positions[p];
				const glm::vec3& e = positions[c];
				const glm::vec3  d = e - s;
				const float      len = glm::length(d);
				if (len < 1e-5f) continue;

				const glm::vec3    center = (s + e) * 0.5f;
				const physx::PxVec3 axis(d.x / len, d.y / len, d.z / len);
				const physx::PxQuat rot = RotationBetween(physx::PxVec3(1, 0, 0), axis);

				ApplyPose(actor,
					physx::PxTransform(physx::PxVec3(center.x, center.y, center.z), rot),
					config.bKinematic);
			}
		}
	}
}

void HandBuilder::BuildGeometry(const HandPoseResult& result)
{
	m_geometry.Clear();
	m_geometry.SetWorldSpace(config.Space);
	glm::mat4 handTransform = BuildHandTransform();

	if (result.ImageWidth <= 0 || result.ImageHeight <= 0
		|| config.ViewportWidth <= 0 || config.ViewportHeight <= 0)
		return;

	const bool  world = config.Space == PointLineSpace::World;
	const float width = config.ViewportWidth;
	const float height = config.ViewportHeight;

	const float     scale = std::min(width / result.ImageWidth, height / result.ImageHeight);
	const glm::vec2 offset((width - result.ImageWidth * scale) * 0.5f,
		(height - result.ImageHeight * scale) * 0.5f);

	static const glm::vec4 colors[] = {
		{1.0f, 0.31f, 0.31f, 1.0f},
		{1.0f, 0.78f, 0.31f, 1.0f},
		{0.39f, 1.0f, 0.31f, 1.0f},
		{0.31f, 0.71f, 1.0f, 1.0f},
		{0.78f, 0.31f, 1.0f, 1.0f}
	};

	const float radius = world ? config.JointRadius : config.ScreenPointRadius;
	const float thickness = world ? config.BoneThickness : config.ScreenLineThickness;

	for (const auto& hand : result.Hands)
	{
		if (world && !hand.HasWorldKeypoints) continue;

		const auto& wrist = hand.Keypoints[0];
		if (world && (!std::isfinite(wrist.X) || !std::isfinite(wrist.Y))) continue;

		glm::vec3 anchor(0.0f);
		glm::vec3 wristWorld(0.0f);
		if (world)
		{
			const auto& p0 = hand.WorldKeypoints[0];
			if (!std::isfinite(p0[0]) || !std::isfinite(p0[1]) || !std::isfinite(p0[2])) continue;
			wristWorld = glm::vec3(p0[0], p0[1], p0[2]);
			anchor = glm::vec3(
				(wrist.X / result.ImageWidth - 0.5f) * config.HandImageWidth,
				(wrist.Y / result.ImageHeight - 0.5f) * config.HandImageWidth
				* result.ImageHeight / result.ImageWidth,
				0.0f);
		}

		auto getWorldPoint = [&](int i)
			{
				const auto& p = hand.WorldKeypoints[i];
				return glm::vec3(p[0], p[1], p[2]);
			};

		auto isVisible = [&](int i) -> bool
			{
				const auto& p = hand.Keypoints[i];
				if (!std::isfinite(p.Confidence) || p.Confidence < config.Confidence)
					return false;
				if (world)
					return IsFinite3(hand.WorldKeypoints[i].data());
				return std::isfinite(p.X) && std::isfinite(p.Y)
					&& p.X >= 0 && p.X < result.ImageWidth
					&& p.Y >= 0 && p.Y < result.ImageHeight;
			};

		auto getPosition = [&](int i) -> glm::vec3
			{
				const auto& p = hand.Keypoints[i];
				if (world)
				{
					const glm::vec3 wp = getWorldPoint(i);
					return glm::vec3(handTransform * glm::vec4(wp - wristWorld + anchor, 1.0f));
				}
				return glm::vec3(offset + glm::vec2(p.X, p.Y) * scale, 0.0f);
			};

		for (int finger = 0; finger < 5; ++finger)
		{
			int previous = 0;
			for (int joint = 1; joint <= 4; ++joint)
			{
				const int current = finger * 4 + joint;
				if (isVisible(previous) && isVisible(current))
					m_geometry.AddLine(getPosition(previous), getPosition(current),
						colors[finger], thickness);
				previous = current;
			}
		}

		for (int i = 0; i < 21; ++i)
		{
			if (!isVisible(i)) continue;
			const glm::vec4 color = (i == 0) ? glm::vec4(1.0f) : colors[(i - 1) / 4];
			m_geometry.AddPoint(getPosition(i), color, radius);
		}
	}
}

void HandBuilder::DrawUI()
{
	if (ImGui::CollapsingHeader("Hand 设置"))
	{
		int space = static_cast<int>(config.Space);
		if (ImGui::Combo("绘制空间", &space, "屏幕空间\0世界空间\0"))
			config.Space = static_cast<PointLineSpace>(space);

		if (config.Space == PointLineSpace::World)
		{
			ImGui::SliderFloat("关节球半径", &config.JointRadius, 0.001f, 0.03f, "%.3f");
			ImGui::SliderFloat("骨骼直径", &config.BoneThickness, 0.001f, 0.02f, "%.3f");
			ImGui::DragFloat3("手部世界位置", &config.WorldPositionOffset.x, 0.01f);
			ImGui::DragFloat3("手部世界旋转", &config.WorldRotationOffset.x, 1.0f);
			ImGui::DragFloat3("手部世界缩放", &config.WorldScaleOffset.x, 0.01f, 0.01f, 10.0f);
			ImGui::SliderFloat("手部位置映射宽度", &config.HandImageWidth, 0.0f, 2.0f);
		}
		else
		{
			ImGui::SliderFloat("关键点半径", &config.ScreenPointRadius, 1.0f, 12.0f, "%.1f px");
			ImGui::SliderFloat("骨骼线宽", &config.ScreenLineThickness, 1.0f, 8.0f, "%.1f px");
		}
		ImGui::SliderFloat("关键点置信度", &config.Confidence, 0.0f, 1.0f);
	}
}
