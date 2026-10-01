#include "USDLoader.h"

#include "../Core/Log.h"
#include <pxr/usd/usd/stage.h>

bool USDLoader::Load(const std::string& filePath)
{
	pxr::UsdStageRefPtr m_stage;
	m_stage = pxr::UsdStage::Open(filePath, pxr::UsdStage::LoadAll);

	if (!m_stage)
	{
		LOG_ERROR("[USD] Failed to load File: {}", filePath);
		return false;
	}

	LOG_INFO("[USD] loaded File: {}", filePath);
	return true;
}

void USDLoader::Unload()
{

}
