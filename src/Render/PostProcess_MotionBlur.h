#pragma once
#include "PostProcessBase.h"
#include <array>
#include "RenderBase/RenderConfig.hpp"

struct MotionBlurConstants {
	glm::vec2  screenSize;       // 屏幕分辨率 (width, height)
	glm::vec2  ndcToPixelScale;  // NDC到像素空间的缩放因子 (screenSize.xy / 2.0)
	float   maxBlurRadius;    // 最大模糊半径（像素），避免过度模糊
	float   depthThreshold;   // 深度一致性阈值，控制跨物体模糊
	float   alphaWeightScale = 0.5f; // Alpha权重缩放（0.0~1.0，控制透明像素的贡献度）
};
class PostProcessMotionBlur : public PostProcessBase
{
public:
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	std::array<VkDescriptorSet, MaxConcurrentFrames> descriptorSet;
	std::array<vks::Buffer, MaxConcurrentFrames> paramBuffer;
	MotionBlurConstants params;
public:
	void prepare();
	void destroy();
	void setParams(float maxBlurRadius, float depthThreshold, float alphaWeightScale);
	//对sampleImage进行色调映射处理，并将结果写入writeImage
	void excute(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo colorImageInfo, VkDescriptorImageInfo depthImageInfo, VkImageView writeImage);
private:
	void BindDescriptorSets(VkCommandBuffer cmdBuffer, VkDescriptorImageInfo colorImageInfo, VkDescriptorImageInfo depthImageInfo);
	void prepareDescriptorSet();
	void preparePipeline();

};
