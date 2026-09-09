#pragma once
#include <memory>
#include "Render/VulkanRenderer.h"
#include "GlfwWindow.h"
#include "ImGuiLayer.h"
class Application
{
public:
	static bool IsInit() { return m_init; }
	static Application& GetInstance()
	{
		static Application instance;
		return instance;
	}
	Application() = default;
	~Application() { Destroy(); }
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	bool Init();
	void Destroy();
	void Resize(int width, int height);
	bool Run();
	bool BeginFrame();
	void UpdateScene();
	void Simulate();
	void Render();
	void EndFrame();

	void OnKey(int key, int scancode, int action, int mods);
	void OnMouseButton(int button, int action, int mods, double cursorX, double cursorY);
	void OnMouseMove(double x, double y);
	void OnScroll(double xOffset, double yOffset);
	void OnFramebufferResize(int framebufferWidth, int framebufferHeight);
private:
	static bool m_init;
	bool m_needResize = false; // 窗口是否处于Resize状态
	std::chrono::steady_clock::time_point m_lastResizeTime;//记录最后一次调用OnFramebufferResize的时间
	std::unique_ptr<VulkanRenderer> m_renderer;
	std::unique_ptr<GlfwWindow> m_window;
	std::unique_ptr<ImGuiLayer> m_guiLayer;
};
