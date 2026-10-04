#pragma once
#include <vulkan/vulkan.h>
#include "RenderBase/VulkanTexture.h"

namespace VulkanImageUtils
{
	void TransitionImageLayout(VkCommandBuffer cmd, vks::Texture& texture, VkImageLayout newLayout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkAccessFlags2 dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT);
	void CopyImageToImage(VkCommandBuffer cmd, vks::Texture& srcTexture, vks::Texture& dstTexture);
	// 添加附件的读写屏障，确保在同一渲染通道中对附件的写入操作完成后，才能进行后续的读写操作。
	void SynchronizeColorAttachment(VkCommandBuffer cmdBuffer);
	void SynchronizeDepthAttachment(VkCommandBuffer cmdBuffer);
}