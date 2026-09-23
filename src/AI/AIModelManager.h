#pragma once

#include "AIModel.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class AIModelManager
{
public:
	static AIModelManager& Get()
	{
		static AIModelManager instance;
		return instance;
	}

	AIModelManager() = default;
	~AIModelManager() { Shutdown(); }
	AIModelManager(const AIModelManager&) = delete;
	AIModelManager& operator=(const AIModelManager&) = delete;
	AIModelManager(AIModelManager&&) = delete;
	AIModelManager& operator=(AIModelManager&&) = delete;
public:
	bool Initialize(const std::filesystem::path& modelRoot);
	void Shutdown();
	void ScanModels();

	const std::vector<std::unique_ptr<AIModel>>& GetModels() const { return m_models; }
	AIModel* FindModel(const std::string& name);
	const AIModel* FindModel(const std::string& name) const;
	const std::filesystem::path& GetModelRoot() const { return m_modelRoot; }
	bool IsInitialized() const { return m_initialized; }

private:
	std::filesystem::path m_modelRoot;
	std::vector<std::unique_ptr<AIModel>> m_models;
	bool m_initialized = false;
};
