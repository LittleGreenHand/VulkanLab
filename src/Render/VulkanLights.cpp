#include "VulkanLights.h"
#include "VulkanContext.h"
#include "PipelineBuilder.h"
#include "RenderResource/MeshManager.h"
#include "VulkanDebugUtils.h"
#include "Math/MathUtils.h"

VkDescriptorSetLayout vkLight::descriptorSetLayout{ VK_NULL_HANDLE };
VkDescriptorSet vkLight::descriptorSet{ VK_NULL_HANDLE };
vkLight::LightUbo vkLight::lightData{};
vks::Buffer vkLight::lightBuffer{};

void vkLight::preperDescriptor(vks::VulkanDevice* vulkanDevice, VkDescriptorPool descriptorPool)
{
	//创建布局
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayout, "LightsDescriptorSetLayout");
	
	//分配描述符集
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.pSetLayouts = &descriptorSetLayout;
	allocInfo.descriptorSetCount = 1;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet));
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, "LightsDescriptorSet");

	//创建Buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &lightBuffer, sizeof(LightUbo)));
	VK_CHECK_RESULT(lightBuffer.map());
	updateLightBuffer();

	//绑定Buffer到描述符集
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = descriptorSet;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &lightBuffer.descriptor;
	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
}

void vkLight::updateLightBuffer()
{
	memcpy(lightBuffer.mapped, &lightData, sizeof(LightUbo));
}

void vkLight::destroyLightBuffer()
{
	lightBuffer.destroy();
}

//-------------------------点光 START-------------------------
namespace vkLight {

	void VulkanPointLights::preperShadowCubeMap()
	{
		lightResource.shadowCubeMap.width = shadowCubeSize;
		lightResource.shadowCubeMap.height = shadowCubeSize;
		lightResource.shadowCubeMap.format = shadowCubeFormat;
		lightResource.shadowCubeMap.device = vulkanDevice;
		lightResource.shadowCubeMap.mipLevels = 1;
		lightResource.shadowCubeMap.layerCount = 6 * lightData.activePointLightCount;//每个点光源都有6个面来构成立方体阴影贴图

		// Cube map image description
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = shadowCubeFormat;
		imageCreateInfo.extent = { shadowCubeSize, shadowCubeSize, 1 };
		imageCreateInfo.mipLevels = lightResource.shadowCubeMap.mipLevels;
		imageCreateInfo.arrayLayers = lightResource.shadowCubeMap.layerCount;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Create cube map image
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &lightResource.shadowCubeMap.image));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)lightResource.shadowCubeMap.image, "PointLight ShadowCubeArray");

		vkGetImageMemoryRequirements(device, lightResource.shadowCubeMap.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &lightResource.shadowCubeMap.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, lightResource.shadowCubeMap.image, lightResource.shadowCubeMap.deviceMemory, 0));

		// Image barrier for optimal image (target)
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = lightResource.shadowCubeMap.layerCount;
		vks::tools::setImageLayout(
			layoutCmd,
			lightResource.shadowCubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);
		vulkanDevice->flushCommandBuffer(layoutCmd, VulkanContext::GetVulkanRenderer()->m_queue, true);
		lightResource.shadowCubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &lightResource.shadowCubeMap.sampler));

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.image = lightResource.shadowCubeMap.image;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		view.format = shadowCubeFormat;
		view.components = { VK_COMPONENT_SWIZZLE_R };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = lightResource.shadowCubeMap.layerCount;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &lightResource.shadowCubeMap.view));

		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.subresourceRange.layerCount = 1;
		view.image = lightResource.shadowCubeMap.image;

		for (int i = 0; i < lightData.activePointLightCount; ++i)
		{
			for (uint32_t face = 0; face < 6; face++)
			{
				int index = i * 6 + face;
				view.subresourceRange.baseArrayLayer = index;
				VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &lightResource.shadowCubeMapFaceImageViews[index]));
			}
		}

		//更新描述符集
		if (isDescriptorUpdated)
		{
			lightResource.shadowCubeMap.updateDescriptor();
			
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &lightResource.shadowCubeMap.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
			isDescriptorUpdated = false;
		}
	}

	void VulkanPointLights::prepareFramebuffer()
	{
		// Color attachment
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = shadowCubeFormat;
		imageCreateInfo.extent.width = renderPass.width;
		imageCreateInfo.extent.height = renderPass.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		// Image of the framebuffer is blit source
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		// Depth stencil attachment
		imageCreateInfo.format = shadowDepthFormat;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format = shadowDepthFormat;
		depthStencilView.flags = 0;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (shadowDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
			depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &renderPass.depthAttachment.image));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)renderPass.depthAttachment.image, "PointLight DepthAttachment");
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, renderPass.depthAttachment.image, &memReqs);

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &renderPass.depthAttachment.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, renderPass.depthAttachment.image, renderPass.depthAttachment.deviceMemory, 0));

		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vks::tools::setImageLayout(
			layoutCmd,
			renderPass.depthAttachment.image,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		vulkanDevice->flushCommandBuffer(layoutCmd, VulkanContext::GetVulkanRenderer()->m_queue, true);

		depthStencilView.image = renderPass.depthAttachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &renderPass.depthAttachment.view));
		renderPass.depthAttachment.device = vulkanDevice;
		VkImageView attachments[2];
		attachments[1] = renderPass.depthAttachment.view;

		VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = renderPass.renderPass;
		fbufCreateInfo.attachmentCount = 2;
		fbufCreateInfo.pAttachments = attachments;
		fbufCreateInfo.width = renderPass.width;
		fbufCreateInfo.height = renderPass.height;
		fbufCreateInfo.layers = 1;

		for (int id = 0; id < lightData.activePointLightCount; ++id)
		{
			for (uint32_t face = 0; face < 6; face++)
			{
				int index = id * 6 + face;
				attachments[0] = lightResource.shadowCubeMapFaceImageViews[index];
				VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &lightResource.frameBuffers[index]));
			}
		}
	}

	void VulkanPointLights::preperRenderPass()
	{
		VkAttachmentDescription osAttachments[2] = {};

		osAttachments[0].format = shadowCubeFormat;
		osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Depth attachment
		osAttachments[1].format = shadowDepthFormat;
		osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

		VkRenderPassCreateInfo renderPassCreateInfo = vks::initializers::renderPassCreateInfo();
		renderPassCreateInfo.attachmentCount = 2;
		renderPassCreateInfo.pAttachments = osAttachments;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass.renderPass));
	}
	struct PushConstants {
		glm::mat4 MVP;
		glm::mat4 Model;
		glm::vec4 LightPos;
	};
	void VulkanPointLights::preperPipeline()
	{
		VkPushConstantRange pushConstantRange[1]{};
		pushConstantRange[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange[0].offset = 0;
		pushConstantRange[0].size = sizeof(PushConstants);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo();
		pipelineLayoutCreateInfo.setLayoutCount = 0;
		pipelineLayoutCreateInfo.pSetLayouts = nullptr;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		PipelineBuilder builder(device);
		builder.addShaderStage(VulkanContext::GetVulkanRenderer()->loadShader(VulkanContext::GetVulkanRenderer()->getShadersPath() + "Light_point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		builder.addShaderStage(VulkanContext::GetVulkanRenderer()->loadShader(VulkanContext::GetVulkanRenderer()->getShadersPath() + "Light_point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		builder.setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		builder.buildPipeline(renderPass.renderPass, VulkanContext::GetVulkanRenderer()->pipelineCache, pipelineLayout, pipeline);
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, "DirectLightShadowMapGenerate pipeline");
	}

	void VulkanPointLights::preperViewMatrix()
	{
		// POSITIVE_X
		viewMatrix[0] = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewMatrix[0] = glm::rotate(viewMatrix[0], glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// NEGATIVE_X
		viewMatrix[1] = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewMatrix[1] = glm::rotate(viewMatrix[1], glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// POSITIVE_Y
		viewMatrix[2] = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// NEGATIVE_Y
		viewMatrix[3] = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// POSITIVE_Z
		viewMatrix[4] = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// NEGATIVE_Z
		viewMatrix[5] = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	}

	void VulkanPointLights::Render(VkCommandBuffer cmdBuffer)
	{
		VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "PointLightShadowCube", { 1.0f, 1.0f, 1.0f });

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = vks::initializers::viewport((float)renderPass.width, (float)renderPass.height, 0.0f, 1.0f);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = vks::initializers::rect2D(renderPass.width, renderPass.height, 0, 0);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderPass.renderPass;
		renderPassBeginInfo.renderArea.extent.width = renderPass.width;
		renderPassBeginInfo.renderArea.extent.height = renderPass.height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		//为每个点光源的阴影立方体贴图的每个面渲染阴影
		for (int id = 0; id < lightData.activePointLightCount; ++id)
		{
			if (lightData.pointLights[id].isRnder)
			{
				glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.001f, 1024.f);
				//shadowProj[1][1] *= -1;
				for (uint32_t face = 0; face < 6; face++)
				{
					int index = id * 6 + face;
					renderPassBeginInfo.framebuffer = lightResource.frameBuffers[index];
					vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

					for (auto& [key, model] : MeshManager::Get().m_sceneTree)
					{
						vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) * 2, sizeof(glm::vec4), &lightData.pointLights[id].position);
						model.drawWithPushConstant(cmdBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pipelineLayout, shadowProj * glm::translate(viewMatrix[face], glm::vec3(-lightData.pointLights[id].position)), true); 
					}

					vkCmdEndRenderPass(cmdBuffer);
				}
			}
		}

		VulkanDebugUtils::CmdEndLabel(cmdBuffer);
	}

}
//-------------------------点光 END-------------------------

//-------------------------定向光 START-------------------------
namespace vkLight {
	void VulkanDirectLights::updateCascades()
	{
		float cascadeSplits[MAX_CASCADES];

		float nearClip = VulkanContext::GetVulkanRenderer()->camera.getNearClip();
		float farClip = VulkanContext::GetVulkanRenderer()->camera.getFarClip();
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (int i = 0; i < lightData.directLight.cascadeCount; i++) {
			float p = (i + 1) / static_cast<float>(lightData.directLight.cascadeCount);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = cascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (int i = 0; i < lightData.directLight.cascadeCount; i++) {
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] = {
				glm::vec3(-1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(VulkanContext::GetVulkanRenderer()->camera.matrices.perspective * VulkanContext::GetVulkanRenderer()->camera.matrices.view);
			for (int j = 0; j < 8; j++) {
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
				frustumCorners[j] = invCorner / invCorner.w;
			}

			for (uint32_t j = 0; j < 4; j++) {
				glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
				frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
				frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t j = 0; j < 8; j++) {
				frustumCenter += frustumCorners[j];
			}
			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (uint32_t j = 0; j < 8; j++) {
				float distance = glm::length(frustumCorners[j] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = glm::normalize(-lightData.directLight.direct);
			glm::vec3 up = GenerateUpVector(lightDir);
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, up);
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
			lightOrthoMatrix[1][1] *= -1.0f;
			// Store split distance and matrix in cascade
			cascades[i].splitDepth = (VulkanContext::GetVulkanRenderer()->camera.getNearClip() + splitDist * clipRange) * -1.0f;
			cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;
			cascades[i].viewMat = lightViewMatrix;
			cascades[i].Proj = lightOrthoMatrix;

			lastSplitDist = cascadeSplits[i];
		}
		for (int i = 0; i < lightData.directLight.cascadeCount; i++) {
			lightData.directLight.ViewProj[i] = cascades[i].viewProjMatrix;
			lightData.directLight.cascadeSplits[i].value = cascades[i].splitDepth;
		}

		updateLightBuffer();
	}

	void VulkanDirectLights::prepareFramebuffer()
	{
		renderPass.width = shadowMapize;
		renderPass.height = shadowMapize;

		// For shadow mapping we only need a depth attachment
		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.extent.width = renderPass.width;
		image.extent.height = renderPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = lightData.directLight.cascadeCount;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.format = shadowMapFormat;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;		// We will sample directly from the depth attachment for the shadow mapping
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &renderPass.depthAttachment.image));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)renderPass.depthAttachment.image, "DirectLightShadowTexture");

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, renderPass.depthAttachment.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &renderPass.depthAttachment.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, renderPass.depthAttachment.image, renderPass.depthAttachment.deviceMemory, 0));

		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = lightData.directLight.cascadeCount;
		vks::tools::setImageLayout(
			layoutCmd,
			renderPass.depthAttachment.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(layoutCmd, VulkanContext::GetVulkanRenderer()->m_queue, true);
		renderPass.depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		depthStencilView.format = shadowMapFormat;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = lightData.directLight.cascadeCount;
		depthStencilView.image = renderPass.depthAttachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &renderPass.depthAttachment.view));

		// Create sampler to sample from to depth attachment
		// Used to sample in the fragment shader for shadowed rendering
		VkFilter shadowmap_filter = vks::tools::formatIsFilterable(vulkanDevice->physicalDevice, shadowMapFormat, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = shadowmap_filter;
		sampler.minFilter = shadowmap_filter;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &renderPass.depthAttachment.sampler));
		renderPass.depthAttachment.device = vulkanDevice;
		preperRenderPass();

		// One image view and framebuffer per cascade
		for (int i = 0; i < lightData.directLight.cascadeCount; i++) {
			// Image view for this cascade's layer (inside the depth map)
			// This view is used to render to that specific depth image layer
			VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.format = shadowMapFormat;
			viewInfo.subresourceRange = {};
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = i;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.image = renderPass.depthAttachment.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &cascades[i].view));
			// Framebuffer
			VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
			framebufferInfo.renderPass = renderPass.renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &cascades[i].view;
			framebufferInfo.width = renderPass.width;
			framebufferInfo.height = renderPass.height;
			framebufferInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &cascades[i].frameBuffer));
		}

		//更新描述符集
		if (isDescriptorUpdated)
		{
			renderPass.depthAttachment.updateDescriptor();
			renderPass.depthAttachment.descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &renderPass.depthAttachment.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
			isDescriptorUpdated = false;
		}
	}

	void VulkanDirectLights::preperRenderPass(bool useDepth)
	{
		VkAttachmentDescription attachmentDescription{};
		attachmentDescription.format = shadowMapFormat;
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at beginning of the render pass
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// We will read from depth, so it's important to store the depth attachment results
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// We don't care about initial layout of the attachment
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;// Attachment will be transitioned to shader read at render pass end

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 0;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;			// Attachment will be used as depth/stencil during render pass

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 0;													// No color attachments
		subpass.pDepthStencilAttachment = &depthReference;									// Reference to our depth attachment

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies{};

		//确保在当前渲染通道开始前，所有可能读取阴影图的操作已经完成，避免新的深度写入与旧的读取冲突
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//确保当前渲染通道的深度写入操作完全完成后，后续阶段才能读取该阴影图，保证读取到的是最新数据
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo = vks::initializers::renderPassCreateInfo();
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &attachmentDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassCreateInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassCreateInfo, nullptr, &renderPass.renderPass));
	}

	void VulkanDirectLights::preperPipeline()
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(glm::mat4);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo();
		pipelineLayoutCreateInfo.setLayoutCount = 0;
		pipelineLayoutCreateInfo.pSetLayouts = nullptr;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		PipelineBuilder builder(device);
		// 启用深度测试和写入，仅当前像素深度值“小于或等于”深度缓冲区中已存值时，通过测试
		builder.setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		builder.addShaderStage(VulkanContext::GetVulkanRenderer()->loadShader(VulkanContext::GetVulkanRenderer()->getShadersPath() + "Light_direct.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		builder.colorBlendState.attachmentCount = 0;
		builder.rasterizationState.cullMode = VK_CULL_MODE_NONE;
		builder.rasterizationState.depthBiasEnable = VK_TRUE;
		builder.dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS); //向动态状态添加深度偏移，这样我们就能在运行时对其进行更改
		builder.dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(builder.dynamicStateEnables);
		builder.buildPipeline(renderPass.renderPass, VulkanContext::GetVulkanRenderer()->pipelineCache, pipelineLayout, pipeline);
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, "DirectLightShadowMapGenerate pipeline");
	}

	void VulkanDirectLights::Render(VkCommandBuffer cmdBuffer)
	{
		if (lightData.directLight.isRnder == 0)
			return;
		VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "DirectLightShadow", { 1.0f, 1.0f, 1.0f });

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[1]{};
		{
			clearValues[0].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = renderPass.renderPass;
			renderPassBeginInfo.renderArea.extent.width = renderPass.width;
			renderPassBeginInfo.renderArea.extent.height = renderPass.height;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			VkViewport viewport = vks::initializers::viewport((float)renderPass.width, (float)renderPass.height, 0.0f, 1.0f);
			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
			VkRect2D scissor = vks::initializers::rect2D(renderPass.width, renderPass.height, 0, 0);
			vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdSetDepthBias(cmdBuffer, depthBiasConstant, 0.0f, depthBiasSlope);
			// One pass per cascade
			for (int j = 0; j < lightData.directLight.cascadeCount; j++) {
				renderPassBeginInfo.framebuffer = cascades[j].frameBuffer;
				vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				for (auto& [key, model] : MeshManager::Get().m_sceneTree)
				{
					model.drawWithPushConstant(cmdBuffer, VK_SHADER_STAGE_VERTEX_BIT, pipelineLayout, cascades[j].viewProjMatrix);
				}

				vkCmdEndRenderPass(cmdBuffer);
			}
		}
		VulkanDebugUtils::CmdEndLabel(cmdBuffer);

		/*
			Note: 与后续的渲染通道之间不需要显式同步，因为其同步规则已经通过子通道依赖关系隐式完成
		*/
	}
}
//-------------------------定向光 END-------------------------