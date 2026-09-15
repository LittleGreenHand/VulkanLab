#include "VulkanglTFMaterial.h"

#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RenderBase/VulkanDevice.h"
#include "RenderBase/VulkanTexture.h"

namespace vkglTF::detail
{
	void createMaterialDescriptorSetLayout(VkDevice device)
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0));

		for (uint32_t i = 0; i < imageDescriptorBindingCount; i++)
		{
			setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				i + 1));
		}

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &MaterialDescriptorSetLayout));
	}
}

namespace vkglTF
{
	Material::Material(vks::VulkanDevice* device) : device(device)
	{
	}

	Material::~Material()
	{
		MaterialParametersBuffer.destroy();
	}

	void Material::initMaterialTexture(vks::Texture* emptyTexture)
	{
		if (!baseColorTexture)
		{
			baseColorTexture = emptyTexture;
			materialParameters.baseColorTextureEmpty = true;
		}

		if (!normalTexture)
		{
			normalTexture = emptyTexture;
			materialParameters.normalTextureEmpty = true;
		}

		if (!metallicRoughnessTexture)
		{
			metallicRoughnessTexture = emptyTexture;
			materialParameters.metallicRoughnessTextureEmpty = true;
		}

		if (!metallicTexture)
		{
			metallicTexture = emptyTexture;
			materialParameters.metallicTextureEmpty = true;
		}

		if (!roughnessTexture)
		{
			roughnessTexture = emptyTexture;
			materialParameters.roughnessTextureEmpty = true;
		}

		if (!occlusionTexture)
		{
			occlusionTexture = emptyTexture;
			materialParameters.occlusionTextureEmpty = true;
		}

		if (!emissiveTexture)
		{
			emissiveTexture = emptyTexture;
			materialParameters.emissiveTextureEmpty = true;
		}

		if (!AOTexture)
		{
			AOTexture = emptyTexture;
			materialParameters.AOTextureEmpty = true;
		}

		if (!diffuseTexture)
		{
			diffuseTexture = emptyTexture;
			materialParameters.diffuseTextureEmpty = true;
		}

		if (!specularGlossinessTexture)
		{
			specularGlossinessTexture = emptyTexture;
			materialParameters.specularGlossinessTextureEmpty = true;
		}
	}

	void Material::allocateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = descriptorPool;
		descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;
		descriptorSetAllocInfo.descriptorSetCount = 1;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocInfo, &descriptorSet));
	}

	void Material::updateMaterialParametersBuffer()
	{
		memcpy(MaterialParametersBuffer.mapped, &materialParameters, sizeof(Material::MaterialParameters));
	}

	void Material::updateDescriptorSet()
	{
		const std::unordered_map<DescriptorBindingFlags, uint32_t> bindingMap = {
			{DescriptorBindingFlags::baseColorTexture, baseColorTextureIndex},
			{DescriptorBindingFlags::normalTexture, normalTextureIndex},
			{DescriptorBindingFlags::metallicRoughnessTexture, metallicRoughnessTextureIndex},
			{DescriptorBindingFlags::metallicTexture, metallicTextureIndex},
			{DescriptorBindingFlags::RoughnessTexture, RoughnessTextureIndex},
			{DescriptorBindingFlags::occlusionTexture, occlusionTextureIndex},
			{DescriptorBindingFlags::emissiveTexture, emissiveTextureIndex},
			{DescriptorBindingFlags::AOTexture, AOTextureIndex},
			{DescriptorBindingFlags::diffuseTexture, diffuseTextureIndex},
			{DescriptorBindingFlags::specularGlossinessTexture, specularGlossinessTextureIndex}
		};

		std::vector<std::pair<DescriptorBindingFlags, VkDescriptorImageInfo*>> textureInfos = {
			{DescriptorBindingFlags::baseColorTexture, &baseColorTexture->descriptor},
			{DescriptorBindingFlags::normalTexture, &normalTexture->descriptor},
			{DescriptorBindingFlags::metallicRoughnessTexture, &metallicRoughnessTexture->descriptor},
			{DescriptorBindingFlags::metallicTexture, &metallicTexture->descriptor},
			{DescriptorBindingFlags::RoughnessTexture, &roughnessTexture->descriptor},
			{DescriptorBindingFlags::occlusionTexture, &occlusionTexture->descriptor},
			{DescriptorBindingFlags::emissiveTexture, &emissiveTexture->descriptor},
			{DescriptorBindingFlags::AOTexture, &AOTexture->descriptor},
			{DescriptorBindingFlags::diffuseTexture, &diffuseTexture->descriptor},
			{DescriptorBindingFlags::specularGlossinessTexture, &specularGlossinessTexture->descriptor}
		};

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
			descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &MaterialParametersBuffer.descriptor));

		for (const auto& [flag, imageInfo] : textureInfos) {
			if ((descriptorBindingFlags & flag) && imageInfo != nullptr) {
				VkWriteDescriptorSet write{};
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.dstSet = descriptorSet;
				write.dstBinding = bindingMap.at(flag);
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write.pImageInfo = imageInfo;
				writeDescriptorSets.push_back(write);
			}
		}
		vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}
