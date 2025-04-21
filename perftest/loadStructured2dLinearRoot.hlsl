#define LOAD_WIDTH 2
#define LOAD_LINEAR
#define ROOT_DESCRIPTOR
StructuredBuffer<float2> sourceData : register(t0);
#include "loadStructuredBody.hlsli"
