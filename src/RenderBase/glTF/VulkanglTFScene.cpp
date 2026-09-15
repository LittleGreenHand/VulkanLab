#include "VulkanglTFScene.h"

#include <cstring>

#include <glm/gtx/matrix_decompose.hpp>

#include "Core/Log.h"
#include "RenderBase/VulkanDevice.h"
#include "Simulation/PhysicsContext.h"

namespace vkglTF
{
	void Primitive::setDimensions(glm::vec3 min, glm::vec3 max)
	{
		dimensions.min = min;
		dimensions.max = max;
		dimensions.size = max - min;
		dimensions.center = (min + max) / 2.0f;
		dimensions.radius = glm::distance(min, max) / 2.0f;
	}

	Mesh::Mesh(vks::VulkanDevice* device, glm::mat4 matrix)
	{
		this->device = device;
		this->uniformBlock.modelMatrix = matrix;
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uniformBlock),
			&uniformBuffer.buffer,
			&uniformBuffer.memory,
			&uniformBlock));
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, uniformBuffer.memory, 0, sizeof(uniformBlock), 0, &uniformBuffer.mapped));
		uniformBuffer.descriptor = { uniformBuffer.buffer, 0, sizeof(uniformBlock) };
	}

	Mesh::~Mesh()
	{
		vkDestroyBuffer(device->logicalDevice, uniformBuffer.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, uniformBuffer.memory, nullptr);
		for (auto primitive : primitives) {
			delete primitive;
		}
		if (physicsMesh.convexMesh)
		{
			physicsMesh.convexMesh->release();
			physicsMesh.convexMesh = nullptr;
		}
		if (physicsMesh.triangleMesh)
		{
			physicsMesh.triangleMesh->release();
			physicsMesh.triangleMesh = nullptr;
		}
	}

	void Mesh::updateUniformBuffer()
	{
		memcpy(uniformBuffer.mapped, &uniformBlock, sizeof(uniformBlock));
	}

	void Mesh::updatePrevMatrix()
	{
		uniformBlock.prevModelMatrix = uniformBlock.modelMatrix;
		memcpy(uniformBuffer.mapped, &uniformBlock, sizeof(uniformBlock));
	}

	glm::mat4 Node::GetLocalMatrix()
	{
		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
	}

	glm::mat4 Node::GetWorldMatrix()
	{
		glm::mat4 m = GetLocalMatrix();
		Node* p = parent;
		while (p) {
			m = p->GetLocalMatrix() * m;
			p = p->parent;
		}
		return m;
	}

	void Node::update(bool isTransformChanged)
	{
		if (mesh) {
			glm::mat4 m = GetWorldMatrix();
			mesh->uniformBlock.modelMatrix = m;
			if (skin) {
				glm::mat4 inverseTransform = glm::inverse(m);
				for (size_t i = 0; i < skin->joints.size(); i++) {
					Node* jointNode = skin->joints[i];
					glm::mat4 jointMat = jointNode->GetWorldMatrix() * skin->inverseBindMatrices[i];
					jointMat = inverseTransform * jointMat;
					mesh->uniformBlock.jointMatrix[i] = jointMat;
				}
				mesh->uniformBlock.jointcount = (float)skin->joints.size();
				memcpy(mesh->uniformBuffer.mapped, &mesh->uniformBlock, sizeof(mesh->uniformBlock));
			} else {
				memcpy(mesh->uniformBuffer.mapped, &m, sizeof(glm::mat4));
			}

			if (isTransformChanged && physicsComponent.physicsActor)
			{
				glm::vec3 worldScale;
				glm::quat worldRotation;
				glm::vec3 worldTranslation;
				glm::vec3 skew;
				glm::vec4 perspective;
				if (glm::decompose(m, worldScale, worldRotation, worldTranslation, skew, perspective))
				{
					worldRotation = glm::normalize(worldRotation);
					PhysicsContext::Get().UpdateActorTransform(physicsComponent.physicsActor, worldTranslation, worldRotation);
					PhysicsContext::Get().UpdateActorScale(physicsComponent.physicsActor, worldScale);
				}
				else
				{
					LOG_WARNING("[MeshManager] Failed to decompose Node transform: {}", name);
				}
			}
		}

		for (auto& child : children) {
			child->update(isTransformChanged);
		}
	}

	void Node::clearTransform()
	{
		translation = { 0, 0, 0 };
		scale = { 1.0f, 1.0f, 1.0f };
		rotation.x = 0;
		rotation.y = 0;
		rotation.z = 0;
		rotation.w = 1;
		for (auto& child : children) {
			child->clearTransform();
		}
	}

	Node::~Node()
	{
		if (mesh) {
			delete mesh;
		}
		for (auto& child : children) {
			delete child;
		}
		if (physicsComponent.physicsActor) {
			physicsComponent.physicsActor->release();
			physicsComponent.physicsActor = nullptr;
		}
	}
}
