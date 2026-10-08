#pragma once

#include "PostProcessBase.h"
#include "RenderResource/PointLineGeometry.h"
#include "RenderBase/RenderConfig.hpp"
#include "RenderBase/VulkanBuffer.h"
#include <array>

// 只负责渲染外部提供的 PointLineGeometry，不再管理手的姿态/几何。
class PostProcessPointLine : public PostProcessBase
{
public:
	void prepare();
	void destroy();
	void excute(VkCommandBuffer cmdBuffer, VkImageView writeImage);

	void AddGeometry(PointLineGeometry* geom)
	{
		if (geom) geometries.push_back(geom);
	}
	void ClearGeometries() { geometries.clear(); }

public:
	bool enabled = true;

private:
	VkPipeline       pipeline = VK_NULL_HANDLE;
	VkPipeline       worldPipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	std::array<vks::Buffer, MaxConcurrentFrames> vertexBuffers{};

	std::vector<PointLineGeometry*> geometries;
};