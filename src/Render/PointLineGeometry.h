#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include <glm/glm.hpp>
#include "AI/HandPose.h"

enum class PointLineSpace : uint32_t { Screen, World };

struct alignas(16) PointLinePushConstants
{
	glm::mat4 ViewProjection{1.0f};
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
		if (Finite(position) && std::isfinite(radius) && radius > 0) points.push_back({position, color, radius});
	}
	void AddLine(glm::vec2 start, glm::vec2 end, glm::vec4 color, float thickness = 2.0f) 
	{
		AddLine(glm::vec3(start, 0), glm::vec3(end, 0), color, thickness);
	}
	void AddLine(glm::vec3 start, glm::vec3 end, glm::vec4 color, float thickness = 0.003f)
	{
		if (Finite(start) && Finite(end) && std::isfinite(thickness) && thickness > 0) lines.push_back({start, end, color, thickness});
	}

	std::vector<Vertex> BuildVertices(PointLineSpace space, const glm::mat4& inverseView = glm::mat4(1.0f)) const
	{
		std::vector<Vertex> vertices;
		vertices.reserve(lines.size() * 6 + points.size() * 48);
		const bool world = space == PointLineSpace::World;
		const glm::vec3 right = world ? glm::normalize(glm::vec3(inverseView[0])) : glm::vec3(1, 0, 0);
		const glm::vec3 up = world ? glm::normalize(glm::vec3(inverseView[1])) : glm::vec3(0, 1, 0);
		const glm::vec3 forward = world ? glm::normalize(glm::vec3(inverseView[2])) : glm::vec3(0, 0, 1);
		for (const auto& line : lines)
		{
			const auto start = world ? line.Start : glm::vec3(glm::vec2(line.Start), 0);
			const auto end = world ? line.End : glm::vec3(glm::vec2(line.End), 0);
			const auto delta = end - start;
			const float length = glm::length(delta);
			if (!std::isfinite(length) || length < 0.000001f) continue;
			auto normal = glm::cross(forward, delta / length);
			const float normalLength = glm::length(normal);
			normal = (normalLength > 0.000001f ? normal / normalLength : right) * (line.Thickness * 0.5f);
			vertices.insert(vertices.end(), {{start + normal, line.Color}, {start - normal, line.Color}, {end + normal, line.Color}, {end + normal, line.Color}, {start - normal, line.Color}, {end - normal, line.Color}});
		}
		constexpr int segments = 16;
		for (const auto& point : points)
		{
			const auto center = world ? point.Position : glm::vec3(glm::vec2(point.Position), 0);
			for (int i = 0; i < segments; ++i)
			{
				const float a = i * 2.0f * std::numbers::pi_v<float> / segments;
				const float b = (i + 1) * 2.0f * std::numbers::pi_v<float> / segments;
				vertices.insert(vertices.end(), {{center, point.Color}, {center + point.Radius * (std::cos(a) * right + std::sin(a) * up), point.Color}, {center + point.Radius * (std::cos(b) * right + std::sin(b) * up), point.Color}});
			}
		}
		return vertices;
	}

	// 世界姿态以腕部为锚点，原图腕部位置分布在可配置宽度的场景平面上。
	// handTransform 定义手部坐标到场景的变换，handImageWidth 使用变换前的米制单位。
	void AddHandPoses(const HandPoseResult& result, float width, float height, float radius, float thickness, float confidence, PointLineSpace space = PointLineSpace::Screen, const glm::mat4& handTransform = glm::mat4(1.0f), float handImageWidth = 0.6f)
	{
		if (result.ImageWidth <= 0 || result.ImageHeight <= 0 || width <= 0 || height <= 0) 
			return;
		const bool world = space == PointLineSpace::World;
		const float scale = std::min(width / result.ImageWidth, height / result.ImageHeight);
		const glm::vec2 offset((width - result.ImageWidth * scale) * 0.5f, (height - result.ImageHeight * scale) * 0.5f);
		const glm::vec4 colors[] = {{1, .31f, .31f, 1}, {1, .78f, .31f, 1}, {.39f, 1, .31f, 1}, {.31f, .71f, 1, 1}, {.78f, .31f, 1, 1}};
		for (const auto& hand : result.Hands)
		{
			auto worldPoint = [&](int index) { 
					const auto& p = hand.WorldKeypoints[index]; 
					return glm::vec3(p[0], p[1], p[2]); 
				};
			if (world && (!hand.HasWorldKeypoints || !Finite(worldPoint(0)))) 
				continue;
			const auto& wrist = hand.Keypoints[0];
			if (world && (!std::isfinite(wrist.X) || !std::isfinite(wrist.Y)))
				continue;
			const glm::vec3 anchor((wrist.X / result.ImageWidth - .5f) * handImageWidth, (wrist.Y / result.ImageHeight - .5f) * handImageWidth * result.ImageHeight / result.ImageWidth, 0);
			auto visible = [&](int index) {
				const auto& p = hand.Keypoints[index];
				if (!std::isfinite(p.Confidence) || p.Confidence < confidence) return false;
				return world ? Finite(worldPoint(index)) : Finite({p.X, p.Y, 0}) && p.X >= 0 && p.X < result.ImageWidth && p.Y >= 0 && p.Y < result.ImageHeight;
			};
			auto position = [&](int index) {
				const auto& p = hand.Keypoints[index];
				return world ? glm::vec3(handTransform * glm::vec4(worldPoint(index) - worldPoint(0) + anchor, 1)) : glm::vec3(offset + glm::vec2(p.X, p.Y) * scale, 0);
			};
			for (int finger = 0; finger < 5; ++finger)
			{
				int previous = 0;
				for (int joint = 1; joint <= 4; ++joint)
				{
					const int current = finger * 4 + joint;
					if (visible(previous) && visible(current)) AddLine(position(previous), position(current), colors[finger], thickness);
					previous = current;
				}
			}
			for (int index = 0; index < 21; ++index)
				if (visible(index)) AddPoint(position(index), index == 0 ? glm::vec4(1.0f) : colors[(index - 1) / 4], radius);
		}
	}

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
	static bool Finite(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
};
