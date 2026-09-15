#pragma once

#include "vulkan/vulkan.h"
#include "RenderBase/VulkanTexture.h"

enum GBufferId {
	GBufferAlbedoAlpha = 0,
	GBufferNormal,
	GBufferTangent,
	GBufferMaterial,
	GBufferPosition,
	GBufferMotionVector,
	GBufferCount
};

inline VkFormat GBufferFormats[] = {
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferAlbedoAlpha
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferNormal
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferTangent
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferMaterial
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferPosition
	VK_FORMAT_R16G16B16A16_SFLOAT,	// GBufferMotionVector
};

inline const char* GBufferNames[] = {
	"GBufferAlbedoAlpha",
	"GBufferNormal",
	"GBufferTangent",
	"GBufferMaterial",
	"GBufferPosition",
	"GBufferMotionVector",
};

struct GBuffer {
	vks::Texture texture[GBufferCount];

	void destroy()
	{
		for (int i = 0; i < GBufferCount; ++i)
			texture[i].destroy();
	}
};
