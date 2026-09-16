#include "GlfwWindow.h"
#include "Log.h"

#include <stdexcept>
#include <utility>

GlfwWindow::~GlfwWindow()
{
	Shutdown();
}

bool GlfwWindow::Init(const CreateInfo& createInfo)
{
	if (m_window)
	{
		LOG_WARNING("GLFW window is already initialized");
		return true;
	}

	if (glfwInit() != GLFW_TRUE)
	{
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	if (glfwVulkanSupported() != GLFW_TRUE)
	{
		LOG_ERROR("GLFW reports that Vulkan is not supported");
		glfwTerminate();
		return false;
	}

	//不需要创建 OpenGL Context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, createInfo.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_MAXIMIZED, createInfo.maximized ? GLFW_TRUE : GLFW_FALSE);

	m_window = glfwCreateWindow(
		static_cast<int>(createInfo.width),
		static_cast<int>(createInfo.height),
		createInfo.title.c_str(),
		nullptr,
		nullptr);

	if (!m_window)
	{
		LOG_ERROR("Failed to create GLFW window ({}x{})", createInfo.width, createInfo.height);
		glfwTerminate();
		return false;
	}

	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
	glfwSetKeyCallback(m_window, GlfwWindow::KeyCallback);
	glfwSetMouseButtonCallback(m_window, GlfwWindow::MouseButtonCallback);
	glfwSetCursorPosCallback(m_window, GlfwWindow::CursorPositionCallback);
	glfwSetScrollCallback(m_window, GlfwWindow::ScrollCallback);

	// 在高 DPI 屏幕上，framebuffer size 可能和 window size 不一样。
	int framebufferWidth = 0;
	int framebufferHeight = 0;

	glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);

	m_framebufferWidth = framebufferWidth > 0 ? static_cast<uint32_t>(framebufferWidth) : 0;
	m_framebufferHeight = framebufferHeight > 0 ? static_cast<uint32_t>(framebufferHeight) : 0;
	LOG_INFO("GLFW window created with framebuffer size: {}x{}", m_framebufferWidth, m_framebufferHeight);

	return true;
}

void GlfwWindow::Shutdown()
{
	LOG_DEBUG("Shutting down GLFW window");
	m_inputListener.ClearCallbacks();

	if (m_window)
	{
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
	glfwTerminate();

	m_framebufferWidth = 0;
	m_framebufferHeight = 0;
	LOG_DEBUG("Shutting down GLFW window successfully");
}

void GlfwWindow::PollEvents() const
{
	glfwPollEvents();
}

void GlfwWindow::WaitEvents() const
{
	glfwWaitEvents();
}

bool GlfwWindow::ShouldClose() const
{
	return m_window == nullptr ||
		glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void GlfwWindow::SetInputCallbacks(WindowInputListener::Callbacks callbacks)
{
	m_inputListener.SetCallbacks(std::move(callbacks));
}

void GlfwWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
	if (self)
		self->m_inputListener.NotifyKey({ key, scancode, action, mods });
}

void GlfwWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
	if (!self)
		return;

	double cursorX = 0.0;
	double cursorY = 0.0;
	glfwGetCursorPos(window, &cursorX, &cursorY);
	self->m_inputListener.NotifyMouseButton({ button, action, mods, cursorX, cursorY });
}

void GlfwWindow::CursorPositionCallback(GLFWwindow* window, double x, double y)
{
	auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
	if (self)
		self->m_inputListener.NotifyCursorPosition({ x, y });
}

void GlfwWindow::ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
	if (self)
		self->m_inputListener.NotifyScroll({ xOffset, yOffset });
}

float GlfwWindow::GetAspectRatio() const
{
	if (m_framebufferHeight == 0)
		return 0.0f;

	return static_cast<float>(m_framebufferWidth) / static_cast<float>(m_framebufferHeight);
}

VkSurfaceKHR GlfwWindow::CreateSurface(VkInstance instance) const
{
	if (!m_window)
	{
		LOG_ERROR("Cannot create Vulkan surface without a GLFW window");
		throw std::runtime_error("Cannot create Vulkan surface: GLFW window is null.");
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	const VkResult result = glfwCreateWindowSurface(instance, m_window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		LOG_ERROR("Failed to create Vulkan window surface, VkResult={}", static_cast<int>(result));
		throw std::runtime_error("Failed to create Vulkan window surface.");
	}
	LOG_DEBUG("VkSurfaceKHR created successfully");

	return surface;
}

const char** GlfwWindow::GetRequiredVulkanExtensions(uint32_t& extensionCount)
{
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	if (!extensions)
	{
		LOG_ERROR("Failed to get required Vulkan instance extensions from GLFW");
		extensionCount = 0;
		throw std::runtime_error("Failed to get required Vulkan extensions from GLFW.");
	}
	LOG_DEBUG("GLFW requires {} Vulkan instance extensions", extensionCount);
	for (int i = 0; i < extensionCount; ++i)
	{
		LOG_DEBUG("GLFW Required Vulkan extension {}: {}", i, extensions[i]);
	}

	return extensions;
}

void GlfwWindow::FramebufferResizeCallback(	GLFWwindow* window, int width, int height)
{
	auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));

	if (!self)
		return;

	self->m_framebufferWidth = width > 0 ? static_cast<uint32_t>(width) : 0;
	self->m_framebufferHeight = height > 0 ? static_cast<uint32_t>(height) : 0;
	self->m_inputListener.NotifyFramebufferResize({ width, height });
}
