#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

class ImGuiLayer
{
public:
	ImGuiLayer() = default;
	~ImGuiLayer() { Shutdown(); }

	ImGuiLayer(const ImGuiLayer&) = delete;
	ImGuiLayer& operator=(const ImGuiLayer&) = delete;

public:

	bool Init(GLFWwindow* window);
	void Shutdown();

	void BeginFrame();

	// 更新并构建要显示的内容
	void Update();

	// 将 ImGui draw data 写入 Vulkan CommandBuffer
	void Render();

	// Swapchain image 数量发生变化时调用
	void SetMinImageCount(uint32_t minImageCount);

	bool WantCaptureMouse() const;
	bool WantCaptureKeyboard() const;

private:
	bool m_init = false;
};