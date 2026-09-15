#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"
#include "RenderResource/GeometryTypes.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vks
{
	struct VulkanDevice;
}

namespace physx
{
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxRigidActor;
}

namespace vkglTF
{
	struct Material;
	struct Primitive;
	struct Mesh;
	struct Skin;
	struct Node;

	struct PhysicsMesh
	{
		physx::PxConvexMesh* convexMesh = nullptr;
		physx::PxTriangleMesh* triangleMesh = nullptr;
	};

	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount = 0;
		uint32_t firstVertex;
		uint32_t vertexCount = 0;
		MeshTopology topology = MeshTopology::TriangleList;
		Material& material;
		using Dimensions = ::Dimensions;
		Dimensions dimensions;

		void setDimensions(glm::vec3 min, glm::vec3 max);
		Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {};
	};

	struct Mesh {
		vks::VulkanDevice* device;
		Node* parentNode = nullptr;
		PhysicsMesh physicsMesh;
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
			glm::mat4 prevModelMatrix;
			glm::mat4 jointMatrix[64]{};
			float jointcount{ 0 };
		} uniformBlock;

		Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
		~Mesh();
		void updateUniformBuffer();
		void updatePrevMatrix();
	};

	struct Skin {
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	struct PhysicsComponent
	{
		physx::PxRigidActor* physicsActor = nullptr;
		bool isPhysics = false;
		bool isDynamic = false;
	};

	struct Node {
		Node* parent = nullptr;
		uint32_t index = 0;
		std::vector<Node*> children;
		glm::mat4 matrix = glm::mat4(1.0f);
		std::string name;
		bool visible = true;
		Mesh* mesh = nullptr;
		Skin* skin = nullptr;
		PhysicsComponent physicsComponent;
		int32_t skinIndex = -1;
		glm::vec3 translation{ 0, 0, 0 };
		glm::vec3 scale{ 1.0f };
		glm::quat rotation = glm::quat(0, 0, 0, 1);
		glm::mat4 GetLocalMatrix();
		glm::mat4 GetWorldMatrix();
		void update(bool isTransformChanged = false);
		void clearTransform();
		~Node();
	};

	struct AnimationChannel {
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node* node;
		uint32_t samplerIndex;
	};

	struct AnimationSampler {
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	struct Animation {
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};
}
