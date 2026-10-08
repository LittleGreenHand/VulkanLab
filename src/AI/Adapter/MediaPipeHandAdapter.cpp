#include "MediaPipeHandAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <imgui.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>
#include "../core/Log.h"

namespace
{
	// ---- 模型常量 ----
	constexpr int kPalmInputSize = 192;
	constexpr int kHandInputSize = 224;
	constexpr int kNumAnchors = 2016;   // 24*24*2 + 12*12*6
	constexpr int kPalmValues = 18;     // 4 box + 7*2 keypoints
	constexpr int kNumKeypoints = 21;
	constexpr int kKeypointValues = kNumKeypoints * 3; // x, y, z
	constexpr int kWorldValues = kNumKeypoints * 3;

	Tensor MakeInput(const cv::Mat& bgr, int size)
	{
		cv::Mat resized, rgb;
		cv::resize(bgr, resized, cv::Size(size, size), 0, 0, cv::INTER_AREA);
		cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
		rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

		Tensor tensor;
		tensor.Shape = { 1, size, size, 3 };
		tensor.DataType = TensorDataType::Float32;
		const auto* p = rgb.ptr<float>();
		tensor.Data = std::vector<float>(p, p + rgb.total() * 3);
		return tensor;
	}

	const std::vector<float>* ReadTensor(const Tensor& tensor,
		const std::vector<std::int64_t>& shape)
	{
		const auto* values = std::get_if<std::vector<float>>(&tensor.Data);
		if (tensor.DataType != TensorDataType::Float32 ||
			tensor.Shape != shape || !values ||
			tensor.GetShapeElementCount() != std::optional<std::size_t>(values->size()))
			return nullptr;
		return values;
	}

	cv::Point2f Transform(const cv::Mat& matrix, cv::Point2f point)
	{
		return {
			static_cast<float>(matrix.at<double>(0, 0) * point.x +
							   matrix.at<double>(0, 1) * point.y +
							   matrix.at<double>(0, 2)),
			static_cast<float>(matrix.at<double>(1, 0) * point.x +
							   matrix.at<double>(1, 1) * point.y +
							   matrix.at<double>(1, 2))
		};
	}

	// 与仓库 mp_handpose.py 的两次裁剪相同：
	// 旋转前放大 4 倍，旋转后上移并放大 3 倍。
	bool CropAndPad(const cv::Mat& image, cv::Rect2f box, bool forRotation,
		cv::Mat& crop, cv::Rect& clipped, cv::Point2f& bias)
	{
		const cv::Point2f center(box.x + box.width * 0.5f,
			box.y + box.height * (forRotation ? 0.5f : 0.1f));
		const float factor = forRotation ? 4.0f : 3.0f;
		const auto clip = [](float v, int limit)
			{
				return static_cast<int>(std::clamp(v, 0.0f, static_cast<float>(limit)));
			};

		const int left = clip(center.x - box.width * factor * 0.5f, image.cols);
		const int top = clip(center.y - box.height * factor * 0.5f, image.rows);
		const int right = clip(center.x + box.width * factor * 0.5f, image.cols);
		const int bottom = clip(center.y + box.height * factor * 0.5f, image.rows);

		clipped = cv::Rect(left, top, right - left, bottom - top);
		if (clipped.empty())
			return false;

		const int side = forRotation
			? static_cast<int>(std::hypot(clipped.width, clipped.height))
			: std::max(clipped.width, clipped.height);

		const int padLeft = (side - clipped.width) / 2;
		const int padTop = (side - clipped.height) / 2;

		cv::copyMakeBorder(image(clipped), crop,
			padTop, side - clipped.height - padTop,
			padLeft, side - clipped.width - padLeft,
			cv::BORDER_CONSTANT, cv::Scalar());

		bias = cv::Point2f(static_cast<float>(left - padLeft),
			static_cast<float>(top - padTop));
		return true;
	}

	inline float Sigmoid(float x)
	{
		return 1.0f / (1.0f + std::exp(-std::clamp(x, -80.0f, 80.0f)));
	}

	inline bool AllFinite(const float* p, int n)
	{
		return std::all_of(p, p + n, [](float v) { return std::isfinite(v); });
	}
}

MediaPipeHandAdapter::MediaPipeHandAdapter(std::filesystem::path handModelPath,
	float palmThreshold, float handThreshold,
	float nmsThreshold, int maxHands)
	: m_handModelPath(std::move(handModelPath))
	, m_palmThreshold(palmThreshold)
	, m_handThreshold(handThreshold)
	, m_nmsThreshold(nmsThreshold)
	, m_maxHands(maxHands)
{
	for (float threshold : { palmThreshold, handThreshold, nmsThreshold })
		if (!std::isfinite(threshold) || threshold < 0 || threshold > 1)
			throw std::invalid_argument("MediaPipe thresholds must be in [0, 1]");
	if (maxHands < 1)
		throw std::invalid_argument("MediaPipe maxHands must be positive");
}

bool MediaPipeHandAdapter::CreateInput(const cv::Mat& frame,
	InferenceInput& destination,
	std::string& error)
{
	destination = {};
	error.clear();
	if (frame.empty() || frame.dims != 2 || frame.type() != CV_8UC3)
	{
		error = "MediaPipe input must be a non-empty CV_8UC3 BGR image";
		return false;
	}

	Tensor tensor;
	tensor.Shape = { frame.rows, frame.cols, 3 };
	tensor.DataType = TensorDataType::UInt8;

	std::vector<std::uint8_t> pixels(frame.total() * 3);
	const std::size_t rowBytes = static_cast<std::size_t>(frame.cols) * 3;
	for (int row = 0; row < frame.rows; ++row)
		std::memcpy(pixels.data() + row * rowBytes, frame.ptr(row), rowBytes);

	tensor.Data = std::move(pixels);
	destination.Tensors.push_back(std::move(tensor));
	return true;
}

bool MediaPipeHandAdapter::PreProcess(const InferenceInput& source,
	InferenceInput& destination,
	std::string& error)
{
	LOG_TIME_BEGIN(MediaPipeHandAdapter::PreProcess);
	destination = {};
	error.clear();
	m_scale = 0;
	m_frame.release();

	if (source.Tensors.size() != 1)
	{
		error = "MediaPipe expects one HWC UInt8 BGR tensor";
		return false;
	}

	const auto& tensor = source.Tensors.front();
	const auto* pixels = std::get_if<std::vector<std::uint8_t>>(&tensor.Data);
	if (tensor.DataType != TensorDataType::UInt8 || !pixels ||
		tensor.Shape.size() != 3 || tensor.Shape[2] != 3 ||
		!tensor.GetShapeElementCount() ||
		*tensor.GetShapeElementCount() != pixels->size() ||
		tensor.Shape[0] > std::numeric_limits<int>::max() ||
		tensor.Shape[1] > std::numeric_limits<int>::max())
	{
		error = "Invalid MediaPipe HWC UInt8 BGR tensor shape or storage";
		return false;
	}

	try
	{
		if (!m_handBackend.IsLoaded() && !m_handBackend.LoadModel(m_handModelPath, error))
			return false;

		const int rows = static_cast<int>(tensor.Shape[0]);
		const int cols = static_cast<int>(tensor.Shape[1]);

		// 拷贝输入到成员，后续 PostProcess 用得上。
		m_frame.create(rows, cols, CV_8UC3);
		std::memcpy(m_frame.data, pixels->data(),
			static_cast<std::size_t>(rows) * cols * 3);

		const float scale = static_cast<float>(kPalmInputSize) / std::max(cols, rows);
		const int width = std::clamp(static_cast<int>(cols * scale), 1, kPalmInputSize);
		const int height = std::clamp(static_cast<int>(rows * scale), 1, kPalmInputSize);
		m_padLeft = (kPalmInputSize - width) / 2;
		m_padTop = (kPalmInputSize - height) / 2;

		cv::Mat resized, padded;
		cv::resize(m_frame, resized, cv::Size(width, height));
		cv::copyMakeBorder(resized, padded,
			m_padTop, kPalmInputSize - height - m_padTop,
			m_padLeft, kPalmInputSize - width - m_padLeft,
			cv::BORDER_CONSTANT, cv::Scalar());

		destination.Tensors.push_back(MakeInput(padded, kPalmInputSize));
		m_scale = scale;

		LOG_TIME_END(MediaPipeHandAdapter::PreProcess, false);
		return true;
	}
	catch (const std::exception& exception)
	{
		error = exception.what();
		return false;
	}
}

bool MediaPipeHandAdapter::PostProcess(const InferenceOutput& source,
	InferenceOutput& destination,
	std::string& error)
{
	LOG_TIME_BEGIN(MediaPipeHandAdapter::PostProcess);
	const double timestamp = std::chrono::duration<double>(	std::chrono::steady_clock::now().time_since_epoch()).count();
	destination = {};
	error.clear();

	constexpr const char* kPalmOutputError =
		"Expected MediaPipe palm Float32 outputs [1, 2016, 18] and [1, 2016, 1]";
	constexpr const char* kHandOutputError =
		"Expected MediaPipe hand Float32 outputs Identity/Identity_3 [1, 63] "
		"and Identity_1/Identity_2 [1, 1]";

	if (m_scale <= 0 || m_frame.empty())
	{
		error = "MediaPipe postprocess requires a successful preprocess for the same frame";
		return false;
	}
	const float scale = std::exchange(m_scale, 0.0f);
	cv::Mat frame = std::move(m_frame);

	//查找输出张量 ----------
	if (source.Tensors.size() != 2)
	{
		error = kPalmOutputError;
		return false;
	}

	const std::vector<float>* boxes = nullptr;
	const std::vector<float>* logits = nullptr;
	for (const auto& tensor : source.Tensors)
	{
		if (const auto* v = ReadTensor(tensor, { 1, kNumAnchors, kPalmValues }))
			boxes = v;
		else if (const auto* v = ReadTensor(tensor, { 1, kNumAnchors, 1 }))
			logits = v;
	}
	if (!boxes || !logits)
	{
		error = kPalmOutputError;
		return false;
	}

	try
	{
		//// 解码所有锚点，收集候选
		//std::vector<cv::Rect2d>                 candidates;
		//std::vector<float>                      scores;
		//std::vector<std::array<cv::Point2f, 7>> landmarks;
		//candidates.reserve(64);
		//scores.reserve(64);
		//landmarks.reserve(64);

		// ============ 预计算：把能提到循环外的全部提出来 ============
		// sigmoid(x) < m_palmThreshold  <==>  x < logit(m_palmThreshold)
		// 这样每个锚点只需一次浮点比较，而不是一次 std::exp。
		const float clampedPalm = std::clamp(m_palmThreshold, 1e-6f, 1.0f - 1e-6f);
		const float logitThreshold = std::log(clampedPalm / (1.0f - clampedPalm));
		const float invScale = 1.0f / scale;

		// 每个线程本地累积结果，避免锁竞争。
		struct LocalSink
		{
			std::vector<cv::Rect2d>                 boxes;
			std::vector<float>                      scores;
			std::vector<std::array<cv::Point2f, 7>> landmarks;
		};
		tbb::combinable<LocalSink> sink;

		// ============ 并行遍历 2016 个锚点 ============
		// 索引布局：
		//   [0, 1152)     -> 24x24 网格，每格 2 个锚点
		//   [1152, 2016)  -> 12x12 网格，每格 6 个锚点
		tbb::parallel_for(0, kNumAnchors, [&](int index)
			{
				int grid, row, col;
				if (index < 1152)
				{
					const int i = index >> 1;      // 除以 2
					grid = 24;
					row = i / 24;
					col = i % 24;
				}
				else
				{
					const int i = (index - 1152) / 6;
					grid = 12;
					row = i / 12;
					col = i % 12;
				}

				// 该锚点相对特征图中心的偏移（映射回原图坐标后）
				const float cell = static_cast<float>(kPalmInputSize) / grid;
				const float anchorOffsetX = ((col + 0.5f) * cell - m_padLeft) * invScale;
				const float anchorOffsetY = ((row + 0.5f) * cell - m_padTop) * invScale;

				const float* data = boxes->data() + index * kPalmValues;
				const float  logit = (*logits)[index];

				// -------- 廉价过滤优先 --------
				if (!std::isfinite(logit) || logit < logitThreshold) return;
				if (data[2] <= 0 || data[3] <= 0)                    return;
				// 昂贵的 18 元素有限性检查放到最后
				if (!AllFinite(data, kPalmValues))                   return;

				const float score = Sigmoid(logit);

				// 解码 + 乘法代替除法
				const float cx = data[0] * invScale + anchorOffsetX;
				const float cy = data[1] * invScale + anchorOffsetY;
				const float w = data[2] * invScale;
				const float h = data[3] * invScale;

				std::array<cv::Point2f, 7> pts;
				for (int p = 0; p < 7; ++p)
				{
					pts[p] = { data[4 + p * 2] * invScale + anchorOffsetX,
							   data[5 + p * 2] * invScale + anchorOffsetY };
				}

				auto& local = sink.local();
				local.boxes.emplace_back(cx - w * 0.5f, cy - h * 0.5f, w, h);
				local.scores.push_back(score);
				local.landmarks.push_back(pts);
			});

		// ============ 合并各线程结果 ============
		std::vector<cv::Rect2d>                 candidates;
		std::vector<float>                      scores;
		std::vector<std::array<cv::Point2f, 7>> landmarks;
		sink.combine_each([&](const LocalSink& local)
			{
				candidates.insert(candidates.end(), local.boxes.begin(), local.boxes.end());
				scores.insert(scores.end(), local.scores.begin(), local.scores.end());
				landmarks.insert(landmarks.end(), local.landmarks.begin(), local.landmarks.end());
			});

		std::vector<int> keep;
		cv::dnn::NMSBoxes(candidates, scores, m_palmThreshold, m_nmsThreshold, keep);

		// 逐手掌精定位
		HandPoseResult result;
		result.ImageWidth = frame.cols;
		result.ImageHeight = frame.rows;
		result.Hands.reserve(std::min<std::size_t>(keep.size(), m_maxHands));
		cv::Mat     crop, rotated, handImage;
		cv::Rect    clipped, handBox;
		cv::Point2f bias, handBias;
		for (int idx : keep)
		{
			if (result.Hands.size() >= static_cast<std::size_t>(m_maxHands))
				break;

			if (!CropAndPad(frame, candidates[idx], true, crop, clipped, bias))
				continue;

			// 用 0 号和 2 号关键点确定手掌朝向
			const auto& pts = landmarks[idx];
			const auto  dir = pts[2] - pts[0];
			const double angle = 90.0 - std::atan2(-dir.y, dir.x) * 180.0 / CV_PI;

			const cv::Point2f center(clipped.x + clipped.width * 0.5f - bias.x,
				clipped.y + clipped.height * 0.5f - bias.y);
			const cv::Mat rotation = cv::getRotationMatrix2D(center, angle, 1.0);
			cv::warpAffine(crop, rotated, rotation, crop.size());

			// 关键点在旋转坐标系下的包围盒
			cv::Point2f lo(std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max());
			cv::Point2f hi(-lo.x, -lo.y);
			for (auto p : pts)
			{
				p = Transform(rotation, p - bias);
				lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y);
				hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y);
			}

			cv::Mat     handImage;
			cv::Rect    handBox;
			cv::Point2f handBias;
			if (!CropAndPad(rotated, cv::Rect2f(lo, hi), false, handImage, handBox, handBias))
				continue;

			// 手部关键点模型
			InferenceInput handInput;
			handInput.Tensors.push_back(MakeInput(handImage, kHandInputSize));

			InferenceOutput handOutput;
			LOG_TIME_BEGIN(handpose_estimation_mediapipe);
			if (!m_handBackend.Run(handInput, handOutput, error))
				return false;
			LOG_TIME_END(handpose_estimation_mediapipe, false);
			if (handOutput.Tensors.size() != 4)
			{
				error = kHandOutputError;
				return false;
			}

			const std::vector<float>* keypoints = nullptr;
			const std::vector<float>* confidence = nullptr;
			const std::vector<float>* handedness = nullptr;
			const std::vector<float>* world = nullptr;
			for (const auto& t : handOutput.Tensors)
			{
				if (t.Name == "Identity")   keypoints = ReadTensor(t, { 1, kKeypointValues });
				else if (t.Name == "Identity_1") confidence = ReadTensor(t, { 1, 1 });
				else if (t.Name == "Identity_2") handedness = ReadTensor(t, { 1, 1 });
				else if (t.Name == "Identity_3") world = ReadTensor(t, { 1, kWorldValues });
			}
			if (!keypoints || !confidence || !handedness || !world)
			{
				error = kHandOutputError;
				return false;
			}

			const float conf = (*confidence)[0];
			const float handed = (*handedness)[0];
			if (!std::isfinite(conf) || conf < m_handThreshold || conf   > 1 ||
				!std::isfinite(handed) || handed < 0 || handed > 1 ||
				!AllFinite(keypoints->data(), kKeypointValues) ||
				!AllFinite(world->data(), kWorldValues))
				continue;

			// 关键点映射回原图
			cv::Mat inverse;
			cv::invertAffineTransform(rotation, inverse);
			const float handScale = static_cast<float>(handImage.cols) / kHandInputSize;

			// 只对世界坐标做旋转（不含平移）
			const auto rotateWorld = [&inverse](float x, float y) -> std::array<float, 2>
				{
					return {
						static_cast<float>(inverse.at<double>(0, 0) * x + inverse.at<double>(0, 1) * y),
						static_cast<float>(inverse.at<double>(1, 0) * x + inverse.at<double>(1, 1) * y)
					};
				};

			const auto& kp = *keypoints;
			const auto& wp = *world;

			HandPose hand;
			hand.Confidence = conf;
			hand.RightHandProbability = handed;
			hand.HasWorldKeypoints = true;

			cv::Point2f lo2(std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max());
			cv::Point2f hi2(-lo2.x, -lo2.y);

			for (std::size_t p = 0; p < hand.Keypoints.size(); ++p)
			{
				const cv::Point2f local(kp[p * 3] * handScale + handBias.x,
					kp[p * 3 + 1] * handScale + handBias.y);
				const cv::Point2f pos = Transform(inverse, local) + bias;

				hand.Keypoints[p] = { pos.x, pos.y, conf };
				hand.Keypoints[p].Z = kp[p * 3 + 2] * handScale;

				const auto worldRot = rotateWorld(wp[p * 3], wp[p * 3 + 1]);
				hand.WorldKeypoints[p] = { worldRot[0], worldRot[1], wp[p * 3 + 2] };

				lo2.x = std::min(lo2.x, pos.x); lo2.y = std::min(lo2.y, pos.y);
				hi2.x = std::max(hi2.x, pos.x); hi2.y = std::max(hi2.y, pos.y);
			}

			const float w = hi2.x - lo2.x;
			const float h = hi2.y - lo2.y;
			hand.X = std::clamp(lo2.x - w * 0.325f, 0.0f, static_cast<float>(frame.cols));
			hand.Y = std::clamp(lo2.y - h * 0.425f, 0.0f, static_cast<float>(frame.rows));
			hand.Width = std::clamp(hi2.x + w * 0.325f, 0.0f, static_cast<float>(frame.cols)) - hand.X;
			hand.Height = std::clamp(hi2.y + h * 0.225f, 0.0f, static_cast<float>(frame.rows)) - hand.Y;

			if (hand.Width > 0 && hand.Height > 0)
				result.Hands.push_back(std::move(hand));
		}
		if (m_filterEnabled && !result.Hands.empty())
			ApplyTemporalFilter(result.Hands, timestamp);
		destination.HandPoses = std::move(result);
		LOG_TIME_END(MediaPipeHandAdapter::PostProcess, false);
		//LOG_DEBUG("MediaPipe detected {} hands", destination.HandPoses->Hands.size());
		return true;
	}
	catch (const std::exception& e)
	{
		error = e.what();
		return false;
	}
}

void MediaPipeHandAdapter::ApplyTemporalFilter(std::vector<HandPose>& hands,
	double timestamp)
{
	// 移除长时间未出现的 track
	m_tracks.erase(
		std::remove_if(m_tracks.begin(), m_tracks.end(),
			[&](const HandFilterTrack& t)
			{
				return timestamp - t.LastSeen > m_trackMaxAge;
			}),
		m_tracks.end());

	for (auto& t : m_tracks)
		t.Configure(m_filterMinCutoff, m_filterBeta, m_filterDCutoff);
	for (auto& t : m_tracks)
		t.Matched = false;

	// 贪心最近邻匹配
	std::vector<int> assigned(hands.size(), -1);
	for (std::size_t i = 0; i < hands.size(); ++i)
	{
		const cv::Point2f center(hands[i].X + hands[i].Width * 0.5f,
			hands[i].Y + hands[i].Height * 0.5f);
		float bestDist = m_trackMatchDist;
		int   bestIdx = -1;
		for (std::size_t t = 0; t < m_tracks.size(); ++t)
		{
			if (m_tracks[t].Matched) continue;
			const float d = cv::norm(center - m_tracks[t].Center);
			if (d < bestDist)
			{
				bestDist = d;
				bestIdx = static_cast<int>(t);
			}
		}
		if (bestIdx >= 0)
		{
			assigned[i] = bestIdx;
			m_tracks[bestIdx].Matched = true;
		}
	}

	// 应用 OEF
	for (std::size_t i = 0; i < hands.size(); ++i)
	{
		HandFilterTrack* track = nullptr;
		if (assigned[i] >= 0)
		{
			track = &m_tracks[assigned[i]];
		}
		else
		{
			m_tracks.emplace_back();
			track = &m_tracks.back();
			track->Configure(m_filterMinCutoff, m_filterBeta, m_filterDCutoff);
		}

		HandPose& hand = hands[i];

		// 包围盒
		hand.X = track->Box[0].Filter(hand.X, timestamp);
		hand.Y = track->Box[1].Filter(hand.Y, timestamp);
		hand.Width = track->Box[2].Filter(hand.Width, timestamp);
		hand.Height = track->Box[3].Filter(hand.Height, timestamp);

		for (std::size_t p = 0; p < hand.Keypoints.size(); ++p)
		{
			auto& kp = hand.Keypoints[p];
			kp.X = track->Keypoint[p * 3 + 0].Filter(kp.X, timestamp);
			kp.Y = track->Keypoint[p * 3 + 1].Filter(kp.Y, timestamp);
			kp.Z = track->Keypoint[p * 3 + 2].Filter(kp.Z, timestamp);
		}

		for (std::size_t p = 0; p < hand.WorldKeypoints.size(); ++p)
		{
			auto& wp = hand.WorldKeypoints[p];
			wp[0] = track->World[p * 3 + 0].Filter(wp[0], timestamp);
			wp[1] = track->World[p * 3 + 1].Filter(wp[1], timestamp);
			wp[2] = track->World[p * 3 + 2].Filter(wp[2], timestamp);
		}

		track->Center = { hand.X + hand.Width * 0.5f, hand.Y + hand.Height * 0.5f };
		track->LastSeen = timestamp;
	}
}

void MediaPipeHandAdapter::DrawUI()
{
	ImGui::DragFloat("Palm confidence", &m_palmThreshold, 0.001f, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Hand confidence", &m_handThreshold, 0.001f, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Palm NMS", &m_nmsThreshold, 0.001f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragInt("Max hands", &m_maxHands, 1, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SeparatorText("One Euro Filter");
	ImGui::Checkbox("Enable", &m_filterEnabled);
	ImGui::BeginDisabled(!m_filterEnabled);
	ImGui::DragFloat("Min cutoff (Hz)", &m_filterMinCutoff, 0.01f, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Beta", &m_filterBeta, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Derivative cutoff (Hz)", &m_filterDCutoff, 0.01f, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Track max age (s)", &m_trackMaxAge, 0.01f, 0.05f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Track match dist (px)", &m_trackMatchDist, 1.0f, 10.0f, 2000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndDisabled();

	ImGui::TextDisabled("Hand landmark backend: ONNX Runtime");
}