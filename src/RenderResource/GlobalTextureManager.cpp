#include "GlobalTextureManager.h"
#include "Render/VulkanContext.h"
#include "Render/VulkanDebugUtils.h"
#include "Core/Log.h"

void GlobalTextureManager::Destroy()
{
	LOG_DEBUG("Destroying texture manager resources");
	if (isTexturesLoaded)
	{
		textures.albedoMap.destroy();
		textures.normalMap.destroy();
		textures.aoMap.destroy();
		textures.metallicMap.destroy();
		textures.roughnessMap.destroy();
	}
	isTexturesLoaded = false;
	LOG_DEBUG("Destroying texture manager resources successfully");
}

void GlobalTextureManager::LoadTextures()
{
	LOG_DEBUG("Loading material textures");
	auto vulkanDevice = VulkanContext::GetVulkanDevice();
	textures.albedoMap.loadFromFile(getAssetPath() + "models/cerberus/albedo.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, VulkanContext::GetGraphicsQueue());
	textures.normalMap.loadFromFile(getAssetPath() + "models/cerberus/normal.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, VulkanContext::GetGraphicsQueue());
	textures.aoMap.loadFromFile(getAssetPath() + "models/cerberus/ao.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, VulkanContext::GetGraphicsQueue());
	textures.metallicMap.loadFromFile(getAssetPath() + "models/cerberus/metallic.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, VulkanContext::GetGraphicsQueue());
	textures.roughnessMap.loadFromFile(getAssetPath() + "models/cerberus/roughness.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, VulkanContext::GetGraphicsQueue());

	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)textures.albedoMap.image, "albedoMap");
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)textures.normalMap.image, "normalMap");
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)textures.aoMap.image, "aoMap");
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)textures.metallicMap.image, "metallicMap");
	VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)textures.roughnessMap.image, "roughnessMap");
	isTexturesLoaded = true;
	LOG_DEBUG("Material textures loaded successfully");
}
