#include "hash.hlsli"
#include "loadConstantsGPU.h"

RWBuffer<float> output : register(u0);

cbuffer CB0 : register(b0)
{
	LoadConstantsWithArray loadConstants;
};

#if defined(ROOT_DESCRIPTOR)
	#define ROOT_SIGNATURE \
		"DescriptorTable(" \
			"UAV(u0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE))," \
		"CBV(b0, flags = DATA_VOLATILE)"
#else
	#define ROOT_SIGNATURE \
		"DescriptorTable(" \
			"CBV(b0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
			"UAV(u0, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE))"
#endif

#define THREAD_GROUP_SIZE 256

groupshared float dummyLDS[THREAD_GROUP_SIZE];

[RootSignature(ROOT_SIGNATURE)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID, uint gix : SV_GroupIndex)
{
	float4 value = 0.0;
	
#if defined(LOAD_INVARIANT)
    // All threads load from same address. Index is wave invariant.
	uint htid = 0;
#elif defined(LOAD_LINEAR)
	// Linearly increasing starting address to allow memory coalescing
	uint htid = gix;
#elif defined(LOAD_RANDOM)
    // Randomize start address offset (0-15) to prevent memory coalescing
	uint htid = (hash1(gix) & 0xf);
#endif

	[loop]
	for (int i = 0; i < 256; ++i)
	{
		// Mask with runtime constant to prevent unwanted compiler optimizations
		uint elemIdx = (htid + i) | loadConstants.elementsMask;

		value += loadConstants.benchmarkArray[elemIdx].xyzw;
	}

    // Linear write to LDS (no bank conflicts). Significantly faster than memory loads.
	dummyLDS[gix] = value.x + value.y + value.z + value.w;

	GroupMemoryBarrierWithGroupSync();

	// This branch is never taken, but the compiler doesn't know it
	// Optimizer would remove all the memory loads if the data wouldn't be potentially used
	[branch]
	if (loadConstants.writeIndex != 0xffffffff)
	{
        output[tid.x + tid.y] = dummyLDS[loadConstants.writeIndex];
    }
}
