#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

enum class PointLineSpace : uint32_t { Screen, World };
struct alignas(16) PointLinePushConstants
{
	glm::mat4 ViewProjection{ 1.0f };
	glm::vec2 Extent{};
	PointLineSpace Space = PointLineSpace::Screen;
	uint32_t Padding = 0;
};

// 屏幕位置以左上角为原点；世界位置使用场景坐标。粗细与所选空间使用相同单位。
class PointLineGeometry
{
public:
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	void Clear()
	{
		points.clear();
		lines.clear();
	}

	// vec2 的默认尺寸用于屏幕像素，vec3 的默认尺寸用于世界单位。
	void AddPoint(glm::vec2 position, glm::vec4 color, float radius = 4.0f)
	{
		AddPoint(glm::vec3(position, 0), color, radius);
	}

	void AddPoint(glm::vec3 position, glm::vec4 color, float radius = 0.006f)
	{
		if (Finite(position) && std::isfinite(radius) && radius > 0)
			points.push_back({ position, color, radius });
	}

	void AddLine(glm::vec2 start, glm::vec2 end, glm::vec4 color, float thickness = 2.0f)
	{
		AddLine(glm::vec3(start, 0), glm::vec3(end, 0), color, thickness);
	}

	void AddLine(glm::vec3 start, glm::vec3 end, glm::vec4 color, float thickness = 0.003f)
	{
		if (Finite(start) && Finite(end) && std::isfinite(thickness) && thickness > 0)
			lines.push_back({ start, end, color, thickness });
	}

	std::vector<Vertex> BuildVertices(PointLineSpace space, const glm::mat4& inverseView = glm::mat4(1.0f));
	void SetWorldSpace(PointLineSpace space) { worldSpace = space; }
	PointLineSpace GetWorldSpace() { return worldSpace; }
private:
	struct Point
	{
		glm::vec3 Position;
		glm::vec4 Color;
		float Radius;
	};

	struct Line
	{
		glm::vec3 Start;
		glm::vec3 End;
		glm::vec4 Color;
		float Thickness;
	};

	std::vector<Point> points;
	std::vector<Line> lines;
	PointLineSpace worldSpace = PointLineSpace::Screen;

	static bool Finite(const glm::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}
};