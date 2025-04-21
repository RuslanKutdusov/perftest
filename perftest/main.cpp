#include "window.h"
#include "directx.h"
#include "graphicsUtil.h"
#include "loadConstantsGPU.h"
#include <map>
#include <cmath>

class BenchTest
{
public:
	BenchTest(DirectXDevice& dx, const UnorderedAccessView& output) : dx(dx), output(output), testCaseNumber(0)
	{
	}

	void testCase(ComputePSO& shader, ID3D12Resource* cb, const ShaderResourceView& source, const std::string& name)
	{
		const uint3 workloadThreadCount(1024, 1024, 1);
		const uint3 workloadGroupSize(256, 1, 1);

		QueryHandle query = dx.startPerformanceQuery(testCaseNumber, name);
		dx.dispatch(shader, workloadThreadCount, workloadGroupSize, { cb }, { &source }, { &output }, {});
		dx.endPerformanceQuery(query);

		testCaseNumber++;
	}

	void testCaseWithSampler(ComputePSO& shader, ID3D12Resource* cb, const ShaderResourceView& source, const SamplerState& sampler, const std::string& name)
	{
		const uint3 workloadThreadCount(1024, 1024, 1);
		const uint3 workloadGroupSize(256, 1, 1);

		QueryHandle query = dx.startPerformanceQuery(testCaseNumber, name);
		dx.dispatch(shader, workloadThreadCount, workloadGroupSize, { cb }, { &source }, { &output }, { &sampler });
		dx.endPerformanceQuery(query);

		testCaseNumber++;
	}

private:
	DirectXDevice& dx;
	const UnorderedAccessView& output;
	unsigned testCaseNumber;
};


int main(int argc, char *argv[])
{
	// Enumerate adapters
	std::vector<ComPtr<IDXGIAdapter>> adapters = enumerateAdapters();
	printf("PerfTest\nTo select adapter, use: PerfTest.exe [ADAPTER_INDEX]\n\n");
	printf("Adapters found:\n");
	int index = 0;
	for (auto&& adapter : adapters)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		printf("%d: %S\n", index++, desc.Description);
	}

	// Command line index can be used to select adapter
	int selectedAdapterIdx = 0;
	if (argc == 2)
	{
		selectedAdapterIdx = std::stoi(argv[1]);
		selectedAdapterIdx = min(max(0, selectedAdapterIdx), (int)adapters.size() - 1);
	}
	printf("Using adapter %d\n", selectedAdapterIdx);

	// Init systems
	uint2 resolution(256, 256);
	HWND window = createWindow(resolution);
	DirectXDevice dx(window, resolution, adapters[selectedAdapterIdx].Get());

	// Load shaders 
	printf("Loading shaders...");
	ComputePSO shaderLoadTyped1dInvariant = loadComputeShader(dx, "shaders/loadTyped1dInvariant.cso");
	ComputePSO shaderLoadTyped1dLinear = loadComputeShader(dx, "shaders/loadTyped1dLinear.cso");
	ComputePSO shaderLoadTyped1dRandom = loadComputeShader(dx, "shaders/loadTyped1dRandom.cso");
	ComputePSO shaderLoadTyped2dInvariant = loadComputeShader(dx, "shaders/loadTyped2dInvariant.cso");
	ComputePSO shaderLoadTyped2dLinear = loadComputeShader(dx, "shaders/loadTyped2dLinear.cso");
	ComputePSO shaderLoadTyped2dRandom = loadComputeShader(dx, "shaders/loadTyped2dRandom.cso");
	ComputePSO shaderLoadTyped4dInvariant = loadComputeShader(dx, "shaders/loadTyped4dInvariant.cso");
	ComputePSO shaderLoadTyped4dLinear = loadComputeShader(dx, "shaders/loadTyped4dLinear.cso");
	ComputePSO shaderLoadTyped4dRandom = loadComputeShader(dx, "shaders/loadTyped4dRandom.cso");

	ComputePSO shaderLoadRaw1dInvariant = loadComputeShader(dx, "shaders/loadRaw1dInvariant.cso");
	ComputePSO shaderLoadRaw1dInvariantRoot = loadComputeShader(dx, "shaders/loadRaw1dInvariantRoot.cso");
	ComputePSO shaderLoadRaw1dLinear = loadComputeShader(dx, "shaders/loadRaw1dLinear.cso");
	ComputePSO shaderLoadRaw1dLinearRoot = loadComputeShader(dx, "shaders/loadRaw1dLinearRoot.cso");
	ComputePSO shaderLoadRaw1dRandom = loadComputeShader(dx, "shaders/loadRaw1dRandom.cso");
	ComputePSO shaderLoadRaw1dRandomRoot = loadComputeShader(dx, "shaders/loadRaw1dRandomRoot.cso");
	ComputePSO shaderLoadRaw2dInvariant = loadComputeShader(dx, "shaders/loadRaw2dInvariant.cso");
	ComputePSO shaderLoadRaw2dInvariantRoot = loadComputeShader(dx, "shaders/loadRaw2dInvariantRoot.cso");
	ComputePSO shaderLoadRaw2dLinear = loadComputeShader(dx, "shaders/loadRaw2dLinear.cso");
	ComputePSO shaderLoadRaw2dLinearRoot = loadComputeShader(dx, "shaders/loadRaw2dLinearRoot.cso");
	ComputePSO shaderLoadRaw2dRandom = loadComputeShader(dx, "shaders/loadRaw2dRandom.cso");
	ComputePSO shaderLoadRaw2dRandomRoot = loadComputeShader(dx, "shaders/loadRaw2dRandomRoot.cso");
	ComputePSO shaderLoadRaw3dInvariant = loadComputeShader(dx, "shaders/loadRaw3dInvariant.cso");
	ComputePSO shaderLoadRaw3dInvariantRoot = loadComputeShader(dx, "shaders/loadRaw3dInvariantRoot.cso");
	ComputePSO shaderLoadRaw3dLinear = loadComputeShader(dx, "shaders/loadRaw3dLinear.cso");
	ComputePSO shaderLoadRaw3dLinearRoot = loadComputeShader(dx, "shaders/loadRaw3dLinearRoot.cso");
	ComputePSO shaderLoadRaw3dRandom = loadComputeShader(dx, "shaders/loadRaw3dRandom.cso");
	ComputePSO shaderLoadRaw3dRandomRoot = loadComputeShader(dx, "shaders/loadRaw3dRandomRoot.cso");
	ComputePSO shaderLoadRaw4dInvariant = loadComputeShader(dx, "shaders/loadRaw4dInvariant.cso");
	ComputePSO shaderLoadRaw4dInvariantRoot = loadComputeShader(dx, "shaders/loadRaw4dInvariantRoot.cso");
	ComputePSO shaderLoadRaw4dLinear = loadComputeShader(dx, "shaders/loadRaw4dLinear.cso");
	ComputePSO shaderLoadRaw4dLinearRoot = loadComputeShader(dx, "shaders/loadRaw4dLinearRoot.cso");
	ComputePSO shaderLoadRaw4dRandom = loadComputeShader(dx, "shaders/loadRaw4dRandom.cso");
	ComputePSO shaderLoadRaw4dRandomRoot = loadComputeShader(dx, "shaders/loadRaw4dRandomRoot.cso");

	ComputePSO shaderLoadTex1dInvariant = loadComputeShader(dx, "shaders/loadTex1dInvariant.cso");
	ComputePSO shaderLoadTex1dLinear = loadComputeShader(dx, "shaders/loadTex1dLinear.cso");
	ComputePSO shaderLoadTex1dRandom = loadComputeShader(dx, "shaders/loadTex1dRandom.cso");
	ComputePSO shaderLoadTex2dInvariant = loadComputeShader(dx, "shaders/loadTex2dInvariant.cso");
	ComputePSO shaderLoadTex2dLinear = loadComputeShader(dx, "shaders/loadTex2dLinear.cso");
	ComputePSO shaderLoadTex2dRandom = loadComputeShader(dx, "shaders/loadTex2dRandom.cso");
	ComputePSO shaderLoadTex4dInvariant = loadComputeShader(dx, "shaders/loadTex4dInvariant.cso");
	ComputePSO shaderLoadTex4dLinear = loadComputeShader(dx, "shaders/loadTex4dLinear.cso");
	ComputePSO shaderLoadTex4dRandom = loadComputeShader(dx, "shaders/loadTex4dRandom.cso");

	ComputePSO shaderSampleTex1dInvariant = loadComputeShader(dx, "shaders/sampleTex1dInvariant.cso");
	ComputePSO shaderSampleTex1dLinear = loadComputeShader(dx, "shaders/sampleTex1dLinear.cso");
	ComputePSO shaderSampleTex1dRandom = loadComputeShader(dx, "shaders/sampleTex1dRandom.cso");
	ComputePSO shaderSampleTex2dInvariant = loadComputeShader(dx, "shaders/sampleTex2dInvariant.cso");
	ComputePSO shaderSampleTex2dLinear = loadComputeShader(dx, "shaders/sampleTex2dLinear.cso");
	ComputePSO shaderSampleTex2dRandom = loadComputeShader(dx, "shaders/sampleTex2dRandom.cso");
	ComputePSO shaderSampleTex4dInvariant = loadComputeShader(dx, "shaders/sampleTex4dInvariant.cso");
	ComputePSO shaderSampleTex4dLinear = loadComputeShader(dx, "shaders/sampleTex4dLinear.cso");
	ComputePSO shaderSampleTex4dRandom = loadComputeShader(dx, "shaders/sampleTex4dRandom.cso");

	ComputePSO shaderLoadConstant4dInvariant = loadComputeShader(dx, "shaders/loadConstant4dInvariant.cso");
	ComputePSO shaderLoadConstant4dLinear = loadComputeShader(dx, "shaders/loadConstant4dLinear.cso");
	ComputePSO shaderLoadConstant4dRandom = loadComputeShader(dx, "shaders/loadConstant4dRandom.cso");
	ComputePSO shaderLoadConstant4dInvariantRoot = loadComputeShader(dx, "shaders/loadConstant4dInvariantRoot.cso");
	ComputePSO shaderLoadConstant4dLinearRoot = loadComputeShader(dx, "shaders/loadConstant4dLinearRoot.cso");
	ComputePSO shaderLoadConstant4dRandomRoot = loadComputeShader(dx, "shaders/loadConstant4dRandomRoot.cso");

	ComputePSO shaderLoadStructured1dInvariant = loadComputeShader(dx, "shaders/loadStructured1dInvariant.cso");
	ComputePSO shaderLoadStructured1dInvariantRoot = loadComputeShader(dx, "shaders/loadStructured1dInvariantRoot.cso");
	ComputePSO shaderLoadStructured1dLinear = loadComputeShader(dx, "shaders/loadStructured1dLinear.cso");
	ComputePSO shaderLoadStructured1dLinearRoot = loadComputeShader(dx, "shaders/loadStructured1dLinearRoot.cso");
	ComputePSO shaderLoadStructured1dRandom = loadComputeShader(dx, "shaders/loadStructured1dRandom.cso");
	ComputePSO shaderLoadStructured1dRandomRoot = loadComputeShader(dx, "shaders/loadStructured1dRandomRoot.cso");
	ComputePSO shaderLoadStructured2dInvariant = loadComputeShader(dx, "shaders/loadStructured2dInvariant.cso");
	ComputePSO shaderLoadStructured2dInvariantRoot = loadComputeShader(dx, "shaders/loadStructured2dInvariantRoot.cso");
	ComputePSO shaderLoadStructured2dLinear = loadComputeShader(dx, "shaders/loadStructured2dLinear.cso");
	ComputePSO shaderLoadStructured2dLinearRoot = loadComputeShader(dx, "shaders/loadStructured2dLinearRoot.cso");
	ComputePSO shaderLoadStructured2dRandom = loadComputeShader(dx, "shaders/loadStructured2dRandom.cso");
	ComputePSO shaderLoadStructured2dRandomRoot = loadComputeShader(dx, "shaders/loadStructured2dRandomRoot.cso");
	ComputePSO shaderLoadStructured4dInvariant = loadComputeShader(dx, "shaders/loadStructured4dInvariant.cso");
	ComputePSO shaderLoadStructured4dInvariantRoot = loadComputeShader(dx, "shaders/loadStructured4dInvariantRoot.cso");
	ComputePSO shaderLoadStructured4dLinear = loadComputeShader(dx, "shaders/loadStructured4dLinear.cso");
	ComputePSO shaderLoadStructured4dLinearRoot = loadComputeShader(dx, "shaders/loadStructured4dLinearRoot.cso");
	ComputePSO shaderLoadStructured4dRandom = loadComputeShader(dx, "shaders/loadStructured4dRandom.cso");
	ComputePSO shaderLoadStructured4dRandomRoot = loadComputeShader(dx, "shaders/loadStructured4dRandomRoot.cso");
	printf(" Done\n");
	// Create buffers and output UAV
	ComPtr<ID3D12Resource> bufferOutput = dx.createBuffer(2048, 4);
	ComPtr<ID3D12Resource> bufferInput = dx.createBuffer(1024, 16);
	ComPtr<ID3D12Resource> bufferInputStructured4 = dx.createBuffer(1024, 4);
	ComPtr<ID3D12Resource> bufferInputStructured8 = dx.createBuffer(1024, 8);
	ComPtr<ID3D12Resource> bufferInputStructured16 = dx.createBuffer(1024, 16);
	UnorderedAccessView outputUAV = dx.createTypedUAV(bufferOutput.Get(), 2048, DXGI_FORMAT_R32_FLOAT);

	// SRVs for benchmarking different buffer view formats/types
	ShaderResourceView typedSRV_R8 = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R8_UNORM);
	ShaderResourceView typedSRV_R16F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R16_FLOAT);
	ShaderResourceView typedSRV_R32F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R32_FLOAT);
	ShaderResourceView typedSRV_RG8 = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R8G8_UNORM);
	ShaderResourceView typedSRV_RG16F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R16G16_FLOAT);
	ShaderResourceView typedSRV_RG32F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R32G32_FLOAT);
	ShaderResourceView typedSRV_RGBA8 = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R8G8B8A8_UNORM);
	ShaderResourceView typedSRV_RGBA16F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R16G16B16A16_FLOAT);
	ShaderResourceView typedSRV_RGBA32F = dx.createTypedSRV(bufferInput.Get(), 1024, DXGI_FORMAT_R32G32B32A32_FLOAT);
	ShaderResourceView structuredSRV_R32F = dx.createStructuredSRV(bufferInputStructured4.Get(), 1024, 4);
	ShaderResourceView structuredSRV_RG32F = dx.createStructuredSRV(bufferInputStructured8.Get(), 1024, 8);
	ShaderResourceView structuredSRV_RGBA32F = dx.createStructuredSRV(bufferInputStructured16.Get(), 1024, 16);
	ShaderResourceView byteAddressSRV = dx.createByteAddressSRV(bufferInput.Get(), 1024);

	// Create input textures
	ComPtr<ID3D12Resource> texR8 = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R8_UNORM, 1);
	ComPtr<ID3D12Resource> texR16F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R16_FLOAT, 1);
	ComPtr<ID3D12Resource> texR32F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R32_FLOAT, 1);
	ComPtr<ID3D12Resource> texRG8 = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R8G8_UNORM, 1);
	ComPtr<ID3D12Resource> texRG16F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R16G16_FLOAT, 1);
	ComPtr<ID3D12Resource> texRG32F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R32G32_FLOAT, 1);
	ComPtr<ID3D12Resource> texRGBA8 = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
	ComPtr<ID3D12Resource> texRGBA16F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R16G16B16A16_FLOAT, 1);
	ComPtr<ID3D12Resource> texRGBA32F = dx.createTexture2d(uint2(32, 32), DXGI_FORMAT_R32G32B32A32_FLOAT, 1);
	
	// Texture SRVs
	ShaderResourceView texSRV_R8 = dx.createSRV(texR8.Get());
	ShaderResourceView texSRV_R16F = dx.createSRV(texR16F.Get());
	ShaderResourceView texSRV_R32F = dx.createSRV(texR32F.Get());
	ShaderResourceView texSRV_RG8 = dx.createSRV(texRG8.Get());
	ShaderResourceView texSRV_RG16F = dx.createSRV(texRG16F.Get());
	ShaderResourceView texSRV_RG32F = dx.createSRV(texRG32F.Get());
	ShaderResourceView texSRV_RGBA8 = dx.createSRV(texRGBA8.Get());
	ShaderResourceView texSRV_RGBA16F = dx.createSRV(texRGBA16F.Get());
	ShaderResourceView texSRV_RGBA32F = dx.createSRV(texRGBA32F.Get());

	// Samplers
	SamplerState samplerNearest = dx.createSampler(DirectXDevice::SamplerType::Nearest);
	SamplerState samplerBilinear = dx.createSampler(DirectXDevice::SamplerType::Bilinear);
	SamplerState samplerTrilinear = dx.createSampler(DirectXDevice::SamplerType::Trilinear);

	// Setup the constant buffer
	LoadConstants loadConstants;
	ComPtr<ID3D12Resource> loadCB = dx.createConstantBuffer(sizeof(LoadConstants));
	ComPtr<ID3D12Resource> loadCBUnaligned = dx.createConstantBuffer(sizeof(LoadConstants));
	loadConstants.elementsMask = 0;				// Dummy mask to prevent unwanted compiler optimizations
	loadConstants.writeIndex = 0xffffffff;		// Never write
	loadConstants.readStartAddress = 0;			// Aligned
	dx.updateConstantBuffer(loadCB.Get(), loadConstants);
	loadConstants.readStartAddress = 4;			// Unaligned
	dx.updateConstantBuffer(loadCBUnaligned.Get(), loadConstants);
	
	// Setup constant buffer with float4 array for constant buffer load benchmarking
	LoadConstantsWithArray loadConstantsWithArray;
	ComPtr<ID3D12Resource> loadWithArrayCB = dx.createConstantBuffer(sizeof(LoadConstantsWithArray));
	loadConstantsWithArray.elementsMask = 0;				// Dummy mask to prevent unwanted compiler optimizations
	loadConstantsWithArray.writeIndex = 0xffffffff;			// Never write
	loadConstantsWithArray.readStartAddress = 0;			// Aligned
	memset(loadConstantsWithArray.benchmarkArray, 0, sizeof(loadConstantsWithArray.benchmarkArray));
	dx.updateConstantBuffer(loadWithArrayCB.Get(), loadConstantsWithArray);

	const unsigned numWarmUpFramesBeforeBenchmark = 30;
	const unsigned numBenchmarkFrames = 30;
	const unsigned maxTestCases = 200;

	printf("\nRunning %d warm-up frames and %d benchmark frames:\n", numWarmUpFramesBeforeBenchmark, numBenchmarkFrames);

	struct TestCaseTiming
	{
		std::string name;
		float totalTime = 0.0f;
		std::vector<float> timings;
	};

	std::array<TestCaseTiming, maxTestCases> timingResults;

	// Frame loop
	MessageStatus status = MessageStatus::Default;
	unsigned frameNumber = 0;
	do
	{
		dx.processPerformanceResults([&](float timeMillis, unsigned id, std::string& name)
		{
			if (frameNumber >= numWarmUpFramesBeforeBenchmark)
			{
				if (timingResults[id].name == "")
				{
					timingResults[id] = { name, 0 };
				}
				timingResults[id].totalTime += timeMillis;
				timingResults[id].timings.push_back(timeMillis);
			}
		});

		dx.beginFrame();

		BenchTest bench(dx, outputUAV);

		bench.testCase(shaderLoadTyped1dInvariant, loadCB.Get(), typedSRV_R8, "Buffer<R8>.Load uniform");
		bench.testCase(shaderLoadTyped1dLinear, loadCB.Get(), typedSRV_R8, "Buffer<R8>.Load linear");
		bench.testCase(shaderLoadTyped1dRandom, loadCB.Get(), typedSRV_R8, "Buffer<R8>.Load random");
		bench.testCase(shaderLoadTyped2dInvariant, loadCB.Get(), typedSRV_RG8, "Buffer<RG8>.Load uniform");
		bench.testCase(shaderLoadTyped2dLinear, loadCB.Get(), typedSRV_RG8, "Buffer<RG8>.Load linear");
		bench.testCase(shaderLoadTyped2dRandom, loadCB.Get(), typedSRV_RG8, "Buffer<RG8>.Load random");
		bench.testCase(shaderLoadTyped4dInvariant, loadCB.Get(), typedSRV_RGBA8, "Buffer<RGBA8>.Load uniform");
		bench.testCase(shaderLoadTyped4dLinear, loadCB.Get(), typedSRV_RGBA8, "Buffer<RGBA8>.Load linear");
		bench.testCase(shaderLoadTyped4dRandom, loadCB.Get(), typedSRV_RGBA8, "Buffer<RGBA8>.Load random");

		bench.testCase(shaderLoadTyped1dInvariant, loadCB.Get(), typedSRV_R16F, "Buffer<R16f>.Load uniform");
		bench.testCase(shaderLoadTyped1dLinear, loadCB.Get(), typedSRV_R16F, "Buffer<R16f>.Load linear");
		bench.testCase(shaderLoadTyped1dRandom, loadCB.Get(), typedSRV_R16F, "Buffer<R16f>.Load random");
		bench.testCase(shaderLoadTyped2dInvariant, loadCB.Get(), typedSRV_RG16F, "Buffer<RG16f>.Load uniform");
		bench.testCase(shaderLoadTyped2dLinear, loadCB.Get(), typedSRV_RG16F, "Buffer<RG16f>.Load linear");
		bench.testCase(shaderLoadTyped2dRandom, loadCB.Get(), typedSRV_RG16F, "Buffer<RG16f>.Load random");
		bench.testCase(shaderLoadTyped4dInvariant, loadCB.Get(), typedSRV_RGBA16F, "Buffer<RGBA16f>.Load uniform");
		bench.testCase(shaderLoadTyped4dLinear, loadCB.Get(), typedSRV_RGBA16F, "Buffer<RGBA16f>.Load linear");
		bench.testCase(shaderLoadTyped4dRandom, loadCB.Get(), typedSRV_RGBA16F, "Buffer<RGBA16f>.Load random");

		bench.testCase(shaderLoadTyped1dInvariant, loadCB.Get(), typedSRV_R32F, "Buffer<R32f>.Load uniform");
		bench.testCase(shaderLoadTyped1dLinear, loadCB.Get(), typedSRV_R32F, "Buffer<R32f>.Load linear");
		bench.testCase(shaderLoadTyped1dRandom, loadCB.Get(), typedSRV_R32F, "Buffer<R32f>.Load random");
		bench.testCase(shaderLoadTyped2dInvariant, loadCB.Get(), typedSRV_RG32F, "Buffer<RG32f>.Load uniform");
		bench.testCase(shaderLoadTyped2dLinear, loadCB.Get(), typedSRV_RG32F, "Buffer<RG32f>.Load linear");
		bench.testCase(shaderLoadTyped2dRandom, loadCB.Get(), typedSRV_RG32F, "Buffer<RG32f>.Load random");
		bench.testCase(shaderLoadTyped4dInvariant, loadCB.Get(), typedSRV_RGBA32F, "Buffer<RGBA32f>.Load uniform");
		bench.testCase(shaderLoadTyped4dLinear, loadCB.Get(), typedSRV_RGBA32F, "Buffer<RGBA32f>.Load linear");
		bench.testCase(shaderLoadTyped4dRandom, loadCB.Get(), typedSRV_RGBA32F, "Buffer<RGBA32f>.Load random");

		bench.testCase(shaderLoadRaw1dInvariant, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load uniform");
		bench.testCase(shaderLoadRaw1dLinear, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load linear");
		bench.testCase(shaderLoadRaw1dRandom, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load random");
		bench.testCase(shaderLoadRaw2dInvariant, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 uniform");
		bench.testCase(shaderLoadRaw2dLinear, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 linear");
		bench.testCase(shaderLoadRaw2dRandom, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 random");
		bench.testCase(shaderLoadRaw3dInvariant, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load3 uniform");
		bench.testCase(shaderLoadRaw3dLinear, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load3 linear");
		bench.testCase(shaderLoadRaw3dRandom, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load3 random");
		bench.testCase(shaderLoadRaw4dInvariant, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 uniform");
		bench.testCase(shaderLoadRaw4dLinear, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 linear");
		bench.testCase(shaderLoadRaw4dRandom, loadCB.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 random");

		bench.testCase(shaderLoadRaw2dInvariant, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 unaligned uniform");
		bench.testCase(shaderLoadRaw2dLinear, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 unaligned linear");
		bench.testCase(shaderLoadRaw2dRandom, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load2 unaligned random");
		bench.testCase(shaderLoadRaw4dInvariant, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 unaligned uniform");
		bench.testCase(shaderLoadRaw4dLinear, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 unaligned linear");
		bench.testCase(shaderLoadRaw4dRandom, loadCBUnaligned.Get(), byteAddressSRV, "ByteAddressBuffer.Load4 unaligned random");

		bench.testCase(shaderLoadRaw1dInvariantRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load uniform");
		bench.testCase(shaderLoadRaw1dLinearRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load linear");
		bench.testCase(shaderLoadRaw1dRandomRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load random");
		bench.testCase(shaderLoadRaw2dInvariantRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load2 uniform");
		bench.testCase(shaderLoadRaw2dLinearRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load2 linear");
		bench.testCase(shaderLoadRaw2dRandomRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load2 random");
		bench.testCase(shaderLoadRaw3dInvariantRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load3 uniform");
		bench.testCase(shaderLoadRaw3dLinearRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load3 linear");
		bench.testCase(shaderLoadRaw3dRandomRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load3 random");
		bench.testCase(shaderLoadRaw4dInvariantRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load4 uniform");
		bench.testCase(shaderLoadRaw4dLinearRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load4 linear");
		bench.testCase(shaderLoadRaw4dRandomRoot, loadCB.Get(), byteAddressSRV, "ByteAddressBufferRoot.Load4 random");

		bench.testCase(shaderLoadStructured1dInvariant, loadCB.Get(), structuredSRV_R32F, "StructuredBuffer<float>.Load uniform");
		bench.testCase(shaderLoadStructured1dLinear, loadCB.Get(), structuredSRV_R32F, "StructuredBuffer<float>.Load linear");
		bench.testCase(shaderLoadStructured1dRandom, loadCB.Get(), structuredSRV_R32F, "StructuredBuffer<float>.Load random");
		bench.testCase(shaderLoadStructured2dInvariant, loadCB.Get(), structuredSRV_RG32F, "StructuredBuffer<float2>.Load uniform");
		bench.testCase(shaderLoadStructured2dLinear, loadCB.Get(), structuredSRV_RG32F, "StructuredBuffer<float2>.Load linear");
		bench.testCase(shaderLoadStructured2dRandom, loadCB.Get(), structuredSRV_RG32F, "StructuredBuffer<float2>.Load random");
		bench.testCase(shaderLoadStructured4dInvariant, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBuffer<float4>.Load uniform");
		bench.testCase(shaderLoadStructured4dLinear, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBuffer<float4>.Load linear");
		bench.testCase(shaderLoadStructured4dRandom, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBuffer<float4>.Load random");

		bench.testCase(shaderLoadStructured1dInvariantRoot, loadCB.Get(), structuredSRV_R32F, "StructuredBufferRoot<float>.Load uniform");
		bench.testCase(shaderLoadStructured1dLinearRoot, loadCB.Get(), structuredSRV_R32F, "StructuredBufferRoot<float>.Load linear");
		bench.testCase(shaderLoadStructured1dRandomRoot, loadCB.Get(), structuredSRV_R32F, "StructuredBufferRoot<float>.Load random");
		bench.testCase(shaderLoadStructured2dInvariantRoot, loadCB.Get(), structuredSRV_RG32F, "StructuredBufferRoot<float2>.Load uniform");
		bench.testCase(shaderLoadStructured2dLinearRoot, loadCB.Get(), structuredSRV_RG32F, "StructuredBufferRoot<float2>.Load linear");
		bench.testCase(shaderLoadStructured2dRandomRoot, loadCB.Get(), structuredSRV_RG32F, "StructuredBufferRoot<float2>.Load random");
		bench.testCase(shaderLoadStructured4dInvariantRoot, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBufferRoot<float4>.Load uniform");
		bench.testCase(shaderLoadStructured4dLinearRoot, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBufferRoot<float4>.Load linear");
		bench.testCase(shaderLoadStructured4dRandomRoot, loadCB.Get(), structuredSRV_RGBA32F, "StructuredBufferRoot<float4>.Load random");

		bench.testCase(shaderLoadConstant4dInvariant, loadWithArrayCB.Get(), {}, "cbuffer{float4} load uniform");
		bench.testCase(shaderLoadConstant4dLinear, loadWithArrayCB.Get(), {}, "cbuffer{float4} load linear");
		bench.testCase(shaderLoadConstant4dRandom, loadWithArrayCB.Get(), {}, "cbuffer{float4} load random");

		bench.testCase(shaderLoadConstant4dInvariantRoot, loadWithArrayCB.Get(), {}, "cbuffer_root{float4} load uniform");
		bench.testCase(shaderLoadConstant4dLinearRoot, loadWithArrayCB.Get(), {}, "cbuffer_root{float4} load linear");
		bench.testCase(shaderLoadConstant4dRandomRoot, loadWithArrayCB.Get(), {}, "cbuffer_root{float4} load random");

		bench.testCase(shaderLoadTex1dInvariant, loadCB.Get(), texSRV_R8, "Texture2D<R8>.Load uniform");
		bench.testCase(shaderLoadTex1dLinear, loadCB.Get(), texSRV_R8, "Texture2D<R8>.Load linear");
		bench.testCase(shaderLoadTex1dRandom, loadCB.Get(), texSRV_R8, "Texture2D<R8>.Load random");
		bench.testCase(shaderLoadTex2dInvariant, loadCB.Get(), texSRV_RG8, "Texture2D<RG8>.Load uniform");
		bench.testCase(shaderLoadTex2dLinear, loadCB.Get(), texSRV_RG8, "Texture2D<RG8>.Load linear");
		bench.testCase(shaderLoadTex2dRandom, loadCB.Get(), texSRV_RG8, "Texture2D<RG8>.Load random");
		bench.testCase(shaderLoadTex4dInvariant, loadCB.Get(), texSRV_RGBA8, "Texture2D<RGBA8>.Load uniform");
		bench.testCase(shaderLoadTex4dLinear, loadCB.Get(), texSRV_RGBA8, "Texture2D<RGBA8>.Load linear");
		bench.testCase(shaderLoadTex4dRandom, loadCB.Get(), texSRV_RGBA8, "Texture2D<RGBA8>.Load random");

		bench.testCase(shaderLoadTex1dInvariant, loadCB.Get(), texSRV_R16F, "Texture2D<R16F>.Load uniform");
		bench.testCase(shaderLoadTex1dLinear, loadCB.Get(), texSRV_R16F, "Texture2D<R16F>.Load linear");
		bench.testCase(shaderLoadTex1dRandom, loadCB.Get(), texSRV_R16F, "Texture2D<R16F>.Load random");
		bench.testCase(shaderLoadTex2dInvariant, loadCB.Get(), texSRV_RG16F, "Texture2D<RG16F>.Load uniform");
		bench.testCase(shaderLoadTex2dLinear, loadCB.Get(), texSRV_RG16F, "Texture2D<RG16F>.Load linear");
		bench.testCase(shaderLoadTex2dRandom, loadCB.Get(), texSRV_RG16F, "Texture2D<RG16F>.Load random");
		bench.testCase(shaderLoadTex4dInvariant, loadCB.Get(), texSRV_RGBA16F, "Texture2D<RGBA16F>.Load uniform");
		bench.testCase(shaderLoadTex4dLinear, loadCB.Get(), texSRV_RGBA16F, "Texture2D<RGBA16F>.Load linear");
		bench.testCase(shaderLoadTex4dRandom, loadCB.Get(), texSRV_RGBA16F, "Texture2D<RGBA16F>.Load random");

		bench.testCase(shaderLoadTex1dInvariant, loadCB.Get(), texSRV_R32F, "Texture2D<R32F>.Load uniform");
		bench.testCase(shaderLoadTex1dLinear, loadCB.Get(), texSRV_R32F, "Texture2D<R32F>.Load linear");
		bench.testCase(shaderLoadTex1dRandom, loadCB.Get(), texSRV_R32F, "Texture2D<R32F>.Load random");
		bench.testCase(shaderLoadTex2dInvariant, loadCB.Get(), texSRV_RG32F, "Texture2D<RG32F>.Load uniform");
		bench.testCase(shaderLoadTex2dLinear, loadCB.Get(), texSRV_RG32F, "Texture2D<RG32F>.Load linear");
		bench.testCase(shaderLoadTex2dRandom, loadCB.Get(), texSRV_RG32F, "Texture2D<RG32F>.Load random");
		bench.testCase(shaderLoadTex4dInvariant, loadCB.Get(), texSRV_RGBA32F, "Texture2D<RGBA32F>.Load uniform");
		bench.testCase(shaderLoadTex4dLinear, loadCB.Get(), texSRV_RGBA32F, "Texture2D<RGBA32F>.Load linear");
		bench.testCase(shaderLoadTex4dRandom, loadCB.Get(), texSRV_RGBA32F, "Texture2D<RGBA32F>.Load random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R8, samplerNearest, "Texture2D<R8>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R8, samplerNearest, "Texture2D<R8>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R8, samplerNearest, "Texture2D<R8>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG8, samplerNearest, "Texture2D<RG8>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG8, samplerNearest, "Texture2D<RG8>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG8, samplerNearest, "Texture2D<RG8>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA8, samplerNearest, "Texture2D<RGBA8>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA8, samplerNearest, "Texture2D<RGBA8>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA8, samplerNearest, "Texture2D<RGBA8>.Sample(nearest) random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R16F, samplerNearest, "Texture2D<R16F>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R16F, samplerNearest, "Texture2D<R16F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R16F, samplerNearest, "Texture2D<R16F>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG16F, samplerNearest, "Texture2D<RG16F>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG16F, samplerNearest, "Texture2D<RG16F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG16F, samplerNearest, "Texture2D<RG16F>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA16F, samplerNearest, "Texture2D<RGBA16F>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA16F, samplerNearest, "Texture2D<RGBA16F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA16F, samplerNearest, "Texture2D<RGBA16F>.Sample(nearest) random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R32F, samplerNearest, "Texture2D<R32F>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R32F, samplerNearest, "Texture2D<R32F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R32F, samplerNearest, "Texture2D<R32F>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG32F, samplerNearest, "Texture2D<RG32F>.Sample(nearest) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG32F, samplerNearest, "Texture2D<RG32F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG32F, samplerNearest, "Texture2D<RG32F>.Sample(nearest) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA32F, samplerNearest, "Texture2D<RGBA32F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA32F, samplerNearest, "Texture2D<RGBA32F>.Sample(nearest) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA32F, samplerNearest, "Texture2D<RGBA32F>.Sample(nearest) random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R8, samplerBilinear, "Texture2D<R8>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R8, samplerBilinear, "Texture2D<R8>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R8, samplerBilinear, "Texture2D<R8>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG8, samplerBilinear, "Texture2D<RG8>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG8, samplerBilinear, "Texture2D<RG8>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG8, samplerBilinear, "Texture2D<RG8>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA8, samplerBilinear, "Texture2D<RGBA8>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA8, samplerBilinear, "Texture2D<RGBA8>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA8, samplerBilinear, "Texture2D<RGBA8>.Sample(bilinear) random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R16F, samplerBilinear, "Texture2D<R16F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R16F, samplerBilinear, "Texture2D<R16F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R16F, samplerBilinear, "Texture2D<R16F>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG16F, samplerBilinear, "Texture2D<RG16F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG16F, samplerBilinear, "Texture2D<RG16F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG16F, samplerBilinear, "Texture2D<RG16F>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA16F, samplerBilinear, "Texture2D<RGBA16F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA16F, samplerBilinear, "Texture2D<RGBA16F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA16F, samplerBilinear, "Texture2D<RGBA16F>.Sample(bilinear) random");

		bench.testCaseWithSampler(shaderSampleTex1dInvariant, loadCB.Get(), texSRV_R32F, samplerBilinear, "Texture2D<R32F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex1dLinear, loadCB.Get(), texSRV_R32F, samplerBilinear, "Texture2D<R32F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex1dRandom, loadCB.Get(), texSRV_R32F, samplerBilinear, "Texture2D<R32F>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex2dInvariant, loadCB.Get(), texSRV_RG32F, samplerBilinear, "Texture2D<RG32F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex2dLinear, loadCB.Get(), texSRV_RG32F, samplerBilinear, "Texture2D<RG32F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex2dRandom, loadCB.Get(), texSRV_RG32F, samplerBilinear, "Texture2D<RG32F>.Sample(bilinear) random");
		bench.testCaseWithSampler(shaderSampleTex4dInvariant, loadCB.Get(), texSRV_RGBA32F, samplerBilinear, "Texture2D<RGBA32F>.Sample(bilinear) uniform");
		bench.testCaseWithSampler(shaderSampleTex4dLinear, loadCB.Get(), texSRV_RGBA32F, samplerBilinear, "Texture2D<RGBA32F>.Sample(bilinear) linear");
		bench.testCaseWithSampler(shaderSampleTex4dRandom, loadCB.Get(), texSRV_RGBA32F, samplerBilinear, "Texture2D<RGBA32F>.Sample(bilinear) random");

		dx.presentFrame();

		status = messagePump();

		frameNumber++;
		if (frameNumber < numWarmUpFramesBeforeBenchmark)
		{
			printf(".");
		}
		else
		{
			printf("X");
		}
	}
	while (status != MessageStatus::Exit && frameNumber < numBenchmarkFrames + numWarmUpFramesBeforeBenchmark);

	// Find comparison case
	float compareToTime = 1.0f;
	std::string compareToCase = "Buffer<RGBA8>.Load random";
	printf("\n\nPerformance compared to %s\n\n", compareToCase.c_str());
	for (auto&& row : timingResults)
	{
		if (row.name == compareToCase)
		{
			compareToTime = row.totalTime;
			break;
		}
	}

	// Print results
	for (auto&& row : timingResults)
	{
		if (row.name == "") break;
		float average = row.totalTime / row.timings.size();
		float stdDev = 0.0f;
		for (float t : row.timings)
			stdDev += std::powf(t - average, 2.0f);
		stdDev = std::sqrtf(stdDev / row.timings.size());
		printf(
			"%s: %.3fms %.3fms %.3fms %.3fx\n",
			row.name.c_str(),
			row.totalTime,
			average,
			stdDev,
			compareToTime / row.totalTime);
	}

	return 0;
} 