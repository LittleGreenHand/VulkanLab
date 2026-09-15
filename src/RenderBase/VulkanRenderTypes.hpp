#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTexture.h"

struct RenderPassInfo {
	int32_t width = 0;
	int32_t height = 0;
	VkRenderPass renderPass{ VK_NULL_HANDLE };
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<vks::Texture> colorAttachments;
	vks::Texture depthAttachment;

	void destroy(VkDevice device)
	{
		if (renderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(device, renderPass, nullptr);
		for (auto& frameBuffer : frameBuffers)
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
		for (auto& color : colorAttachments)
			color.destroy();
		colorAttachments.clear();
		depthAttachment.destroy();
	}
};

struct PipelineInfo {
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };

	void destroy(VkDevice device)
	{
		if (pipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, pipeline, nullptr);
		if (layout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(device, layout, nullptr);
	}
};

struct PushConstantConfig {
	const void* data = nullptr;
	size_t size = 0;
	VkShaderStageFlags stages = 0;
	uint32_t offset = 0;
};
