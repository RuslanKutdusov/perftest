#define LOAD_WIDTH 4
#define LOAD_LINEAR
#define ROOT_DESCRIPTOR
StructuredBuffer<float4> sourceData : register(t0);
#include "loadStructuredBody.hlsli"
