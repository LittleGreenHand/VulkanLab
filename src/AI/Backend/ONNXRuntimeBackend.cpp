#include "Backend/ONNXRuntimeBackend.h"

#include <algorithm>
#include <cstdint>
#include <limits>

TensorDataType FromONNXType(ONNXTensorElementDataType type)
{
    switch (type)
    {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return TensorDataType::Float32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return TensorDataType::Int32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return TensorDataType::Int64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return TensorDataType::UInt8;
    default: return TensorDataType::Unknown;
    }
}

bool ValidateTensor(const Tensor& tensor, const TensorInfo& expected, std::string& error)
{
    if (tensor.DataType != expected.DataType)
    {
        error = "Input '" + expected.Name + "' expects " + ToString(expected.DataType) +
            ", received " + ToString(tensor.DataType);
        return false;
    }

    if (tensor.Shape.size() != expected.Shape.size())
    {
        error = "Input '" + expected.Name + "' has an unexpected rank";
        return false;
    }

    for (std::size_t index = 0; index < expected.Shape.size(); ++index)
    {
        if (tensor.Shape[index] <= 0 ||
            (expected.Shape[index] > 0 && expected.Shape[index] != tensor.Shape[index]))
        {
            error = "Input '" + expected.Name + "' has an incompatible shape";
            return false;
        }
    }

    const auto expectedCount = tensor.GetShapeElementCount();
    if (!expectedCount || *expectedCount != tensor.GetDataElementCount())
    {
        error = "Input '" + expected.Name + "' shape does not match its data size";
        return false;
    }
    return true;
}

template<typename T>
Ort::Value CreateValue(
    const Tensor& tensor,
    Ort::MemoryInfo& memoryInfo,
    const std::vector<T>& values)
{
    return Ort::Value::CreateTensor<T>(
        memoryInfo,
        const_cast<T*>(values.data()),
        values.size(),
        tensor.Shape.data(),
        tensor.Shape.size());
}

bool CreateInputValue(
    const Tensor& tensor,
    Ort::MemoryInfo& memoryInfo,
    Ort::Value& value,
    std::string& error)
{
    switch (tensor.DataType)
    {
    case TensorDataType::Float32:
        value = CreateValue(tensor, memoryInfo, std::get<std::vector<float>>(tensor.Data));
        return true;
    case TensorDataType::Int32:
        value = CreateValue(tensor, memoryInfo, std::get<std::vector<std::int32_t>>(tensor.Data));
        return true;
    case TensorDataType::Int64:
        value = CreateValue(tensor, memoryInfo, std::get<std::vector<std::int64_t>>(tensor.Data));
        return true;
    case TensorDataType::UInt8:
        value = CreateValue(tensor, memoryInfo, std::get<std::vector<std::uint8_t>>(tensor.Data));
        return true;
    case TensorDataType::Unknown:
        error = "Unsupported input tensor data type";
        return false;
    }
    error = "Unsupported input tensor data type";
    return false;
}

template<typename T>
std::vector<T> CopyTensorData(const Ort::Value& value, std::size_t count)
{
    const T* begin = value.GetTensorData<T>();
    return { begin, begin + count };
}

bool ExtractOutput(
    const Ort::Value& value,
    const std::string& name,
    Tensor& tensor,
    std::string& error)
{
    if (!value.IsTensor())
    {
        error = "Output '" + name + "' is not a tensor";
        return false;
    }

    const auto typeInfo = value.GetTensorTypeAndShapeInfo();
    tensor.Name = name;
    tensor.Shape = typeInfo.GetShape();
    tensor.DataType = FromONNXType(typeInfo.GetElementType());
    const std::size_t count = typeInfo.GetElementCount();

    switch (tensor.DataType)
    {
    case TensorDataType::Float32:
        tensor.Data = CopyTensorData<float>(value, count);
        return true;
    case TensorDataType::Int32:
        tensor.Data = CopyTensorData<std::int32_t>(value, count);
        return true;
    case TensorDataType::Int64:
        tensor.Data = CopyTensorData<std::int64_t>(value, count);
        return true;
    case TensorDataType::UInt8:
        tensor.Data = CopyTensorData<std::uint8_t>(value, count);
        return true;
    case TensorDataType::Unknown:
        error = "Output '" + name + "' uses an unsupported tensor data type";
        return false;
    }
    return false;
}

TensorInfo ReadTensorInfo(
    const Ort::Session& session,
    std::size_t index,
    bool input,
    Ort::AllocatorWithDefaultOptions& allocator,
    std::string& nameStorage)
{
    auto name = input
        ? session.GetInputNameAllocated(index, allocator)
        : session.GetOutputNameAllocated(index, allocator);
    nameStorage = name.get();

    const Ort::TypeInfo typeInfo = input
        ? session.GetInputTypeInfo(index)
        : session.GetOutputTypeInfo(index);
    const auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
    return { nameStorage, tensorInfo.GetShape(), FromONNXType(tensorInfo.GetElementType()) };
}

ONNXRuntimeBackend::ONNXRuntimeBackend()
{
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

ONNXRuntimeBackend::~ONNXRuntimeBackend() = default;

bool ONNXRuntimeBackend::LoadModel(const std::filesystem::path& modelPath, std::string& error)
{
    UnloadModel();

    try
    {
        m_session = std::make_unique<Ort::Session>(GetEnv(), modelPath.c_str(), m_sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;
        const std::size_t inputCount = m_session->GetInputCount();
        const std::size_t outputCount = m_session->GetOutputCount();

        m_inputNames.resize(inputCount);
        m_inputInfos.reserve(inputCount);
        for (std::size_t index = 0; index < inputCount; ++index)
            m_inputInfos.push_back(ReadTensorInfo(*m_session, index, true, allocator, m_inputNames[index]));

        m_outputNames.resize(outputCount);
        m_outputInfos.reserve(outputCount);
        for (std::size_t index = 0; index < outputCount; ++index)
            m_outputInfos.push_back(ReadTensorInfo(*m_session, index, false, allocator, m_outputNames[index]));

        return true;
    }
    catch (const Ort::Exception& exception)
    {
        error = exception.what();
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
    }

    UnloadModel();
    return false;
}

void ONNXRuntimeBackend::UnloadModel()
{
    m_session.reset();
    m_inputInfos.clear();
    m_outputInfos.clear();
    m_inputNames.clear();
    m_outputNames.clear();
}

bool ONNXRuntimeBackend::IsLoaded() const
{
    return m_session != nullptr;
}

bool ONNXRuntimeBackend::Run(
    const InferenceInput& input,
    InferenceOutput& output,
    std::string& error)
{
    output.Tensors.clear();
    if (!m_session)
    {
        error = "ONNX Runtime model is not loaded";
        return false;
    }
    if (input.Tensors.size() != m_inputInfos.size())
    {
        error = "Expected " + std::to_string(m_inputInfos.size()) + " input tensor(s), received " +
            std::to_string(input.Tensors.size());
        return false;
    }

    try
    {
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        std::vector<const char*> inputNames;
        std::vector<Ort::Value> inputValues;
        inputNames.reserve(m_inputInfos.size());
        inputValues.reserve(m_inputInfos.size());

        for (std::size_t index = 0; index < m_inputInfos.size(); ++index)
        {
            const TensorInfo& expected = m_inputInfos[index];
            const Tensor* tensor = nullptr;

            if (!input.Tensors[index].Name.empty() && input.Tensors[index].Name == expected.Name)
                tensor = &input.Tensors[index];
            else
            {
                const auto found = std::find_if(input.Tensors.begin(), input.Tensors.end(),
                    [&expected](const Tensor& candidate) { return candidate.Name == expected.Name; });
                if (found != input.Tensors.end())
                    tensor = &*found;
                else if (m_inputInfos.size() == 1 && input.Tensors.front().Name.empty())
                    tensor = &input.Tensors.front();
            }

            if (!tensor)
            {
                error = "Missing required input tensor '" + expected.Name + "'";
                return false;
            }
            if (!ValidateTensor(*tensor, expected, error))
                return false;

            Ort::Value value{ nullptr };
            if (!CreateInputValue(*tensor, memoryInfo, value, error))
                return false;

            inputNames.push_back(m_inputNames[index].c_str());
            inputValues.push_back(std::move(value));
        }

        std::vector<const char*> outputNames;
        outputNames.reserve(m_outputNames.size());
        for (const std::string& name : m_outputNames)
            outputNames.push_back(name.c_str());

        std::vector<Ort::Value> values = m_session->Run(
            Ort::RunOptions{ nullptr },
            inputNames.data(),
            inputValues.data(),
            inputValues.size(),
            outputNames.data(),
            outputNames.size());

        output.Tensors.reserve(values.size());
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            Tensor tensor;
            if (!ExtractOutput(values[index], m_outputNames[index], tensor, error))
            {
                output.Tensors.clear();
                return false;
            }
            output.Tensors.push_back(std::move(tensor));
        }
        return true;
    }
    catch (const Ort::Exception& exception)
    {
        error = exception.what();
    }
    catch (const std::bad_variant_access&)
    {
        error = "Tensor data storage does not match its declared data type";
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
    }

    output.Tensors.clear();
    return false;
}

Ort::Env& ONNXRuntimeBackend::GetEnv()
{
    static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "AI");
	return environment;
}
