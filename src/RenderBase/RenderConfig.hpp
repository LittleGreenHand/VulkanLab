#pragma once

#include <cstdint>

constexpr uint32_t MaxConcurrentFrames{ 2 };

// Descriptor-set indices shared with shaders/types.slang.
enum DescriptorSetBindIndex {
	LBI_GLOBAL = 0,
	LBI_IBL,
	LBI_LIGHTS,
	LBI_MATERIALS,
	LBI_MESH,
	LBI_CUSTOM,
	LBI_COUNT
};
