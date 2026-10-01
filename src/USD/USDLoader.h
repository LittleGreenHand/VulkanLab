#pragma once

#include <string>

class USDLoader
{
public:
	USDLoader() = default;
	~USDLoader() = default;

	bool Load(const std::string& filePath);
	void Unload();

private:
};
