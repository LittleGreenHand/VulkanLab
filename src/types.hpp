#pragma once
#include <vulkan/vulkan.h>
#include "RenderBase/VulkanTexture.h"
#include <glm/glm.hpp>

constexpr uint32_t MaxConcurrentFrames{ 2 };
//描述符集在shader中的绑定索引，对应于types.slang中的声明
enum DescriptorSetBindIndex {
	LBI_GLOBAL = 0,
	LBI_IBL,
	LBI_LIGHTS,
	LBI_MATERIALS,
	LBI_MESH,
	LBI_CUSTOM,
	LBI_COUNT
};

enum BaseModels {
	M_Cube,
	M_Cerberus,
	M_Sponza,
	M_Sphere,
	M_Axis,
	M_Terrain
};

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

struct Dimensions {
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);
	glm::vec3 size;
	glm::vec3 center;
	float radius;

	// 生成AABB的8个世界空间角点
	std::vector<glm::vec3> getAABBCorners(const glm::mat4& modelMatrix = glm::mat4(1.0f)) {
		std::vector<glm::vec3> corners(8);

		// 定义AABB在本地空间的8个角点（基于min和max）
		corners[0] = glm::vec3(min.x, min.y, min.z); // 前下左
		corners[1] = glm::vec3(max.x, min.y, min.z); // 前下右
		corners[2] = glm::vec3(max.x, max.y, min.z); // 前上右
		corners[3] = glm::vec3(min.x, max.y, min.z); // 前上左
		corners[4] = glm::vec3(min.x, min.y, max.z); // 后下左
		corners[5] = glm::vec3(max.x, min.y, max.z); // 后下右
		corners[6] = glm::vec3(max.x, max.y, max.z); // 后上右
		corners[7] = glm::vec3(min.x, max.y, max.z); // 后上左

		// 将本地空间角点转换为世界空间
		for (int i = 0; i < 8; i++) {
			glm::vec4 worldPos = modelMatrix * glm::vec4(corners[i], 1.0f);
			corners[i] = worldPos / worldPos.w; // 齐次坐标归一化
		}

		return corners;
	}
};

struct PushConstantConfig {
	const void* data = nullptr;               // 自定义推送常量数据
	size_t size = 0;                          // 自定义推送常量大小
	VkShaderStageFlags stages = 0;            // 影响的着色器阶段
	uint32_t offset = 0;                      // 在推送常量布局中的偏移量
};

enum GBufferId {
	GBufferAlbedoAlpha = 0,
	GBufferNormal,
	GBufferTangent,
	GBufferMaterial,
	GBufferPosition,
	GBufferMotionVector,
	GBufferCount
};
inline VkFormat GBufferFormats[] = {
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferAlbedoAlpha
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferNormal
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferTangent
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferMaterial
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferPosition
		VK_FORMAT_R16G16B16A16_SFLOAT,	//GBufferMotionVector
};
inline const char* GBufferNames[] = {
	"GBufferAlbedoAlpha",
	"GBufferNormal",
	"GBufferTangent",
	"GBufferMaterial",
	"GBufferPosition",
	"GBufferMotionVector",
};
struct GBuffer {
	vks::Texture texture[GBufferCount];
	void destroy()
	{
		for (int i = 0; i < GBufferCount; ++i)
			texture[i].destroy();
	}
};

enum class MeshTopology
{
	Points,
	Lines,
	LineLoop,
	LineStrip,
	TriangleList,
	TriangleStrip,
	TriangleFan,
};