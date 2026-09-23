#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

enum class TensorDataType
{
    Unknown,
    Float32,
    Int32,
    Int64,
    UInt8
};

enum class InferenceBackendType
{
    OpenCVDNN,
    ONNXRuntime
};

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
};

constexpr const char* ToString(TensorDataType type)
{
    switch (type)
    {
    case TensorDataType::Float32: return "Float32";
    case TensorDataType::Int32: return "Int32";
    case TensorDataType::Int64: return "Int64";
    case TensorDataType::UInt8: return "UInt8";
    case TensorDataType::Unknown: return "Unknown";
    }
    return "Unknown";
}

constexpr const char* ToString(InferenceBackendType type)
{
    switch (type)
    {
    case InferenceBackendType::OpenCVDNN: return "OpenCV DNN";
    case InferenceBackendType::ONNXRuntime: return "ONNX Runtime";
    }
    return "Unknown";
}
