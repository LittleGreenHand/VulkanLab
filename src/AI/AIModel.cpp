#include "AIModel.h"
#include "Backend/ONNXRuntimeBackend.h"
#include "Backend/OpenCVDNNBackend.h"
#include "../Core/Log.h"

#include <chrono>
#include <utility>

const std::vector<TensorInfo> g_emptyTensorInfos;

AIModel::AIModel(ModelInfo info)
	: m_info(std::move(info))
{}

AIModel::~AIModel() = default;
AIModel::AIModel(AIModel&&) noexcept = default;
AIModel& AIModel::operator=(AIModel&&) noexcept = default;

bool AIModel::Load()
{
	if (IsLoaded())
		return true;

	LOG_INFO("Loading model: {} (backend: {})", m_info.Name, ToString(m_info.BackendType));

	std::unique_ptr<IInferenceBackend> candidate = CreateBackend(m_info.BackendType);
	std::string error;
	if (!candidate || !candidate->LoadModel(m_info.Path, error))
	{
		SetError(error.empty() ? "Unable to create inference backend" : std::move(error));
		LOG_ERROR("Failed to load model '{}': {}", m_info.Name, m_lastError);
		return false;
	}

	m_backend = std::move(candidate);
	m_state = ModelState::Loaded;
	m_lastError.clear();
	LOG_INFO("Model loaded: {}", m_info.Name);
	return true;
}

void AIModel::Unload()
{
	if (m_backend)
		m_backend->UnloadModel();
	m_backend.reset();
	m_lastInferenceTimeMs.reset();
	m_state = m_enabled ? ModelState::Unloaded : ModelState::Disabled;
}

bool AIModel::IsLoaded() const
{
	return m_backend && m_backend->IsLoaded();
}

bool AIModel::SetEnabled(bool enabled)
{
	if (enabled == m_enabled)
	{
		if (enabled)
			return IsLoaded() || Load();

		if (IsLoaded())
			Unload();
		m_state = ModelState::Disabled;
		m_lastError.clear();
		return true;
	}

	m_enabled = enabled;
	if (!enabled)
	{
		Unload();
		m_lastError.clear();
		return true;
	}

	m_state = ModelState::Unloaded;
	return Load();
}

bool AIModel::SetBackend(InferenceBackendType backendType)
{
	if (backendType == m_info.BackendType)
		return true;

	LOG_INFO("Switching backend for '{}': {} -> {}",
		m_info.Name, ToString(m_info.BackendType), ToString(backendType));

	if (!m_enabled)
	{
		Unload();
		m_info.BackendType = backendType;
		m_state = ModelState::Disabled;
		m_lastError.clear();
		return true;
	}

	std::unique_ptr<IInferenceBackend> candidate = CreateBackend(backendType);
	std::string error;
	if (!candidate || !candidate->LoadModel(m_info.Path, error))
	{
		m_lastError = error.empty() ? "Unable to create inference backend" : std::move(error);
		LOG_ERROR("Failed to switch backend for '{}': {}", m_info.Name, m_lastError);
		return false;
	}

	if (m_backend)
		m_backend->UnloadModel();
	m_backend = std::move(candidate);
	m_info.BackendType = backendType;
	m_state = ModelState::Loaded;
	m_lastError.clear();
	return true;
}

const char* AIModel::GetBackendName() const
{
	return m_backend ? m_backend->GetName() : ToString(m_info.BackendType);
}

bool AIModel::Run(const InferenceInput& input, InferenceOutput& output)
{
	output.Tensors.clear();
	if (!m_enabled)
	{
		m_lastError = "Model is disabled";
		m_state = ModelState::Disabled;
		return false;
	}
	if (!IsLoaded() && !Load())
		return false;

	const auto begin = std::chrono::steady_clock::now();
	InferenceInput processedInput;
	const InferenceInput* backendInput = &input;
	std::string error;

	if (m_adapter)
	{
		if (!m_adapter->PreProcess(input, processedInput, error))
		{
			SetError(std::move(error));
			return false;
		}
		backendInput = &processedInput;
	}

	InferenceOutput backendOutput;
	if (!m_backend->Run(*backendInput, backendOutput, error))
	{
		SetError(std::move(error));
		LOG_ERROR("Inference failed for '{}': {}", m_info.Name, m_lastError);
		return false;
	}

	if (m_adapter)
	{
		if (!m_adapter->PostProcess(backendOutput, output, error))
		{
			SetError(std::move(error));
			return false;
		}
	}
	else
	{
		output = std::move(backendOutput);
	}

	const auto end = std::chrono::steady_clock::now();
	m_lastInferenceTimeMs = std::chrono::duration<double, std::milli>(end - begin).count();
	m_state = ModelState::Loaded;
	m_lastError.clear();
	return true;
}

void AIModel::SetAdapter(std::unique_ptr<IModelAdapter> adapter)
{
	m_adapter = std::move(adapter);
}

const std::vector<TensorInfo>& AIModel::GetInputInfos() const
{
	return m_backend ? m_backend->GetInputInfos() : g_emptyTensorInfos;
}

const std::vector<TensorInfo>& AIModel::GetOutputInfos() const
{
	return m_backend ? m_backend->GetOutputInfos() : g_emptyTensorInfos;
}

std::unique_ptr<IInferenceBackend> AIModel::CreateBackend(InferenceBackendType type)
{
	switch (type)
	{
	case InferenceBackendType::OpenCVDNN:
		return std::make_unique<OpenCVDNNBackend>();
	case InferenceBackendType::ONNXRuntime:
		return std::make_unique<ONNXRuntimeBackend>();
	}
	return nullptr;
}

void AIModel::SetError(std::string error)
{
	m_lastError = error.empty() ? "Unknown inference error" : std::move(error);
	m_state = ModelState::Error;
}
