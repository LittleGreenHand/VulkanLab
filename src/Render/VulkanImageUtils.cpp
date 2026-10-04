#include "VulkanImageUtils.h"

namespace VulkanImageUtils
{

	void TransitionImageLayout(VkCommandBuffer cmd, vks::Texture& texture, VkImageLayout newLayout, VkImageAspectFlags aspectMask, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
	{
		if (texture.imageLayout == newLayout)
			return;
		//VkImageMemoryBarrier2 是Vulkan1.3引入的扩展（VK_KHR_synchronization2）中定义的同步原语，用于更精细地控制图像内存访问顺序和布局转换
		VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		imageBarrier.pNext = nullptr;

		//VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT 是一个特殊的管线阶段标志，用于表示所有可能的管线阶段
		imageBarrier.srcStageMask = srcStageMask;//指定哪些管线阶段必须在屏障前完成
		imageBarrier.srcAccessMask = srcAccessMask;//确保内存写入在屏障前完成
		imageBarrier.dstStageMask = dstStageMask;//指定屏障后允许继续执行的管线阶段
		imageBarrier.dstAccessMask = dstAccessMask;//指定屏障后允许对资源进行的具体访问操作（如读、写），确保数据一致性

		imageBarrier.oldLayout = texture.imageLayout;
		imageBarrier.newLayout = newLayout;

		//指定图像的哪些子资源（Mip 层级、数组层）受屏障影响，要尽量缩小subresourceRange的范围（避免全局屏障）
		imageBarrier.subresourceRange = vks::initializers::ImageSubresourceRange(aspectMask);

		imageBarrier.image = texture.image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		//在命令缓冲区中插入显式屏障，或者说在命令缓冲区中记录屏障指令，实际屏障的执行发生在GPU运行该命令缓冲区时（提交到队列后）
		//vkCmdPipelineBarrier2强制GPU在屏障前完成srcStageMask指定的所有操作，并阻塞dstStageMask之前的操作，直到屏障完成
		//过度使用屏障会破坏 GPU 并行性，应尽量合并多个屏障到一次调用
		vkCmdPipelineBarrier2(cmd, &depInfo);
		texture.imageLayout = newLayout;
		texture.updateDescriptor();
	}
	
	void CopyImageToImage(VkCommandBuffer cmd, vks::Texture& srcTexture, vks::Texture& dstTexture)
	{
		assert(srcTexture.device != nullptr && dstTexture.device != nullptr && "Texture 的 device 指针不可为空");
		assert(srcTexture.image != VK_NULL_HANDLE && dstTexture.image != VK_NULL_HANDLE && "源/目标图像句柄不可为空");
		assert(srcTexture.view != VK_NULL_HANDLE && dstTexture.view != VK_NULL_HANDLE && "源/目标图像视图不可为空");
		assert(srcTexture.width == dstTexture.width && srcTexture.height == dstTexture.height && "源/目标图像宽高必须一致");
		assert(srcTexture.mipLevels == dstTexture.mipLevels && "源/目标图像 Mip 层级必须一致");
		assert(srcTexture.layerCount == dstTexture.layerCount && "源/目标图像图层数量必须一致");
		assert(srcTexture.format == dstTexture.format && "源/目标图像格式必须一致");

		// 确定图像的 AspectMask（颜色/深度/模板）
		VkImageAspectFlags aspectMask = 0;
		switch (srcTexture.format) {
			// 纯深度格式
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
			// 深度+模板格式（需确保 ImageView 创建时仅包含一个 Aspect，遵循 VUID-01976）
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			// 注意：此处默认使用深度 Aspect，若你需要复制模板，需改为 VK_IMAGE_ASPECT_STENCIL_BIT
			// （需确保 ImageView 创建时的 Aspect 与此处一致）
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
			// 颜色格式（默认所有非深度/模板格式为颜色）
		default:
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}

		// 保存原始布局（用于复制后恢复）
		VkImageLayout srcOriginalLayout = srcTexture.imageLayout;
		VkImageLayout dstOriginalLayout = dstTexture.imageLayout;

		// 转换源/目标图像到复制所需布局
		TransitionImageLayout(cmd, srcTexture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aspectMask);
		TransitionImageLayout(cmd, dstTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspectMask);

		// 配置复制区域（支持 Mip 层级和图层）
		std::vector<VkImageCopy> copyRegions;
		copyRegions.reserve(srcTexture.mipLevels * srcTexture.layerCount);

		for (uint32_t mip = 0; mip < srcTexture.mipLevels; ++mip) {
			for (uint32_t layer = 0; layer < srcTexture.layerCount; ++layer) {
				VkImageCopy region{};
				// 1. 源图像子资源配置
				region.srcSubresource.aspectMask = aspectMask;    // 与上面确定的 Aspect 一致
				region.srcSubresource.mipLevel = mip;             // 当前 Mip 层级
				region.srcSubresource.baseArrayLayer = layer;     // 当前图层
				region.srcSubresource.layerCount = 1;             // 每次复制 1 个图层
				region.srcOffset = { 0, 0, 0 };                    // 复制起始偏移（左上角）

				// 2. 目标图像子资源配置（与源完全对齐）
				region.dstSubresource.aspectMask = aspectMask;
				region.dstSubresource.mipLevel = mip;
				region.dstSubresource.baseArrayLayer = layer;
				region.dstSubresource.layerCount = 1;
				region.dstOffset = { 0, 0, 0 };

				// 3. 复制尺寸（Mip 层级缩放：原始尺寸 / 2^mip，确保不为 0）
				region.extent.width = std::max(1U, srcTexture.width >> mip);    // 宽度缩放
				region.extent.height = std::max(1U, srcTexture.height >> mip);  // 高度缩放
				region.extent.depth = 1;                                        // 你的类无 depth，默认 2D 图像

				copyRegions.push_back(region);
			}
		}

		// --------------------------
		// 步骤6：执行图像复制命令
		// --------------------------
		vkCmdCopyImage(
			cmd,
			srcTexture.image,                          // 源图像
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,      // 源布局（必须与转换后一致）
			dstTexture.image,                          // 目标图像
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,      // 目标布局（必须与转换后一致）
			static_cast<uint32_t>(copyRegions.size()), // 复制区域数量
			copyRegions.data()                         // 复制区域数组
		);
	}

	void SynchronizeColorAttachment(VkCommandBuffer cmdBuffer)
	{
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmdBuffer, &dependency);
	}

	void SynchronizeDepthAttachment(VkCommandBuffer cmdBuffer)
	{
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmdBuffer, &dependency);
	}
}