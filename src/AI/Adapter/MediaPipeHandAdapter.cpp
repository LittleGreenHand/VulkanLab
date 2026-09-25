#include "MediaPipeHandAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <imgui.h>

namespace
{
	Tensor MakeInput(const cv::Mat& bgr, int size)
	{
		cv::Mat resized, rgb;
		cv::resize(bgr, resized, cv::Size(size, size), 0, 0, cv::INTER_AREA);
		cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
		rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);
		Tensor tensor;
		tensor.Shape = { 1, size, size, 3 };
		tensor.DataType = TensorDataType::Float32;
		tensor.Data = std::vector<float>(rgb.ptr<float>(), rgb.ptr<float>() + rgb.total() * 3);
		return tensor;
	}

	const std::vector<float>* ReadTensor(const Tensor& tensor, const std::vector<std::int64_t>& shape)
	{
		const auto* values = std::get_if<std::vector<float>>(&tensor.Data);
		if (tensor.DataType != TensorDataType::Float32 || tensor.Shape != shape || !values ||
			tensor.GetShapeElementCount() != std::optional<std::size_t>(values->size()))
			return nullptr;
		return values;
	}

	cv::Point2f Transform(const cv::Mat& matrix, cv::Point2f point)
	{
		return { static_cast<float>(matrix.at<double>(0, 0) * point.x + matrix.at<double>(0, 1) * point.y + matrix.at<double>(0, 2)),
			static_cast<float>(matrix.at<double>(1, 0) * point.x + matrix.at<double>(1, 1) * point.y + matrix.at<double>(1, 2)) };
	}

	// 与仓库 mp_handpose.py 的两次裁剪相同：旋转前放大 4 倍，旋转后上移并放大 3 倍。
	bool CropAndPad(const cv::Mat& image, cv::Rect2f box, bool forRotation, cv::Mat& crop, cv::Rect& clipped, cv::Point2f& bias)
	{
		cv::Point2f center(box.x + box.width * 0.5f, box.y + box.height * (forRotation ? 0.5f : 0.1f));
		const float factor = forRotation ? 4.0f : 3.0f;
		const auto clip = [](float value, int limit) { return static_cast<int>(std::clamp(value, 0.0f, static_cast<float>(limit))); };
		const int left = clip(center.x - box.width * factor * 0.5f, image.cols);
		const int top = clip(center.y - box.height * factor * 0.5f, image.rows);
		const int right = clip(center.x + box.width * factor * 0.5f, image.cols);
		const int bottom = clip(center.y + box.height * factor * 0.5f, image.rows);
		clipped = cv::Rect(left, top, right - left, bottom - top);
		if (clipped.empty())
			return false;
		const int side = forRotation ? static_cast<int>(std::hypot(clipped.width, clipped.height)) : std::max(clipped.width, clipped.height);
		const int padLeft = (side - clipped.width) / 2;
		const int padTop = (side - clipped.height) / 2;
		cv::copyMakeBorder(image(clipped), crop, padTop, side - clipped.height - padTop, padLeft, side - clipped.width - padLeft, cv::BORDER_CONSTANT, cv::Scalar());
		bias = cv::Point2f(static_cast<float>(left - padLeft), static_cast<float>(top - padTop));
		return true;
	}
}

MediaPipeHandAdapter::MediaPipeHandAdapter(std::filesystem::path handModelPath, float palmThreshold, float handThreshold, float nmsThreshold, int maxHands)
	: m_handModelPath(std::move(handModelPath)), m_palmThreshold(palmThreshold), m_handThreshold(handThreshold), m_nmsThreshold(nmsThreshold), m_maxHands(maxHands)
{
	for (float threshold : { palmThreshold, handThreshold, nmsThreshold })
		if (!std::isfinite(threshold) || threshold < 0 || threshold > 1)
			throw std::invalid_argument("MediaPipe thresholds must be in [0, 1]");
	if (maxHands < 1)
		throw std::invalid_argument("MediaPipe maxHands must be positive");
}

bool MediaPipeHandAdapter::CreateInput(const cv::Mat& frame, InferenceInput& destination, std::string& error)
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

bool MediaPipeHandAdapter::PreProcess(const InferenceInput& source, InferenceInput& destination, std::string& error)
{
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
	if (tensor.DataType != TensorDataType::UInt8 || !pixels || tensor.Shape.size() != 3 || tensor.Shape[2] != 3 ||
		!tensor.GetShapeElementCount() || *tensor.GetShapeElementCount() != pixels->size() ||
		tensor.Shape[0] > std::numeric_limits<int>::max() || tensor.Shape[1] > std::numeric_limits<int>::max())
	{
		error = "Invalid MediaPipe HWC UInt8 BGR tensor shape or storage";
		return false;
	}
	try
	{
		if (!m_handBackend.IsLoaded() && !m_handBackend.LoadModel(m_handModelPath, error))
			return false;
		cv::Mat frame(static_cast<int>(tensor.Shape[0]), static_cast<int>(tensor.Shape[1]), CV_8UC3, const_cast<std::uint8_t*>(pixels->data()));
		m_frame = frame.clone();
		const float scale = 192.0f / std::max(frame.cols, frame.rows);
		const int width = std::clamp(static_cast<int>(frame.cols * scale), 1, 192);
		const int height = std::clamp(static_cast<int>(frame.rows * scale), 1, 192);
		m_padLeft = (192 - width) / 2;
		m_padTop = (192 - height) / 2;
		cv::Mat resized, padded;
		cv::resize(frame, resized, cv::Size(width, height));
		cv::copyMakeBorder(resized, padded, m_padTop, 192 - height - m_padTop, m_padLeft, 192 - width - m_padLeft, cv::BORDER_CONSTANT, cv::Scalar());
		destination.Tensors.push_back(MakeInput(padded, 192));
		m_scale = scale;
		return true;
	}
	catch (const std::exception& exception)
	{
		error = exception.what();
		return false;
	}
}

bool MediaPipeHandAdapter::PostProcess(const InferenceOutput& source, InferenceOutput& destination, std::string& error)
{
	destination = {};
	error.clear();
	if (m_scale <= 0 || m_frame.empty())
	{
		error = "MediaPipe postprocess requires a successful preprocess for the same frame";
		return false;
	}
	const float scale = m_scale;
	m_scale = 0;
	cv::Mat frame = std::move(m_frame);
	const std::vector<float>* boxes = nullptr;
	const std::vector<float>* logits = nullptr;
	for (const auto& tensor : source.Tensors)
	{
		if (const auto* values = ReadTensor(tensor, { 1, 2016, 18 })) boxes = values;
		if (const auto* values = ReadTensor(tensor, { 1, 2016, 1 })) logits = values;
	}
	if (source.Tensors.size() != 2 || !boxes || !logits)
	{
		error = "Expected MediaPipe palm Float32 outputs [1, 2016, 18] and [1, 2016, 1]";
		return false;
	}
	try
	{
		std::vector<cv::Rect2d> candidates;
		std::vector<float> scores;
		std::vector<std::array<cv::Point2f, 7>> landmarks;
		std::size_t index = 0;
		// stride 8: 24x24x2；三个 stride 16 层合并为 12x12x6。
		for (int grid : { 24, 12 })
			for (int row = 0; row < grid; ++row)
				for (int column = 0; column < grid; ++column)
					for (int repeat = 0; repeat < (grid == 24 ? 2 : 6); ++repeat, ++index)
					{
						const float* data = boxes->data() + index * 18;
						if (!std::isfinite((*logits)[index]) || !std::all_of(data, data + 18, [](float value) { return std::isfinite(value); }))
							continue;
						const float score = 1.0f / (1.0f + std::exp(-std::clamp((*logits)[index], -80.0f, 80.0f)));
						if (score < m_palmThreshold || data[2] <= 0 || data[3] <= 0)
							continue;
						const cv::Point2f anchor((column + 0.5f) * 192.0f / grid, (row + 0.5f) * 192.0f / grid);
						const auto decode = [&](float x, float y) { return cv::Point2f((x + anchor.x - m_padLeft) / scale, (y + anchor.y - m_padTop) / scale); };
						const auto center = decode(data[0], data[1]);
						const float width = data[2] / scale, height = data[3] / scale;
						if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(width) || !std::isfinite(height))
							continue;
						std::array<cv::Point2f, 7> points;
						for (int point = 0; point < 7; ++point)
							points[point] = decode(data[4 + point * 2], data[5 + point * 2]);
						if (!std::all_of(points.begin(), points.end(), [](cv::Point2f p) { return std::isfinite(p.x) && std::isfinite(p.y); }))
							continue;
						candidates.emplace_back(center.x - width * 0.5f, center.y - height * 0.5f, width, height);
						scores.push_back(score);
						landmarks.push_back(points);
					}
		std::vector<int> keep;
		cv::dnn::NMSBoxes(candidates, scores, m_palmThreshold, m_nmsThreshold, keep);
		HandPoseResult result;
		result.ImageWidth = frame.cols;
		result.ImageHeight = frame.rows;
		for (int candidate : keep)
		{
			if (result.Hands.size() >= static_cast<std::size_t>(m_maxHands)) break;
			cv::Mat crop, rotated, handImage;
			cv::Rect clipped, handBox;
			cv::Point2f bias, handBias;
			if (!CropAndPad(frame, candidates[candidate], true, crop, clipped, bias)) continue;
			const auto& points = landmarks[candidate];
			const auto direction = points[2] - points[0];
			const double angle = 90.0 - std::atan2(-direction.y, direction.x) * 180.0 / CV_PI;
			const cv::Point2f center(clipped.x + clipped.width * 0.5f - bias.x, clipped.y + clipped.height * 0.5f - bias.y);
			const cv::Mat rotation = cv::getRotationMatrix2D(center, angle, 1.0);
			cv::warpAffine(crop, rotated, rotation, crop.size());
			cv::Point2f minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			cv::Point2f maximum(-minimum.x, -minimum.y);
			for (auto point : points)
			{
				point = Transform(rotation, point - bias);
				minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y);
				maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y);
			}
			if (!CropAndPad(rotated, cv::Rect2f(minimum, maximum), false, handImage, handBox, handBias)) continue;
			InferenceInput input;
			input.Tensors.push_back(MakeInput(handImage, 224));
			InferenceOutput output;
			if (!m_handBackend.Run(input, output, error)) return false;
			const std::vector<float>* keypoints = nullptr;
			const std::vector<float>* confidence = nullptr;
			const std::vector<float>* handedness = nullptr;
			const std::vector<float>* world = nullptr;
			for (const auto& tensor : output.Tensors)
			{
				if (tensor.Name == "Identity") keypoints = ReadTensor(tensor, { 1, 63 });
				if (tensor.Name == "Identity_1") confidence = ReadTensor(tensor, { 1, 1 });
				if (tensor.Name == "Identity_2") handedness = ReadTensor(tensor, { 1, 1 });
				if (tensor.Name == "Identity_3") world = ReadTensor(tensor, { 1, 63 });
			}
			if (!keypoints || !confidence || !handedness || !world || output.Tensors.size() != 4)
			{
				error = "Expected MediaPipe hand Float32 outputs Identity/Identity_3 [1, 63] and Identity_1/Identity_2 [1, 1]";
				return false;
			}
			if (!std::isfinite((*confidence)[0]) || (*confidence)[0] < m_handThreshold || (*confidence)[0] > 1 ||
				!std::isfinite((*handedness)[0]) || (*handedness)[0] < 0 || (*handedness)[0] > 1 ||
				!std::all_of(world->begin(), world->end(), [](float value) { return std::isfinite(value); }) ||
				!std::all_of(keypoints->begin(), keypoints->end(), [](float value) { return std::isfinite(value); })) continue;
			cv::Mat inverse;
			cv::invertAffineTransform(rotation, inverse);
			const float handScale = static_cast<float>(handImage.cols) / 224.0f;
			HandPose hand;
			hand.Confidence = (*confidence)[0];
			hand.RightHandProbability = (*handedness)[0];
			hand.HasWorldKeypoints = true;
			minimum = cv::Point2f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			maximum = cv::Point2f(-minimum.x, -minimum.y);
			for (std::size_t point = 0; point < hand.Keypoints.size(); ++point)
			{
				const auto position = Transform(inverse, cv::Point2f((*keypoints)[point * 3] * handScale, (*keypoints)[point * 3 + 1] * handScale) + handBias) + bias;
				hand.Keypoints[point] = { position.x, position.y, hand.Confidence };
				hand.Keypoints[point].Z = (*keypoints)[point * 3 + 2] * handScale;
				const float wx = (*world)[point * 3], wy = (*world)[point * 3 + 1];
				hand.WorldKeypoints[point] = { static_cast<float>(inverse.at<double>(0, 0) * wx + inverse.at<double>(0, 1) * wy),
					static_cast<float>(inverse.at<double>(1, 0) * wx + inverse.at<double>(1, 1) * wy), (*world)[point * 3 + 2] };
				minimum.x = std::min(minimum.x, position.x); minimum.y = std::min(minimum.y, position.y);
				maximum.x = std::max(maximum.x, position.x); maximum.y = std::max(maximum.y, position.y);
			}
			const float width = maximum.x - minimum.x, height = maximum.y - minimum.y;
			hand.X = std::clamp(minimum.x - width * 0.325f, 0.0f, static_cast<float>(frame.cols));
			hand.Y = std::clamp(minimum.y - height * 0.425f, 0.0f, static_cast<float>(frame.rows));
			hand.Width = std::clamp(maximum.x + width * 0.325f, 0.0f, static_cast<float>(frame.cols)) - hand.X;
			hand.Height = std::clamp(maximum.y + height * 0.225f, 0.0f, static_cast<float>(frame.rows)) - hand.Y;
			if (hand.Width > 0 && hand.Height > 0) result.Hands.push_back(std::move(hand));
		}
		destination.HandPoses = std::move(result);
		return true;
	}
	catch (const std::exception& exception)
	{
		error = exception.what();
		return false;
	}
}

void MediaPipeHandAdapter::DrawUI()
{
	ImGui::DragFloat("Palm confidence", &m_palmThreshold, 0.001f, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Hand confidence", &m_handThreshold, 0.001f, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Palm NMS", &m_nmsThreshold, 0.001f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragInt("Max hands", &m_maxHands, 1, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::TextDisabled("Hand landmark backend: ONNX Runtime");
}
