/*
 * Vulkan glTF model and texture loading class.
 *
 * The concrete glTF loader implementation and its third-party dependencies
 * live in VulkanglTFModel.cpp. This header contains the runtime model API.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"
#include <glm/glm.hpp>
#include "RenderResource/GeometryTypes.hpp"
#include "RenderBase/glTF/VulkanglTFMaterial.h"
#include "RenderBase/glTF/VulkanglTFScene.h"
#include "RenderBase/glTF/VulkanglTFTypes.h"
#include "RenderBase/glTF/VulkanglTFVertex.h"

namespace tinygltf
{
	class Node;
	class Model;
}

namespace vks
{
	class Texture;
	struct VulkanDevice;
}

namespace vkglTF
{
	extern VkMemoryPropertyFlags memoryPropertyFlags;
	extern vks::Texture emptyTexture;
	void destroyEmptyTexture();

	class Model {
	private:
		void createEmptyTexture(VkQueue transferQueue);
		void loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void loadSkins(tinygltf::Model& gltfModel);
		void loadImages(tinygltf::Model& gltfModel, vks::VulkanDevice* device, VkQueue transferQueue);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);
		void prepareNodeDescriptor(Node* node, VkDescriptorSetLayout descriptorSetLayout);

	public:
		vks::VulkanDevice* device = nullptr;
		VkDescriptorPool descriptorPool;
		std::string modelName;

		std::vector<Vertex> m_vertexBuffer{};
		std::vector<uint32_t> m_indexBuffer{};
		struct Vertices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertices;
		struct Indices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} indices;

		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;
		std::vector<Mesh*> meshes;
		std::vector<Skin*> skins;
		std::vector<vks::Texture> textures;
		std::vector<Material> materials;
		std::vector<Animation> animations;

		Dimensions dimensions;
		bool metallicRoughnessWorkflow = true;
		bool buffersBound = false;
		std::string path;

		Model();
		~Model();
		void Destroy();
		vks::Texture* getTexture(uint32_t index);
		void loadFromFile(std::string filename, vks::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::None, float scale = 1.0f);
		void bindBuffers(VkCommandBuffer commandBuffer);
		void drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE);
		void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE);
		void drawWithPushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags stageFlags, VkPipelineLayout pipelineLayout, const glm::mat4& VP, bool pushModelMatrix = false);
		void drawNodeWithPushConstant(Node* node, VkCommandBuffer commandBuffer, VkShaderStageFlags stageFlags, VkPipelineLayout pipelineLayout, const glm::mat4& VP, bool pushModelMatrix = false);
		void getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max);
		void getSceneDimensions();
		void updateAnimation(uint32_t index, float time);
		Node* findNode(Node* parent, uint32_t index);
		Node* nodeFromIndex(uint32_t index);
		void updatePrevMatrix();
		std::span<const Vertex> GetPrimitiveVertices(const Primitive& primitive) const;
		std::span<const uint32_t> GetPrimitiveIndices(const Primitive& primitive) const;
	};
}
