#pragma once
#include "InferenceBackend.h"
#include <opencv2/dnn.hpp>

class OpenCVDNNBackend final : public IInferenceBackend
{
public:
	bool LoadModel(const std::filesystem::path& modelPath, std::string& error) override;
	void UnloadModel() override;
	[[nodiscard]] bool IsLoaded() const override;
	bool Run(const InferenceInput& input, InferenceOutput& output, std::string& error) override;

	[[nodiscard]] InferenceBackendType GetType() const override { return InferenceBackendType::OpenCVDNN; }
	[[nodiscard]] const char* GetName() const override { return "OpenCV DNN"; }
	[[nodiscard]] const std::vector<TensorInfo>& GetInputInfos() const override { return m_inputInfos; }
	[[nodiscard]] const std::vector<TensorInfo>& GetOutputInfos() const override { return m_outputInfos; }

private:
	cv::dnn::Net m_net;
	std::vector<TensorInfo> m_inputInfos;
	std::vector<TensorInfo> m_outputInfos;
};
