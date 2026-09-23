#pragma once

#include "InferenceBackend.h"

#include <onnxruntime_cxx_api.h>

#include <memory>

class ONNXRuntimeBackend final : public IInferenceBackend
{
public:
	ONNXRuntimeBackend();
	~ONNXRuntimeBackend() override;

	bool LoadModel(const std::filesystem::path& modelPath, std::string& error) override;
	void UnloadModel() override;
	[[nodiscard]] bool IsLoaded() const override;
	bool Run(const InferenceInput& input, InferenceOutput& output, std::string& error) override;

	[[nodiscard]] InferenceBackendType GetType() const override { return InferenceBackendType::ONNXRuntime; }
	[[nodiscard]] const char* GetName() const override { return "ONNX Runtime"; }
	[[nodiscard]] const std::vector<TensorInfo>& GetInputInfos() const override { return m_inputInfos; }
	[[nodiscard]] const std::vector<TensorInfo>& GetOutputInfos() const override { return m_outputInfos; }

public:
	static Ort::Env& GetEnv();
private:
	Ort::SessionOptions m_sessionOptions;
	std::unique_ptr<Ort::Session> m_session;
	std::vector<TensorInfo> m_inputInfos;
	std::vector<TensorInfo> m_outputInfos;
	std::vector<std::string> m_inputNames;
	std::vector<std::string> m_outputNames;
};
