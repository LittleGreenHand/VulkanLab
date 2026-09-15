#pragma once

#include <cfloat>
#include <vector>

#include <glm/glm.hpp>

struct Dimensions {
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);
	glm::vec3 size;
	glm::vec3 center;
	float radius;

	// Generate the eight world-space corners of the AABB.
	std::vector<glm::vec3> getAABBCorners(const glm::mat4& modelMatrix = glm::mat4(1.0f)) {
		std::vector<glm::vec3> corners(8);

		corners[0] = glm::vec3(min.x, min.y, min.z);
		corners[1] = glm::vec3(max.x, min.y, min.z);
		corners[2] = glm::vec3(max.x, max.y, min.z);
		corners[3] = glm::vec3(min.x, max.y, min.z);
		corners[4] = glm::vec3(min.x, min.y, max.z);
		corners[5] = glm::vec3(max.x, min.y, max.z);
		corners[6] = glm::vec3(max.x, max.y, max.z);
		corners[7] = glm::vec3(min.x, max.y, max.z);

		for (int i = 0; i < 8; i++) {
			glm::vec4 worldPos = modelMatrix * glm::vec4(corners[i], 1.0f);
			corners[i] = worldPos / worldPos.w;
		}

		return corners;
	}
};

enum class MeshTopology
{
	Points,
	Lines,
	LineLoop,
	LineStrip,
	TriangleList,
	TriangleStrip,
	TriangleFan,
};
