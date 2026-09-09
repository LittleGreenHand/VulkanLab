/*
* Vulkan glTF model and texture loading class based on tinyglTF (https://github.com/syoyo/tinygltf)
*
* Copyright (C) 2018-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
 * Note that this isn't a complete glTF loader and not all features of the glTF 2.0 spec are supported
 * For details on how glTF 2.0 works, see the official spec at https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
 *
 * If you are looking for a complete glTF implementation, check out https://github.com/SaschaWillems/Vulkan-glTF-PBR/
 */

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Types.hpp"


#include <ktx.h>
#include <ktxvulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

namespace vkglTF
{
	extern VkDescriptorSetLayout MaterialDescriptorSetLayout;
	extern VkDescriptorSetLayout MeshDescriptorSetLayout;
	extern VkMemoryPropertyFlags memoryPropertyFlags;
	extern uint32_t descriptorBindingFlags;
	extern vks::Texture emptyTexture;
	static void destroyEmptyTexture() {
		if(emptyTexture.image == VK_NULL_HANDLE)
			return;
		emptyTexture.destroy();
	}
	struct Node;

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

	//static VkDescriptorSetLayout MaterialDescriptorSetLayout = VK_NULL_HANDLE;
	const uint32_t imageDescriptorBindingCount = 10;
	static void createMaterialDescriptorSetLayout(VkDevice device) {
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		//这个缓冲区用于存储材质参数
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0));

		for (int i = 0; i < imageDescriptorBindingCount; i++)
		{
			setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, i + 1));
		}
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &MaterialDescriptorSetLayout));
	}
	static void createMeshDescriptorSetLayout(VkDevice device) {
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0));

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &MeshDescriptorSetLayout));
	}

	/*
		glTF material class
	*/
	struct Material {
		vks::VulkanDevice* device = nullptr;
		enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		struct alignas(16) MaterialParameters
		{
			glm::vec4 baseColorFactor = glm::vec4(1.0f);
			glm::vec4 tangent = glm::vec4(0.0f);//如果是零向量，则表示使用物体本身的切线，否则使用该切线
			float alphaCutoff = 1.0f;
			float alphaFactor = 1.0f;//透明度，1表示不透明，0表示完全透明
			float metallicFactor = 1.0f;	
			float roughnessFactor = 1.0f;
			float anisotropicFactor = 0.0f;//各向异性强度，0表示不使用各向异性，值为[-1, 1]
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
		}materialParameters;
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

		Material(vks::VulkanDevice* device) : device(device) {};
		~Material() {
			MaterialParametersBuffer.destroy();
		};
		void initMaterialTexture(vks::Texture* emptyTexture);
		void allocateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);
		void updateMaterialParametersBuffer();
		void updateDescriptorSet();

		void setBaseColorTexture(vks::Texture* texture) {
			baseColorTexture = texture;
			materialParameters.baseColorTextureEmpty = (texture == nullptr);
		};

		void setNormalTexture(vks::Texture* texture) {
			normalTexture = texture;
			materialParameters.normalTextureEmpty = (texture == nullptr);
		};

		void setMetallicRoughnessTexture(vks::Texture* texture) {
			metallicRoughnessTexture = texture;
			materialParameters.metallicRoughnessTextureEmpty = (texture == nullptr);
		};

		void setMetallicTexture(vks::Texture* texture) {
			metallicTexture = texture;
			materialParameters.metallicTextureEmpty = (texture == nullptr);
		};

		void setRoughnessTexture(vks::Texture* texture) {
			roughnessTexture = texture;
			materialParameters.roughnessTextureEmpty = (texture == nullptr);
		};

		void setOcclusionTexture(vks::Texture* texture) {
			occlusionTexture = texture;
			materialParameters.occlusionTextureEmpty = (texture == nullptr);
		};

		void setEmissiveTexture(vks::Texture* texture) {
			emissiveTexture = texture;
			materialParameters.emissiveTextureEmpty = (texture == nullptr);
		};

		void setAOTexture(vks::Texture* texture) {
			AOTexture = texture;
			materialParameters.AOTextureEmpty = (texture == nullptr);
		};

		void setDiffuseTexture(vks::Texture* texture) {
			diffuseTexture = texture;
			materialParameters.diffuseTextureEmpty = (texture == nullptr);
		};

		void setSpecularGlossinessTexture(vks::Texture* texture) {
			specularGlossinessTexture = texture;
			materialParameters.specularGlossinessTextureEmpty = (texture == nullptr);
		};
	};

	/*
		glTF primitive
	*/
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount = 0;
		uint32_t firstVertex;
		uint32_t vertexCount = 0;
		Material& material;

		struct Dimensions {
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius;
		} dimensions;

		void setDimensions(glm::vec3 min, glm::vec3 max);
		Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {};
	};

	/*
		glTF mesh
	*/
	struct Mesh {
		vks::VulkanDevice* device;
		Node* parentNode = nullptr;//所属节点

		std::vector<Primitive*> primitives;
		std::string name;

		struct UniformBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			void* mapped;
		} uniformBuffer;

		struct alignas(16) MeshInfo {
			glm::mat4 modelMatrix;
			glm::mat4 prevModelMatrix;//上一帧的模型矩阵
			glm::mat4 jointMatrix[64]{};
			float jointcount{ 0 };
		} uniformBlock;

		Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
		~Mesh();
		void updateUniformBuffer()
		{
			memcpy(uniformBuffer.mapped, &uniformBlock, sizeof(uniformBlock));
		}
		void updatePrevMatrix()
		{
			uniformBlock.prevModelMatrix = uniformBlock.modelMatrix;
			memcpy(uniformBuffer.mapped, &uniformBlock, sizeof(uniformBlock));
		}
	};

	/*
		glTF skin
	*/
	struct Skin {
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	/*
		glTF node
	*/
	struct Node {
		Node* parent = nullptr;
		uint32_t index = 0;
		std::vector<Node*> children;
		glm::mat4 matrix = glm::mat4(1.0f);
		std::string name;
		bool visible = true;
		Mesh* mesh = nullptr;
		Skin* skin = nullptr;
		int32_t skinIndex = -1;
		glm::vec3 translation{0, 0, 0};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation = glm::quat(0, 0, 0, 1);
		glm::mat4 localMatrix();
		glm::mat4 getWorldMatrix();
		void update();
		//将节点和子节点的变换重置为默认值
		void clearTransform();
		~Node();
	};

	/*
		glTF animation channel
	*/
	struct AnimationChannel {
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node* node;
		uint32_t samplerIndex;
	};

	/*
		glTF animation sampler
	*/
	struct AnimationSampler {
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	/*
		glTF animation
	*/
	struct Animation {
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	/*
		glTF default vertex layout with easy Vulkan mapping functions
	*/
	enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec4 color;
		glm::vec4 joint0;
		glm::vec4 weight0;
		glm::vec4 tangent;
		static VkVertexInputBindingDescription vertexInputBindingDescription;
		static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
		static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
		static VkVertexInputBindingDescription inputBindingDescription(uint32_t binding);
		static VkVertexInputAttributeDescription inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components);
		/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent> components);
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

	/*
		glTF model loading and rendering class
	*/
	class Model {
	private:
		void createEmptyTexture(VkQueue transferQueue);
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

		bool isPhysics = false;// 是否启用物理模拟
		bool isDynamic = false;// 以动态还是静态物体的方式执行物理模拟
		bool metallicRoughnessWorkflow = true;
		bool buffersBound = false;
		std::string path;

		Model();
		~Model();
		void Destroy();
		vks::Texture* getTexture(uint32_t index);
		void loadNode(vkglTF::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void loadSkins(tinygltf::Model& gltfModel);
		void loadImages(tinygltf::Model& gltfModel, vks::VulkanDevice* device, VkQueue transferQueue);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);
		void loadFromFile(std::string filename, vks::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::None, float scale = 1.0f);
		void bindBuffers(VkCommandBuffer commandBuffer);
		void drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE);
		void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE);
		//使用推送常量传递MVP矩阵
		void drawWithPushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags stageFlags, VkPipelineLayout pipelineLayout, const glm::mat4& VP, bool pushModelMatrix = false);
		void drawNodeWithPushConstant(Node* node, VkCommandBuffer commandBuffer, VkShaderStageFlags stageFlags, VkPipelineLayout pipelineLayout, const glm::mat4& VP, bool pushModelMatrix = false);
		void getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max);
		void getSceneDimensions();
		void updateAnimation(uint32_t index, float time);
		Node* findNode(Node* parent, uint32_t index);
		Node* nodeFromIndex(uint32_t index);
		void prepareNodeDescriptor(vkglTF::Node* node, VkDescriptorSetLayout descriptorSetLayout);
		void updatePrevMatrix();
		std::span<const Vertex> GetPrimitiveVertices(const Primitive& primitive)const;
		std::span<const uint32_t> GetPrimitiveIndices(const Primitive& primitive)const;
	};
}