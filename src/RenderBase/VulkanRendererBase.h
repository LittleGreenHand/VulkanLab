#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

#include "vulkan/vulkan.h"

#include "RenderBase/CommandLineParser.hpp"
#include "RenderBase/VulkanTools.h"
#include "RenderBase/VulkanSwapChain.h"
#include "RenderBase/VulkanBuffer.h"
#include "RenderBase/VulkanDevice.h"
#include "RenderBase/VulkanTexture.h"

#include "RenderBase/VulkanInitializers.hpp"
#include "RenderBase/camera.hpp"
#include "RenderBase/benchmark.hpp"
#include "Types.hpp"

//主要负责Vulkan的初始化
class VulkanRendererBase
{
private:
	bool resizePending = false;
	void nextFrame();
	void updateOverlay();
	void createPipelineCache();
	void createCommandPool();
	void createSynchronizationPrimitives();
	void createSwapChain();
	void createCommandBuffers();
	void destroyCommandBuffers();
public:
	// Frame counter to display fps
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;
	// Vulkan instance, stores all per-application states
	VkInstance instance{ VK_NULL_HANDLE };
	std::vector<std::string> supportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties deviceProperties{};
	// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures deviceFeatures{};
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};
	/** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
	VkPhysicalDeviceFeatures enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledDeviceExtensions;
	/** @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledInstanceExtensions;
	/** @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) */
	std::vector<VkLayerSettingEXT> enabledLayerSettings;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* deviceCreatepNextChain = nullptr;
	/** @brief Logical device, application's view of the physical device (GPU) */
	VkDevice device{ VK_NULL_HANDLE };
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue m_queue{ VK_NULL_HANDLE };
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat depthFormat{VK_FORMAT_UNDEFINED};
	// Command buffer pool
	VkCommandPool cmdPool{ VK_NULL_HANDLE };
	// Command buffers used for rendering
	std::array<VkCommandBuffer, MaxConcurrentFrames> drawCmdBuffers;
	//主渲染Pass，包含渲染目标和深度模板缓冲
	RenderPassInfo mainRenderPass;
	//// List of available frame buffers (same as number of swap chain images)
	//std::vector<VkFramebuffer>frameBuffers;
	// Descriptor set pool
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;
	// Pipeline cache object
	VkPipelineCache pipelineCache{ VK_NULL_HANDLE };
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain swapChain;

	// Synchronization related objects and variables
	// These are used to have multiple frame buffers "in flight" to get some CPU/GPU parallelism
	uint32_t currentImageIndex{ 0 };
	uint32_t currentBuffer{ 0 };
	std::array<VkSemaphore, MaxConcurrentFrames> presentCompleteSemaphores{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};
	std::array<VkFence, MaxConcurrentFrames> waitFences;

	bool requiresStencil{ false };
public:
	bool m_init = false;
	bool resized = false; // 是否正在resize后缓冲的分辨率
	uint32_t m_renderWidth = 1280; // 渲染分辨率，渲染时依此分辨率进行渲染
	uint32_t m_renderHeiht = 720;
	uint32_t m_framebufferWidth = 1280; //交换链Present的分辨率，也可以说是窗口的显示分辨率
	uint32_t m_framebufferHeiht = 720;

	CommandLineParser commandLineParser;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float frameTimer = 1.0f;

	vks::Benchmark benchmark;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice *vulkanDevice{};

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct Settings {
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
	} settings;

	/** @brief State of gamepad input (only used on Android) */
	struct {
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;

	/** @brief State of mouse/touch input */
	struct {
		struct {
			bool left = false;
			bool right = false;
			bool middle = false;
		} buttons;
		glm::vec2 position;
	} mouseState;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	static std::vector<const char*> args;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	bool paused = false;

	Camera camera;

	std::string title = "Vulkan Example";
	std::string name = "vulkanExample";
	uint32_t apiVersion = VK_API_VERSION_1_4;

	std::array<vks::Texture, 2> offscreenTexture;
	VkFormat offscreenFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	/** @brief Default depth stencil attachment used by the default render pass */
	vks::Texture depthStencil{};
	GBuffer gBuffer;

	/** @brief Default base class constructor */
	VulkanRendererBase();
	virtual ~VulkanRendererBase();
	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool InitVulkan();
	// Returns the path to the root of the glsl, hlsl or slang shader directory.
	std::string getShadersPath() const;

	/** @brief (Virtual) Creates the application wide Vulkan instance */
	virtual VkResult createInstance();
	/** @brief (Virtual) 创建延迟渲染所需的GBuffer */
	virtual void setupGBuffer();
	/** @brief (Virtual) 创建离屏渲染目标 */
	virtual void setupOffscreenAttachment();
	/** @brief (Virtual) Setup default depth and stencil views */
	virtual void setupDepthStencil();
	/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
	virtual void setupFrameBuffer();
	/** @brief (Virtual) Setup a default renderpass */
	virtual void setupRenderPass();
	/** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
	virtual void getEnabledFeatures();
	/** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
	virtual void getEnabledExtensions();

	void InitSurfaceKHR(VkSurfaceKHR surface);
	//创建基础的渲染资源
	void InitRenderResource();
	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	void windowResize();

	/** @brief Entry point for the main render loop */
	void renderLoop();

	/** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
	//void drawUI(const VkCommandBuffer commandBuffer);

	/** Prepare the next frame for workload submission by acquiring the next swap chain image and waiting for the previous command buffer to finish */
	VkResult prepareFrame(bool waitForFence = true);
	/** @brief Presents the current image to the swap chain */
	void submitFrame(bool skipQueueSubmit = false);

};
