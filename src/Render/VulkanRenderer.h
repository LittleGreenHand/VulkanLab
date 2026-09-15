#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fstream>
#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <vulkan/vulkan.h>
#include "RenderBase/VulkanRendererBase.h"
#include "VulkanLights.h"

enum PipelinesIndex {
	PL_Skybox = 0,
	PL_PBR_BLEND,						//半透明物体前向渲染
	PL_PBR_DEFER_GEOMETRY_Opaque,		//不透明物体的延迟渲染几何阶段
	PL_PBR_DEFER_GEOMETRY_AlphaMasked,	//遮罩物体的延迟渲染几何阶段
	PL_PBR_DEFER_LIGHTING,				//延迟渲染光照阶段
	PL_Count
};

class PostProcessManager;

// 主要负责渲染管线和渲染过程的管理
class VulkanRenderer : public VulkanRendererBase
{
public:
	bool displaySkybox = true;
	int showGBuffer = -1;
	vkLight::VulkanPointLights pointLights;
	vkLight::VulkanDirectLights directLight;
	PostProcessManager* postProcessManager = nullptr;

	std::array<PipelineInfo, PL_Count> pipelines{};
	std::array<VkDescriptorSetLayout, LBI_COUNT> setLayouts{};
	VkDescriptorSet IBLDescriptorSet{ VK_NULL_HANDLE };

	struct alignas(16) GlobalParams {
		glm::mat4 view;
		glm::mat4 inverseView;
		glm::mat4 projection;
		glm::mat4 viewProj = glm::mat4{1.0f};
		glm::mat4 prevViewProj;
		glm::vec4 jitter;//xy为当前帧的抖动值，zw为上一帧的抖动值
		glm::vec3 camPos;
		float nearPlane;
		float farPlane;
		float exposure = 4.5f;
		float gamma = 2.2f;
	} globalParam;
	struct UniformBuffers {
		vks::Buffer globalParamBuffer;
	};
	std::array<UniformBuffers, MaxConcurrentFrames> globalParamBuffers;

	//每帧独立使用的全局参数的描述符集
	std::array<VkDescriptorSet, MaxConcurrentFrames> globalDescriptorSets{};

	VkPhysicalDeviceVulkan11Features Features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT libraryFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT };
	VulkanRenderer();
	~VulkanRenderer();
	void Init(VkSurfaceKHR surface);
	virtual void getEnabledFeatures() override;
	void AddEnabledInstanceExtensions(int extensionCount, const char** extensions);
	void AddEnabledDeviceExtensions(int extensionCount, const char** extensions);
	virtual void getEnabledExtensions() override;
	void buildCommandBuffer();
	void prepareDescriptors();
	void preparePipelines();
	void prepareUniformBuffers();
	void InitPostProcess();
	void updateUniformBuffers();
	void UpdateDescriptorSets();
	void render();
	bool BeginFrame(double deltaTime);
	void EndFrame();
	void DrawImGui();
	void OnFramebufferResize(int framebufferWidth, int framebufferHeight);
};

