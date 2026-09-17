#include "RenderBase/VulkanRendererBase.h"
#include "Core/Application.h"
#include <iostream>

int main(int argc, char** argv)
{
	for (size_t i = 0; i < argc; i++) { VulkanRendererBase::args.push_back(argv[i]); };
	VulkanRendererBase::args.push_back("--validation");
	VulkanRendererBase::args.push_back("--vsync");
	VulkanRendererBase::args.push_back("--width");
	VulkanRendererBase::args.push_back("2560");
	VulkanRendererBase::args.push_back("--height");
	VulkanRendererBase::args.push_back("1440");
	VulkanRendererBase::args.push_back("--resourcepath");
	VulkanRendererBase::args.push_back(ENGINE_SOURCE_DIR);
	VulkanRendererBase::args.push_back("--shadersspvpath");
	VulkanRendererBase::args.push_back(SHADERS_SPV_DIR);

	auto& application = Application::GetInstance();
	if (application.Init())
	{		
		application.Run();
	}
	else
	{
		std::cerr << "Failed to initialize application!\n";
		application.Destroy();
		return 1;
	}
	application.Destroy();
	return 0;
}
