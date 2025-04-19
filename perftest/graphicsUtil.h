#pragma once
#include "directx.h"
#include "file.h"

inline ComputePSO loadComputeShader(DirectXDevice &dx, const std::string &filename)
{
	auto shaderBlob = loadFile(filename);
	return dx.createComputeShader(filename, shaderBlob);
}

