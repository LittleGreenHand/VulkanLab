#include "VulkanRenderer.h"
#include "VulkanContext.h"
#include "VulkanImageUtils.h"
#include "RenderBase/RenderConfig.hpp"
#include "Render/GBuffer.hpp"
#include "RenderBase/glTF/VulkanglTFMaterial.h"
#include "RenderBase/glTF/VulkanglTFTypes.h"
#include "PostProcessBase.h"
#include "PipelineBuilder.h"
#include "PostProcess_ToneMapping.h"
#include "PostProcess_DOF.h"
#include "imgui_impl_vulkan.h"
#include "RenderResource/TextureManager.h"
#include "RenderResource/MeshManager.h"
#include "RenderResource/EnvironmentManager.h"
#include "VulkanDebugUtils.h"
#include "Core/Log.h"

VulkanRenderer::VulkanRenderer() : VulkanRendererBase()
{
	title = "VulkanEngine";
	camera.type = Camera::CameraType::firstperson;
	camera.movementSpeed = 0.5f;
	camera.setPerspective(60.0f, (float)m_renderWidth / (float)m_renderHeiht, 0.01f, 256.0f);

	camera.rotationSpeed = 0.15f;
	camera.setRotation({ 0.0f, 0.0f, 0.0f });
	camera.setPosition({ 0.f, 0.f, -1.f });

	camera.focusDistance = 1.0f;
	camera.focusRange = 1.0f;
	camera.maxBlurRadius = 4.5f;
	camera.aperture = 0.56f;
}

VulkanRenderer::~VulkanRenderer()
{
	LOG_DEBUG("Destroying Vulkan renderer");
	vkDeviceWaitIdle(device);
	vkLight::destroyLightBuffer();
	if (postProcessManager)
	{
		postProcessManager->destroyALL();
		delete postProcessManager;
		postProcessManager = nullptr;
	}
	TextureManager::Get().Destroy();
	MeshManager::Get().Destroy();
	EnvironmentManager::Get().Destroy();
	if (device) {
		for (auto& buffer : globalParamBuffers) {
			buffer.globalParamBuffer.destroy();
		}
		for (auto& pipeline : pipelines)
		{
			pipeline.destroy(device);
		}
		for (auto& layout : setLayouts)
		{
			vkDestroyDescriptorSetLayout(device, layout, nullptr);
		}
	}
	PostProcessBase::cleanUp();
	LOG_DEBUG("Destroy Vulkan renderer successfully");
}

void VulkanRenderer::Init(VkSurfaceKHR surface)
{
	LOG_DEBUG("Initializing Vulkan renderer");
	VulkanContext::Init(this);
	VulkanDebugUtils::InitDebugUtils(instance, device);
	VulkanRendererBase::InitSurfaceKHR(surface);
	VulkanRendererBase::InitRenderResource();

	//加载渲染资源
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		TextureManager::Get().LoadTextures();
		MeshManager::Get().LoadModels();
		EnvironmentManager::Get().LoadIBLTextures();
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto takeTime = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		LOG_INFO("Render resources loaded in {:.2f} ms", takeTime);
	}

	//初始化主渲染模块相关资源
	{
		prepareUniformBuffers();
		prepareDescriptors();
		preparePipelines();
	}
	//初始化灯光资源
	{
		pointLights.prepare(vulkanDevice, vulkanDevice->getSupportedDepthFormat(false));
		directLight.prepare(vulkanDevice, vulkanDevice->getSupportedDepthFormat(true));
	}
	InitPostProcess();
	m_init = true;
	LOG_DEBUG("Vulkan renderer initialized successfully");
}

void VulkanRenderer::getEnabledFeatures()
{
	if (deviceFeatures.samplerAnisotropy) {
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}	
	enabledFeatures.imageCubeArray = VK_TRUE;//启用立方体贴图数组

	Features11.shaderDrawParameters = VK_TRUE;//用于标识指示物理设备是否支持 "着色器绘制参数"，例如gl_DrawID、gl_InstanceID、gl_BaseVertex等

	features12.bufferDeviceAddress = true; //允许应用程序获取缓冲区的设备地址（GPU可直接访问的内存地址），并在着色器中通过此地址直接访问缓冲区数据
	features12.descriptorIndexing = true; //允许在着色器中动态索引描述符数组，着色器可以使用变量（而非编译期常量）作为索引访问描述符数组

	features13.dynamicRendering = true; //启用动态渲染，这样可以减少对渲染通道（Render Pass）和帧缓冲（Framebuffer）的依赖
	features13.synchronization2 = true; //用于标识物理设备是否支持Vulkan1.3引入的第二代同步机制，旨在简化同步逻辑、减少错误，并提供更精细的同步控制

	Features11.pNext = &features12;
	features12.pNext = &features13;
	features13.pNext = nullptr;
	deviceCreatepNextChain = &Features11;
}

void VulkanRenderer::AddEnabledInstanceExtensions(int extensionCount, const char** extensions)
{
	for (int i = 0; i < extensionCount; i++)
	{
		enabledInstanceExtensions.push_back(extensions[i]);
	}
}
void VulkanRenderer::AddEnabledDeviceExtensions(int extensionCount, const char** extensions)
{
	for (int i = 0; i < extensionCount; i++)
	{
		enabledDeviceExtensions.push_back(extensions[i]);
	}
}
void VulkanRenderer::getEnabledExtensions()
{	
	enabledDeviceExtensions.push_back("VK_KHR_pipeline_library");
	enabledDeviceExtensions.push_back("VK_EXT_graphics_pipeline_library");
	libraryFeatures.graphicsPipelineLibrary = VK_TRUE;
	libraryFeatures.pNext = deviceCreatepNextChain;

	deviceCreatepNextChain = &libraryFeatures;
}

void VulkanRenderer::prepareDescriptors()
{
	// 创建DescriptorPool
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MaxConcurrentFrames * 8),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxConcurrentFrames * 16)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, MaxConcurrentFrames * 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)descriptorPool, "MainDescriptorPool");
	}
	
	// 创建DescriptorSetLayout
	{
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(nullptr, 0);

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		for (int i = 0; i < GBufferCount; i++)
		{
			setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 + i));
		}
		descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &setLayouts[LBI_GLOBAL]));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)setLayouts[LBI_GLOBAL], "globalParamDescriptorSetLayout");

		setLayoutBindings.clear();
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0));
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1));
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2));
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3));
		descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &setLayouts[LBI_IBL]));
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)setLayouts[LBI_IBL], "IBLDescriptorLayout");
	}

	// 创建DescriptorSet
	{
		//全局参数
		for (auto i = 0; i < globalDescriptorSets.size(); i++) {
			// 分配描述符集
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &setLayouts[LBI_GLOBAL], 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &globalDescriptorSets[i]));
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)globalDescriptorSets[i], "frameDescriptorSets[" + std::to_string(i) + "].globalParamDescriptorSet ");			
		}

		//灯光
		{
			vkLight::preperDescriptor(vulkanDevice, descriptorPool);
		}

		//IBL贴图
		{
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &setLayouts[LBI_IBL], 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &IBLDescriptorSet));
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)IBLDescriptorSet, "IBLDescriptorSet ");			
		}
	}
	UpdateDescriptorSets();
	setLayouts[LBI_LIGHTS] = vkLight::descriptorSetLayout;
	setLayouts[LBI_MATERIALS] = vkglTF::MaterialDescriptorSetLayout;
	setLayouts[LBI_MESH] = vkglTF::MeshDescriptorSetLayout;
}

void VulkanRenderer::preparePipelines()
{
	auto tStart = std::chrono::high_resolution_clock::now();

	//skybox pipeline
	{
		PipelineBuilder builder(device);
		VkPipelineColorBlendAttachmentState colorBlendAttachments[1] = { vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)};
		builder.colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, colorBlendAttachments);

		std::vector<VkDescriptorSetLayout> setLayoutsVector;
		setLayoutsVector.resize(2);
		setLayoutsVector[LBI_GLOBAL] = setLayouts[LBI_GLOBAL];
		setLayoutsVector[LBI_IBL] = setLayouts[LBI_IBL];
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayoutsVector);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelines[PL_Skybox].layout));

		// Skybox pipeline
		builder.rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		builder.depthStencilState.depthWriteEnable = VK_FALSE;
		builder.depthStencilState.depthTestEnable = VK_TRUE;
		builder.addShaderStage(loadShader(getShadersPath() + "skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		builder.addShaderStage(loadShader(getShadersPath() + "skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		builder.buildPipeline(mainRenderPass.renderPass, pipelineCache, pipelines[PL_Skybox].layout, pipelines[PL_Skybox].pipeline);
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines[PL_Skybox].pipeline, "skybox pipeline");
	}

	// PBR pipeline
	{
		std::vector<VkDescriptorSetLayout> setLayoutsVector;
		setLayoutsVector.resize(5);
		setLayoutsVector[LBI_GLOBAL] = setLayouts[LBI_GLOBAL];
		setLayoutsVector[LBI_IBL] = setLayouts[LBI_IBL];
		setLayoutsVector[LBI_LIGHTS] = setLayouts[LBI_LIGHTS];
		setLayoutsVector[LBI_MATERIALS] = setLayouts[LBI_MATERIALS];
		setLayoutsVector[LBI_MESH] = setLayouts[LBI_MESH];
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayoutsVector);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelines[PL_PBR_BLEND].layout));
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].layout));
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].layout));
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelines[PL_PBR_DEFER_LIGHTING].layout));

		//不透明与遮罩物体的延迟渲染几何阶段。用于生成GBuffer
		{
			PipelineBuilder builder(device);
			builder.depthStencilState.depthWriteEnable = VK_TRUE;
			builder.depthStencilState.depthTestEnable = VK_TRUE;
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_deferMRT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_deferMRT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

			//通过特化常量设置透明度模式和渲染模式
			std::array<int32_t, 1> SpecializationConstant;
			VkSpecializationMapEntry specializationMapEntry[1] = { vks::initializers::specializationMapEntry(0, 0, sizeof(int32_t)) };
			VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(1, specializationMapEntry, sizeof(SpecializationConstant), SpecializationConstant.data());
			builder.shaderStages[0].pSpecializationInfo = &specializationInfo;
			builder.shaderStages[1].pSpecializationInfo = &specializationInfo;

			//设置多渲染目标
			VkPipelineColorBlendAttachmentState colorBlendAttachments[GBufferCount];
			for (int i = 0; i < GBufferCount; i++)
			{
				colorBlendAttachments[i] = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
			}
			builder.colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(GBufferCount, colorBlendAttachments);
			VkPipelineRenderingCreateInfo renderInfo = vks::initializers::pipelineRenderingCreateInfo(GBufferCount, GBufferFormats);
			renderInfo.depthAttachmentFormat = depthFormat;
			renderInfo.stencilAttachmentFormat = depthFormat;

			SpecializationConstant[0] = 0;//不透明模式
			builder.buildPipeline(renderInfo, pipelineCache, pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].layout, pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].pipeline);
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].pipeline, "PL_PBR_DEFER_GEOMETRY_Opaque pipeline");

			SpecializationConstant[0] = 1;//遮罩模式
			builder.buildPipeline(renderInfo, pipelineCache, pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].layout, pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].pipeline);
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].pipeline, "PL_PBR_DEFER_GEOMETRY_AlphaMasked pipeline");
		}

		//PBR延迟渲染光照管线
		//透明物体前向渲染管线
		{
			PipelineBuilder builder(device);
			builder.depthStencilState.depthWriteEnable = VK_FALSE;//禁用深度写入
			builder.depthStencilState.depthTestEnable = VK_TRUE;//启用深度测试
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_fullScreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_Render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

			//通过特化常量设置透明度模式和渲染模式
			struct SpecializationData {
				int32_t ALPHAMODE;
				int32_t RENDER_MODE;
			} specializationData;
			std::array<VkSpecializationMapEntry, 2> specializationMapEntries;
			specializationMapEntries[0].constantID = 0;
			specializationMapEntries[0].size = sizeof(specializationData.ALPHAMODE);
			specializationMapEntries[0].offset = 0;
			specializationMapEntries[1].constantID = 1;
			specializationMapEntries[1].size = sizeof(specializationData.RENDER_MODE);
			specializationMapEntries[1].offset = offsetof(SpecializationData, RENDER_MODE);
			VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries.size(), specializationMapEntries.data(), sizeof(specializationData), &specializationData);
			builder.shaderStages[0].pSpecializationInfo = &specializationInfo;
			builder.shaderStages[1].pSpecializationInfo = &specializationInfo;

			//不透明与遮罩物体的延迟渲染光照阶段
			specializationData.RENDER_MODE = 1;//延迟渲染模式
			specializationData.ALPHAMODE = 0;//不透明模式
			builder.buildPipeline(mainRenderPass.renderPass, pipelineCache, pipelines[PL_PBR_DEFER_LIGHTING].layout, pipelines[PL_PBR_DEFER_LIGHTING].pipeline);
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines[PL_PBR_DEFER_LIGHTING].pipeline, "PL_PBR_DEFER_LIGHTING pipeline");

			//透明物体前向渲染管线
			builder.shaderStages.clear();
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_Render.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
			builder.addShaderStage(loadShader(getShadersPath() + "PBR_Render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
			builder.shaderStages[0].pSpecializationInfo = &specializationInfo;
			builder.shaderStages[1].pSpecializationInfo = &specializationInfo;
			specializationData.RENDER_MODE = 0;//前向渲染模式
			specializationData.ALPHAMODE = 2;//透明模式
			builder.setColorBlendAttachmentState(0xf, VK_TRUE);
			builder.enableBlendingAdditive();
			//builder.enableBlendingAlphaBlend();
			builder.buildPipeline(mainRenderPass.renderPass, pipelineCache, pipelines[PL_PBR_BLEND].layout, pipelines[PL_PBR_BLEND].pipeline);
			VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipelines[PL_PBR_BLEND].pipeline, "PBR_BLEND pipeline");
		}
	}

	auto tEnd = std::chrono::high_resolution_clock::now();
	auto takeTime = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	LOG_INFO("PreparePipelines cost time: {:.2f} ms", takeTime);
}

void VulkanRenderer::prepareUniformBuffers()
{
	for (auto& buffer : globalParamBuffers) {
		// Scene matrices uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer.globalParamBuffer, sizeof(globalParam)));
		VK_CHECK_RESULT(buffer.globalParamBuffer.map());
		static int count = 0;
		std::string name= "globalParamBuffers" + std::to_string(count++);
		VulkanDebugUtils::SetObjectDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer.globalParamBuffer.buffer, name);		
	}
}

void VulkanRenderer::updateUniformBuffers()
{
	for (auto& [key, model] : MeshManager::Get().m_sceneTree)
	{
		model.updatePrevMatrix();
	}
	// 3D object
	globalParam.view = camera.matrices.view;
	globalParam.inverseView = glm::inverse(camera.matrices.view);
	globalParam.projection = camera.matrices.perspective;
	globalParam.prevViewProj = globalParam.viewProj;
	globalParam.viewProj = globalParam.projection * globalParam.view;
	globalParam.jitter = glm::vec4(0.f, 0.f, 0.f, 0.f);
	globalParam.camPos = camera.position;
	globalParam.nearPlane = camera.znear;
	globalParam.farPlane = camera.zfar;	
	memcpy(globalParamBuffers[currentBuffer].globalParamBuffer.mapped, &globalParam, sizeof(GlobalParams));

	postProcessManager->dofProcess->setDOFParams(camera.znear, camera.zfar, camera.focusDistance, camera.focusRange, camera.maxBlurRadius, camera.aperture);
	//models[BaseModel::Cerberus].nodes[0]->scale = (glm::vec3(0.2f * (timer + 1)));
	//models[BaseModel::Cerberus].nodes[0]->rotation = VulkanUtils::eularToQuaternion(glm::vec3(-90, 90, (timer + 1) * 360.0));
	//models[BaseModel::Cerberus].nodes[0]->update();
	
}

void VulkanRenderer::UpdateDescriptorSets()
{
	// 全局参数
	{
		for (auto i = 0; i < globalDescriptorSets.size(); i++)
		{
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = { vks::initializers::writeDescriptorSet(globalDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalParamBuffers[i].globalParamBuffer.descriptor) };
			for (int id = 0; id < GBufferCount; id++)
			{
				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(globalDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 + id, &gBuffer.texture[id].descriptor));
			}
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}
	// IBL
	{
		// 更新描述符集
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(IBLDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &EnvironmentManager::Get().IBL.irradianceCube.descriptor),
			vks::initializers::writeDescriptorSet(IBLDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &EnvironmentManager::Get().IBL.prefilteredCube.descriptor),
			vks::initializers::writeDescriptorSet(IBLDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &EnvironmentManager::Get().IBL.lutBrdf.descriptor),
			vks::initializers::writeDescriptorSet(IBLDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &EnvironmentManager::Get().IBL.environmentCube.descriptor)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::InitPostProcess() 
{
	PostProcessBase::preparePostProcessBase(vulkanDevice);
	if (!postProcessManager)
	{
		postProcessManager = new PostProcessManager();
		postProcessManager->Init();
	}
}

void VulkanRenderer::render()
{
	VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];

	const int descriptorSetCount = 3;
	std::vector<VkDescriptorSet> descriptorSetsArray(descriptorSetCount);
	descriptorSetsArray[LBI_GLOBAL] = globalDescriptorSets[currentBuffer];
	descriptorSetsArray[LBI_IBL] = IBLDescriptorSet;
	descriptorSetsArray[LBI_LIGHTS] = vkLight::descriptorSet;
	//资源预处理
	{
		//绘制方向光和点光的阴影贴图	
		directLight.Render(cmdBuffer);
		pointLights.Render(cmdBuffer);

		//几何Pass，为不透明和遮罩物体生成GBuffer
		{
			VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "PBR_DEFER_GEOMETRY", { 1.0f, 1.0f, 1.0f });
			const VkViewport viewport = vks::initializers::viewport((float)m_renderWidth, (float)m_renderHeiht, 0.0f, 1.0f);
			const VkRect2D scissor = vks::initializers::rect2D(m_renderWidth, m_renderHeiht, 0, 0);
			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);


			//设置动态渲染信息
			const int colorAttachmentCount = GBufferCount;
			VkRenderingAttachmentInfo colorAttachment[colorAttachmentCount]; 
			VkClearValue clearValues;
			clearValues.color = { { 0.25f, 0.25f, 0.25f, 1.0f } };
			for (int i = 0; i < GBufferCount; i++)
			{
				colorAttachment[i] = vks::initializers::RenderingAttachmentInfo_Color(gBuffer.texture[i].view, &clearValues, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				VulkanImageUtils::TransitionImageLayout(cmdBuffer, gBuffer.texture[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);//转换布局
			}
			VkRenderingAttachmentInfo depthAttachment = vks::initializers::RenderingAttachmentInfo_Depth(depthStencil.view);
			depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
			VulkanImageUtils::TransitionImageLayout(cmdBuffer, depthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
			VkRenderingInfo renderInfo = vks::initializers::RenderingInfo({ m_renderWidth, m_renderHeiht }, colorAttachmentCount, colorAttachment, &depthAttachment);

			//开始动态渲染
			vkCmdBeginRendering(cmdBuffer, &renderInfo);
			{
				//绘制不透明物体
				vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].layout, 0, descriptorSetCount, descriptorSetsArray.data(), 0, nullptr);
				vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].pipeline);
				for (auto& [key, model] : MeshManager::Get().m_sceneTree)
				{
					model.draw(cmdBuffer, vkglTF::RenderFlags::BindMaterial | vkglTF::RenderFlags::RenderOpaqueNodes, pipelines[PL_PBR_DEFER_GEOMETRY_Opaque].layout);
				}

				//绘制遮罩物体
				vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].layout, 0, descriptorSetCount, descriptorSetsArray.data(), 0, nullptr);
				vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].pipeline);
				for (auto& [key, model] : MeshManager::Get().m_sceneTree)
				{
					model.draw(cmdBuffer, vkglTF::RenderFlags::BindMaterial | vkglTF::RenderFlags::RenderAlphaMaskedNodes, pipelines[PL_PBR_DEFER_GEOMETRY_AlphaMasked].layout);
				}
			}
			vkCmdEndRendering(cmdBuffer);
			VulkanDebugUtils::CmdEndLabel(cmdBuffer);
		}
	}

	//转换GBuffer布局
	for (int i = 0; i < GBufferCount; i++)
	{
		VulkanImageUtils::TransitionImageLayout(cmdBuffer, gBuffer.texture[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	//主渲染Pass
	{
		VkClearValue clearValues[3]{};
		clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
		clearValues[1].depthStencil = { 1.0f, 0 };
		clearValues[2].color = { {0.0f, 0.0f, 0.0f, 0.0f} };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = mainRenderPass.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_renderWidth;
		renderPassBeginInfo.renderArea.extent.height = m_renderHeiht;
		renderPassBeginInfo.clearValueCount = 3;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = mainRenderPass.frameBuffers[0];

		const VkViewport viewport = vks::initializers::viewport((float)m_renderWidth, (float)m_renderHeiht, 0.0f, 1.0f);
		const VkRect2D scissor = vks::initializers::rect2D(m_renderWidth, m_renderHeiht, 0, 0);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		VulkanImageUtils::TransitionImageLayout(cmdBuffer, depthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


		//不透明与遮罩物体的延迟渲染光照阶段
		{
			VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "PBR_DEFER_LIGHTING", { 1.0f, 1.0f, 1.0f });
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_LIGHTING].layout, 0, descriptorSetCount, descriptorSetsArray.data(), 0, nullptr);
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_DEFER_LIGHTING].pipeline);
			vkCmdDraw(cmdBuffer, 3, 1, 0, 0);//绘制全屏三角形
			VulkanDebugUtils::CmdEndLabel(cmdBuffer);
		}

		//透明物体的前向渲染
		{
			VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "PBR_AlphaBLEND", { 1.0f, 1.0f, 1.0f });
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_BLEND].layout, 0, descriptorSetCount, descriptorSetsArray.data(), 0, nullptr);
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_PBR_BLEND].pipeline);
			for (auto& [key, model] : MeshManager::Get().m_sceneTree)
			{
				model.draw(cmdBuffer, vkglTF::RenderFlags::BindMaterial | vkglTF::RenderFlags::RenderAlphaBlendedNodes, pipelines[PL_PBR_BLEND].layout);
			}
			VulkanDebugUtils::CmdEndLabel(cmdBuffer);
		}

		// Skybox
		if (displaySkybox && !camera.useOrthographic)
		{
			VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "Skybox", { 1.0f, 1.0f, 1.0f });
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_Skybox].pipeline);
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PL_Skybox].layout, 0, 2, descriptorSetsArray.data(), 0, nullptr);
			MeshManager::Get().skybox.draw(cmdBuffer);//不需要绑定材质描述符
			VulkanDebugUtils::CmdEndLabel(cmdBuffer);
		}
		vkCmdEndRenderPass(cmdBuffer);
	}
	//更新布局
	offscreenTexture[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthStencil.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	offscreenTexture[0].updateDescriptor();
	depthStencil.updateDescriptor();

	//后处理
	{
		//景深
		if(camera.enableDOF)
		{
			VulkanImageUtils::TransitionImageLayout(cmdBuffer, offscreenTexture[1], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			postProcessManager->dofProcess->excute(cmdBuffer, offscreenTexture[0].descriptor, depthStencil.descriptor, offscreenTexture[1].view);
			VulkanImageUtils::CopyImageToImage(cmdBuffer, offscreenTexture[1], offscreenTexture[0]);
		}
		//调试输出
		{
			if (showGBuffer >= 0)
			{
				VulkanImageUtils::CopyImageToImage(cmdBuffer, gBuffer.texture[showGBuffer], offscreenTexture[0]);
			}
		}
		//色调映射
		{
			VulkanImageUtils::TransitionImageLayout(cmdBuffer, offscreenTexture[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			VulkanImageUtils::TransitionImageLayout(cmdBuffer, swapChain.swapChainImages[currentImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			postProcessManager->toneMappingProcess->excute(cmdBuffer, offscreenTexture[0].descriptor, swapChain.swapChainImages[currentImageIndex].view);
		}

	}

}

bool VulkanRenderer::BeginFrame(double deltaTime)
{
	const VkResult acquireResult = VulkanRendererBase::prepareFrame();
	if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
		return false;

	frameCounter++;
	frameTimer = deltaTime;
	timer += timerSpeed * frameTimer;
	if (timer > 1.0)
	{
		timer -= 1.0f;
	}
	camera.update(frameTimer);
	PostProcessBase::UpdateResolution(m_renderWidth, m_renderHeiht);
	updateUniformBuffers();

	VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	return true;
}

void VulkanRenderer::EndFrame()
{
	VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];
	VulkanImageUtils::TransitionImageLayout(cmdBuffer, swapChain.swapChainImages[currentImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
	VulkanRendererBase::submitFrame();
}

void VulkanRenderer::DrawImGui()
{
	VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];
	VulkanDebugUtils::CmdBeginLabel(cmdBuffer, "ImGUI", { 1.0f, 1.0f, 1.0f });
	VulkanImageUtils::TransitionImageLayout(cmdBuffer, swapChain.swapChainImages[currentImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo colorAttachment = vks::initializers::RenderingAttachmentInfo_Color(swapChain.swapChainImages[currentImageIndex].view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vks::initializers::RenderingInfo({ m_renderWidth, m_renderHeiht }, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmdBuffer, &renderInfo);
	const VkViewport viewport = vks::initializers::viewport((float)m_renderWidth, (float)m_renderHeiht, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::rect2D(m_renderWidth, m_renderHeiht, 0, 0);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
	vkCmdEndRendering(cmdBuffer);
	VulkanDebugUtils::CmdEndLabel(cmdBuffer);
}

void VulkanRenderer::OnFramebufferResize(int framebufferWidth, int framebufferHeight)
{
	if (m_framebufferWidth != framebufferWidth || m_framebufferHeiht != framebufferHeight)
	{
		m_renderWidth = m_framebufferWidth = framebufferWidth;
		m_renderHeiht = m_framebufferHeiht = framebufferHeight;
		windowResize();
		UpdateDescriptorSets();
		LOG_INFO("Framebuffer resized to {} x {}", framebufferWidth, framebufferHeight);
	}
}
