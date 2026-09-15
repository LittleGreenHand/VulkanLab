#include "PostProcess_ToneMapping.h"
#include "PipelineBuilder.h"
#include "VulkanRenderer.h"
#include "RenderBase/VulkanInitializers.hpp"
#include "VulkanDebugUtils.h"

void PostProcessToneMapping::prepare()
{
	prepareDescriptorSet();
	preparePipeline();
}
void PostProcessToneMapping::destroy()
{
	if (pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	if (pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}
	if (descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;
	}
}

void PostProcessToneMapping::excute(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo sampleImageInfo, VkImageView writeImage)
{
	VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "ToneMapping", { 1.0f, 1.0f, 1.0f });

	BindDescriptorSets(cmdBuffer, sampleImageInfo);
	VkRenderingAttachmentInfo colorAttachment = vks::initializers::RenderingAttachmentInfo_Color(writeImage, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vks::initializers::RenderingInfo({width, height}, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmdBuffer, &renderInfo);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
	vkCmdEndRendering(cmdBuffer);

	VulkanDebugUtils::CmdEndLabel(cmdBuffer);
}

void PostProcessToneMapping::BindDescriptorSets(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo sampleImageInfo)
{
	// 更新描述符集
	{
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		vks::initializers::writeDescriptorSet(descriptorSet[VulkanContext::GetVulkanRenderer()->currentBuffer], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &sampleImageInfo)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
	//绑定描述符集
	{
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, LBI_GLOBAL, 1, &VulkanContext::GetVulkanRenderer()->globalDescriptorSets[VulkanContext::GetVulkanRenderer()->currentBuffer], 0, nullptr);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, LBI_CUSTOM, 1, &descriptorSet[VulkanContext::GetVulkanRenderer()->currentBuffer], 0, nullptr);
	}
}
void PostProcessToneMapping::prepareDescriptorSet()
{
	//创建描述符集布局
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0));

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		descriptorLayoutCI.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayout, "PostProcess_toneMapping DescriptorSetLayout ");
	}

	//创建描述符集
	{
		// 分配描述符集
		for (auto& descriptor : descriptorSet)
		{
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor));
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptor, "PostProcess_toneMapping DescriptorSet");
		}
	}
}

void PostProcessToneMapping::preparePipeline()
{
	std::array<VkDescriptorSetLayout, LBI_COUNT> setLayoutsVector;
	setLayoutsVector.fill(VK_NULL_HANDLE);
	setLayoutsVector[LBI_GLOBAL] = VulkanContext::GetVulkanRenderer()->setLayouts[LBI_GLOBAL];
	setLayoutsVector[LBI_CUSTOM] = descriptorSetLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayoutsVector);
	pipelineLayoutCreateInfo.setLayoutCount = LBI_COUNT;
	pipelineLayoutCreateInfo.flags |= VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;//允许描述符集可以为空
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	PipelineBuilder builder(device);
	//builder.setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	builder.addShaderStage(fullScreenShaderStage);
	builder.addShaderStage(VulkanContext::GetVulkanRenderer()->loadShader(VulkanContext::GetVulkanRenderer()->getShadersPath() + "PostProcess_toneMapping.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	VkPipelineRenderingCreateInfo renderInfo = vks::initializers::pipelineRenderingCreateInfo(1, &VulkanContext::GetVulkanRenderer()->swapChain.colorFormat);

	builder.buildPipeline(renderInfo, VulkanContext::GetVulkanRenderer()->pipelineCache, pipelineLayout, pipeline);
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, "PostProcess_toneMapping pipeline");
}
