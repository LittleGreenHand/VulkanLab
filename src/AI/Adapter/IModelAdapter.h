#pragma once

#include "InferenceTypes.h"

class IModelAdapter
{
public:
	virtual ~IModelAdapter() = default;
	virtual bool PreProcess(const InferenceInput& source, InferenceInput& destination, std::string& error) = 0;
	virtual bool PostProcess(const InferenceOutput& source, InferenceOutput& destination, std::string& error) = 0;
	virtual void DrawUI() {}
};
