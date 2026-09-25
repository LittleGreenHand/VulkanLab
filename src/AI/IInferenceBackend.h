#pragma once

#include "InferenceTypes.h"

#include <filesystem>
#include <string>
#include <vector>

class IInferenceBackend
{
public:
	virtual ~IInferenceBackend() = default;

	virtual bool LoadModel(const std::filesystem::path& modelPath, std::string& error) = 0;
	virtual void UnloadModel() = 0;
	virtual bool IsLoaded() const = 0;
	virtual bool Run(const InferenceInput& input, InferenceOutput& output, std::string& error) = 0;

	virtual InferenceBackendType GetType() const = 0;
	virtual const char* GetName() const = 0;
	virtual const std::vector<TensorInfo>& GetInputInfos() const = 0;
	virtual const std::vector<TensorInfo>& GetOutputInfos() const = 0;
};
