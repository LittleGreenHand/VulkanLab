#include "MathUtils.h"
#include <glm/gtc/type_ptr.hpp>

glm::quat EularToQuaternion(const glm::vec3& euler)
{
	// 将角度转换为弧度
	glm::vec3 radians = glm::radians(euler);

	// 计算各个轴的旋转四元数
	glm::quat pitch = glm::angleAxis(radians.x, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat yaw = glm::angleAxis(radians.y, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat roll = glm::angleAxis(radians.z, glm::vec3(0.0f, 0.0f, 1.0f));

	// 组合旋转：注意旋转顺序是xyz
	// 因为GLM使用的是右乘，旋转顺序是从右到左应用
	return roll * yaw * pitch;
}

glm::vec3 GenerateUpVector(const glm::vec3& forward)
{
	// 找到与forward不共线的垂直向量
	glm::vec3 ref = (std::abs(forward.x) > std::abs(forward.z))
		? glm::vec3(forward.z, 0, -forward.x)  // 与X-Z平面垂直
		: glm::vec3(0, -forward.z, forward.y); // 与Y-Z平面垂直
	return glm::normalize(ref);
}

physx::PxVec3 ToPxVec3(const glm::vec3& v)
{
	return physx::PxVec3(v.x, v.y, v.z);
}

physx::PxQuat ToPxQuat(const glm::quat& q)
{
	return physx::PxQuat(q.x, q.y, q.z, q.w);
}

physx::PxTransform ToPxTransform(const glm::vec3& position, const glm::quat& rotation)
{
	return physx::PxTransform(
		ToPxVec3(position),
		ToPxQuat(rotation)
	);
}