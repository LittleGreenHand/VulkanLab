#pragma once
#include "InferenceTypes.h"
#include <filesystem>
#include <string>

enum class ModelFormat
{
    Unknown,
    ONNX
};

enum class ModelState
{
    Disabled,
    Unloaded,
    Loaded,
    Error
};

struct ModelInfo
{
    std::string Name;
    std::filesystem::path Path;
    ModelFormat Format = ModelFormat::Unknown;
    InferenceBackendType BackendType = InferenceBackendType::ONNXRuntime;
};

[[nodiscard]] constexpr const char* ToString(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::ONNX: return "ONNX";
    case ModelFormat::Unknown: return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] constexpr const char* ToString(ModelState state)
{
    switch (state)
    {
    case ModelState::Disabled: return "Disabled";
    case ModelState::Unloaded: return "Unloaded";
    case ModelState::Loaded: return "Loaded";
    case ModelState::Error: return "Error";
    }
    return "Unknown";
}
