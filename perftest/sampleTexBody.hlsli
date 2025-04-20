#include "hash.hlsli"
#include "loadConstantsGPU.h"

RWBuffer<float> output : register(u0);

cbuffer CB0 : register(b0)
{
	LoadConstants loadConstants;
};

#define ROOT_SIGNATURE \
	"DescriptorTable(" \
		"CBV(b0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
		"SRV(t0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
		"UAV(u0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)),"\
	"DescriptorTable(Sampler(s0))"

#define THREAD_GROUP_DIM 16

groupshared float dummyLDS[THREAD_GROUP_DIM][THREAD_GROUP_DIM];

[RootSignature(ROOT_SIGNATURE)]
[numthreads(THREAD_GROUP_DIM, THREAD_GROUP_DIM, 1)]
void main(uint3 tid : SV_DispatchThreadID, uint3 gid : SV_GroupThreadID)
{
	float4 value = 0.0;
	
#if defined(LOAD_INVARIANT)
    // All threads load from same address. Index is wave invariant.
	uint2 htid = 0;
#elif defined(LOAD_LINEAR)
	// Linearly increasing starting address.
	uint2 htid = gid.xy;
#elif defined(LOAD_RANDOM)
    // Randomize start address offset (0-3, 0-3)
	uint2 htid = uint2((hash1(gid.x) & 0x4), (hash1(gid.y) & 0x4));
#endif

	const float2 invTextureDims = 1.0f / float2(32.0f, 32.0f);
	const float2 texCenter = invTextureDims * 0.5;

	[loop]
	for (int y = 0; y < 16; ++y)
	{
		[loop]
		for (int x = 0; x < 16; ++x)
		{
			// Mask with runtime constant to prevent unwanted compiler optimizations
			uint2 elemIdx = (htid + uint2(x, y)) | loadConstants.elementsMask;

			float2 uv = float2(elemIdx) * invTextureDims + texCenter;

#if LOAD_WIDTH == 1
			value += sourceData.SampleLevel(texSampler, uv, 0.0f).xxxx;
#elif LOAD_WIDTH == 2
			value += sourceData.SampleLevel(texSampler, uv, 0.0f).xyxy;
#elif LOAD_WIDTH == 4
			value += sourceData.SampleLevel(texSampler, uv, 0.0f).xyzw;
#endif
		}
	}
    // Linear write to LDS (no bank conflicts). Significantly faster than memory loads.
	dummyLDS[gid.y][gid.x] = value.x + value.y + value.z + value.w;

	GroupMemoryBarrierWithGroupSync();

	// This branch is never taken, but the compiler doesn't know it
	// Optimizer would remove all the memory loads if the data wouldn't be potentially used
	[branch]
	if (loadConstants.writeIndex != 0xffffffff)
	{
        output[tid.x + tid.y] = dummyLDS[(loadConstants.writeIndex >> 8) & 0xff][loadConstants.writeIndex & 0xff];
    }
}
