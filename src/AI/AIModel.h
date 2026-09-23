#pragma once

#include "InferenceBackend.h"
#include "ModelInfo.h"

#include <memory>
#include <optional>
#include <string>

class IModelAdapter
{
public:
	virtual ~IModelAdapter() = default;

	virtual bool PreProcess(
		const InferenceInput& source,
		InferenceInput& destination,
		std::string& error) = 0;
	virtual bool PostProcess(
		const InferenceOutput& source,
		InferenceOutput& destination,
		std::string& error) = 0;
};

class AIModel
{
public:
	explicit AIModel(ModelInfo info);
	~AIModel();

	AIModel(const AIModel&) = delete;
	AIModel& operator=(const AIModel&) = delete;
	AIModel(AIModel&&) noexcept;
	AIModel& operator=(AIModel&&) noexcept;

	bool Load();
	void Unload();
	bool IsLoaded() const;

	bool SetEnabled(bool enabled);
	bool IsEnabled() const { return m_enabled; }

	bool SetBackend(InferenceBackendType backendType);
	InferenceBackendType GetBackendType() const { return m_info.BackendType; }
	const char* GetBackendName() const;

	bool Run(const InferenceInput& input, InferenceOutput& output);
	void SetAdapter(std::unique_ptr<IModelAdapter> adapter);

	const ModelInfo& GetInfo() const { return m_info; }
	ModelState GetState() const { return m_state; }
	const std::string& GetLastError() const { return m_lastError; }
	std::optional<double> GetLastInferenceTimeMs() const { return m_lastInferenceTimeMs; }
	const std::vector<TensorInfo>& GetInputInfos() const;
	const std::vector<TensorInfo>& GetOutputInfos() const;

private:
	static std::unique_ptr<IInferenceBackend> CreateBackend(InferenceBackendType type);
	void SetError(std::string error);

	ModelInfo m_info;
	ModelState m_state = ModelState::Disabled;
	bool m_enabled = false;
	std::unique_ptr<IInferenceBackend> m_backend;
	std::unique_ptr<IModelAdapter> m_adapter;
	std::string m_lastError;
	std::optional<double> m_lastInferenceTimeMs;
};
