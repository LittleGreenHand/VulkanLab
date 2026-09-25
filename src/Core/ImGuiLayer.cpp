#include "ImGuiLayer.h"

#include <vector>
#include <cassert>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "Render/VulkanContext.h"
#include "Render/VulkanRenderer.h"
#include "RenderResource/MeshManager.h"
#include "Simulation/PhysicsContext.h"
#include "Core/FrameClock.h"
#include "RenderBase/glTF/VulkanglTFScene.h"
#include "AI/AIModelManager.h"
#include "Device/CameraDevice.h"

void DrawCameraDevices()
{
	ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
	ImGui::Begin("相机设备");

	if (ImGui::Button("刷新相机列表"))
		CameraDevice::RefreshCameraList();
	ImGui::SameLine();
	ImGui::TextDisabled("设备检测可能需要几秒钟");
	const auto& cameras = CameraDevice::GetCameraDeviceList();
	ImGui::Text("相机数量: %zu", cameras.size());
	ImGui::TextWrapped("参数为检测时的当前或默认值。");
	ImGui::Separator();

	if (cameras.empty())
		ImGui::TextDisabled("未检测到相机设备");

	for (size_t index = 0; index < cameras.size(); ++index)
	{
		const auto& camera = cameras[index];
		ImGui::PushID(static_cast<int>(index));
		const std::string title = "[" + std::to_string(index) + "] " + camera.DeviceName;
		if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();
			ImGui::Text("列表索引 (CameraDevice::Open): %zu", index);
			ImGui::Text("OpenCV 设备索引: %d", camera.DeviceIndex);
			ImGui::TextWrapped("名称: %s", camera.DeviceName.empty() ? "未知" : camera.DeviceName.c_str());
			ImGui::TextWrapped("设备路径: %s", camera.DevicePath.empty() ? "未知" : camera.DevicePath.c_str());
			ImGui::Text("后端: %s (%d)", camera.BackendName.c_str(), camera.Backend);
			ImGui::Text("状态: %s", camera.Available ? "可用" : "不可用");
			if (camera.Width > 0)
				ImGui::Text("宽度: %d", camera.Width);
			else
				ImGui::TextDisabled("宽度: 未知");
			if (camera.Height > 0)
				ImGui::Text("高度: %d", camera.Height);
			else
				ImGui::TextDisabled("高度: 未知");
			if (camera.FPS > 0.0)
				ImGui::Text("帧率: %.2f FPS", camera.FPS);
			else
				ImGui::TextDisabled("帧率: 未知");
			if (!camera.Error.empty())
				ImGui::TextWrapped("检测信息: %s", camera.Error.c_str());
			ImGui::Unindent();
		}
		ImGui::PopID();
	}
	ImGui::End();
}

void DrawAIModels()
{
	auto& manager = AIModelManager::Get();
	ImGui::Begin("AI Models");

	if (ImGui::Button("Rescan Models"))
		manager.ScanModels();

	ImGui::SameLine();
	ImGui::TextDisabled("%zu model(s)", manager.GetModels().size());
	ImGui::Separator();

	if (manager.GetModels().empty())
	{
		ImGui::TextDisabled("No ONNX models found in:");
		ImGui::TextWrapped("%s", manager.GetModelRoot().string().c_str());
		ImGui::End();
		return;
	}

	constexpr const char* backendNames[] = { "OpenCV DNN", "ONNX Runtime" };
	for (const std::unique_ptr<AIModel>& modelPtr : manager.GetModels())
	{
		AIModel& model = *modelPtr;
		ImGui::PushID(modelPtr.get());

		bool enabled = model.IsEnabled();
		if (ImGui::Checkbox("##Enabled", &enabled))
			model.SetEnabled(enabled);
		ImGui::SameLine();

		const ModelInfo& info = model.GetInfo();
		const bool open = ImGui::CollapsingHeader(info.Name.c_str());
		if (open)
		{
			ImGui::Indent();
			model.DrawUI();			
			ImGui::Unindent();
		}

		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::End();
}

// 用于跟踪选中的节点
vkglTF::Node* selectedNode = nullptr;
void DrawNodeTree(vkglTF::Node* node, int& nodeId)
{
	if (!node) return;

	// 保存当前节点ID
	int originalNodeId = nodeId;
	std::string baseId = std::to_string(originalNodeId);

	// 处理节点名称
	std::string displayName = node->name.empty() ? "Unnamed Node" : node->name;
	displayName += "##node_" + baseId;

	// 节点是否可展开
	bool isExpandable = !node->children.empty();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

	// 选中节点高亮显示
	if (selectedNode == node)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!isExpandable)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	// 绘制可见性复选框
	ImGui::Checkbox(("##vis_" + baseId).c_str(), &node->visible);
	ImGui::SameLine(0, 4);

	// 绘制节点树
	bool isOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		if (selectedNode == node)
			selectedNode = nullptr; // 再次单击已选中节点，取消选择
		else
			selectedNode = node; // 单击节点文本区域，选中当前节点
	}

	// 递增节点ID
	nodeId++;

	// 递归绘制子节点
	if (isOpen && isExpandable)
	{
		for (vkglTF::Node* child : node->children)
		{
			DrawNodeTree(child, nodeId);
		}
		ImGui::TreePop();
	}
	else if (!isExpandable)
	{
		ImGui::TreePop();
	}
}

void DrawNodePropertiesPanel()
{
	if (!selectedNode)
	{
		ImGui::Text("Select a node to edit properties");
		return;
	}

	// 属性面板标题
	ImGui::Text("Node: %s",
		selectedNode->name.empty() ? "Unnamed Node" : selectedNode->name.c_str());
	ImGui::Separator();

	// 节点名称编辑
	char nameBuffer[256];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", selectedNode->name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		selectedNode->name = nameBuffer;
	}

	// 可见性控制
	ImGui::Checkbox("Visible", &selectedNode->visible);

	bool isTransformChanged = false;
	if (ImGui::InputFloat3("Translation", &selectedNode->translation.x))
	{
		isTransformChanged = true;
	}
	glm::vec3 rotationEuler = glm::eulerAngles(selectedNode->rotation) * (180.0f / glm::pi<float>());
	if (ImGui::InputFloat3("Rotation (deg)", &rotationEuler.x))
	{
		isTransformChanged = true;
		selectedNode->rotation = glm::quat(glm::radians(rotationEuler));
	}
	if (ImGui::InputFloat3("Scale", &selectedNode->scale.x))
	{
		isTransformChanged = true;
	}

	ImGui::Separator();
	ImGui::Separator();
	ImGui::Separator();

	// 材质属性（如果有网格）
	if (selectedNode->mesh)
	{
		ImGui::Text(" ");
		ImGui::Text(" ");
		ImGui::Text("Material Properties");
		ImGui::Separator();
		ImGui::Text("Mesh: %s", selectedNode->mesh->name.c_str());

		for (int i = 0; i < selectedNode->mesh->primitives.size(); ++i)
		{
			auto& primitive = selectedNode->mesh->primitives[i];
			auto& material = primitive->material;

			ImGui::PushID(i);
			ImGui::Text("Primitive %d Material", (int)i);
			ImGui::Separator();

			ImGui::ColorEdit4("基础颜色", &material.materialParameters.baseColorFactor.x);
			ImGui::SliderFloat("金属度", &material.materialParameters.metallicFactor, 0.0f, 1.0f);
			ImGui::SliderFloat("粗糙度", &material.materialParameters.roughnessFactor, 0.0f, 1.0f);
			ImGui::SliderFloat("Alpha裁切系数", &material.materialParameters.alphaCutoff, 0.0f, 1.0f);
			ImGui::SliderFloat("透明度", &material.materialParameters.alphaFactor, 0.0f, 1.0f);
			ImGui::SliderFloat("Anisotropic Factor", &material.materialParameters.anisotropicFactor, -1.0f, 1.0f);
			ImGui::InputFloat4("tangent", &material.materialParameters.tangent.x);

			const char* alphaModes[] = { "不透明", "遮罩", "透明" };
			ImGui::Combo("Alpha Mode", (int*)&material.alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes));
			// 定义纹理参数与对应显示文本的映射关系
			std::vector<std::pair<bool, const char*>> textureInfo = {
				{!material.materialParameters.baseColorTextureEmpty, "Base Color Texture: Present"},
				{!material.materialParameters.normalTextureEmpty, "Normal Texture: Present"},
				{!material.materialParameters.metallicRoughnessTextureEmpty, "Metallic-Roughness Texture: Present"},
				{!material.materialParameters.metallicTextureEmpty, "Metallic Texture: Present"},
				{!material.materialParameters.roughnessTextureEmpty, "Roughness Texture: Present"},
				{!material.materialParameters.occlusionTextureEmpty, "Occlusion Texture: Present"},
				{!material.materialParameters.emissiveTextureEmpty, "Emissive Texture: Present"},
				{!material.materialParameters.AOTextureEmpty, "AO Texture: Present"},
				{!material.materialParameters.diffuseTextureEmpty, "Diffuse Texture: Present"},
				{!material.materialParameters.specularGlossinessTextureEmpty, "Specular-Glossiness Texture: Present"}
			};

			// 循环显示存在的纹理信息
			for (const auto& [isPresent, text] : textureInfo) {
				if (isPresent) {
					ImGui::Text("%s", text);
				}
			}

			ImGui::Separator();
			ImGui::Separator();
			ImGui::Text(" ");
			ImGui::PopID();
		}
	}
	selectedNode->update(isTransformChanged);
	// 取消选择按钮
	if (ImGui::Button("Deselect"))
	{
		selectedNode = nullptr;
	}
}

bool ImGuiLayer::Init(GLFWwindow* window)
{
	auto renderer = VulkanContext::GetVulkanRenderer();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();	
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// 键盘导航，可选
	// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;// 开启停靠功能
	ImGui::StyleColorsDark();
	if (!ImGui_ImplGlfw_InitForVulkan(window, true))
	{
		return false;
	}

	// 初始化 Vulkan backend
	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.ApiVersion = VulkanContext::GetAPIVersion();
	initInfo.Instance = VulkanContext::GetVkInstance();
	initInfo.PhysicalDevice = VulkanContext::GetVkPhysicalDevice();
	initInfo.Device = VulkanContext::GetVkDevice();
	initInfo.QueueFamily = VulkanContext::GetGraphicsQueueFamily();
	initInfo.Queue = VulkanContext::GetGraphicsQueue();	
	initInfo.DescriptorPoolSize = 64;// 让 ImGui backend 自己创建 DescriptorPool,不需要手动额外管理 VkDescriptorPool。
	initInfo.MinImageCount = VulkanContext::GetMinImageCount();
	initInfo.ImageCount = VulkanContext::GetImageCount();
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	VkFormat colorAttachmentFormat = VulkanContext::GetSwapChainColorFormat();
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
	initInfo.UseDynamicRendering = true;

	if (!ImGui_ImplVulkan_Init(&initInfo)) 
	{
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		return false;
	}

	std::filesystem::path fontPath = GetAssetRootPath() / "Font/Noto_Sans_SC/static/NotoSansSC-Medium.ttf";
	auto cn_font = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
	io.FontDefault = cn_font;
	io.FontGlobalScale = 1.3;
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(1.3);

	m_init = true;
	return true;
}

void ImGuiLayer::Shutdown()
{
	if (!m_init)
		return;
	vkDeviceWaitIdle(VulkanContext::GetVkDevice());

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	m_init = false;
}

void ImGuiLayer::BeginFrame()
{
	if (!m_init)
		return;
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame(); 
	Update();
	if (m_showAIModelPanel)
		DrawAIModels();
	if (m_showCameraDevicePanel)
		DrawCameraDevices();
}

void ImGuiLayer::Update()
{
	if (!m_init)
		return;
	//渲染设置
	{
		auto renderer = VulkanContext::GetVulkanRenderer();
		ImGui::Text("FPS: %.1f  (%.3f ms)", FrameClock::Get().FPS(), FrameClock::Get().DeltaSeconds() * 1000);
		ImGui::Checkbox("显示AI模型面板", &m_showAIModelPanel);
		ImGui::Checkbox("显示相机设备面板", &m_showCameraDevicePanel);
		ImGui::Checkbox("启动物理模拟", &PhysicsContext::Get().isSimulationEnabled);

		if (ImGui::CollapsingHeader("相机")) {
			ImGui::Indent();
			{
				ImGui::SliderFloat("移动速度", &renderer->camera.movementSpeed, 0.1f, 10);
				ImGui::SliderFloat("旋转速度", &renderer->camera.rotationSpeed, 0.1f, 1.f);
				ImGui::InputFloat3("位置", (float*)&renderer->camera.position);
				ImGui::InputFloat3("旋转", (float*)&renderer->camera.rotation);
				ImGui::Checkbox("景深", &renderer->camera.enableDOF);
				if (renderer->camera.enableDOF)
				{
					ImGui::InputFloat("焦距", &renderer->camera.focusDistance, 0.1f, 1, "%.1f");
					ImGui::InputFloat("焦平面范围", &renderer->camera.focusRange, 0.1f, 1, "%.1f");
					ImGui::InputFloat("光圈", &renderer->camera.aperture, 0.1f, 1, "%.1f");
					ImGui::InputFloat("最大模糊半径", &renderer->camera.maxBlurRadius, 0.1f, 1, "%.1f");
				}
				if (ImGui::Checkbox("正交视图", &renderer->camera.useOrthographic))
					renderer->camera.switchProjectionType();
				if (renderer->camera.useOrthographic)
				{
					float left = renderer->camera.orthoLeft;
					float right = renderer->camera.orthoRight;
					float top = renderer->camera.orthoTop;
					float bottom = renderer->camera.orthoBottom;
					float znear = renderer->camera.znear;
					float zfar = renderer->camera.zfar;
					ImGui::InputFloat("Left", &left, 0.5f, 5, "%.1f");
					ImGui::InputFloat("Right", &right, 0.5f, 5, "%.1f");
					ImGui::InputFloat("Top", &top, 0.5f, 5, "%.1f");
					ImGui::InputFloat("Bottom", &bottom, 0.5f, 5, "%.1f");
					ImGui::InputFloat("NearPlane", &znear, 1, 100, "%.4f");
					ImGui::InputFloat("FarPlane", &zfar, 1, 100, "%.1f");
					if (left != renderer->camera.orthoLeft || right != renderer->camera.orthoRight || bottom != renderer->camera.orthoBottom || top != renderer->camera.orthoTop || znear != renderer->camera.znear || zfar != renderer->camera.zfar)
						renderer->camera.setOrthographic(left, right, bottom, top, znear, zfar);
				}
				else
				{
					float fov = renderer->camera.fov;
					float znear = renderer->camera.znear;
					float zfar = renderer->camera.zfar;
					ImGui::InputFloat("FOV", &fov, 0.5f, 5, "%.1f");
					ImGui::InputFloat("NearPlane", &znear, 1, 100, "%.4f");
					ImGui::InputFloat("FarPlane", &zfar, 1, 100, "%.1f");
					if (fov != renderer->camera.fov || znear != renderer->camera.znear || zfar != renderer->camera.zfar)
					{
						renderer->camera.setPerspective(fov, (float)renderer->m_renderWidth / (float)renderer->m_renderHeiht, znear, zfar);
						renderer->directLight.updateCascades();
					}
				}
			}
			ImGui::Unindent();
		}
		if (ImGui::CollapsingHeader("全局设置")) {
			ImGui::Indent();
			{
				ImGui::InputFloat("曝光", &renderer->globalParam.exposure, 0.01f, 0.1f, "%.2f");
				ImGui::InputFloat("Gamma", &renderer->globalParam.gamma, 0.01f, 0.1f, "%.2f");
				ImGui::Checkbox("Skybox", &renderer->displaySkybox);
				if (renderer->showGBuffer == -1)
					ImGui::SliderInt("显示GBuffer", &renderer->showGBuffer, -1, GBufferCount - 1);
				else
					ImGui::SliderInt(GBufferNames[renderer->showGBuffer], &renderer->showGBuffer, -1, GBufferCount - 1);
			}
			ImGui::Unindent();
		}
		if (ImGui::CollapsingHeader("光源设置")) {
			ImGui::Indent();
			{
				bool isRnder = vkLight::lightData.directLight.isRnder;
				if (ImGui::Checkbox("##vis_SunLight", &isRnder))
				{
					vkLight::lightData.directLight.isRnder = isRnder;
					vkLight::updateLightBuffer();
				}
				ImGui::SameLine(0, 4);
				if (ImGui::CollapsingHeader("太阳光")) {
					bool PCF = vkLight::lightData.directLight.usePCF;
					bool colorCascades = vkLight::lightData.directLight.colorCascades;
					if (ImGui::Checkbox("PCF", &PCF) ||
						ImGui::Checkbox("colorCascades", &colorCascades) ||
						ImGui::InputFloat("深度偏移", &renderer->directLight.depthBiasConstant) ||
						ImGui::InputFloat("深度偏移斜率", &renderer->directLight.depthBiasSlope) ||
						ImGui::ColorEdit3("太阳光颜色", (float*)&vkLight::lightData.directLight.color) ||
						ImGui::InputFloat("太阳光强度", &vkLight::lightData.directLight.color.w)
						)
					{
						vkLight::lightData.directLight.usePCF = PCF;
						vkLight::lightData.directLight.colorCascades = colorCascades;
						vkLight::updateLightBuffer();
					}
					if (ImGui::InputFloat3("太阳方向", (float*)&vkLight::lightData.directLight.direct) ||
						ImGui::SliderFloat("Split lambda", &renderer->directLight.cascadeSplitLambda, 0.1f, 1.f))
					{
						renderer->directLight.updateCascades();
					}
				}

				if (ImGui::CollapsingHeader("点光源")) {
					int oldCount = vkLight::lightData.activePointLightCount;
					if (ImGui::SliderInt("光源数量", &vkLight::lightData.activePointLightCount, 0, vkLight::MAX_POINTLIGHTS))
					{
						vkLight::updateLightBuffer();
						if (vkLight::lightData.activePointLightCount > oldCount)
						{
							vkDeviceWaitIdle(renderer->device);
							renderer->pointLights.destroy();
							renderer->pointLights.prepare(renderer->vulkanDevice, renderer->vulkanDevice->getSupportedDepthFormat(false));
						}
					}
					ImGui::Separator();
					ImGui::Separator();
					ImGui::Text("");
					for (int i = 0; i < vkLight::lightData.activePointLightCount; i++)
					{
						std::string lightName = "点光源" + std::to_string(i);
						std::string str_id = "##PointLight" + std::to_string(i);
						bool isRnder = vkLight::lightData.pointLights[i].isRnder;
						if (ImGui::Checkbox((str_id + "isRnder").c_str(), &isRnder))
						{
							vkLight::lightData.pointLights[i].isRnder = isRnder;
							vkLight::updateLightBuffer();
						}
						ImGui::SameLine(0, 4);
						if (ImGui::CollapsingHeader(lightName.c_str())) {
							if (ImGui::InputFloat3(("位置" + str_id).c_str(), (float*)&vkLight::lightData.pointLights[i].position, "%.2f") ||
								ImGui::ColorEdit3(("颜色" + str_id).c_str(), (float*)&vkLight::lightData.pointLights[i].color) ||
								ImGui::SliderFloat(("范围" + str_id).c_str(), &vkLight::lightData.pointLights[i].range, 0, 256) ||
								ImGui::SliderInt(("衰减模式" + str_id).c_str(), &vkLight::lightData.pointLights[i].attenuationMode, 0, 2))
							{
								vkLight::updateLightBuffer();
							}
						}
					}
				}
			}
			ImGui::Unindent();
		}
	}

	//场景树
	{
		int nodeId = 0;
		{
			// 创建左右分栏布局
			ImGui::Begin("场景树");

			// 左侧节点树
			ImGui::BeginChild("节点树", ImVec2(300, 800), true);
			static bool visibleAll = true;
			if (ImGui::Checkbox("显示所有模型", &visibleAll))
			{
				if (visibleAll)
				{
					for (auto& [key, model] : MeshManager::Get().m_sceneTree)
					{
						model.nodes[0]->visible = true;
					}
				}
				else
				{
					for (auto& [key, model] : MeshManager::Get().m_sceneTree)
					{
						model.nodes[0]->visible = false;
					}
				}
			}
			for (auto& [key, model] : MeshManager::Get().m_sceneTree)
			{
				DrawNodeTree(model.nodes[0], nodeId);
			}
			ImGui::EndChild();

			ImGui::SameLine(0, 4);

			// 右侧属性面板
			ImGui::BeginChild("节点属性", ImVec2(550, 800), true);
			DrawNodePropertiesPanel();
			ImGui::EndChild();

			ImGui::End();
		}
	}
}


void ImGuiLayer::Render()
{
	if (!m_init)
		return;
	ImGui::Render();
	VulkanContext::GetVulkanRenderer()->DrawImGui();
}

void ImGuiLayer::SetMinImageCount(int minImageCount)
{
	if (!m_init)
		return;

	ImGui_ImplVulkan_SetMinImageCount(minImageCount);
}


bool ImGuiLayer::WantCaptureMouse() const
{
	if (!m_init)
		return false;

	return ImGui::GetIO().WantCaptureMouse;
}


bool ImGuiLayer::WantCaptureKeyboard() const
{
	if (!m_init)
		return false;

	return ImGui::GetIO().WantCaptureKeyboard;
}
