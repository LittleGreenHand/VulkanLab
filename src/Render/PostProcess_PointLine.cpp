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

	builder.setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
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

void PostProcessPointLine::excute(VkCommandBuffer cmdBuffer, VkImageView writeImage)
{
	if (!enabled || width == 0 || height == 0 || geometries.empty())
	{
		geometries.clear();
		return;
	}

	auto* renderer = VulkanContext::GetVulkanRenderer();
	std::vector<PointLineGeometry::Vertex> screenVerts;
	std::vector<PointLineGeometry::Vertex> worldVerts;

	for (auto* geom : geometries)
	{
		if (!geom) continue;
		PointLineSpace space = geom->GetWorldSpace();
		auto verts = geom->BuildVertices(space, renderer->globalParam.inverseView);
		if (space == PointLineSpace::World)
			worldVerts.insert(worldVerts.end(), verts.begin(), verts.end());
		else
			screenVerts.insert(screenVerts.end(), verts.begin(), verts.end());
	}

	geometries.clear();   // 自动清空，无论后面是否早退

	if (screenVerts.empty() && worldVerts.empty())
		return;

	// 合并进同一个顶点缓冲：先 Screen 后 World
	const uint32_t screenCount = static_cast<uint32_t>(screenVerts.size());
	const uint32_t worldCount = static_cast<uint32_t>(worldVerts.size());
	std::vector<PointLineGeometry::Vertex> allVerts;
	allVerts.reserve(screenCount + worldCount);
	allVerts.insert(allVerts.end(), screenVerts.begin(), screenVerts.end());
	allVerts.insert(allVerts.end(), worldVerts.begin(), worldVerts.end());

	const VkDeviceSize bytes = allVerts.size() * sizeof(PointLineGeometry::Vertex);
	auto& buffer = vertexBuffers[renderer->currentBuffer];
	if (buffer.size < bytes)
	{
		buffer.unmap();
		buffer.destroy();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&buffer, std::max<VkDeviceSize>(bytes, 65536)));
		VK_CHECK_RESULT(buffer.map());
	}
	std::memcpy(buffer.mapped, allVerts.data(), static_cast<size_t>(bytes));

	VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "Point/Line Overlay", { 1.0f, 1.0f, 1.0f });

	auto attachment = vks::initializers::RenderingAttachmentInfo_Color(writeImage);
	auto renderInfo = vks::initializers::RenderingInfo({ width, height }, &attachment, nullptr);

	// 只要有一组 World，就需要 depth attachment
	VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (worldCount > 0)
	{
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = barrier.srcStageMask;
		barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.memoryBarrierCount = 1;
		dep.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmdBuffer, &dep);

		depthAttachment.imageView = renderer->depthStencil.view;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		renderInfo.pDepthAttachment = &depthAttachment;
	}

	vkCmdBeginRendering(cmdBuffer, &renderInfo);
	const auto viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	const auto scissor = vks::initializers::rect2D(width, height, 0, 0);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer.buffer, &offset);

	// ---- Screen 组 ----
	if (screenCount > 0)
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		const PointLinePushConstants params{
			renderer->globalParam.viewProj,
			glm::vec2(width, height),
			PointLineSpace::Screen };
		vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(params), &params);

		vkCmdDraw(cmdBuffer, screenCount, 1, 0, 0);
	}

	// ---- World 组 ----
	if (worldCount > 0)
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);

		const PointLinePushConstants params{
			renderer->globalParam.viewProj,
			glm::vec2(width, height),
			PointLineSpace::World };
		vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(params), &params);

		// firstVertex = screenCount，接在 Screen 组后面
		vkCmdDraw(cmdBuffer, worldCount, 1, screenCount, 0);
	}

	vkCmdEndRendering(cmdBuffer);
	VulkanDebugUtils::CmdEndLabel(cmdBuffer);
}