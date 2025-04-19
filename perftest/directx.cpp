#include "directx.h"
#include <assert.h>
#define USE_PIX 1
#include <pix3.h>

ComPtr<IDXGIFactory4> GDXGIFactory;

static D3D12_RESOURCE_DESC InitBufferResourceDesc(size_t sizeInBytes)
{
	return
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0,
		.Width = sizeInBytes,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = 
		{
			.Count = 1,
			.Quality = 0,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};
}

static UINT Align(UINT value, UINT alignment)
{
	UINT mask = alignment - 1;
	return (value + mask) & ~mask;
}

ComputePSO::ComputePSO(ID3D12Device* device, const std::string& name, const std::vector<unsigned char>& shaderBytes)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc =
	{
		.CS =
		{
			.pShaderBytecode = shaderBytes.data(),
			.BytecodeLength = shaderBytes.size()
		}
	};

	HRESULT result = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
	assert(SUCCEEDED(result));

	std::vector<wchar_t> wname;
	wname.resize(name.length() + 1);
	size_t wnameLen = 0;
	mbstowcs_s(&wnameLen, wname.data(), wname.size(), name.c_str(), name.length());
	pso->SetName(wname.data());

	ComPtr<ID3D12RootSignatureDeserializer> deserializer;
	result = D3D12CreateRootSignatureDeserializer(
		shaderBytes.data(),
		shaderBytes.size(),
		IID_PPV_ARGS(deserializer.GetAddressOf()));
	assert(SUCCEEDED(result));

	auto dxRootSigDesc = deserializer->GetRootSignatureDesc();
	for (uint32_t rootParamIdx = 0; rootParamIdx < dxRootSigDesc->NumParameters; rootParamIdx++)
	{
		Binding binding = { .rootParamIdx = rootParamIdx };
		const D3D12_ROOT_PARAMETER& rootParam = dxRootSigDesc->pParameters[rootParamIdx];
		if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			binding.descriptorOffset = 0;
			bool isSamplerDescriptorTable = false;
			for (uint32_t rangeIdx = 0; rangeIdx < rootParam.DescriptorTable.NumDescriptorRanges; rangeIdx++)
			{
				auto& range = rootParam.DescriptorTable.pDescriptorRanges[rangeIdx];
				if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
					binding.descriptorOffset = range.OffsetInDescriptorsFromTableStart;

				EBindingType bindingType = {};
				if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
					bindingType = EBindingType::kSrv;
				else if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
					bindingType = EBindingType::kCbv;
				else if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
					bindingType = EBindingType::kUav;
				else if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
				{
					bindingType = EBindingType::kSampler;
					isSamplerDescriptorTable = true;
				}

				binding.isRootDescriptor = false;
				for (uint32_t descriptorIdx = 0; descriptorIdx < range.NumDescriptors; descriptorIdx++)
				{
					uint32_t reg = range.BaseShaderRegister + descriptorIdx;
					bindings[(int)bindingType][reg] = binding;
					binding.descriptorOffset++;
				}
			}

			RootParameter ourRootParam = {
				.type = rootParam.ParameterType,
				.numDescriptors = binding.descriptorOffset,
				.isSamplerDescriptorTable = isSamplerDescriptorTable
			};
			rootSignatureDesc.push_back(ourRootParam);
		}
		else
		{
			EBindingType bindingType = {};
			if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV)
				bindingType = EBindingType::kSrv;
			else if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
				bindingType = EBindingType::kCbv;
			else if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
				bindingType = EBindingType::kUav;
			else
				assert(false);

			binding.isRootDescriptor = true;
			bindings[(int)bindingType][rootParam.Descriptor.ShaderRegister] = binding;

			RootParameter ourRootParam = {
				.type = rootParam.ParameterType,
				.numDescriptors = 1
			};
			rootSignatureDesc.push_back(ourRootParam);
		}
	}

	result = device->CreateRootSignature(
		0,
		shaderBytes.data(),
		shaderBytes.size(),
		IID_PPV_ARGS(rootSig.GetAddressOf()));
	assert(SUCCEEDED(result));
}

const ComputePSO::Binding* ComputePSO::getBinding(uint32_t slot, EBindingType type) const
{
	auto& map = bindings[(int)type];
	auto iterator = map.find(slot);
	if (iterator == map.end())
		return nullptr;
	return &iterator->second;
}

std::vector<ComPtr<IDXGIAdapter>> enumerateAdapters()
{
	std::vector<ComPtr<IDXGIAdapter>> adapters;

#if _DEBUG
	UINT dxgiFactoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#else
	UINT dxgiFactoryFlag = 0;
#endif

	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlag, IID_PPV_ARGS(GDXGIFactory.GetAddressOf()));
	assert(SUCCEEDED(hr));

	ComPtr<IDXGIAdapter> adapter;
	for (UINT i = 0; GDXGIFactory->EnumAdapters(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(adapter);
	}

	return adapters;
}

DirectXDevice::DirectXDevice(HWND window, uint2 resolution, IDXGIAdapter* adapter) : 
	windowHandle(window),
	resolution(resolution)
{
#if _DEBUG
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugInterface.GetAddressOf()))))
	{
		debugInterface->EnableDebugLayer();
		//debugInterface->SetEnableGPUBasedValidation(true);
	}
#endif

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;

	HRESULT result = D3D12CreateDevice(adapter, featureLevel, IID_PPV_ARGS(device.GetAddressOf()));
	assert(SUCCEEDED(result));

#if _DEBUG
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(pInfoQueue.GetAddressOf()))))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	}
#endif

	D3D12_COMMAND_QUEUE_DESC queueDesc =
	{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.NodeMask = 1
	};
	result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(cmdQueue.GetAddressOf()));
	assert(SUCCEEDED(result));

	fenceLastSignalVal = 0;
	result = device->CreateFence(fenceLastSignalVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
	assert(SUCCEEDED(result));

	fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	assert(fenceEvent);

	result = device->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(cmdAllocator.GetAddressOf()));
	assert(SUCCEEDED(result));

	result = device->CreateCommandList(
		0,
		queueDesc.Type,
		cmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(cmdList.GetAddressOf()));
	assert(SUCCEEDED(result));
	cmdList->Close();

	DXGI_SWAP_CHAIN_DESC1 swapDesc = {
		.Width = resolution.x,
		.Height = resolution.y,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, // RT needed for GDI text output
		.BufferCount = 2,
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH };

	result = GDXGIFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		window,
		&swapDesc,
		nullptr,
		nullptr,
		swapChain.GetAddressOf());
	assert(SUCCEEDED(result));

	D3D12_QUERY_HEAP_DESC queryHeapDesc = {
		.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
		.Count = (UINT)queries.size() * 2};
	result = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(queryHeap.GetAddressOf()));
	assert(SUCCEEDED(result));

	D3D12_RESOURCE_DESC resourceDesc = InitBufferResourceDesc(queryHeapDesc.Count * sizeof(uint64_t));
	D3D12_HEAP_PROPERTIES heapProps = { .Type = D3D12_HEAP_TYPE_READBACK };
	result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(queryResultBuffer.GetAddressOf()));
	assert(SUCCEEDED(result));

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = 100'000,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
	result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(cbvSrvUavDescriptorHeap.GetAddressOf()));

	heapDesc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		.NumDescriptors = 1'000,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
	result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(samplerDescriptorHeap.GetAddressOf()));
}

DirectXDevice::~DirectXDevice()
{
	cmdQueue->Signal(fence.Get(), ++fenceLastSignalVal);
	HRESULT hr = fence->SetEventOnCompletion(fenceLastSignalVal, fenceEvent);
	assert(SUCCEEDED(hr));
	WaitForSingleObject(fenceEvent, INFINITE);

	CloseHandle(fenceEvent);
}

ComPtr<ID3D12Resource> DirectXDevice::createConstantBuffer(unsigned bytes)
{
	auto resourceDesc = InitBufferResourceDesc(Align(bytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	D3D12_HEAP_PROPERTIES heapProps = { .Type = D3D12_HEAP_TYPE_UPLOAD };
	ComPtr<ID3D12Resource> resource;
	HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(resource.GetAddressOf()));
	assert(SUCCEEDED(result));
	return resource;
}

ComPtr<ID3D12Resource> DirectXDevice::createBuffer(unsigned numElements, unsigned strideBytes)
{
	auto resourceDesc = InitBufferResourceDesc(strideBytes * numElements);
	resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_HEAP_PROPERTIES heapProps = { .Type = D3D12_HEAP_TYPE_DEFAULT };
	ComPtr<ID3D12Resource> resource;
	HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(resource.GetAddressOf()));
	assert(SUCCEEDED(result));
	return resource;
}

ComPtr<ID3D12Resource> DirectXDevice::createTexture2d(uint2 dimensions, DXGI_FORMAT format, unsigned mips)
{
	D3D12_RESOURCE_DESC textureDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = dimensions.x,
		.Height = dimensions.y,
		.DepthOrArraySize = 1,
		.MipLevels = (UINT16)mips,
		.Format = format,
		.SampleDesc =
		{
			.Count = 1,
			.Quality = 0,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

	D3D12_HEAP_PROPERTIES heapProps = { .Type = D3D12_HEAP_TYPE_DEFAULT };
	ComPtr<ID3D12Resource> resource;
	HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(resource.GetAddressOf()));
	assert(SUCCEEDED(result));
	return resource;
}

ComPtr<ID3D12Resource> DirectXDevice::createTexture3d(uint3 dimensions, DXGI_FORMAT format, unsigned mips)
{
	D3D12_RESOURCE_DESC textureDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D,
		.Alignment = 0,
		.Width = dimensions.x,
		.Height = dimensions.y,
		.DepthOrArraySize = (UINT16)dimensions.z,
		.MipLevels = (UINT16)mips,
		.Format = format,
		.SampleDesc =
		{
			.Count = 1,
			.Quality = 0,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

	D3D12_HEAP_PROPERTIES heapProps = { .Type = D3D12_HEAP_TYPE_DEFAULT };
	ComPtr<ID3D12Resource> resource;
	HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(resource.GetAddressOf()));
	assert(SUCCEEDED(result));
	return resource;
}

UnorderedAccessView DirectXDevice::createUAV(ID3D12Resource *buffer)
{
	return UnorderedAccessView(buffer, {});
}

UnorderedAccessView DirectXDevice::createByteAddressUAV(ID3D12Resource *buffer, unsigned numElements)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.FirstElement = 0,
			.NumElements = numElements,
			.Flags = D3D12_BUFFER_UAV_FLAG_RAW}};

	return UnorderedAccessView(buffer, desc);
}

UnorderedAccessView DirectXDevice::createTypedUAV(ID3D12Resource *buffer, unsigned numElements, DXGI_FORMAT format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.FirstElement = 0,
			.NumElements = numElements }};

	return UnorderedAccessView(buffer, desc);
}

ShaderResourceView DirectXDevice::createSRV(ID3D12Resource *resource)
{
	return ShaderResourceView(resource, {});
}

ShaderResourceView DirectXDevice::createTypedSRV(ID3D12Resource *buffer, unsigned numElements, DXGI_FORMAT format)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.FirstElement = 0,
			.NumElements = numElements }};

	return ShaderResourceView(buffer, desc);
}

ShaderResourceView DirectXDevice::createStructuredSRV(ID3D12Resource* buffer, unsigned numElements, unsigned stride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer =
		{
			.FirstElement = 0,
			.NumElements = numElements,
			.StructureByteStride = stride }};

	return ShaderResourceView(buffer, desc);
}

ShaderResourceView DirectXDevice::createByteAddressSRV(ID3D12Resource *buffer, unsigned numElements)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer =
		{
			.FirstElement = 0,
			.NumElements = numElements,
			.Flags = D3D12_BUFFER_SRV_FLAG_RAW }};

	return ShaderResourceView(buffer, desc);
}

SamplerState DirectXDevice::createSampler(SamplerType type)
{
	D3D12_SAMPLER_DESC desc =
	{
		.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
		.MaxLOD = D3D12_FLOAT32_MAX
	};

	switch (type)
	{
	case SamplerType::Nearest: 
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		break;

	case SamplerType::Bilinear:
		desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		break;

	case SamplerType::Trilinear:
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		break;
	}

	return SamplerState(desc);
}

ComputePSO DirectXDevice::createComputeShader(const std::string& name, const std::vector<unsigned char> &shaderBytes)
{
	return ComputePSO(device.Get(), name, shaderBytes);
}

void DirectXDevice::beginFrame()
{
	cmdAllocator->Reset();
	cmdList->Reset(cmdAllocator.Get(), nullptr);

	D3D12_VIEWPORT viewport;
	viewport.Height = (float)resolution.y;
	viewport.Width = (float)resolution.x;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	cmdList->RSSetViewports(1, &viewport);

	ID3D12DescriptorHeap* heaps[] = { cbvSrvUavDescriptorHeap.Get(), samplerDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(2, heaps);
	cbvSrvUavDescriptorHeapOffset = 0;
	samplerDescriptorHeapOffset = 0;

	frameFirstQuery = queryCounter;
}

void DirectXDevice::dispatch(
	const ComputePSO& shader,
	uint3 resolution,
	uint3 groupSize,
	std::initializer_list<ID3D12Resource*> cbs,
	std::initializer_list<const ShaderResourceView*> srvs,
	std::initializer_list<const UnorderedAccessView*> uavs,
	std::initializer_list<const SamplerState*> samplers)
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorTablesCpu[D3D12_MAX_ROOT_COST] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE descriptorTablesGpu[D3D12_MAX_ROOT_COST] = {};

	const ComputePSO::RootSignatureDesc& rootSigDesc = shader.getRootSignatureDesc();
	for (size_t rootParamIdx = 0; rootParamIdx < rootSigDesc.size(); rootParamIdx++)
	{
		const ComputePSO::RootParameter& rootParam = rootSigDesc[rootParamIdx];
		if (rootParam.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			uint32_t& heapOffset = rootParam.isSamplerDescriptorTable ? samplerDescriptorHeapOffset : cbvSrvUavDescriptorHeapOffset;
			uint32_t tableOffset = heapOffset;
			heapOffset += rootParam.numDescriptors;
			ID3D12DescriptorHeap* heap = rootParam.isSamplerDescriptorTable ? samplerDescriptorHeap.Get() : cbvSrvUavDescriptorHeap.Get();
			uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(
				rootParam.isSamplerDescriptorTable ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorTablesCpu[rootParamIdx] = { heap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize * tableOffset };
			descriptorTablesGpu[rootParamIdx] = { heap->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize * tableOffset };
		}
	}

	cmdList->SetComputeRootSignature(shader.getRootSignature());

	auto bindResources = [this, shader, &descriptorTablesCpu]<typename T>(std::initializer_list<T> resources)
	{
		ComputePSO::EBindingType bindingType = {};
		if constexpr (std::is_same_v<T, ID3D12Resource*>)
			bindingType = ComputePSO::EBindingType::kCbv;
		else if constexpr (std::is_same_v<T, const ShaderResourceView*>)
			bindingType = ComputePSO::EBindingType::kSrv;
		else if constexpr (std::is_same_v<T, const UnorderedAccessView*>)
			bindingType = ComputePSO::EBindingType::kUav;
		else if constexpr (std::is_same_v<T, const SamplerState*>)
			bindingType = ComputePSO::EBindingType::kSampler;
		else
			assert(false);

		for (size_t idx = 0; idx < resources.size(); idx++)
		{
			auto& resource = resources.begin()[idx];
			const ComputePSO::Binding* binding = shader.getBinding((uint32_t)idx, bindingType);
			if (!binding)
				continue;

			if (binding->isRootDescriptor)
			{
				if constexpr (std::is_same_v<T, ID3D12Resource*>)
					cmdList->SetComputeRootConstantBufferView(binding->rootParamIdx, resource->GetGPUVirtualAddress());
				else if constexpr (std::is_same_v<T, const ShaderResourceView*>)
					cmdList->SetComputeRootShaderResourceView(binding->rootParamIdx, resource->resource->GetGPUVirtualAddress());
				else if constexpr (std::is_same_v<T, const UnorderedAccessView*>)
					cmdList->SetComputeRootUnorderedAccessView(binding->rootParamIdx, resource->resource->GetGPUVirtualAddress());
				else
					static_assert("Unknown type");
			}
			else
			{
				uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(
					bindingType == ComputePSO::EBindingType::kSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				D3D12_CPU_DESCRIPTOR_HANDLE descriptorAddr = { descriptorTablesCpu[binding->rootParamIdx].ptr + binding->descriptorOffset * descriptorSize };

				if constexpr (std::is_same_v<T, ID3D12Resource*>)
				{
					D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
						.BufferLocation = resource->GetGPUVirtualAddress(),
						.SizeInBytes = (UINT)resource->GetDesc().Width };
					device->CreateConstantBufferView(&desc, descriptorAddr);
				}
				else if constexpr (std::is_same_v<T, const ShaderResourceView*>)
				{
					device->CreateShaderResourceView(
						resource->resource,
						resource->desc.has_value() ? &resource->desc.value() : nullptr,
						descriptorAddr);
				}
				else if constexpr (std::is_same_v<T, const UnorderedAccessView*>)
				{
					device->CreateUnorderedAccessView(
						resource->resource,
						nullptr,
						resource->desc.has_value() ? &resource->desc.value() : nullptr,
						descriptorAddr);
				}
				else if constexpr (std::is_same_v<T, const SamplerState*>)
				{
					device->CreateSampler(&resource->samplerDesc, descriptorAddr);
				}
				else
					static_assert("Unknown type");
			}
		}
	};

	bindResources(cbs);
	bindResources(srvs);
	bindResources(uavs);
	bindResources(samplers);

	for (size_t rootParamIdx = 0; rootParamIdx < rootSigDesc.size(); rootParamIdx++)
	{
		const ComputePSO::RootParameter& rootParam = rootSigDesc[rootParamIdx];
		if (rootParam.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			cmdList->SetComputeRootDescriptorTable((UINT)rootParamIdx, descriptorTablesGpu[rootParamIdx]);
	}

	cmdList->SetPipelineState(shader.getPso());
	uint3 groups = divRoundUp(resolution, groupSize);
	cmdList->Dispatch(groups.x, groups.y, groups.z);

	D3D12_RESOURCE_BARRIER barrier =
	{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV
	};
	cmdList->ResourceBarrier(1, &barrier);
}

void DirectXDevice::presentFrame()
{
	uint32_t firstIdx = frameFirstQuery % queries.size();
	uint32_t remain = queryCounter - frameFirstQuery;
	while (remain)
	{
		uint32_t num = remain;
		if (firstIdx + remain > queries.size())
			num = queries.size() - firstIdx;

		cmdList->ResolveQueryData(
			queryHeap.Get(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			firstIdx * 2,
			num * 2,
			queryResultBuffer.Get(),
			firstIdx * sizeof(uint64_t) * 2);

		firstIdx = (firstIdx + num) % queries.size();
		remain -= num;
	}
	cmdList->Close();

	auto cmdListToSubmit = (ID3D12CommandList*)cmdList.Get();
	cmdQueue->ExecuteCommandLists(1, &cmdListToSubmit);

	const bool vsync = false;
	swapChain->Present(vsync ? 1 : 0, 0);

	cmdQueue->Signal(fence.Get(), ++fenceLastSignalVal);
	HRESULT hr = fence->SetEventOnCompletion(fenceLastSignalVal, fenceEvent);
	assert(SUCCEEDED(hr));
	WaitForSingleObject(fenceEvent, INFINITE);
}

QueryHandle DirectXDevice::startPerformanceQuery(unsigned id, const std::string& name)
{
	PIXBeginEvent(cmdList.Get(), 0xffff00ff, name.c_str());
	
	uint32_t queryIndex = queryCounter % queries.size();
	PerformanceQuery& query = queries[queryIndex];
	
	query.id = id;
	query.name = name;

	cmdList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex * 2);

	return {queryCounter++};
}

void DirectXDevice::endPerformanceQuery(QueryHandle queryHandle)
{
	cmdList->EndQuery(
		queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		(queryHandle.queryIndex % queries.size()) * 2 + 1);

	PIXEndEvent(cmdList.Get());
}

void DirectXDevice::processPerformanceResults(const std::function<void(float, unsigned, std::string&)>& functor)
{
	uint64_t* results = nullptr;
	HRESULT hr = queryResultBuffer->Map(0, nullptr, (void**)&results);
	assert(SUCCEEDED(hr));

	uint64_t frequency;
	hr = cmdQueue->GetTimestampFrequency(&frequency);
	assert(SUCCEEDED(hr));

	for (uint32_t idx = frameFirstQuery; idx < queryCounter; idx++)
	{
		uint32_t queryIdx = idx % queries.size();
		PerformanceQuery& query = queries[queryIdx];
		uint64_t start = results[queryIdx * 2];
		uint64_t end = results[queryIdx * 2 + 1];

		UINT64 d = end - start;
		float delta = (float(d) / float(frequency)) * 1000.0f;

		// Call functor to process results
		functor(delta, query.id, query.name);
	}

	queryResultBuffer->Unmap(0, nullptr);
}
