#pragma once
#include "RenderBase/VulkanTexture.h"

class GlobalTextureManager
{
public:
	static GlobalTextureManager& Get()
	{
		static GlobalTextureManager instance;
		return instance;
	}

	GlobalTextureManager() = default;
	~GlobalTextureManager() { Destroy(); }
	GlobalTextureManager(const GlobalTextureManager&) = delete;
	GlobalTextureManager& operator=(const GlobalTextureManager&) = delete;
	GlobalTextureManager(GlobalTextureManager&&) = delete;
	GlobalTextureManager& operator=(GlobalTextureManager&&) = delete;
public:
	void Destroy();
	void LoadTextures();

public:
	struct ObjectTextures {
		vks::Texture2D albedoMap;
		vks::Texture2D normalMap;
		vks::Texture2D aoMap;
		vks::Texture2D metallicMap;
		vks::Texture2D roughnessMap;
	} textures{};
	bool isTexturesLoaded = false;
};