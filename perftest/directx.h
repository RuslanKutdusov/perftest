#pragma once
#include "datatypes.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <wrl.h>
#include <dxgidebug.h>
#include <unordered_map>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct QueryHandle
{
	unsigned queryIndex;
};

struct PerformanceQuery
{
	unsigned id;
	std::string name;
};

class SamplerState
{
public:
	SamplerState() = default;
	SamplerState(const D3D12_SAMPLER_DESC& samplerDesc)
		: samplerDesc(samplerDesc)
	{}

	const D3D12_SAMPLER_DESC samplerDesc = {};
};

class ShaderResourceView
{
public:
	ShaderResourceView() = default;
	ShaderResourceView(ID3D12Resource* resource, std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> desc)
		: resource(resource), desc(desc)
	{}

	ID3D12Resource* const resource = {};
	const std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> desc = {};
};

class UnorderedAccessView
{
public:
	UnorderedAccessView() = default;
	UnorderedAccessView(ID3D12Resource* resource, std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> desc)
		: resource(resource), desc(desc)
	{}

	ID3D12Resource* const resource = {};
	const std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> desc = {};
};

class ComputePSO
{
public:
	struct Binding
	{
		uint32_t rootParamIdx;
		bool isRootDescriptor;
		uint32_t descriptorOffset;
	};

	enum class EBindingType
	{
		kCbv = 0,
		kSrv,
		kUav,
		kSampler,
		kCount
	};

	struct RootParameter
	{
		D3D12_ROOT_PARAMETER_TYPE type;
		uint32_t numDescriptors;
		bool isSamplerDescriptorTable;
	};

	using RootSignatureDesc = std::vector<RootParameter>;

	ComputePSO() = delete;
	ComputePSO(ID3D12Device* device, const std::string& name, const std::vector<unsigned char>& shaderBytes);

	ID3D12PipelineState* getPso() const { return pso.Get(); }
	ID3D12RootSignature* getRootSignature() const { return rootSig.Get(); }
	const RootSignatureDesc& getRootSignatureDesc() const { return rootSignatureDesc; }
	const Binding* getBinding(uint32_t slot, EBindingType type) const;

private:
	ComPtr<ID3D12PipelineState> pso;
	ComPtr<ID3D12RootSignature> rootSig;
	RootSignatureDesc rootSignatureDesc;
	std::unordered_map<uint32_t, Binding> bindings[(int)EBindingType::kCount];
};

std::vector<ComPtr<IDXGIAdapter>> enumerateAdapters();

class DirectXDevice
{
public:
	enum class SamplerType
	{
		Nearest,
		Bilinear,
		Trilinear
	};

	DirectXDevice(HWND window, uint2 resolution, IDXGIAdapter* adapter = nullptr);
	~DirectXDevice();

	// Create resources
	ComputePSO createComputeShader(const std::string& name, const std::vector<unsigned char>& shaderBytes);

	ComPtr<ID3D12Resource> createConstantBuffer(unsigned bytes);
	ComPtr<ID3D12Resource> createBuffer(unsigned numElements, unsigned strideBytes);
	ComPtr<ID3D12Resource> createTexture2d(uint2 dimensions, DXGI_FORMAT format, unsigned mips);
	ComPtr<ID3D12Resource> createTexture3d(uint3 dimensions, DXGI_FORMAT format, unsigned mips);
	SamplerState createSampler(SamplerType type);

	UnorderedAccessView createUAV(ID3D12Resource* resource);
	UnorderedAccessView createTypedUAV(ID3D12Resource* buffer, unsigned numElements, DXGI_FORMAT format);
	UnorderedAccessView createByteAddressUAV(ID3D12Resource* buffer, unsigned numElements);

	ShaderResourceView createSRV(ID3D12Resource* resource);
	ShaderResourceView createTypedSRV(ID3D12Resource* buffer, unsigned numElements, DXGI_FORMAT format);
	ShaderResourceView createStructuredSRV(ID3D12Resource* buffer, unsigned numElements, unsigned stride);
	ShaderResourceView createByteAddressSRV(ID3D12Resource* buffer, unsigned numElements);

	// Data update
	template <typename T>
	void updateConstantBuffer(ID3D12Resource* cbuffer, const T& cb)
	{
		void* ptr = nullptr;
		cbuffer->Map(0, nullptr, &ptr);
		memcpy(ptr, &cb, sizeof(cb));
		cbuffer->Unmap(0, nullptr);
	}

	// Commands
	void beginFrame();
	void dispatch(
		const ComputePSO& shader,
		uint3 resolution,
		uint3 groupSize,
		std::initializer_list<ID3D12Resource*> cbs,
		std::initializer_list<const ShaderResourceView*> srvs,
		std::initializer_list<const UnorderedAccessView*> uavs = {},
		std::initializer_list<const SamplerState*> samplers = {});
	void presentFrame();

	// Performance querys
	QueryHandle startPerformanceQuery(unsigned id, const std::string& name);
	void endPerformanceQuery(QueryHandle queryHandle);
	void processPerformanceResults(const std::function<void(float, unsigned, std::string&)>& functor);

	// Device and window
	HWND getWindowHandle() { return windowHandle; }
	uint2 getResolution() { return resolution; }
	ID3D12Device* getDevice() { return device.Get(); }
	ID3D12GraphicsCommandList* getCmdList() { return cmdList.Get(); }

private:

	// Window
	HWND windowHandle;
	uint2 resolution;

	// DirectX
	ComPtr<ID3D12Debug1> debugInterface;
	ComPtr<IDXGISwapChain1> swapChain;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> cmdQueue;
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceLastSignalVal;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	ComPtr<ID3D12QueryHeap> queryHeap;
	ComPtr<ID3D12Resource> queryResultBuffer;
	ComPtr<ID3D12DescriptorHeap> cbvSrvUavDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> samplerDescriptorHeap;
	uint32_t cbvSrvUavDescriptorHeapOffset = 0;
	uint32_t samplerDescriptorHeapOffset = 0;

	// Queries
	std::array<PerformanceQuery, 4096> queries;
	unsigned queryCounter = 0;
	unsigned frameFirstQuery = 0;
};
