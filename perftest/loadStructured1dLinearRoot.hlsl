#define LOAD_WIDTH 1
#define LOAD_LINEAR
#define ROOT_DESCRIPTOR
StructuredBuffer<float> sourceData : register(t0);
#include "loadStructuredBody.hlsli"
