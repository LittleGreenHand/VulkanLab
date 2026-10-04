#pragma once

#include "PostProcessBase.h"
#include "PointLineGeometry.h"
#include "RenderBase/RenderConfig.hpp"
#include "RenderBase/VulkanBuffer.h"
#include <array>
#include <chrono>

class PostProcessPointLine : public PostProcessBase
{
public:
	bool enabled = true;
	PointLineSpace space = PointLineSpace::Screen;
	float pointRadius = 4.0f;
	float lineThickness = 2.0f;
	float worldPointRadius = 0.006f;
	float worldLineThickness = 0.003f;
	float keypointConfidence = 0.5f;
	glm::vec3 handWorldPosition{0, 0, -2};
	glm::vec3 handWorldRotation{0}; // 角度单位为度。
	float handWorldScale = 1.0f;
	float handImageWidth = 0.6f; // 原图腕部位置映射平面的宽度，单位为米。
	PointLineGeometry geometry;
	void prepare();
	void destroy();
	void SetHandPoses(const HandPoseResult& result);
	void ClearHandPoses();
	void excute(VkCommandBuffer cmdBuffer, VkImageView writeImage);

private:
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipeline worldPipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	std::array<vks::Buffer, MaxConcurrentFrames> vertexBuffers{};
	HandPoseResult handPoses;
	std::chrono::steady_clock::time_point lastHandPoseUpdate{};
};
