#include "Backend/OpenCVDNNBackend.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>

bool BuildMat(const Tensor& tensor, cv::Mat& matrix, std::string& error)
{
    if (tensor.DataType != TensorDataType::Float32 ||
        !std::holds_alternative<std::vector<float>>(tensor.Data))
    {
        error = "OpenCV DNN currently accepts Float32 tensors only";
        return false;
    }

    const auto& values = std::get<std::vector<float>>(tensor.Data);
    const auto expectedCount = tensor.GetShapeElementCount();
    if (!expectedCount || *expectedCount != values.size())
    {
        error = "Tensor '" + tensor.Name + "' shape does not match its data size";
        return false;
    }

    if (tensor.Shape.empty())
    {
        error = "OpenCV DNN does not accept scalar input tensors";
        return false;
    }

    std::vector<int> dimensions;
    dimensions.reserve(tensor.Shape.size());
    for (const std::int64_t dimension : tensor.Shape)
    {
        if (dimension <= 0 || dimension > std::numeric_limits<int>::max())
        {
            error = "Tensor '" + tensor.Name + "' has an invalid dimension";
            return false;
        }
        dimensions.push_back(static_cast<int>(dimension));
    }

    matrix = cv::Mat(
        static_cast<int>(dimensions.size()),
        dimensions.data(),
        CV_32F,
        const_cast<float*>(values.data()));
    return true;
}

bool MatToTensor(const cv::Mat& source, std::string name, Tensor& tensor, std::string& error)
{
    if (source.type() != CV_32F)
    {
        error = "OpenCV DNN produced a non-Float32 output tensor";
        return false;
    }

    cv::Mat continuous = source.isContinuous() ? source : source.clone();
    tensor.Name = std::move(name);
    tensor.DataType = TensorDataType::Float32;

    tensor.Shape.reserve(static_cast<std::size_t>(continuous.dims));
    for (int index = 0; index < continuous.dims; ++index)
        tensor.Shape.push_back(continuous.size[index]);

    const auto* begin = continuous.ptr<float>();
    tensor.Data = std::vector<float>(begin, begin + continuous.total());
    return true;
}

bool OpenCVDNNBackend::LoadModel(const std::filesystem::path& modelPath, std::string& error)
{
    UnloadModel();

    try
    {
        std::ifstream stream(modelPath, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            error = "Unable to open model file: " + modelPath.string();
            return false;
        }

        const std::streamsize size = stream.tellg();
        if (size <= 0)
        {
            error = "Model file is empty: " + modelPath.string();
            return false;
        }

        stream.seekg(0, std::ios::beg);
        std::vector<unsigned char> modelBytes(static_cast<std::size_t>(size));
        if (!stream.read(reinterpret_cast<char*>(modelBytes.data()), size))
        {
            error = "Unable to read model file: " + modelPath.string();
            return false;
        }

        m_net = cv::dnn::readNetFromONNX(modelBytes);
        if (m_net.empty())
        {
            error = "OpenCV returned an empty network";
            return false;
        }

        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        for (const std::string& name : m_net.getUnconnectedOutLayersNames())
            m_outputInfos.push_back({ name, {}, TensorDataType::Float32 });
        return true;
    }
    catch (const cv::Exception& exception)
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

void OpenCVDNNBackend::UnloadModel()
{
    m_net = cv::dnn::Net{};
    m_inputInfos.clear();
    m_outputInfos.clear();
}

bool OpenCVDNNBackend::IsLoaded() const
{
    return !m_net.empty();
}

bool OpenCVDNNBackend::Run(
    const InferenceInput& input,
    InferenceOutput& output,
    std::string& error)
{
    output.Tensors.clear();
    if (!IsLoaded())
    {
        error = "OpenCV DNN model is not loaded";
        return false;
    }
    if (input.Tensors.empty())
    {
        error = "No input tensors were provided";
        return false;
    }

    try
    {
        m_inputInfos.clear();
        for (const Tensor& tensor : input.Tensors)
        {
            cv::Mat matrix;
            if (!BuildMat(tensor, matrix, error))
                return false;

            m_net.setInput(matrix, tensor.Name);
            m_inputInfos.push_back({ tensor.Name, tensor.Shape, tensor.DataType });
        }

        std::vector<std::string> outputNames = m_net.getUnconnectedOutLayersNames();
        std::vector<cv::Mat> matrices;
        m_net.forward(matrices, outputNames);

        m_outputInfos.clear();
        output.Tensors.reserve(matrices.size());
        for (std::size_t index = 0; index < matrices.size(); ++index)
        {
            Tensor tensor;
            if (!MatToTensor(matrices[index], outputNames[index], tensor, error))
            {
                output.Tensors.clear();
                return false;
            }
            m_outputInfos.push_back({ tensor.Name, tensor.Shape, tensor.DataType });
            output.Tensors.push_back(std::move(tensor));
        }
        return true;
    }
    catch (const cv::Exception& exception)
    {
        error = exception.what();
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
    }

    output.Tensors.clear();
    return false;
}
