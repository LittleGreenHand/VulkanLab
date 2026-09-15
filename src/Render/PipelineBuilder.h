#pragma once
#include <vulkan/vulkan.h>
#include "RenderBase/VulkanInitializers.hpp"
#include "RenderBase/glTF/VulkanglTFVertex.h"

class PipelineBuilder {
public:
    // 构造函数，需要设备句柄
    PipelineBuilder(VkDevice device);

    // 析构函数
    ~PipelineBuilder();

    // 设置输入装配状态
    PipelineBuilder& setInputAssemblyState(
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VkPipelineInputAssemblyStateCreateFlags flags = 0,
        VkBool32 primitiveRestartEnable = VK_FALSE);

    // 设置光栅化状态
    PipelineBuilder& setRasterizationState(
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VkPipelineRasterizationStateCreateFlags flags = 0);

    // 设置颜色混合附件状态
    PipelineBuilder& setColorBlendAttachmentState(
		uint32_t colorWriteMask = 0xf,//启用RGBA通道的写入
        VkBool32 blendEnable = VK_FALSE);

    // 设置深度模板状态
    PipelineBuilder& setDepthStencilState(
        VkBool32 depthTestEnable = VK_FALSE,
        VkBool32 depthWriteEnable = VK_FALSE,
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL);

    // 设置视口状态
    PipelineBuilder& setViewportState(
        uint32_t viewportCount = 1,
        uint32_t scissorCount = 1,
        VkPipelineViewportStateCreateFlags flags = 0);

    // 设置多重采样状态
    PipelineBuilder& setMultisampleState(
        VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        VkPipelineMultisampleStateCreateFlags flags = 0);

    // 设置动态状态
    PipelineBuilder& setDynamicStates(
        const std::vector<VkDynamicState>& dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
        VkPipelineDynamicStateCreateFlags flags = 0);

    // 设置顶点输入状态
    PipelineBuilder& setVertexInputState(
        VkPipelineVertexInputStateCreateInfo* state = vkglTF::Vertex::getPipelineVertexInputState({
        vkglTF::VertexComponent::Position,
        vkglTF::VertexComponent::Normal,
        vkglTF::VertexComponent::UV,
        vkglTF::VertexComponent::Color,
        vkglTF::VertexComponent::Tangent }));

    //配置加法混合模式，让物体有半透明效果
    void enableBlendingAdditive();

	//配置标准透明混合模式，当alpha为1时完全不透明，alpha为0时完全透明
    void enableBlendingAlphaBlend();

    // 添加着色器阶段
    PipelineBuilder& addShaderStage(const VkPipelineShaderStageCreateInfo stage);
    void clearShaderStage();

    // 构建图形管线
    VkResult buildPipeline(VkRenderPass& renderPass, VkPipelineCache& pipelineCache, VkPipelineLayout& pipelineLayout, VkPipeline& outPipeline);
    VkResult buildPipeline(VkPipelineRenderingCreateInfo renderInfo, VkPipelineCache pipelineCache, VkPipelineLayout& pipelineLayout, VkPipeline& outPipeline);

    // 重置构建器状态，用于创建新的管线
    void reset();


public:
    VkDevice device;

    // 管线状态对象
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    VkPipelineColorBlendAttachmentState blendAttachmentState;
    VkPipelineColorBlendStateCreateInfo colorBlendState;
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineMultisampleStateCreateInfo multisampleState;
    VkPipelineDynamicStateCreateInfo dynamicState;
    std::vector<VkDynamicState> dynamicStateEnables;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineVertexInputStateCreateInfo* vertexInputState;
};
