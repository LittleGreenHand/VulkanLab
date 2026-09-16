#pragma once

#include <cstdint>
#include <string>

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "WindowInputListener.h"


class GlfwWindow
{
public:
	struct CreateInfo
	{
		uint32_t width = 2560;
		uint32_t height = 1440;
		std::string title = "Application";

		bool resizable = true;
		bool maximized = false;
	};

public:
	GlfwWindow() = default;
	~GlfwWindow();
	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow& operator=(const GlfwWindow&) = delete;

public:
	bool Init(const CreateInfo& createInfo);
	void Shutdown();
	void PollEvents() const;
	void WaitEvents() const;
	bool ShouldClose() const;
	void SetInputCallbacks(WindowInputListener::Callbacks callbacks);
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPositionCallback(GLFWwindow* window, double x, double y);
	static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

public:
	GLFWwindow* GetNativeWindow() const { return m_window; }
	uint32_t GetFramebufferWidth() const { return m_framebufferWidth; }
	uint32_t GetFramebufferHeight() const { return m_framebufferHeight; }
	float GetAspectRatio() const;
	bool IsMinimized() const { return m_framebufferWidth == 0 || m_framebufferHeight == 0; }

public:
	VkSurfaceKHR CreateSurface(VkInstance instance) const;
	static const char** GetRequiredVulkanExtensions(uint32_t& extensionCount);

private:
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
	GLFWwindow* m_window = nullptr;
	WindowInputListener m_inputListener;

	uint32_t m_framebufferWidth = 0;
	uint32_t m_framebufferHeight = 0;
};
