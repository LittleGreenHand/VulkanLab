#pragma once

#include <cstdint>

#include "vulkan/vulkan.h"
#include "RenderBase/VulkanBuffer.h"
#include "VulkanglTFTypes.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace vks
{
	class Texture;
	struct VulkanDevice;
}

namespace vkglTF
{
	extern VkDescriptorSetLayout MaterialDescriptorSetLayout;
	extern VkDescriptorSetLayout MeshDescriptorSetLayout;
	extern uint32_t descriptorBindingFlags;

	namespace detail
	{
		inline constexpr uint32_t imageDescriptorBindingCount = 10;
		void createMaterialDescriptorSetLayout(VkDevice device);
	}

	struct Material {
		vks::VulkanDevice* device = nullptr;
		enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		struct alignas(16) MaterialParameters
		{
			glm::vec4 baseColorFactor = glm::vec4(1.0f);
			glm::vec4 tangent = glm::vec4(0.0f);
			float alphaCutoff = 1.0f;
			float alphaFactor = 1.0f;
			float metallicFactor = 1.0f;
			float roughnessFactor = 1.0f;
			float anisotropicFactor = 0.0f;
			float baseColorTextureEmpty = true;
			float normalTextureEmpty = true;
			float mergeMetallicRoughnessTexture = true;
			float metallicRoughnessTextureEmpty = true;
			float metallicTextureEmpty = true;
			float roughnessTextureEmpty = true;
			float occlusionTextureEmpty = true;
			float emissiveTextureEmpty = true;
			float AOTextureEmpty = true;
			float diffuseTextureEmpty = true;
			float specularGlossinessTextureEmpty = true;
		} materialParameters;
		vks::Buffer MaterialParametersBuffer;
		vks::Texture* baseColorTexture = nullptr;
		vks::Texture* normalTexture = nullptr;
		vks::Texture* metallicRoughnessTexture = nullptr;
		vks::Texture* metallicTexture = nullptr;
		vks::Texture* roughnessTexture = nullptr;
		vks::Texture* occlusionTexture = nullptr;
		vks::Texture* emissiveTexture = nullptr;
		vks::Texture* AOTexture = nullptr;
		vks::Texture* diffuseTexture = nullptr;
		vks::Texture* specularGlossinessTexture = nullptr;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		Material(vks::VulkanDevice* device);
		~Material();
		void initMaterialTexture(vks::Texture* emptyTexture);
		void allocateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);
		void updateMaterialParametersBuffer();
		void updateDescriptorSet();

		void setBaseColorTexture(vks::Texture* texture) { baseColorTexture = texture; materialParameters.baseColorTextureEmpty = (texture == nullptr); }
		void setNormalTexture(vks::Texture* texture) { normalTexture = texture; materialParameters.normalTextureEmpty = (texture == nullptr); }
		void setMetallicRoughnessTexture(vks::Texture* texture) { metallicRoughnessTexture = texture; materialParameters.metallicRoughnessTextureEmpty = (texture == nullptr); }
		void setMetallicTexture(vks::Texture* texture) { metallicTexture = texture; materialParameters.metallicTextureEmpty = (texture == nullptr); }
		void setRoughnessTexture(vks::Texture* texture) { roughnessTexture = texture; materialParameters.roughnessTextureEmpty = (texture == nullptr); }
		void setOcclusionTexture(vks::Texture* texture) { occlusionTexture = texture; materialParameters.occlusionTextureEmpty = (texture == nullptr); }
		void setEmissiveTexture(vks::Texture* texture) { emissiveTexture = texture; materialParameters.emissiveTextureEmpty = (texture == nullptr); }
		void setAOTexture(vks::Texture* texture) { AOTexture = texture; materialParameters.AOTextureEmpty = (texture == nullptr); }
		void setDiffuseTexture(vks::Texture* texture) { diffuseTexture = texture; materialParameters.diffuseTextureEmpty = (texture == nullptr); }
		void setSpecularGlossinessTexture(vks::Texture* texture) { specularGlossinessTexture = texture; materialParameters.specularGlossinessTextureEmpty = (texture == nullptr); }
	};
}
