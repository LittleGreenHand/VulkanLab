#include "PostProcess_PointLine.h"
#include "PipelineBuilder.h"
#include "VulkanRenderer.h"
#include "VulkanDebugUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include "core/Log.h"

void PostProcessPointLine::prepare()
{
	auto* renderer = VulkanContext::GetVulkanRenderer();
	VkPushConstantRange pushRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLinePushConstants) };
	auto layoutInfo = vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	VkVertexInputBindingDescription binding{ 0, sizeof(PointLineGeometry::Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription attributes[] = {
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PointLineGeometry::Vertex, Position) },
		{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(PointLineGeometry::Vertex, Color) }
	};
	VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = 2;
	vertexInput.pVertexAttributeDescriptions = attributes;

	PipelineBuilder builder(device);
	builder.setVertexInputState(&vertexInput);
	builder.setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
	builder.setDepthStencilState(VK_FALSE, VK_FALSE);
	builder.enableBlendingAlphaBlend();
	builder.addShaderStage(renderer->loadShader(renderer->getShadersPath() + "PostProcess_pointLine.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	builder.addShaderStage(renderer->loadShader(renderer->getShadersPath() + "PostProcess_pointLine.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	auto renderInfo = vks::initializers::pipelineRenderingCreateInfo(1, &renderer->swapChain.colorFormat);
	VK_CHECK_RESULT(builder.buildPipeline(renderInfo, renderer->pipelineCache, pipelineLayout, pipeline));
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, "PostProcess point/line pipeline");

	builder.setDepthStencilState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	renderInfo.depthAttachmentFormat = renderer->depthFormat;
	VK_CHECK_RESULT(builder.buildPipeline(renderInfo, renderer->pipelineCache, pipelineLayout, worldPipeline));
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)worldPipeline, "PostProcess world point/line pipeline");
}

void PostProcessPointLine::destroy()
{
	for (auto& buffer : vertexBuffers) 
	{
		buffer.unmap();
		buffer.destroy();
	}
	if (pipeline)
		vkDestroyPipeline(device, pipeline, nullptr);
	if (worldPipeline)
		vkDestroyPipeline(device, worldPipeline, nullptr);
	if (pipelineLayout)
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	pipeline = VK_NULL_HANDLE;
	worldPipeline = VK_NULL_HANDLE;
	pipelineLayout = VK_NULL_HANDLE;
}

void PostProcessPointLine::SetHandPoses(const HandPoseResult& result)
{
	handPoses = result;
	lastHandPoseUpdate = std::chrono::steady_clock::now();
}

void PostProcessPointLine::ClearHandPoses()
{
	handPoses = {};
}

void PostProcessPointLine::excute(VkCommandBuffer cmdBuffer, VkImageView writeImage)
{
	if (!enabled || width == 0 || height == 0)
		return;

	auto* renderer = VulkanContext::GetVulkanRenderer();
	const bool world = space == PointLineSpace::World;
	PointLineGeometry frameGeometry = geometry;
	// 保持最近一次结果；摄像头或推理中断超过一秒时移除旧姿态。
	if (std::chrono::steady_clock::now() - lastHandPoseUpdate < std::chrono::seconds(1))
	{
		auto transform = glm::translate(glm::mat4(1.0f), handWorldPosition);
		transform = glm::rotate(transform, glm::radians(handWorldRotation.z), glm::vec3(0, 0, 1));
		transform = glm::rotate(transform, glm::radians(handWorldRotation.y), glm::vec3(0, 1, 0));
		transform = glm::rotate(transform, glm::radians(handWorldRotation.x), glm::vec3(1, 0, 0));
		// 手部坐标约定为 X 向右、Y 向下、Z 远离相机，转换到场景 Y 向上、相机朝 -Z。
		transform = glm::scale(transform, glm::vec3(handWorldScale, -handWorldScale, -handWorldScale));
		frameGeometry.AddHandPoses(handPoses, static_cast<float>(width), static_cast<float>(height), world ? worldPointRadius : pointRadius, world ? worldLineThickness : lineThickness, keypointConfidence, space, transform, handImageWidth);
	}
	const auto vertices = frameGeometry.BuildVertices(space, renderer->globalParam.inverseView);
	if (vertices.empty())
		return;
	LOG_TIME_BEGIN(PostProcessPointLine);
	const VkDeviceSize bytes = vertices.size() * sizeof(PointLineGeometry::Vertex);
	// BeginFrame 已等待当前帧的 fence，当前帧缓冲可安全更新或扩容。
	auto& buffer = vertexBuffers[renderer->currentBuffer];
	if (buffer.size < bytes)
	{
		buffer.unmap();
		buffer.destroy();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, std::max<VkDeviceSize>(bytes, 65536)));
		VK_CHECK_RESULT(buffer.map());
	}
	std::memcpy(buffer.mapped, vertices.data(), static_cast<size_t>(bytes));

	VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "Point/Line Overlay", { 1.0f, 1.0f, 1.0f });
	auto attachment = vks::initializers::RenderingAttachmentInfo_Color(writeImage);
	auto renderInfo = vks::initializers::RenderingInfo({width, height}, &attachment, nullptr);
	VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	if (world)
	{
		VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = barrier.srcStageMask;
		barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmdBuffer, &dependency);
		depthAttachment.imageView = renderer->depthStencil.view;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		renderInfo.pDepthAttachment = &depthAttachment;
	}
	vkCmdBeginRendering(cmdBuffer, &renderInfo);
	const auto viewport = vks::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
	const auto scissor = vks::initializers::rect2D(width, height, 0, 0);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world ? worldPipeline : pipeline);
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer.buffer, &offset);
	const PointLinePushConstants params{renderer->globalParam.viewProj, glm::vec2(width, height), space};
	vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(params), &params);
	vkCmdDraw(cmdBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
	vkCmdEndRendering(cmdBuffer);
	VulkanDebugUtils::CmdEndLabel(cmdBuffer);
	LOG_TIME_END(PostProcessPointLine);
}
