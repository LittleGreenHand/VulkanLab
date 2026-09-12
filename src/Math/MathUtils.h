#pragma once
#include <glm/glm.hpp>
#include "foundation/PxMathUtils.h"
#include "geometry/PxMeshScale.h"

glm::quat EularToQuaternion(const glm::vec3& euler);
glm::vec3 GenerateUpVector(const glm::vec3& forward);

physx::PxVec3 ToPxVec3(const glm::vec3& v);
physx::PxQuat ToPxQuat(const glm::quat& q);
physx::PxMeshScale ToPxMeshScale(const glm::vec3& scale, const glm::quat& rotation);
physx::PxTransform ToPxTransform(const glm::vec3& position, const glm::quat& rotation);