#pragma once

#include <cstdint>

namespace vkglTF
{
	enum DescriptorBindingFlags {
		baseColorTexture = 0x00000001,
		normalTexture = baseColorTexture * 2,
		metallicRoughnessTexture = normalTexture * 2,
		metallicTexture = metallicRoughnessTexture * 2,
		RoughnessTexture = metallicTexture * 2,
		occlusionTexture = RoughnessTexture * 2,
		emissiveTexture = occlusionTexture * 2,
		AOTexture = emissiveTexture * 2,
		diffuseTexture = AOTexture * 2,
		specularGlossinessTexture = diffuseTexture * 2,
		allTexture = specularGlossinessTexture * 2 - 1
	};

	enum DescriptorImageBindingIndex {
		baseColorTextureIndex = 1,
		normalTextureIndex,
		metallicRoughnessTextureIndex,
		metallicTextureIndex,
		RoughnessTextureIndex,
		occlusionTextureIndex,
		emissiveTextureIndex,
		AOTextureIndex,
		diffuseTextureIndex,
		specularGlossinessTextureIndex
	};

	enum FileLoadingFlags {
		None = 0x00000000,
		PreTransformVertices = 0x00000001,
		PreMultiplyVertexColors = 0x00000002,
		FlipY = 0x00000004,
		DontLoadImages = 0x00000008
	};

	enum RenderFlags {
		BindMaterial = 0x00000001,
		RenderOpaqueNodes = 0x00000002,
		RenderAlphaMaskedNodes = 0x00000004,
		RenderAlphaBlendedNodes = 0x00000008
	};
}
