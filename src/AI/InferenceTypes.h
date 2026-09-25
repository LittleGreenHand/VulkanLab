#pragma once
#include "HandPose.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include "opencv2/core/hal/interface.h"

enum class TensorDataType
{
	Unknown,
	UInt8,
	Int8,
	UInt16,
	Int16,
	Int32,
	Int64,
	Float16,
	Float32,
	Float64
};

constexpr const char* ToString(TensorDataType type)
{
	switch (type)
	{
	case TensorDataType::UInt8:   return "UInt8";
	case TensorDataType::Int8:    return "Int8";
	case TensorDataType::UInt16:  return "UInt16";
	case TensorDataType::Int16:   return "Int16";
	case TensorDataType::Int32:   return "Int32";
	case TensorDataType::Int64:   return "Int64";
	case TensorDataType::Float16: return "Float16";
	case TensorDataType::Float32: return "Float32";
	case TensorDataType::Float64: return "Float64";
	case TensorDataType::Unknown: return "Unknown";
	}

	return "Unknown";
}
inline TensorDataType CvTypeToTensorDataType(int cvType)
{
	switch (CV_MAT_DEPTH(cvType))
	{
	case CV_8U:  return TensorDataType::UInt8;
	case CV_8S:  return TensorDataType::Int8;
	case CV_16U: return TensorDataType::UInt16;
	case CV_16S: return TensorDataType::Int16;
	case CV_32S: return TensorDataType::Int32;
	case CV_16F: return TensorDataType::Float16;
	case CV_32F: return TensorDataType::Float32;
	case CV_64F: return TensorDataType::Float64;
	default:     return TensorDataType::Unknown;
	}
}
inline int TensorDataTypeToCvDepth(TensorDataType type)
{
	switch (type)
	{
	case TensorDataType::UInt8:   return CV_8U;
	case TensorDataType::Int8:    return CV_8S;
	case TensorDataType::UInt16:  return CV_16U;
	case TensorDataType::Int16:   return CV_16S;
	case TensorDataType::Int32:   return CV_32S;
	case TensorDataType::Float16: return CV_16F;
	case TensorDataType::Float32: return CV_32F;
	case TensorDataType::Float64: return CV_64F;
	default:                       return -1;
	}
}
inline int TensorDataTypeToCvType(TensorDataType type, int channels = 1)
{
	const int depth = TensorDataTypeToCvDepth(type);
	if (depth < 0)
		return -1;

	return CV_MAKETYPE(depth, channels);
}

enum class InferenceBackendType
{
    OpenCVDNN,
    ONNXRuntime
};
inline const char* InferenceBackendTypeString[] = {
	"OpenCV DNN",
	"ONNX Runtime"
};
constexpr const char* ToString(InferenceBackendType type)
{
	switch (type)
	{
	case InferenceBackendType::OpenCVDNN: return InferenceBackendTypeString[0];
	case InferenceBackendType::ONNXRuntime: return InferenceBackendTypeString[1];
	}
	return "Unknown";
}

using TensorData = std::variant<
    std::monostate,
    std::vector<float>,
    std::vector<std::int32_t>,
    std::vector<std::int64_t>,
    std::vector<std::uint8_t>>;

struct Tensor
{
    std::string Name;
    std::vector<std::int64_t> Shape;
    TensorDataType DataType = TensorDataType::Unknown;
    TensorData Data;

    [[nodiscard]] std::size_t GetDataElementCount() const
    {
        return std::visit([](const auto& values) -> std::size_t
        {
            using ValueType = std::decay_t<decltype(values)>;
            if constexpr (std::is_same_v<ValueType, std::monostate>)
                return 0;
            else
                return values.size();
        }, Data);
    }

    [[nodiscard]] std::optional<std::size_t> GetShapeElementCount() const
    {
        std::size_t count = 1;
        for (const std::int64_t dimension : Shape)
        {
            if (dimension <= 0)
                return std::nullopt;

            const auto unsignedDimension = static_cast<std::size_t>(dimension);
            if (count > std::numeric_limits<std::size_t>::max() / unsignedDimension)
                return std::nullopt;
            count *= unsignedDimension;
        }
        return count;
    }
};

struct TensorInfo
{
    std::string Name;
    std::vector<std::int64_t> Shape;
    TensorDataType DataType = TensorDataType::Unknown;
};

struct InferenceInput
{
    std::vector<Tensor> Tensors;
};

struct InferenceOutput
{
    std::vector<Tensor> Tensors;
    std::optional<HandPoseResult> HandPoses;
};
