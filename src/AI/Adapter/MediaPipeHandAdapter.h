#pragma once

#include "IModelAdapter.h"
#include "../Backend/ONNXRuntimeBackend.h"
#include <opencv2/core.hpp>

// 挂载到 palm_detection_mediapipe 模型；关键点阶段使用独立的 ONNX Runtime 会话。
// 一个实例按顺序处理一帧，不支持并发调用 PreProcess/PostProcess。
class MediaPipeHandAdapter : public IModelAdapter
{
public:
	explicit MediaPipeHandAdapter(std::filesystem::path handModelPath, float palmThreshold = 0.5f, float handThreshold = 0.8f, float nmsThreshold = 0.3f, int maxHands = 1);
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

private:
	
	class OneEuroFilter
	{
	public:
		void Configure(float minCutoff, float beta, float dCutoff)
		{
			m_minCutoff = minCutoff;
			m_beta = beta;
			m_dCutoff = dCutoff;
		}

		void Reset() { m_initialized = false; }

		float Filter(float value, double timestamp)
		{
			if (!m_initialized)
			{
				m_initialized = true;
				m_prevTimestamp = timestamp;
				m_prevValue = value;
				m_prevDerivative = 0.0f;
				return value;
			}

			const double dt = timestamp - m_prevTimestamp;
			if (dt <= 0.0)
				return m_prevValue;

			const float rawDerivative = (value - m_prevValue) / static_cast<float>(dt);
			const float alphaD = Alpha(m_dCutoff, dt);
			const float derivative = alphaD * rawDerivative +
				(1.0f - alphaD) * m_prevDerivative;

			const float cutoff = m_minCutoff + m_beta * std::abs(derivative);
			const float alpha = Alpha(cutoff, dt);
			const float filtered = alpha * value + (1.0f - alpha) * m_prevValue;

			m_prevValue = filtered;
			m_prevDerivative = derivative;
			m_prevTimestamp = timestamp;
			return filtered;
		}

	private:
		static float Alpha(float cutoff, double dt)
		{
			const double tau = 1.0 / (2.0 * 3.14159265358979323846 *
				std::max(cutoff, 1e-4f));
			return static_cast<float>(1.0 / (1.0 + tau / dt));
		}

		float  m_minCutoff = 1.0f;
		float  m_beta = 0.05f;
		float  m_dCutoff = 1.0f;
		float  m_prevValue = 0.0f;
		float  m_prevDerivative = 0.0f;
		double m_prevTimestamp = 0.0;
		bool   m_initialized = false;
	};

	// 单只手的滤波器状态（跨帧保留）
	struct HandFilterTrack
	{		
		std::array<OneEuroFilter, 21 * 3> Keypoint;// 每只手 21 个关键点，每个点 3 个通道（x, y, z），交错存放
		std::array<OneEuroFilter, 21 * 3> World;		
		std::array<OneEuroFilter, 4> Box;// 包围盒：X, Y, W, H

		cv::Point2f Center{ 0.0f, 0.0f };
		double LastSeen = 0.0;
		bool Matched = false;// 本帧是否被匹配到

		void Configure(float minCutoff, float beta, float dCutoff)
		{
			for (auto& f : Keypoint) f.Configure(minCutoff, beta, dCutoff);
			for (auto& f : World)    f.Configure(minCutoff, beta, dCutoff);
			for (auto& f : Box)      f.Configure(minCutoff, beta, dCutoff);
		}
	};

	std::vector<HandFilterTrack> m_tracks;
	bool  m_filterEnabled = true;
	float m_filterMinCutoff = 3.0f;    // Hz，越小越平滑
	float m_filterBeta = 0.2f;   // 越大快速运动越跟手
	float m_filterDCutoff = 1.0f;    // Hz，导数滤波截止
	float m_trackMaxAge = 0.5f;    // 秒，超过则丢弃 track
	float m_trackMatchDist = 250.0f;  // 像素，最近邻匹配的最大距离

	// 对最终 hands 结果做 OEF 平滑，并更新跨帧追踪状态
	void ApplyTemporalFilter(std::vector<HandPose>& hands, double timestamp);
};
