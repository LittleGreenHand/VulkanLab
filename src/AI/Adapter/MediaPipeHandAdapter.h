#pragma once

#include "IModelAdapter.h"
#include "../Backend/ONNXRuntimeBackend.h"
#include <opencv2/core.hpp>

// 挂载到 palm_detection_mediapipe 模型；关键点阶段使用独立的 ONNX Runtime 会话。
// 一个实例按顺序处理一帧，不支持并发调用 PreProcess/PostProcess。
class MediaPipeHandAdapter : public IModelAdapter
{
public:
	explicit MediaPipeHandAdapter(std::filesystem::path handModelPath, float palmThreshold = 0.5f, float handThreshold = 0.8f, float nmsThreshold = 0.3f, int maxHands = 2);
	static bool CreateInput(const cv::Mat& frame, InferenceInput& destination, std::string& error);
	bool PreProcess(const InferenceInput& source, InferenceInput& destination, std::string& error) override;
	bool PostProcess(const InferenceOutput& source, InferenceOutput& destination, std::string& error) override;
	void DrawUI() override;

private:
	std::filesystem::path m_handModelPath;
	ONNXRuntimeBackend m_handBackend;
	cv::Mat m_frame;
	float m_palmThreshold;
	float m_handThreshold;
	float m_nmsThreshold;
	int m_maxHands;
	float m_scale = 0;
	int m_padLeft = 0;
	int m_padTop = 0;
};
