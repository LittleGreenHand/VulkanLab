#include "AIModelManager.h"
#include "../Core/Log.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

bool IsSupportFormat(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char value)
		{
			return static_cast<char>(std::tolower(value));
		});
	
	return extension == ".onnx";
}

ModelFormat GetModelFormat(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char value)
		{
			return static_cast<char>(std::tolower(value));
		});

	if (extension == ".onnx")
	{
		return ModelFormat::ONNX;
	}

	return ModelFormat::Unknown;
}

std::string MakeUniqueName(
	const std::filesystem::path& path,
	const std::filesystem::path& root,
	const std::unordered_map<std::string, std::size_t>& stemCounts,
	std::unordered_set<std::string>& usedNames)
{
	const std::string stem = path.stem().string();
	std::string candidate = stem;

	const auto count = stemCounts.find(stem);
	if (count != stemCounts.end() && count->second > 1)
	{
		std::error_code error;
		std::filesystem::path relative = std::filesystem::relative(path.parent_path(), root, error);
		if (!error && !relative.empty() && relative != ".")
			candidate += " [" + relative.generic_string() + "]";
	}

	const std::string base = candidate;
	std::size_t suffix = 2;
	while (!usedNames.insert(candidate).second)
		candidate = base + " #" + std::to_string(suffix++);
	return candidate;
}


bool AIModelManager::Initialize(const std::filesystem::path& modelRoot)
{
	Shutdown();
	m_modelRoot = modelRoot.lexically_normal();
	m_initialized = true;
	ScanModels();
	return true;
}

void AIModelManager::Shutdown()
{
	if (m_initialized)
	{
		for (const std::unique_ptr<AIModel>& model : m_models)
			model->SetEnabled(false);
		m_models.clear();
		m_modelRoot.clear();
		m_initialized = false;
	}
}

void AIModelManager::ScanModels()
{
	m_models.clear();
	if (!m_initialized)
		return;

	std::error_code error;
	if (!std::filesystem::is_directory(m_modelRoot, error))
	{
		LOG_WARNING("Model directory is unavailable: {}", m_modelRoot.string());
		return;
	}

	std::vector<std::filesystem::path> paths;
	std::filesystem::recursive_directory_iterator iterator(m_modelRoot, std::filesystem::directory_options::skip_permission_denied, error);
	const std::filesystem::recursive_directory_iterator end;

	while (!error && iterator != end)
	{
		if (iterator->is_regular_file(error) && !error && IsSupportFormat(iterator->path()))
			paths.push_back(iterator->path().lexically_normal());
		iterator.increment(error);
	}

	if (error)
		LOG_WARNING("Model scan was incomplete: {}", error.message());

	std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right)
		{
			return left.generic_string() < right.generic_string();
		});

	std::unordered_map<std::string, std::size_t> stemCounts;
	for (const auto& path : paths)
		++stemCounts[path.stem().string()];

	std::unordered_set<std::string> usedNames;
	m_models.reserve(paths.size());
	for (const auto& path : paths)
	{
		ModelInfo info;
		info.Name = MakeUniqueName(path, m_modelRoot, stemCounts, usedNames);
		info.Path = path;
		info.Format = GetModelFormat(path);
		LOG_INFO("Found model: {}", info.Name);
		m_models.push_back(std::make_unique<AIModel>(std::move(info)));
	}

	if (m_models.empty())
		LOG_INFO("No ONNX models found in: {}", m_modelRoot.string());
}

AIModel* AIModelManager::FindModel(const std::string& name)
{
	const auto found = std::find_if(m_models.begin(), m_models.end(),
		[&name](const auto& model)
		{
			return model->GetInfo().Name == name;
		});
	return found == m_models.end() ? nullptr : found->get();
}

const AIModel* AIModelManager::FindModel(const std::string& name) const
{
	const auto found = std::find_if(m_models.begin(), m_models.end(),
		[&name](const auto& model)
		{
			return model->GetInfo().Name == name;
		});
	return found == m_models.end() ? nullptr : found->get();
}
