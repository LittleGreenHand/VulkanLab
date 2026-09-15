#pragma once
#include "PostProcessBase.h"
#include <array>
#include "RenderBase/RenderConfig.hpp"

struct DOFParams {
	float zNear;
	float zFar;
	float focusDistance;    // 焦点距离
	float focusRange;       // 焦点范围（清晰区域）
	float maxBlurRadius;    // 最大模糊半径
	float aperture;         // 光圈大小（影响模糊强度）
	glm::vec2 texelSize;	// 纹理单元大小（1/输出纹理宽度, 1/输出纹理高度）
};
class PostProcessDOF : public PostProcessBase
{
public:
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	std::array<VkDescriptorSet, MaxConcurrentFrames> descriptorSet;
	std::array<vks::Buffer, MaxConcurrentFrames> dofParamBuffer;
	DOFParams params;
public:
	void prepare();
	void destroy();
	void setDOFParams(float zNear, float zFar, float focusDistance, float focusRange, float maxBlurRadius, float aperture);
	//对sampleImage进行色调映射处理，并将结果写入writeImage
	void excute(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo colorImageInfo, VkDescriptorImageInfo depthImageInfo, VkImageView writeImage);
private:
	void BindDescriptorSets(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo colorImageInfo, VkDescriptorImageInfo depthImageInfo);
	void prepareDescriptorSet();
	void preparePipeline();

};
