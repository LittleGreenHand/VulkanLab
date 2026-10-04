#pragma once

#include "VulkanContext.h"

class PostProcessBase
{
public:
	static vks::VulkanDevice* vulkanDevice;
	static VkDevice device;
	static VkPipelineShaderStageCreateInfo fullScreenShaderStage;
	static VkDescriptorPool descriptorPool;
	static uint32_t width;
	static uint32_t height;

	static void preparePostProcessBase(vks::VulkanDevice* vulkandevice);
	static void cleanUp();
	static void UpdateResolution(uint32_t Width, uint32_t Height);

};

class PostProcessToneMapping;
class PostProcessDOF;
class PostProcessPointLine;
class PostProcessManager
{
public:
	PostProcessToneMapping* toneMappingProcess = nullptr;
	PostProcessDOF* dofProcess = nullptr;
	PostProcessPointLine* pointLineProcess = nullptr;
public:
	void Init();
	void destroyALL();
};
