#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "RenderBase/VulkanDevice.h"
#include "RenderBase/VulkanRenderTypes.hpp"
#include "RenderBase/VulkanTexture.h"
#include "RenderBase/RenderConfig.hpp"

namespace vkLight
{
	const uint32_t MAX_POINTLIGHTS = 16;
	const uint32_t MAX_CASCADES = 8;
	struct alignas(16) PointLightInfo {
		glm::vec4 position;
		glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.f);
		float range = 256.0f;			//光源影响范围
		int attenuationMode = 2;	//衰减模式 0:线性衰减 1:平方反比衰减 2:物理衰减
		int isRnder = 0;
		float shadowBias = 0.0f; // 世界空间阴影偏置
	};
	struct alignas(16) DirectLightInfo {
		glm::vec4 direct = glm::vec4(0.f, 1.0f, 1.0f, 0.f);
		glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 2.f);
		glm::mat4 ViewProj[MAX_CASCADES];
		//每个float元素按16字节对齐（std140要求）
		struct alignas(16) CascadeSplit {
			float value;
		} cascadeSplits[MAX_CASCADES];
		int cascadeCount = 3; //实际使用的级联数量
		int usePCF = 1;
		int colorCascades = 0;
		int isRnder = 1;
		float shadowBias = 0.15f; // 世界空间阴影偏置，各级联按深度范围归一化

	};
	struct alignas(16) LightUbo {
		PointLightInfo pointLights[MAX_POINTLIGHTS]{};
		int activePointLightCount = 1;

		DirectLightInfo directLight;
	};
	static_assert(sizeof(PointLightInfo) == 48 && offsetof(PointLightInfo, shadowBias) == 44);
	static_assert(sizeof(DirectLightInfo) == 704 && offsetof(DirectLightInfo, shadowBias) == 688);
	static_assert(offsetof(LightUbo, directLight) == 784);

	//此layout由VulkanEngine统一销毁,其中存储的是所有灯光资源的描述符集布局
	extern VkDescriptorSetLayout descriptorSetLayout;
	extern std::array<VkDescriptorSet, MaxConcurrentFrames> descriptorSets;
	extern LightUbo lightData;
	extern std::array<vks::Buffer, MaxConcurrentFrames> lightBuffers;
	void preperDescriptor(vks::VulkanDevice* vulkanDevice, VkDescriptorPool descriptorPool);
	void updateLightBuffer();
	void destroyLightBuffer();

	class VulkanPointLights
	{
	public:
		vks::VulkanDevice* vulkanDevice{ nullptr };
		VkDevice device{ VK_NULL_HANDLE };
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

		//是否需要更新描述符集
		bool isDescriptorUpdated = true;
		//此pass用于生成ShadowMap
		RenderPassInfo renderPass;
		// 阴影立方体贴图每个面的尺寸
		const uint32_t shadowCubeSize{ 1024 };
		const float shadowNearPlane{ 0.001f };
		const float shadowFarPlane{ 1024.0f };
		// 阴影立方体贴图格式
		const VkFormat shadowCubeFormat{ VK_FORMAT_R32_SFLOAT };
		VkFormat shadowDepthFormat{ VK_FORMAT_UNDEFINED };
		struct LitghtResource
		{
			vks::Texture shadowCubeMap{ VK_NULL_HANDLE };//每个点光源都有一个阴影立方体贴图
			std::array<VkImageView, 6 * MAX_POINTLIGHTS> shadowCubeMapFaceImageViews{};//立方体贴图的每个面对应一个ImageView
			std::array<VkFramebuffer, 6 * MAX_POINTLIGHTS> frameBuffers{};//立方体贴图的每个面对应一个Framebuffer
		};
		LitghtResource lightResource;
		glm::mat4 viewMatrix[6];//立方体贴图的6个面的视图矩阵
	public:
		~VulkanPointLights()
		{
			destroy();
		}
		void destroy()
		{
			if (vulkanDevice) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(vulkanDevice->logicalDevice, pipeline, nullptr);
				if (pipelineLayout != VK_NULL_HANDLE)
					vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
				for (int id = 0; id < MAX_POINTLIGHTS * 6; ++id)
				{
					if(lightResource.frameBuffers[id])
						vkDestroyFramebuffer(device, lightResource.frameBuffers[id], nullptr);
					if (lightResource.shadowCubeMapFaceImageViews[id])
						vkDestroyImageView(device, lightResource.shadowCubeMapFaceImageViews[id], nullptr);
				}
				lightResource.shadowCubeMap.destroy();
				renderPass.destroy(device);
				lightResource = {};
				renderPass = {};
				pipeline = VK_NULL_HANDLE;
				pipelineLayout = VK_NULL_HANDLE;
				vulkanDevice = nullptr;
			}
		}

		void prepare(vks::VulkanDevice* vulkanDevice, VkFormat depthFormat, bool isDescriptorUpdated = true)
		{
			this->isDescriptorUpdated = isDescriptorUpdated;
			this->vulkanDevice = vulkanDevice;
			device = vulkanDevice->logicalDevice;
			renderPass.width = shadowCubeSize;
			renderPass.height = shadowCubeSize;
			shadowDepthFormat = depthFormat;
			lightResource.shadowCubeMapFaceImageViews.fill(VK_NULL_HANDLE);
			lightResource.frameBuffers.fill(VK_NULL_HANDLE);
			preperShadowCubeMap();
			preperRenderPass();
			prepareFramebuffer();
			preperPipeline();
			preperViewMatrix();
		}
		void preperShadowCubeMap();
		void prepareFramebuffer();
		void preperRenderPass();
		void preperPipeline();
		void preperViewMatrix();

		//绘制阴影贴图
		void Render(VkCommandBuffer cmdBuffer);
	};

	class VulkanDirectLights
	{
	public:
		vks::VulkanDevice* vulkanDevice{nullptr};
		VkDevice device{ VK_NULL_HANDLE };
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

		//是否需要更新描述符集
		bool isDescriptorUpdated = true;
		// 深度偏移因子,用于避免阴影伪影
		float depthBiasConstant = 0.f;
		// 深度偏移斜率因子，根据多边形的斜率进行应用
		float depthBiasSlope = 0.f;
		float cascadeSplitLambda = 0.75f;

		RenderPassInfo renderPass;//此pass用于生成ShadowMap
		VkFormat shadowMapFormat{ VK_FORMAT_D16_UNORM };//ShadowMap的格式
		uint32_t shadowMapize{ 4096 };
		struct Cascade {
			VkFramebuffer frameBuffer{ VK_NULL_HANDLE };
			VkImageView view{ VK_NULL_HANDLE };
			float splitDepth{0.f};
			glm::mat4 viewProjMatrix;
			glm::mat4 viewMat;
			glm::mat4 Proj;

			void destroy(VkDevice device) const {
				if (view != VK_NULL_HANDLE)
					vkDestroyImageView(device, view, nullptr);
				if (frameBuffer != VK_NULL_HANDLE)
					vkDestroyFramebuffer(device, frameBuffer, nullptr);
			}
		};
		Cascade cascades[MAX_CASCADES];
	public:
		~VulkanDirectLights()
		{
			destroy();
		}
		void destroy()
		{
			if (vulkanDevice) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(vulkanDevice->logicalDevice, pipeline, nullptr);
				if (pipelineLayout != VK_NULL_HANDLE)
					vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
				for (int i = 0; i < MAX_CASCADES; ++i)
					cascades[i].destroy(vulkanDevice->logicalDevice);
				vulkanDevice = nullptr;
			}
			renderPass.destroy(device);
		}
		void updateCascades();
		void updateCascades(const glm::mat4& cameraViewProjection, float nearClip, float farClip);

		void prepare(vks::VulkanDevice* vulkanDevice, VkFormat depthFormat, bool isDescriptorUpdated = true)
		{
			this->isDescriptorUpdated = isDescriptorUpdated;
			this->vulkanDevice = vulkanDevice;
			device = vulkanDevice->logicalDevice;
			shadowMapFormat = depthFormat;
			prepareFramebuffer();
			preperPipeline();
			updateCascades();
		}
		void prepareFramebuffer();
		void preperRenderPass(bool useDepth = true);
		void preperPipeline();

		//绘制阴影贴图
		void Render(VkCommandBuffer cmdBuffer);
	};
}
