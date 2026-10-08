#include "PointLineGeometry.h"


std::vector<PointLineGeometry::Vertex> PointLineGeometry::BuildVertices(PointLineSpace space, const glm::mat4& inverseView /*= glm::mat4(1.0f)*/)
{
	std::vector<Vertex> vertices;
	vertices.reserve(lines.size() * 6 + points.size() * 48);
	worldSpace = space;
	const bool world = space == PointLineSpace::World;
	const glm::vec3 right = world ? glm::normalize(glm::vec3(inverseView[0])) : glm::vec3(1, 0, 0);
	const glm::vec3 up = world ? glm::normalize(glm::vec3(inverseView[1])) : glm::vec3(0, 1, 0);
	const glm::vec3 forward = world ? glm::normalize(glm::vec3(inverseView[2])) : glm::vec3(0, 0, 1);

	// 线
	for (const auto& line : lines)
	{
		const auto& start = line.Start;
		const auto& end = line.End;
		const auto delta = end - start;
		const float length = glm::length(delta);
		if (!std::isfinite(length) || length < 0.000001f) continue;

		auto normal = glm::cross(forward, delta / length);
		const float normalLength = glm::length(normal);
		normal = (normalLength > 0.000001f ? normal / normalLength : right) * (line.Thickness * 0.5f);

		// 两个三角形构成一个矩形
		vertices.push_back({ start + normal, line.Color });
		vertices.push_back({ start - normal, line.Color });
		vertices.push_back({ end + normal, line.Color });

		vertices.push_back({ end + normal, line.Color });
		vertices.push_back({ start - normal, line.Color });
		vertices.push_back({ end - normal, line.Color });
	}

	// 点（圆形）
	constexpr int segments = 16;
	// 预计算三角函数值
	static const float cosTable[segments] = {
		1.0f, 0.9238795f, 0.7071068f, 0.3826834f, 0.0f, -0.3826834f, -0.7071068f, -0.9238795f,
		-1.0f, -0.9238795f, -0.7071068f, -0.3826834f, 0.0f, 0.3826834f, 0.7071068f, 0.9238795f
	};
	static const float sinTable[segments] = {
		0.0f, 0.3826834f, 0.7071068f, 0.9238795f, 1.0f, 0.9238795f, 0.7071068f, 0.3826834f,
		0.0f, -0.3826834f, -0.7071068f, -0.9238795f, -1.0f, -0.9238795f, -0.7071068f, -0.3826834f
	};

	for (const auto& point : points)
	{
		const auto& center = point.Position;
		for (int i = 0; i < segments; ++i)
		{
			const int next = (i + 1) % segments;
			const glm::vec3 p1 = center + point.Radius * (cosTable[i] * right + sinTable[i] * up);
			const glm::vec3 p2 = center + point.Radius * (cosTable[next] * right + sinTable[next] * up);

			vertices.push_back({ center, point.Color });
			vertices.push_back({ p1, point.Color });
			vertices.push_back({ p2, point.Color });
		}
	}

	return vertices;
}
