#include "Application.h"
#include "FrameClock.h"
#include "Log.h"
#include <thread>
#include "RenderResource/MeshManager.h"
#include "Simulation/PhysicsContext.h"
#include "RenderBase/VulkanTools.h"
#include "AI/AIModelManager.h"
#include "AI/Adapter/MediaPipeHandAdapter.h"

namespace
{
	constexpr const char* DefaultHandPoseModel = "palm_detection_mediapipe_2023feb";
}

bool Application::m_init = false;
bool Application::Init()
{
	LOG_DEBUG("Initializing application");

	//Camera设备
	{
		CameraDevice::RefreshCameraList();
		if (CameraDevice::GetCameraDeviceCount() > 0)
		{
			if (!m_cameraDevice.Open(0))
			{
				LOG_ERROR("Failed to open camera device 0");
			}
		}
	}
	// AI模型
	{
		AIModelManager::Get().Initialize(GetAssetRootPath() / "AI_Models");
		auto* model = AIModelManager::Get().FindModel(DefaultHandPoseModel);
		if (!model)
			LOG_ERROR("Default hand pose model not found: {}", DefaultHandPoseModel);
		else if (!model->SetEnabled(true))
			LOG_ERROR("Failed to enable MediaPipe hand pose model: {}", model->GetLastError());
	}

	// 初始化窗口
	{
		GlfwWindow::CreateInfo createInfo;
		m_window = std::make_unique<GlfwWindow>();
		if (!m_window->Init(createInfo)) {
			LOG_ERROR("Failed to initialize GLFW window");
			return false;
		}
	}

	// PhysX
	{
		if (!PhysicsContext::Get().Init()) {
			LOG_ERROR("Failed to initialize PhysX");
		}
	}

	// 初始化Vulkan与渲染器
	{
		uint32_t extCount = 0;
		auto extensions = m_window->GetRequiredVulkanExtensions(extCount);
		m_renderer = std::make_unique<VulkanRenderer>();		
		m_renderer->AddEnabledInstanceExtensions(extCount, extensions);
		if (!m_renderer->InitVulkan()) {
			LOG_ERROR("Failed to initialize Vulkan");
			return false;
		}
		auto surface = m_window->CreateSurface(m_renderer->instance);
		m_renderer->Init(surface);
		LOG_DEBUG("Vulkan renderer initialized");
	}

	//绑定输入回调
	{
		m_window->SetInputCallbacks({
			.key = [this](const WindowKeyEvent& event)
			{
				OnKey(event.key, event.scancode, event.action, event.mods);
			},
			.mouseButton = [this](const WindowMouseButtonEvent& event)
			{
				OnMouseButton(event.button, event.action, event.mods, event.cursorX, event.cursorY);
			},
			.cursorPosition = [this](const WindowCursorPositionEvent& event)
			{
				OnMouseMove(event.x, event.y);
			},
			.scroll = [this](const WindowScrollEvent& event)
			{
				OnScroll(event.xOffset, event.yOffset);
			},
			.framebufferResize = [this](const WindowFramebufferResizeEvent& event)
			{
				OnFramebufferResize(event.width, event.height);
			}
			});
	}

	// ImGUI
	{
		m_guiLayer = std::make_unique<ImGuiLayer>();
		if (!m_guiLayer->Init(m_window->GetNativeWindow())) {
			LOG_ERROR("Failed to initialize ImGui layer");
			return false;
		}
	}

	m_init = true;
	LOG_DEBUG("Application initialized successfully");
	return true;
}

void Application::Destroy()
{
	LOG_DEBUG("Destroying application");
	m_cameraDevice.Close();
	m_guiLayer.reset();	
	m_renderer.reset();
	m_window.reset();
	PhysicsContext::Get().Destroy();
	AIModelManager::Get().Shutdown();
	m_init = false;
	LOG_DEBUG("Application destroyed");
}

void Application::Resize(int width, int height)
{

}

bool Application::Run()
{
	LOG_DEBUG("Entering application main loop");
	while (!m_window->ShouldClose())
	{
		if (!BeginFrame())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		AIInference();
		Simulate();
		UpdateScene();
		Render();
		EndFrame();		
	}
	return true;
}

bool Application::BeginFrame()
{
	m_window->PollEvents();

	if (m_minimized)
		return false;
	// 后缓冲延迟Resize，避免连续reszie
	{
		if (m_needResize)
		{
			static auto ResizeDelay = std::chrono::milliseconds(150);
			auto now = std::chrono::steady_clock::now();
			auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastResizeTime);
			if (time > ResizeDelay)
			{
				m_renderer->OnFramebufferResize(m_window->GetFramebufferWidth(), m_window->GetFramebufferHeight());
				m_needResize = false;
			}
			else
				return false;
		}
	}

	if (m_window->ShouldClose())
		return false;

	FrameClock::Get().Tick();
	if (!m_renderer->BeginFrame(FrameClock::Get().DeltaSeconds()))
	{
		LOG_DEBUG("Frame skipped while the swapchain is being updated");
		return false;
	}
	m_guiLayer->BeginFrame();
	return true;
}

void Application::UpdateScene()
{
}

void Application::AIInference()
{
	auto* model = AIModelManager::Get().FindModel(DefaultHandPoseModel);
	if (!model || !model->IsEnabled() || !m_cameraDevice.IsOpened())
	{
		m_cameraDevice.CloseDebugWindow();
		return;
	}
	cv::Mat frame;
	if (!m_cameraDevice.Capture(frame))
	{
		LOG_ERROR("Failed to capture camera frame");
		m_cameraDevice.CloseDebugWindow();
		return;
	}
	InferenceInput input;
	InferenceOutput output;
	std::string error;
	if (!MediaPipeHandAdapter::CreateInput(frame, input, error))
	{
		LOG_ERROR("Failed to create inference input: {}", error);
		m_cameraDevice.CloseDebugWindow();
		return;
	}
	if (!model->Run(input, output))
	{
		LOG_ERROR("Failed to run inference: {}", model->GetLastError());
		m_cameraDevice.CloseDebugWindow();
		return;
	}
	if (!output.HandPoses)
	{
		LOG_ERROR("Hand pose adapter returned no structured result");
		m_cameraDevice.CloseDebugWindow();
		return;
	}
	if (!m_cameraDevice.ShowHandPoseDebug(frame, *output.HandPoses, error))
	{
		LOG_ERROR("Failed to show MediaPipe hand pose debug image: {}", error);
		m_cameraDevice.CloseDebugWindow();
	}
}

void Application::Simulate()
{
	if (PhysicsContext::Get().IsInit() && PhysicsContext::Get().IsSimulationEnabled())
	{
		PhysicsContext::Get().Simulate(FrameClock::Get().DeltaSeconds());
		MeshManager::Get().UpdateSimulationResults();
	}
}

void Application::Render()
{
	m_renderer->render();
	m_guiLayer->Render();
}
  
void Application::EndFrame()
{
	m_renderer->EndFrame();
}

void Application::OnKey(int key, int scancode, int action, int mods)
{
	if (action == GLFW_RELEASE)
	{
		switch (key)
		{
		case GLFW_KEY_W:
			m_renderer->camera.keys.up = false;
			break;
		case GLFW_KEY_S:
			m_renderer->camera.keys.down = false;
			break;
		case GLFW_KEY_A:
			m_renderer->camera.keys.left = false;
			break;
		case GLFW_KEY_D:
			m_renderer->camera.keys.right = false;
			break;
		case GLFW_KEY_Q:
			m_renderer->camera.keys.top = false;
			break;
		case GLFW_KEY_E:
			m_renderer->camera.keys.bottom = false;
			break;
		}
		return;
	}

	if (action != GLFW_PRESS && action != GLFW_REPEAT)
		return;

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_P:
			m_renderer->paused = !m_renderer->paused;
			break;
		case GLFW_KEY_F1:
			// Reserved for toggling the UI overlay.
			break;
		case GLFW_KEY_F2:
			m_renderer->camera.type = m_renderer->camera.type == Camera::CameraType::lookat
				? Camera::CameraType::firstperson
				: Camera::CameraType::lookat;
			break;
		}
	}

	if (m_renderer->camera.type == Camera::CameraType::firstperson)
	{
		switch (key)
		{
		case GLFW_KEY_W:
			m_renderer->camera.keys.up = true;
			break;
		case GLFW_KEY_S:
			m_renderer->camera.keys.down = true;
			break;
		case GLFW_KEY_A:
			m_renderer->camera.keys.left = true;
			break;
		case GLFW_KEY_D:
			m_renderer->camera.keys.right = true;
			break;
		case GLFW_KEY_Q:
			m_renderer->camera.keys.top = true;
			break;
		case GLFW_KEY_E:
			m_renderer->camera.keys.bottom = true;
			break;
		}
	}
}

void Application::OnMouseButton(int button, int action, int mods, double cursorX, double cursorY)
{
	if (action == GLFW_PRESS)
		m_renderer->mouseState.position = glm::vec2(static_cast<float>(cursorX), static_cast<float>(cursorY));


	if (!m_guiLayer->WantCaptureMouse())
	{
		const bool pressed = action == GLFW_PRESS;
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			m_renderer->mouseState.buttons.left = pressed;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			m_renderer->mouseState.buttons.right = pressed;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			m_renderer->mouseState.buttons.middle = pressed;
			break;
		}
	}
}

void Application::OnMouseMove(double x, double y)
{
	double dx = static_cast<double>(m_renderer->mouseState.position.x) - x;
	double dy = static_cast<double>(m_renderer->mouseState.position.y) - y;

	if (m_guiLayer->WantCaptureMouse()) {
		m_renderer->mouseState.position = glm::vec2((float)x, (float)y);
		return;
	}

	if (m_renderer->mouseState.buttons.left) {
		m_renderer->camera.rotate(glm::vec3(static_cast<float>(dy) * m_renderer->camera.rotationSpeed, -static_cast<float>(dx) * m_renderer->camera.rotationSpeed, 0.0f));
	}
	if (m_renderer->mouseState.buttons.right) {
		m_renderer->camera.Translate(glm::vec3(static_cast<float>(dx) * .005f, 0.0f, static_cast<float>(dy) * .005f));
	}
	if (m_renderer->mouseState.buttons.middle) {
		m_renderer->camera.Translate(glm::vec3(-static_cast<float>(dx) * 0.005f, -static_cast<float>(dy) * 0.005f, 0.0f));
	}
	m_renderer->mouseState.position = glm::vec2(static_cast<float>(x), static_cast<float>(y));
}

void Application::OnScroll(double xOffset, double yOffset)
{
	if (!m_guiLayer->WantCaptureMouse()){
		auto camFront = m_renderer->camera.GetFront() * (static_cast<float>(-yOffset) * 120.0f * 0.005f);
		m_renderer->camera.Translate(camFront);
	}
}

void Application::OnFramebufferResize(int framebufferWidth, int framebufferHeight)
{
	m_lastResizeTime = std::chrono::steady_clock::now();
	m_needResize = true;
	if (framebufferWidth == 0 || framebufferHeight == 0)
	{
		m_minimized = true;
	}
	else
	{
		m_minimized = false;
	}
}
