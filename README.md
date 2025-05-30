# PerfTest

A simple GPU shader memory operation performance test tool. Current implementation is DirectX 11.0 based.

The purpose of this application is not to benchmark different brand GPUs against each other. Its purpose is to help rendering programmers to choose right types of resources when optimizing their compute shader performance.

This application is designed to measure peak data load performance from L1 caches. I tried to avoid known hardware bottlenecks. **If you notice something wrong or suspicious in the shader workload, please inform me immediately and I will fix it.** If my shaders are affected by some hardware bottlenecks, I am glad to hear about it and write more test cases to show the best performance. The goal is that developers gain better understanding of various GPU hardware on the market and gain insight to optimize code for them.

## Features

Designed to measure performance of various types of buffer and image loads. This application is not a GPU memory bandwidth measurement tool. All tests operate inside GPUs L1 caches (no larger than 16 KB working sets). 

- Coalesced loads (100% L1 cache hit)
- Random loads (100% L1 cache hit)
- Uniform address loads (same address for all threads)
- Typed Buffer SRVs: 1/2/4 channels, 8/16/32 bits per channel
- ByteAddressBuffer SRVs: load, load2, load3, load4 - aligned and unaligned
- Structured Buffer SRVs: float/float2/float4
- Constant Buffer float4 array indexed loads
- Texture2D loads: 1/2/4 channels, 8/16/32 bits per channel
- Texture2D nearest sampling: 1/2/4 channels, 8/16/32 bits per channel
- Texture2D bilinear sampling: 1/2/4 channels, 8/16/32 bits per channel

## Explanations

**Coalesced loads:**
GPUs optimize linear address patterns. Coalescing occurs when all threads in a warp/wave (32/64 threads) load from contiguous addresses. In my "linear" test case, memory loads access contiguous addresses in the whole thread group (256 threads). This should coalesce perfectly on all GPUs, independent of warp/wave width.

**Random loads:**
I add a random start offset of 0-15 elements for each thread (still aligned). This prevents GPU coalescing, and provides more realistic view of performance for common case (non-linear) memory accessing. This benchmark is as cache efficient as the previous. All data still comes from the L1 cache.

**Uniform loads:**
All threads in group simultaneously load from the same address. This triggers coalesced path on some GPUs and additonal optimizations on some GPUs, such as scalar loads (SGPR storage) on AMD GCN. I have noticed that recent Intel and Nvidia drivers also implement a software optimization for uniform load loop case (which is employed by this benchmark).

**Notes:**
**Compiler optimizations** can ruin the results. We want to measure only load (read) performance, but write (store) is also needed, otherwise the compiler will just optimize the whole shader away. To avoid this, each thread does first 256 loads followed by a single linear groupshared memory write (no bank-conflicts). Cbuffer contains a write mask (not known at compile time). It controls which elements are written from the groupshared memory to the output buffer. The mask is always zero at runtime. Compilers can also combine multiple narrow raw buffer loads together (as bigger 4d loads) if it an be proven at compile time that loads from the same thread access contiguous offsets. This is prevented by applying an address mask from cbuffer (not known at compile time). 

## Uniform Load Investigation
When I first implemented this benchmark, I noticed that Intel uniform address loads were surprisingly fast. Intel ISA documents don't mention anything about a scalar unit or other hardware feature to make uniform address loads fast. This optimization affected every single resource type, unlike AMDs hardware scalar unit (which only works for raw data loads). I didnt't investigate this further however at that point. When Nvidia released Volta GPUs, they brought new driver that implemented similar compiler optimization. Later drivers introduced the same optimization to Maxwell and Pascal too. And now Turing also has it. It's certainly not hardware based, since 20x+ gains apply to all their existing GPUs too.

In Nov 10-11 weekend (2018) I was toying around with Vulkan/DX12 wave intrinsics, and came up with a crazy idea to use a single wave wide load and then use wave intrinsics to broadcast scalar result (single lane) to each loop iteration. This results in up to wave width reduced amount of loads. 

See the gist and Shader Playground links here:
https://gist.github.com/sebbbi/ba4415339b535d22fb18e2d824564ec4

In Nvidia's uniform load optimization case, their wave width = 32, and their uniform load optimization performance boost is up to 28x. This finding really made me curious. Could Nvidia implement a similar warp shuffle based optimization for this use case? The funny thing is that my tweets escalated the situation, and made Intel reveal their hand:

https://twitter.com/JoshuaBarczak/status/1062060067334189056

Intel has now officially revealed that their driver does a wave shuffle optimization for uniform address loads. They have been doing it for years already. This explains Intel GPU benchmark results perfectly. Now that we have confirmation of Intel's (original) optimization, I suspect that Nvidia's shader compiler employs a highly similar optimization in this case. Both optimizations are great, because Nvidia/Intel do not have a dedicated scalar unit. They need to lean more on vector loads, and this trick allows sharing one vector load with multiple uniform address load loop iterations.

## Results
All results are compared to ```Buffer<RGBA8>.Load random``` result (=1.0x) on the same GPU.

### AMD GCN2 (R9 390X)
```markdown
Buffer<R8>.Load uniform: 11.302ms 3.907x
Buffer<R8>.Load linear: 11.327ms 3.899x
Buffer<R8>.Load random: 44.150ms 1.000x
Buffer<RG8>.Load uniform: 49.611ms 0.890x
Buffer<RG8>.Load linear: 49.835ms 0.886x
Buffer<RG8>.Load random: 49.615ms 0.890x
Buffer<RGBA8>.Load uniform: 44.149ms 1.000x
Buffer<RGBA8>.Load linear: 44.806ms 0.986x
Buffer<RGBA8>.Load random: 44.164ms 1.000x
Buffer<R16f>.Load uniform: 11.131ms 3.968x
Buffer<R16f>.Load linear: 11.139ms 3.965x
Buffer<R16f>.Load random: 44.076ms 1.002x
Buffer<RG16f>.Load uniform: 49.552ms 0.891x
Buffer<RG16f>.Load linear: 49.560ms 0.891x
Buffer<RG16f>.Load random: 49.559ms 0.891x
Buffer<RGBA16f>.Load uniform: 44.066ms 1.002x
Buffer<RGBA16f>.Load linear: 44.687ms 0.988x
Buffer<RGBA16f>.Load random: 44.066ms 1.002x
Buffer<R32f>.Load uniform: 11.132ms 3.967x
Buffer<R32f>.Load linear: 11.139ms 3.965x
Buffer<R32f>.Load random: 44.071ms 1.002x
Buffer<RG32f>.Load uniform: 49.558ms 0.891x
Buffer<RG32f>.Load linear: 49.560ms 0.891x
Buffer<RG32f>.Load random: 49.559ms 0.891x
Buffer<RGBA32f>.Load uniform: 44.061ms 1.002x
Buffer<RGBA32f>.Load linear: 44.613ms 0.990x
Buffer<RGBA32f>.Load random: 49.583ms 0.891x
ByteAddressBuffer.Load uniform: 10.322ms 4.278x
ByteAddressBuffer.Load linear: 11.546ms 3.825x
ByteAddressBuffer.Load random: 44.153ms 1.000x
ByteAddressBuffer.Load2 uniform: 11.499ms 3.841x
ByteAddressBuffer.Load2 linear: 49.628ms 0.890x
ByteAddressBuffer.Load2 random: 49.651ms 0.889x
ByteAddressBuffer.Load3 uniform: 16.985ms 2.600x
ByteAddressBuffer.Load3 linear: 44.142ms 1.000x
ByteAddressBuffer.Load3 random: 88.176ms 0.501x
ByteAddressBuffer.Load4 uniform: 22.472ms 1.965x
ByteAddressBuffer.Load4 linear: 44.212ms 0.999x
ByteAddressBuffer.Load4 random: 49.346ms 0.895x
ByteAddressBuffer.Load2 unaligned uniform: 11.422ms 3.867x
ByteAddressBuffer.Load2 unaligned linear: 49.552ms 0.891x
ByteAddressBuffer.Load2 unaligned random: 49.561ms 0.891x
ByteAddressBuffer.Load4 unaligned uniform: 22.373ms 1.974x
ByteAddressBuffer.Load4 unaligned linear: 44.095ms 1.002x
ByteAddressBuffer.Load4 unaligned random: 54.464ms 0.811x
StructuredBuffer<float>.Load uniform: 12.585ms 3.509x
StructuredBuffer<float>.Load linear: 11.770ms 3.752x
StructuredBuffer<float>.Load random: 44.176ms 1.000x
StructuredBuffer<float2>.Load uniform: 13.210ms 3.343x
StructuredBuffer<float2>.Load linear: 50.217ms 0.879x
StructuredBuffer<float2>.Load random: 49.645ms 0.890x
StructuredBuffer<float4>.Load uniform: 13.818ms 3.196x
StructuredBuffer<float4>.Load random: 49.666ms 0.889x
StructuredBuffer<float4>.Load linear: 44.721ms 0.988x
cbuffer{float4} load uniform: 16.702ms 2.644x
cbuffer{float4} load linear: 44.447ms 0.994x
cbuffer{float4} load random: 49.656ms 0.889x
Texture2D<R8>.Load uniform: 44.214ms 0.999x
Texture2D<R8>.Load linear: 44.795ms 0.986x
Texture2D<R8>.Load random: 44.808ms 0.986x
Texture2D<RG8>.Load uniform: 49.706ms 0.888x
Texture2D<RG8>.Load linear: 50.231ms 0.879x
Texture2D<RG8>.Load random: 50.200ms 0.880x
Texture2D<RGBA8>.Load uniform: 44.760ms 0.987x
Texture2D<RGBA8>.Load linear: 45.339ms 0.974x
Texture2D<RGBA8>.Load random: 45.405ms 0.973x
Texture2D<R16F>.Load uniform: 44.175ms 1.000x
Texture2D<R16F>.Load linear: 44.157ms 1.000x
Texture2D<R16F>.Load random: 44.096ms 1.002x
Texture2D<RG16F>.Load uniform: 49.739ms 0.888x
Texture2D<RG16F>.Load linear: 49.661ms 0.889x
Texture2D<RG16F>.Load random: 49.622ms 0.890x
Texture2D<RGBA16F>.Load uniform: 44.257ms 0.998x
Texture2D<RGBA16F>.Load linear: 44.267ms 0.998x
Texture2D<RGBA16F>.Load random: 88.126ms 0.501x
Texture2D<R32F>.Load uniform: 44.259ms 0.998x
Texture2D<R32F>.Load linear: 44.193ms 0.999x
Texture2D<R32F>.Load random: 44.099ms 1.001x
Texture2D<RG32F>.Load uniform: 49.739ms 0.888x
Texture2D<RG32F>.Load linear: 49.667ms 0.889x
Texture2D<RG32F>.Load random: 88.110ms 0.501x
Texture2D<RGBA32F>.Load uniform: 44.288ms 0.997x
Texture2D<RGBA32F>.Load linear: 66.145ms 0.668x
Texture2D<RGBA32F>.Load random: 88.124ms 0.501x
```
**AMD GCN2** was a very popular architecture. First card using this architecture was Radeon 7790. Many Radeon 200 and 300 series cards also use this architecture. Both Xbox and PS4 (base model) GPUs are based on GCN2 architecture, making this architecture very important optimization target.

**Typed loads:** GCN coalesces linear typed loads. But only 1d loads (R8, R16F, R32F). Coalesced load performance is 4x. Both linear access pattern (all threads in wave load subsequent addresses) and uniform access (all threads in wave load the same address) coalesce perfectly. Typed loads of every dimension (1d/2d/4d) and channel width (8b/16b/32b) perform identically. Best bytes/cycle rate can be achieved either by R32 coalesced load (when access pattern suits this) or always with RGBA32 load.

**Raw (ByteAddressBuffer) loads:** Similar to typed loads. 1d raw loads coalesce perfectly (4x) on linear access. Uniform address raw loads generates scalar unit loads on GCN. Scalar loads use separate cache and are stored to separate SGPR register file -> reduced register & cache pressure & doesn't stress vector load path. Scalar 1d load is 4x faster than normal 1d load. Scalar 2d load is 4x faster than normal 2d load. Scalar 4d load is 2x faster than normal 4d load. Unaligned (alignment=4) loads have equal performance to aligned (alignment=8/16). 3d raw linear loads have equal performance to 4d loads, but random 3d loads are slightly slower.

**Texture loads:** Similar performance as typed buffer loads. However no coalescing in 1d linear access and no scalar unit offload of uniform access. Random access of wide formats tends to be slightly slower (but my 2d random produces different access pattern than 1d).

**Structured buffer loads:** Performance is identical to similar width raw buffer loads.

**Cbuffer loads:** AMD GCN architecture doesn't have special constant buffer hardware. Constant buffer load performance is identical to raw and structured buffers. Prefer uniform addresses to allow the compiler to generate scalar loads, which is around 4x faster and has much lower latency and doesn't waste VGPRs.

**Suggestions:** Prefer wide fat 4d loads instead of multiple narrow loads. If you have perfectly linear memory access pattern, 1d coalesced loads are also fast. ByteAddressBuffers (raw loads) have good performance: Full speed 128 bit 4d loads, 4x rate 1d loads (linear access), and the compiler offloads uniform address loads to scalar unit, saving VGPR pressure and vector memory instructions.

These results match with AMDs wide loads & coalescing documents, see: http://gpuopen.com/gcn-memory-coalescing/. I would be glad if AMD released a public document describing all scalar load optimization cases supported by their compiler.

### AMD GCN3 (R9 Fury 56 CU)
```markdown
Buffer<R8>.Load uniform: 8.963ms 3.911x
Buffer<R8>.Load linear: 8.917ms 3.931x
Buffer<R8>.Load random: 35.058ms 1.000x
Buffer<RG8>.Load uniform: 39.416ms 0.889x
Buffer<RG8>.Load linear: 39.447ms 0.889x
Buffer<RG8>.Load random: 39.413ms 0.889x
Buffer<RGBA8>.Load uniform: 35.051ms 1.000x
Buffer<RGBA8>.Load linear: 35.048ms 1.000x
Buffer<RGBA8>.Load random: 35.051ms 1.000x
Buffer<R16f>.Load uniform: 8.898ms 3.939x
Buffer<R16f>.Load linear: 8.909ms 3.934x
Buffer<R16f>.Load random: 35.050ms 1.000x
Buffer<RG16f>.Load uniform: 39.405ms 0.890x
Buffer<RG16f>.Load linear: 39.435ms 0.889x
Buffer<RG16f>.Load random: 39.407ms 0.889x
Buffer<RGBA16f>.Load uniform: 35.041ms 1.000x
Buffer<RGBA16f>.Load linear: 35.043ms 1.000x
Buffer<RGBA16f>.Load random: 35.046ms 1.000x
Buffer<R32f>.Load uniform: 8.897ms 3.940x
Buffer<R32f>.Load linear: 8.910ms 3.934x
Buffer<R32f>.Load random: 35.048ms 1.000x
Buffer<RG32f>.Load uniform: 39.407ms 0.889x
Buffer<RG32f>.Load linear: 39.433ms 0.889x
Buffer<RG32f>.Load random: 39.406ms 0.889x
Buffer<RGBA32f>.Load uniform: 35.043ms 1.000x
Buffer<RGBA32f>.Load linear: 35.045ms 1.000x
Buffer<RGBA32f>.Load random: 39.405ms 0.890x
ByteAddressBuffer.Load uniform: 10.956ms 3.199x
ByteAddressBuffer.Load linear: 9.100ms 3.852x
ByteAddressBuffer.Load random: 35.038ms 1.000x
ByteAddressBuffer.Load2 uniform: 11.070ms 3.166x
ByteAddressBuffer.Load2 linear: 39.413ms 0.889x
ByteAddressBuffer.Load2 random: 39.411ms 0.889x
ByteAddressBuffer.Load3 uniform: 13.534ms 2.590x
ByteAddressBuffer.Load3 linear: 35.047ms 1.000x
ByteAddressBuffer.Load3 random: 70.033ms 0.500x
ByteAddressBuffer.Load4 uniform: 17.944ms 1.953x
ByteAddressBuffer.Load4 linear: 35.072ms 0.999x
ByteAddressBuffer.Load4 random: 39.149ms 0.895x
ByteAddressBuffer.Load2 unaligned uniform: 11.209ms 3.127x
ByteAddressBuffer.Load2 unaligned linear: 39.408ms 0.889x
ByteAddressBuffer.Load2 unaligned random: 39.406ms 0.890x
ByteAddressBuffer.Load4 unaligned uniform: 17.933ms 1.955x
ByteAddressBuffer.Load4 unaligned linear: 35.066ms 1.000x
ByteAddressBuffer.Load4 unaligned random: 43.241ms 0.811x
StructuredBuffer<float>.Load uniform: 12.653ms 2.770x
StructuredBuffer<float>.Load linear: 8.913ms 3.932x
StructuredBuffer<float>.Load random: 35.059ms 1.000x
StructuredBuffer<float2>.Load uniform: 12.799ms 2.739x
StructuredBuffer<float2>.Load linear: 39.445ms 0.889x
StructuredBuffer<float2>.Load random: 39.413ms 0.889x
StructuredBuffer<float4>.Load uniform: 12.834ms 2.731x
StructuredBuffer<float4>.Load linear: 35.049ms 1.000x
StructuredBuffer<float4>.Load random: 39.411ms 0.889x
cbuffer{float4} load uniform: 14.861ms 2.359x
cbuffer{float4} load linear: 35.534ms 0.986x
cbuffer{float4} load random: 39.412ms 0.889x
Texture2D<R8>.Load uniform: 35.063ms 1.000x
Texture2D<R8>.Load linear: 35.038ms 1.000x
Texture2D<R8>.Load random: 35.040ms 1.000x
Texture2D<RG8>.Load uniform: 39.430ms 0.889x
Texture2D<RG8>.Load linear: 39.436ms 0.889x
Texture2D<RG8>.Load random: 39.436ms 0.889x
Texture2D<RGBA8>.Load uniform: 35.059ms 1.000x
Texture2D<RGBA8>.Load linear: 35.061ms 1.000x
Texture2D<RGBA8>.Load random: 35.055ms 1.000x
Texture2D<R16F>.Load uniform: 35.056ms 1.000x
Texture2D<R16F>.Load linear: 35.038ms 1.000x
Texture2D<R16F>.Load random: 35.040ms 1.000x
Texture2D<RG16F>.Load uniform: 39.431ms 0.889x
Texture2D<RG16F>.Load linear: 39.440ms 0.889x
Texture2D<RG16F>.Load random: 39.436ms 0.889x
Texture2D<RGBA16F>.Load uniform: 35.054ms 1.000x
Texture2D<RGBA16F>.Load linear: 35.061ms 1.000x
Texture2D<RGBA16F>.Load random: 70.037ms 0.500x
Texture2D<R32F>.Load uniform: 35.055ms 1.000x
Texture2D<R32F>.Load linear: 35.041ms 1.000x
Texture2D<R32F>.Load random: 35.041ms 1.000x
Texture2D<RG32F>.Load uniform: 39.433ms 0.889x
Texture2D<RG32F>.Load linear: 39.439ms 0.889x
Texture2D<RG32F>.Load random: 70.039ms 0.500x
Texture2D<RGBA32F>.Load uniform: 35.054ms 1.000x
Texture2D<RGBA32F>.Load linear: 52.549ms 0.667x
Texture2D<RGBA32F>.Load random: 70.037ms 0.500x
 ```
  
**AMD GCN3** results (ratios) are identical to GCN2. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### AMD GCN4 (RX 480)
```markdown
Buffer<R8>.Load uniform: 11.008ms 3.900x
Buffer<R8>.Load linear: 11.187ms 3.838x
Buffer<R8>.Load random: 42.906ms 1.001x
Buffer<RG8>.Load uniform: 48.280ms 0.889x
Buffer<RG8>.Load linear: 48.685ms 0.882x
Buffer<RG8>.Load random: 48.246ms 0.890x
Buffer<RGBA8>.Load uniform: 42.911ms 1.001x
Buffer<RGBA8>.Load linear: 43.733ms 0.982x
Buffer<RGBA8>.Load random: 42.934ms 1.000x
Buffer<R16f>.Load uniform: 10.852ms 3.956x
Buffer<R16f>.Load linear: 10.840ms 3.961x
Buffer<R16f>.Load random: 42.820ms 1.003x
Buffer<RG16f>.Load uniform: 48.153ms 0.892x
Buffer<RG16f>.Load linear: 48.161ms 0.891x
Buffer<RG16f>.Load random: 48.161ms 0.891x
Buffer<RGBA16f>.Load uniform: 42.832ms 1.002x
Buffer<RGBA16f>.Load linear: 42.900ms 1.001x
Buffer<RGBA16f>.Load random: 42.844ms 1.002x
Buffer<R32f>.Load uniform: 10.852ms 3.956x
Buffer<R32f>.Load linear: 10.841ms 3.960x
Buffer<R32f>.Load random: 42.816ms 1.003x
Buffer<RG32f>.Load uniform: 48.158ms 0.892x
Buffer<RG32f>.Load linear: 48.161ms 0.891x
Buffer<RG32f>.Load random: 48.161ms 0.891x
Buffer<RGBA32f>.Load uniform: 42.827ms 1.002x
Buffer<RGBA32f>.Load linear: 42.913ms 1.000x
Buffer<RGBA32f>.Load random: 48.176ms 0.891x
ByteAddressBuffer.Load uniform: 13.403ms 3.203x
ByteAddressBuffer.Load linear: 11.118ms 3.862x
ByteAddressBuffer.Load random: 42.911ms 1.001x
ByteAddressBuffer.Load2 uniform: 13.503ms 3.180x
ByteAddressBuffer.Load2 linear: 48.235ms 0.890x
ByteAddressBuffer.Load2 random: 48.242ms 0.890x
ByteAddressBuffer.Load3 uniform: 16.646ms 2.579x
ByteAddressBuffer.Load3 linear: 42.913ms 1.001x
ByteAddressBuffer.Load3 random: 85.682ms 0.501x
ByteAddressBuffer.Load4 uniform: 21.836ms 1.966x
ByteAddressBuffer.Load4 linear: 42.929ms 1.000x
ByteAddressBuffer.Load4 random: 47.936ms 0.896x
ByteAddressBuffer.Load2 unaligned uniform: 13.454ms 3.191x
ByteAddressBuffer.Load2 unaligned linear: 48.150ms 0.892x
ByteAddressBuffer.Load2 unaligned random: 48.163ms 0.891x
ByteAddressBuffer.Load4 unaligned uniform: 21.765ms 1.973x
ByteAddressBuffer.Load4 unaligned linear: 42.853ms 1.002x
ByteAddressBuffer.Load4 unaligned random: 52.866ms 0.812x
StructuredBuffer<float>.Load uniform: 15.513ms 2.768x
StructuredBuffer<float>.Load linear: 10.895ms 3.941x
StructuredBuffer<float>.Load random: 42.885ms 1.001x
StructuredBuffer<float2>.Load uniform: 15.695ms 2.736x
StructuredBuffer<float2>.Load linear: 48.231ms 0.890x
StructuredBuffer<float2>.Load random: 48.217ms 0.890x
StructuredBuffer<float4>.Load uniform: 15.810ms 2.716x
StructuredBuffer<float4>.Load linear: 42.907ms 1.001x
StructuredBuffer<float4>.Load random: 48.224ms 0.890x
cbuffer{float4} load uniform: 17.249ms 2.489x
cbuffer{float4} load linear: 43.054ms 0.997x
cbuffer{float4} load random: 48.214ms 0.890x
Texture2D<R8>.Load uniform: 42.889ms 1.001x
Texture2D<R8>.Load linear: 42.877ms 1.001x
Texture2D<R8>.Load random: 42.889ms 1.001x
Texture2D<RG8>.Load uniform: 48.252ms 0.890x
Texture2D<RG8>.Load linear: 48.254ms 0.890x
Texture2D<RG8>.Load random: 48.254ms 0.890x
Texture2D<RGBA8>.Load uniform: 42.939ms 1.000x
Texture2D<RGBA8>.Load linear: 42.969ms 0.999x
Texture2D<RGBA8>.Load random: 42.945ms 1.000x
Texture2D<R16F>.Load uniform: 42.891ms 1.001x
Texture2D<R16F>.Load linear: 42.915ms 1.000x
Texture2D<R16F>.Load random: 42.866ms 1.002x
Texture2D<RG16F>.Load uniform: 48.234ms 0.890x
Texture2D<RG16F>.Load linear: 48.365ms 0.888x
Texture2D<RG16F>.Load random: 48.220ms 0.890x
Texture2D<RGBA16F>.Load uniform: 42.911ms 1.001x
Texture2D<RGBA16F>.Load linear: 42.943ms 1.000x
Texture2D<RGBA16F>.Load random: 85.655ms 0.501x
Texture2D<R32F>.Load uniform: 42.896ms 1.001x
Texture2D<R32F>.Load linear: 42.910ms 1.001x
Texture2D<R32F>.Load random: 42.871ms 1.001x
Texture2D<RG32F>.Load uniform: 48.239ms 0.890x
Texture2D<RG32F>.Load linear: 48.367ms 0.888x
Texture2D<RG32F>.Load random: 85.634ms 0.501x
Texture2D<RGBA32F>.Load uniform: 42.927ms 1.000x
Texture2D<RGBA32F>.Load linear: 64.284ms 0.668x
Texture2D<RGBA32F>.Load random: 85.638ms 0.501x
```
**AMD GCN4** results (ratios) are identical to GCN2/3. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### AMD GCN5 (Vega Frontier Edition)
```markdown
Buffer<R8>.Load uniform: 6.024ms 3.693x
Buffer<R8>.Load linear: 5.798ms 3.838x
Buffer<R8>.Load random: 21.411ms 1.039x
Buffer<RG8>.Load uniform: 21.648ms 1.028x
Buffer<RG8>.Load linear: 21.108ms 1.054x
Buffer<RG8>.Load random: 21.721ms 1.024x
Buffer<RGBA8>.Load uniform: 22.315ms 0.997x
Buffer<RGBA8>.Load linear: 22.055ms 1.009x
Buffer<RGBA8>.Load random: 22.251ms 1.000x
Buffer<R16f>.Load uniform: 6.421ms 3.465x
Buffer<R16f>.Load linear: 6.119ms 3.636x
Buffer<R16f>.Load random: 21.534ms 1.033x
Buffer<RG16f>.Load uniform: 21.010ms 1.059x
Buffer<RG16f>.Load linear: 20.785ms 1.071x
Buffer<RG16f>.Load random: 20.903ms 1.064x
Buffer<RGBA16f>.Load uniform: 21.083ms 1.055x
Buffer<RGBA16f>.Load linear: 22.849ms 0.974x
Buffer<RGBA16f>.Load random: 22.189ms 1.003x
Buffer<R32f>.Load uniform: 6.374ms 3.491x
Buffer<R32f>.Load linear: 6.265ms 3.552x
Buffer<R32f>.Load random: 21.892ms 1.016x
Buffer<RG32f>.Load uniform: 21.918ms 1.015x
Buffer<RG32f>.Load linear: 21.081ms 1.056x
Buffer<RG32f>.Load random: 22.866ms 0.973x
Buffer<RGBA32f>.Load uniform: 22.022ms 1.010x
Buffer<RGBA32f>.Load linear: 22.025ms 1.010x
Buffer<RGBA32f>.Load random: 24.889ms 0.894x
ByteAddressBuffer.Load uniform: 5.187ms 4.289x
ByteAddressBuffer.Load linear: 6.682ms 3.330x
ByteAddressBuffer.Load random: 22.153ms 1.004x
ByteAddressBuffer.Load2 uniform: 5.907ms 3.767x
ByteAddressBuffer.Load2 linear: 21.541ms 1.033x
ByteAddressBuffer.Load2 random: 22.435ms 0.992x
ByteAddressBuffer.Load3 uniform: 8.896ms 2.501x
ByteAddressBuffer.Load3 linear: 22.019ms 1.011x
ByteAddressBuffer.Load3 random: 43.438ms 0.512x
ByteAddressBuffer.Load4 uniform: 10.671ms 2.085x
ByteAddressBuffer.Load4 linear: 20.912ms 1.064x
ByteAddressBuffer.Load4 random: 23.508ms 0.947x
ByteAddressBuffer.Load2 unaligned uniform: 6.080ms 3.660x
ByteAddressBuffer.Load2 unaligned linear: 21.813ms 1.020x
ByteAddressBuffer.Load2 unaligned random: 22.436ms 0.992x
ByteAddressBuffer.Load4 unaligned uniform: 11.457ms 1.942x
ByteAddressBuffer.Load4 unaligned linear: 21.817ms 1.020x
ByteAddressBuffer.Load4 unaligned random: 27.530ms 0.808x
StructuredBuffer<float>.Load uniform: 6.384ms 3.486x
StructuredBuffer<float>.Load linear: 6.314ms 3.524x
StructuredBuffer<float>.Load random: 21.424ms 1.039x
StructuredBuffer<float2>.Load uniform: 6.257ms 3.556x
StructuredBuffer<float2>.Load linear: 20.940ms 1.063x
StructuredBuffer<float2>.Load random: 23.044ms 0.966x
StructuredBuffer<float4>.Load uniform: 6.620ms 3.361x
StructuredBuffer<float4>.Load linear: 21.771ms 1.022x
StructuredBuffer<float4>.Load random: 25.229ms 0.882x
cbuffer{float4} load uniform: 8.011ms 2.778x
cbuffer{float4} load linear: 22.951ms 0.969x
cbuffer{float4} load random: 24.806ms 0.897x
Texture2D<R8>.Load uniform: 22.585ms 0.985x
Texture2D<R8>.Load linear: 21.733ms 1.024x
Texture2D<R8>.Load random: 21.371ms 1.041x
Texture2D<RG8>.Load uniform: 20.774ms 1.071x
Texture2D<RG8>.Load linear: 20.806ms 1.069x
Texture2D<RG8>.Load random: 22.936ms 0.970x
Texture2D<RGBA8>.Load uniform: 22.022ms 1.010x
Texture2D<RGBA8>.Load linear: 21.644ms 1.028x
Texture2D<RGBA8>.Load random: 22.586ms 0.985x
Texture2D<R16F>.Load uniform: 22.620ms 0.984x
Texture2D<R16F>.Load linear: 22.730ms 0.979x
Texture2D<R16F>.Load random: 21.356ms 1.042x
Texture2D<RG16F>.Load uniform: 20.722ms 1.074x
Texture2D<RG16F>.Load linear: 20.723ms 1.074x
Texture2D<RG16F>.Load random: 21.893ms 1.016x
Texture2D<RGBA16F>.Load uniform: 22.287ms 0.998x
Texture2D<RGBA16F>.Load linear: 22.116ms 1.006x
Texture2D<RGBA16F>.Load random: 42.739ms 0.521x
Texture2D<R32F>.Load uniform: 21.325ms 1.043x
Texture2D<R32F>.Load linear: 21.370ms 1.041x
Texture2D<R32F>.Load random: 21.393ms 1.040x
Texture2D<RG32F>.Load uniform: 20.747ms 1.072x
Texture2D<RG32F>.Load linear: 20.754ms 1.072x
Texture2D<RG32F>.Load random: 41.415ms 0.537x
Texture2D<RGBA32F>.Load uniform: 20.551ms 1.083x
Texture2D<RGBA32F>.Load linear: 31.748ms 0.701x
Texture2D<RGBA32F>.Load random: 42.097ms 0.529x
```
**AMD GCN5** results (ratios) are identical to GCN2/3/4. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### AMD GCN5 7nm (Radeon VII)
```markdown
Buffer<R8>.Load uniform: 5.214ms 3.667x
Buffer<R8>.Load linear: 5.332ms 3.586x
Buffer<R8>.Load random: 18.861ms 1.014x
Buffer<RG8>.Load uniform: 18.917ms 1.011x
Buffer<RG8>.Load linear: 18.904ms 1.011x
Buffer<RG8>.Load random: 18.885ms 1.012x
Buffer<RGBA8>.Load uniform: 18.882ms 1.013x
Buffer<RGBA8>.Load linear: 19.074ms 1.002x
Buffer<RGBA8>.Load random: 19.119ms 1.000x
Buffer<R16f>.Load uniform: 5.335ms 3.584x
Buffer<R16f>.Load linear: 5.547ms 3.447x
Buffer<R16f>.Load random: 18.872ms 1.013x
Buffer<RG16f>.Load uniform: 19.080ms 1.002x
Buffer<RG16f>.Load linear: 18.911ms 1.011x
Buffer<RG16f>.Load random: 18.996ms 1.007x
Buffer<RGBA16f>.Load uniform: 18.879ms 1.013x
Buffer<RGBA16f>.Load linear: 19.340ms 0.989x
Buffer<RGBA16f>.Load random: 18.985ms 1.007x
Buffer<R32f>.Load uniform: 5.337ms 3.582x
Buffer<R32f>.Load linear: 5.548ms 3.446x
Buffer<R32f>.Load random: 18.873ms 1.013x
Buffer<RG32f>.Load uniform: 19.130ms 0.999x
Buffer<RG32f>.Load linear: 18.934ms 1.010x
Buffer<RG32f>.Load random: 19.100ms 1.001x
Buffer<RGBA32f>.Load uniform: 18.880ms 1.013x
Buffer<RGBA32f>.Load linear: 19.383ms 0.986x
Buffer<RGBA32f>.Load random: 21.310ms 0.897x
ByteAddressBuffer.Load uniform: 4.285ms 4.462x
ByteAddressBuffer.Load linear: 5.542ms 3.450x
ByteAddressBuffer.Load random: 18.869ms 1.013x
ByteAddressBuffer.Load2 uniform: 5.209ms 3.671x
ByteAddressBuffer.Load2 linear: 19.266ms 0.992x
ByteAddressBuffer.Load2 random: 19.005ms 1.006x
ByteAddressBuffer.Load3 uniform: 7.454ms 2.565x
ByteAddressBuffer.Load3 linear: 19.190ms 0.996x
ByteAddressBuffer.Load3 random: 37.705ms 0.507x
ByteAddressBuffer.Load4 uniform: 9.604ms 1.991x
ByteAddressBuffer.Load4 linear: 19.455ms 0.983x
ByteAddressBuffer.Load4 random: 21.360ms 0.895x
ByteAddressBuffer.Load2 unaligned uniform: 5.083ms 3.761x
ByteAddressBuffer.Load2 unaligned linear: 19.190ms 0.996x
ByteAddressBuffer.Load2 unaligned random: 18.920ms 1.011x
ByteAddressBuffer.Load4 unaligned uniform: 9.600ms 1.992x
ByteAddressBuffer.Load4 unaligned linear: 19.234ms 0.994x
ByteAddressBuffer.Load4 unaligned random: 23.485ms 0.814x
StructuredBuffer<float>.Load uniform: 5.360ms 3.567x
StructuredBuffer<float>.Load linear: 5.335ms 3.584x
StructuredBuffer<float>.Load random: 18.879ms 1.013x
StructuredBuffer<float2>.Load uniform: 5.494ms 3.480x
StructuredBuffer<float2>.Load linear: 18.943ms 1.009x
StructuredBuffer<float2>.Load random: 18.898ms 1.012x
StructuredBuffer<float4>.Load uniform: 5.576ms 3.429x
StructuredBuffer<float4>.Load linear: 19.237ms 0.994x
StructuredBuffer<float4>.Load random: 21.314ms 0.897x
cbuffer{float4} load uniform: 6.819ms 2.804x
cbuffer{float4} load linear: 19.731ms 0.969x
cbuffer{float4} load random: 21.368ms 0.895x
Texture2D<R8>.Load uniform: 18.917ms 1.011x
Texture2D<R8>.Load linear: 19.067ms 1.003x
Texture2D<R8>.Load random: 18.925ms 1.010x
Texture2D<RG8>.Load uniform: 18.902ms 1.011x
Texture2D<RG8>.Load linear: 18.952ms 1.009x
Texture2D<RG8>.Load random: 18.888ms 1.012x
Texture2D<RGBA8>.Load uniform: 19.000ms 1.006x
Texture2D<RGBA8>.Load linear: 19.137ms 0.999x
Texture2D<RGBA8>.Load random: 18.965ms 1.008x
Texture2D<R16F>.Load uniform: 18.919ms 1.011x
Texture2D<R16F>.Load linear: 18.936ms 1.010x
Texture2D<R16F>.Load random: 19.034ms 1.004x
Texture2D<RG16F>.Load uniform: 18.910ms 1.011x
Texture2D<RG16F>.Load linear: 18.971ms 1.008x
Texture2D<RG16F>.Load random: 18.895ms 1.012x
Texture2D<RGBA16F>.Load uniform: 18.918ms 1.011x
Texture2D<RGBA16F>.Load linear: 19.094ms 1.001x
Texture2D<RGBA16F>.Load random: 37.952ms 0.504x
Texture2D<R32F>.Load uniform: 18.904ms 1.011x
Texture2D<R32F>.Load linear: 19.040ms 1.004x
Texture2D<R32F>.Load random: 19.053ms 1.003x
Texture2D<RG32F>.Load uniform: 18.942ms 1.009x
Texture2D<RG32F>.Load linear: 18.999ms 1.006x
Texture2D<RG32F>.Load random: 37.708ms 0.507x
Texture2D<RGBA32F>.Load uniform: 18.919ms 1.011x
Texture2D<RGBA32F>.Load linear: 28.305ms 0.675x
Texture2D<RGBA32F>.Load random: 37.705ms 0.507x
```
**AMD GCN5 7nm** is a 7nm die shink of Vega. Has higher clocks, but four disabled CUs (60 vs 64). Results (ratios) are identical to GCN2/3/4/5. See GCN2 for analysis.

### AMD Navi (RX 5700 XT)
```markdown
Buffer<R8>.Load uniform: 5.289ms 2.385x
Buffer<R8>.Load linear: 4.874ms 2.588x
Buffer<R8>.Load random: 4.656ms 2.710x
Buffer<RG8>.Load uniform: 5.986ms 2.108x
Buffer<RG8>.Load linear: 6.514ms 1.937x
Buffer<RG8>.Load random: 6.115ms 2.063x
Buffer<RGBA8>.Load uniform: 12.519ms 1.008x
Buffer<RGBA8>.Load linear: 12.985ms 0.972x
Buffer<RGBA8>.Load random: 12.617ms 1.000x
Buffer<R16f>.Load uniform: 4.769ms 2.645x
Buffer<R16f>.Load linear: 4.599ms 2.744x
Buffer<R16f>.Load random: 4.687ms 2.692x
Buffer<RG16f>.Load uniform: 6.210ms 2.032x
Buffer<RG16f>.Load linear: 6.164ms 2.047x
Buffer<RG16f>.Load random: 6.170ms 2.045x
Buffer<RGBA16f>.Load uniform: 12.838ms 0.983x
Buffer<RGBA16f>.Load linear: 13.138ms 0.960x
Buffer<RGBA16f>.Load random: 12.725ms 0.991x
Buffer<R32f>.Load uniform: 4.818ms 2.619x
Buffer<R32f>.Load linear: 4.697ms 2.686x
Buffer<R32f>.Load random: 4.771ms 2.644x
Buffer<RG32f>.Load uniform: 6.295ms 2.004x
Buffer<RG32f>.Load linear: 6.223ms 2.027x
Buffer<RG32f>.Load random: 6.217ms 2.029x
Buffer<RGBA32f>.Load uniform: 13.099ms 0.963x
Buffer<RGBA32f>.Load linear: 13.312ms 0.948x
Buffer<RGBA32f>.Load random: 12.819ms 0.984x
ByteAddressBuffer.Load uniform: 7.299ms 1.728x
ByteAddressBuffer.Load linear: 6.361ms 1.983x
ByteAddressBuffer.Load random: 6.279ms 2.009x
ByteAddressBuffer.Load2 uniform: 6.913ms 1.825x
ByteAddressBuffer.Load2 linear: 9.648ms 1.308x
ByteAddressBuffer.Load2 random: 9.693ms 1.302x
ByteAddressBuffer.Load3 uniform: 9.650ms 1.307x
ByteAddressBuffer.Load3 linear: 13.069ms 0.965x
ByteAddressBuffer.Load3 random: 26.009ms 0.485x
ByteAddressBuffer.Load4 uniform: 12.956ms 0.974x
ByteAddressBuffer.Load4 linear: 16.076ms 0.785x
ByteAddressBuffer.Load4 random: 16.332ms 0.773x
ByteAddressBuffer.Load2 unaligned uniform: 7.340ms 1.719x
ByteAddressBuffer.Load2 unaligned linear: 12.697ms 0.994x
ByteAddressBuffer.Load2 unaligned random: 12.598ms 1.001x
ByteAddressBuffer.Load4 unaligned uniform: 13.019ms 0.969x
ByteAddressBuffer.Load4 unaligned linear: 19.027ms 0.663x
ByteAddressBuffer.Load4 unaligned random: 25.387ms 0.497x
StructuredBuffer<float>.Load uniform: 9.047ms 1.395x
StructuredBuffer<float>.Load linear: 5.461ms 2.310x
StructuredBuffer<float>.Load random: 4.722ms 2.672x
StructuredBuffer<float2>.Load uniform: 8.770ms 1.439x
StructuredBuffer<float2>.Load linear: 6.795ms 1.857x
StructuredBuffer<float2>.Load random: 6.074ms 2.077x
StructuredBuffer<float4>.Load uniform: 9.013ms 1.400x
StructuredBuffer<float4>.Load linear: 12.948ms 0.974x
StructuredBuffer<float4>.Load random: 12.428ms 1.015x
cbuffer{float4} load uniform: 9.561ms 1.320x
cbuffer{float4} load linear: 13.446ms 0.938x
cbuffer{float4} load random: 12.445ms 1.014x
Texture2D<R8>.Load uniform: 6.537ms 1.930x
Texture2D<R8>.Load linear: 6.652ms 1.897x
Texture2D<R8>.Load random: 6.474ms 1.949x
Texture2D<RG8>.Load uniform: 6.652ms 1.897x
Texture2D<RG8>.Load linear: 6.606ms 1.910x
Texture2D<RG8>.Load random: 6.644ms 1.899x
Texture2D<RGBA8>.Load uniform: 12.992ms 0.971x
Texture2D<RGBA8>.Load linear: 13.012ms 0.970x
Texture2D<RGBA8>.Load random: 12.877ms 0.980x
Texture2D<R16F>.Load uniform: 6.655ms 1.896x
Texture2D<R16F>.Load linear: 6.596ms 1.913x
Texture2D<R16F>.Load random: 6.476ms 1.948x
Texture2D<RG16F>.Load uniform: 6.612ms 1.908x
Texture2D<RG16F>.Load linear: 6.697ms 1.884x
Texture2D<RG16F>.Load random: 6.436ms 1.960x
Texture2D<RGBA16F>.Load uniform: 12.956ms 0.974x
Texture2D<RGBA16F>.Load linear: 12.988ms 0.971x
Texture2D<RGBA16F>.Load random: 12.856ms 0.981x
Texture2D<R32F>.Load uniform: 6.651ms 1.897x
Texture2D<R32F>.Load linear: 6.732ms 1.874x
Texture2D<R32F>.Load random: 6.469ms 1.950x
Texture2D<RG32F>.Load uniform: 6.627ms 1.904x
Texture2D<RG32F>.Load linear: 12.954ms 0.974x
Texture2D<RG32F>.Load random: 6.450ms 1.956x
Texture2D<RGBA32F>.Load uniform: 12.949ms 0.974x
Texture2D<RGBA32F>.Load linear: 12.953ms 0.974x
Texture2D<RGBA32F>.Load random: 12.804ms 0.985x
```
**AMD Navi** TODO.

### AMD Navi 2 (RX 6600) Dx12+Dxc
```markdown
Buffer<R8>.Load uniform: 5.486ms 0.183ms 0.000ms 2.596x
Buffer<R8>.Load linear: 5.884ms 0.196ms 0.007ms 2.421x
Buffer<R8>.Load random: 5.263ms 0.175ms 0.000ms 2.706x
Buffer<RG8>.Load uniform: 7.447ms 0.248ms 0.004ms 1.912x
Buffer<RG8>.Load linear: 7.660ms 0.255ms 0.004ms 1.859x
Buffer<RG8>.Load random: 7.617ms 0.254ms 0.001ms 1.870x
Buffer<RGBA8>.Load uniform: 13.659ms 0.455ms 0.001ms 1.043x
Buffer<RGBA8>.Load linear: 14.304ms 0.477ms 0.006ms 0.996x
Buffer<RGBA8>.Load random: 14.242ms 0.475ms 0.001ms 1.000x
Buffer<R16f>.Load uniform: 6.374ms 0.212ms 0.063ms 2.234x
Buffer<R16f>.Load linear: 5.305ms 0.177ms 0.007ms 2.685x
Buffer<R16f>.Load random: 6.765ms 0.225ms 0.078ms 2.105x
Buffer<RG16f>.Load uniform: 8.138ms 0.271ms 0.066ms 1.750x
Buffer<RG16f>.Load linear: 7.581ms 0.253ms 0.004ms 1.879x
Buffer<RG16f>.Load random: 7.224ms 0.241ms 0.040ms 1.971x
Buffer<RGBA16f>.Load uniform: 14.570ms 0.486ms 0.056ms 0.977x
Buffer<RGBA16f>.Load linear: 14.621ms 0.487ms 0.045ms 0.974x
Buffer<RGBA16f>.Load random: 14.248ms 0.475ms 0.001ms 1.000x
Buffer<R32f>.Load uniform: 5.125ms 0.171ms 0.000ms 2.779x
Buffer<R32f>.Load linear: 5.872ms 0.196ms 0.003ms 2.425x
Buffer<R32f>.Load random: 5.880ms 0.196ms 0.001ms 2.422x
Buffer<RG32f>.Load uniform: 7.474ms 0.249ms 0.001ms 1.906x
Buffer<RG32f>.Load linear: 6.962ms 0.232ms 0.000ms 2.046x
Buffer<RG32f>.Load random: 7.633ms 0.254ms 0.001ms 1.866x
Buffer<RGBA32f>.Load uniform: 14.517ms 0.484ms 0.042ms 0.981x
Buffer<RGBA32f>.Load linear: 14.454ms 0.482ms 0.002ms 0.985x
Buffer<RGBA32f>.Load random: 13.974ms 0.466ms 0.043ms 1.019x
ByteAddressBuffer.Load uniform: 8.129ms 0.271ms 0.001ms 1.752x
ByteAddressBuffer.Load linear: 6.275ms 0.209ms 0.001ms 2.270x
ByteAddressBuffer.Load random: 5.941ms 0.198ms 0.001ms 2.397x
ByteAddressBuffer.Load2 uniform: 7.758ms 0.259ms 0.001ms 1.836x
ByteAddressBuffer.Load2 linear: 8.005ms 0.267ms 0.001ms 1.779x
ByteAddressBuffer.Load2 random: 7.686ms 0.256ms 0.001ms 1.853x
ByteAddressBuffer.Load3 uniform: 8.614ms 0.287ms 0.050ms 1.653x
ByteAddressBuffer.Load3 linear: 17.324ms 0.577ms 0.005ms 0.822x
ByteAddressBuffer.Load3 random: 27.938ms 0.931ms 0.003ms 0.510x
ByteAddressBuffer.Load4 uniform: 8.838ms 0.295ms 0.040ms 1.611x
ByteAddressBuffer.Load4 linear: 14.384ms 0.479ms 0.003ms 0.990x
ByteAddressBuffer.Load4 random: 13.739ms 0.458ms 0.001ms 1.037x
ByteAddressBuffer.Load2 unaligned uniform: 8.841ms 0.295ms 0.005ms 1.611x
ByteAddressBuffer.Load2 unaligned linear: 14.623ms 0.487ms 0.038ms 0.974x
ByteAddressBuffer.Load2 unaligned random: 14.369ms 0.479ms 0.002ms 0.991x
ByteAddressBuffer.Load4 unaligned uniform: 8.303ms 0.277ms 0.052ms 1.715x
ByteAddressBuffer.Load4 unaligned linear: 21.036ms 0.701ms 0.002ms 0.677x
ByteAddressBuffer.Load4 unaligned random: 27.566ms 0.919ms 0.001ms 0.517x
StructuredBuffer<float>.Load uniform: 10.128ms 0.338ms 0.039ms 1.406x
StructuredBuffer<float>.Load linear: 5.907ms 0.197ms 0.002ms 2.411x
StructuredBuffer<float>.Load random: 6.344ms 0.211ms 0.056ms 2.245x
StructuredBuffer<float2>.Load uniform: 9.890ms 0.330ms 0.002ms 1.440x
StructuredBuffer<float2>.Load linear: 7.637ms 0.255ms 0.001ms 1.865x
StructuredBuffer<float2>.Load random: 7.002ms 0.233ms 0.000ms 2.034x
StructuredBuffer<float4>.Load uniform: 10.471ms 0.349ms 0.053ms 1.360x
StructuredBuffer<float4>.Load linear: 14.308ms 0.477ms 0.001ms 0.995x
StructuredBuffer<float4>.Load random: 14.467ms 0.482ms 0.040ms 0.984x
cbuffer{float4} load uniform: 11.762ms 0.392ms 0.004ms 1.211x
cbuffer{float4} load linear: 15.887ms 0.530ms 0.001ms 0.896x
cbuffer{float4} load random: 13.946ms 0.465ms 0.048ms 1.021x
Texture2D<R8>.Load uniform: 7.705ms 0.257ms 0.002ms 1.848x
Texture2D<R8>.Load linear: 7.764ms 0.259ms 0.004ms 1.834x
Texture2D<R8>.Load random: 7.048ms 0.235ms 0.000ms 2.021x
Texture2D<RG8>.Load uniform: 7.778ms 0.259ms 0.002ms 1.831x
Texture2D<RG8>.Load linear: 8.584ms 0.286ms 0.045ms 1.659x
Texture2D<RG8>.Load random: 8.243ms 0.275ms 0.001ms 1.728x
Texture2D<RGBA8>.Load uniform: 14.296ms 0.477ms 0.002ms 0.996x
Texture2D<RGBA8>.Load linear: 14.736ms 0.491ms 0.043ms 0.967x
Texture2D<RGBA8>.Load random: 14.383ms 0.479ms 0.001ms 0.990x
Texture2D<R16F>.Load uniform: 7.735ms 0.258ms 0.001ms 1.841x
Texture2D<R16F>.Load linear: 7.871ms 0.262ms 0.001ms 1.809x
Texture2D<R16F>.Load random: 7.720ms 0.257ms 0.004ms 1.845x
Texture2D<RG16F>.Load uniform: 7.901ms 0.263ms 0.001ms 1.803x
Texture2D<RG16F>.Load linear: 9.486ms 0.316ms 0.001ms 1.501x
Texture2D<RG16F>.Load random: 7.742ms 0.258ms 0.001ms 1.840x
Texture2D<RGBA16F>.Load uniform: 14.601ms 0.487ms 0.002ms 0.975x
Texture2D<RGBA16F>.Load linear: 14.724ms 0.491ms 0.002ms 0.967x
Texture2D<RGBA16F>.Load random: 14.600ms 0.487ms 0.001ms 0.975x
Texture2D<R32F>.Load uniform: 7.833ms 0.261ms 0.002ms 1.818x
Texture2D<R32F>.Load linear: 9.398ms 0.313ms 0.004ms 1.516x
Texture2D<R32F>.Load random: 7.854ms 0.262ms 0.001ms 1.813x
Texture2D<RG32F>.Load uniform: 7.909ms 0.264ms 0.004ms 1.801x
Texture2D<RG32F>.Load linear: 14.557ms 0.485ms 0.004ms 0.978x
Texture2D<RG32F>.Load random: 8.474ms 0.282ms 0.001ms 1.681x
Texture2D<RGBA32F>.Load uniform: 14.683ms 0.489ms 0.002ms 0.970x
Texture2D<RGBA32F>.Load linear: 14.772ms 0.492ms 0.002ms 0.964x
Texture2D<RGBA32F>.Load random: 14.088ms 0.470ms 0.001ms 1.011x
Texture2D<R8>.Sample(nearest) uniform: 28.607ms 0.954ms 0.003ms 0.498x
Texture2D<R8>.Sample(nearest) linear: 28.502ms 0.950ms 0.003ms 0.500x
Texture2D<R8>.Sample(nearest) random: 28.285ms 0.943ms 0.003ms 0.504x
Texture2D<RG8>.Sample(nearest) uniform: 27.499ms 0.917ms 0.004ms 0.518x
Texture2D<RG8>.Sample(nearest) linear: 28.022ms 0.934ms 0.002ms 0.508x
Texture2D<RG8>.Sample(nearest) random: 28.157ms 0.939ms 0.044ms 0.506x
Texture2D<RGBA8>.Sample(nearest) uniform: 27.847ms 0.928ms 0.002ms 0.511x
Texture2D<RGBA8>.Sample(nearest) linear: 27.302ms 0.910ms 0.004ms 0.522x
Texture2D<RGBA8>.Sample(nearest) random: 27.884ms 0.929ms 0.002ms 0.511x
Texture2D<R16F>.Sample(nearest) uniform: 27.725ms 0.924ms 0.002ms 0.514x
Texture2D<R16F>.Sample(nearest) linear: 27.746ms 0.925ms 0.001ms 0.513x
Texture2D<R16F>.Sample(nearest) random: 27.107ms 0.904ms 0.001ms 0.525x
Texture2D<RG16F>.Sample(nearest) uniform: 27.703ms 0.923ms 0.001ms 0.514x
Texture2D<RG16F>.Sample(nearest) linear: 27.772ms 0.926ms 0.001ms 0.513x
Texture2D<RG16F>.Sample(nearest) random: 27.756ms 0.925ms 0.003ms 0.513x
Texture2D<RGBA16F>.Sample(nearest) uniform: 27.173ms 0.906ms 0.004ms 0.524x
Texture2D<RGBA16F>.Sample(nearest) linear: 27.843ms 0.928ms 0.001ms 0.512x
Texture2D<RGBA16F>.Sample(nearest) random: 27.823ms 0.927ms 0.001ms 0.512x
Texture2D<R32F>.Sample(nearest) uniform: 27.672ms 0.922ms 0.001ms 0.515x
Texture2D<R32F>.Sample(nearest) linear: 27.135ms 0.905ms 0.004ms 0.525x
Texture2D<R32F>.Sample(nearest) random: 27.722ms 0.924ms 0.001ms 0.514x
Texture2D<RG32F>.Sample(nearest) uniform: 27.695ms 0.923ms 0.001ms 0.514x
Texture2D<RG32F>.Sample(nearest) linear: 27.794ms 0.926ms 0.001ms 0.512x
Texture2D<RG32F>.Sample(nearest) random: 27.159ms 0.905ms 0.001ms 0.524x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 27.767ms 0.926ms 0.001ms 0.513x
Texture2D<RGBA32F>.Sample(nearest) linear: 27.861ms 0.929ms 0.001ms 0.511x
Texture2D<RGBA32F>.Sample(nearest) random: 27.835ms 0.928ms 0.001ms 0.512x
Texture2D<R8>.Sample(bilinear) uniform: 27.067ms 0.902ms 0.003ms 0.526x
Texture2D<R8>.Sample(bilinear) linear: 27.724ms 0.924ms 0.001ms 0.514x
Texture2D<R8>.Sample(bilinear) random: 27.729ms 0.924ms 0.002ms 0.514x
Texture2D<RG8>.Sample(bilinear) uniform: 27.700ms 0.923ms 0.001ms 0.514x
Texture2D<RG8>.Sample(bilinear) linear: 27.180ms 0.906ms 0.004ms 0.524x
Texture2D<RG8>.Sample(bilinear) random: 27.778ms 0.926ms 0.002ms 0.513x
Texture2D<RGBA8>.Sample(bilinear) uniform: 27.750ms 0.925ms 0.002ms 0.513x
Texture2D<RGBA8>.Sample(bilinear) linear: 27.830ms 0.928ms 0.001ms 0.512x
Texture2D<RGBA8>.Sample(bilinear) random: 27.205ms 0.907ms 0.001ms 0.524x
Texture2D<R16F>.Sample(bilinear) uniform: 27.649ms 0.922ms 0.004ms 0.515x
Texture2D<R16F>.Sample(bilinear) linear: 27.724ms 0.924ms 0.001ms 0.514x
Texture2D<R16F>.Sample(bilinear) random: 27.727ms 0.924ms 0.001ms 0.514x
Texture2D<RG16F>.Sample(bilinear) uniform: 27.081ms 0.903ms 0.001ms 0.526x
Texture2D<RG16F>.Sample(bilinear) linear: 27.765ms 0.925ms 0.004ms 0.513x
Texture2D<RG16F>.Sample(bilinear) random: 27.766ms 0.926ms 0.001ms 0.513x
Texture2D<RGBA16F>.Sample(bilinear) uniform: 27.767ms 0.926ms 0.001ms 0.513x
Texture2D<RGBA16F>.Sample(bilinear) linear: 27.221ms 0.907ms 0.001ms 0.523x
Texture2D<RGBA16F>.Sample(bilinear) random: 27.828ms 0.928ms 0.001ms 0.512x
Texture2D<R32F>.Sample(bilinear) uniform: 27.651ms 0.922ms 0.004ms 0.515x
Texture2D<R32F>.Sample(bilinear) linear: 27.708ms 0.924ms 0.004ms 0.514x
Texture2D<R32F>.Sample(bilinear) random: 27.103ms 0.903ms 0.000ms 0.525x
Texture2D<RG32F>.Sample(bilinear) uniform: 27.703ms 0.923ms 0.001ms 0.514x
Texture2D<RG32F>.Sample(bilinear) linear: 27.780ms 0.926ms 0.002ms 0.513x
Texture2D<RG32F>.Sample(bilinear) random: 27.771ms 0.926ms 0.001ms 0.513x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 54.000ms 1.800ms 0.001ms 0.264x
Texture2D<RGBA32F>.Sample(bilinear) linear: 54.737ms 1.825ms 0.005ms 0.260x
Texture2D<RGBA32F>.Sample(bilinear) random: 54.696ms 1.823ms 0.004ms 0.260x
```

### NVidia Maxwell (GTX 980 Ti)
```markdown
Buffer<R8>.Load uniform: 1.249ms 28.812x
Buffer<R8>.Load linear: 34.105ms 1.055x
Buffer<R8>.Load random: 34.187ms 1.053x
Buffer<RG8>.Load uniform: 1.847ms 19.485x
Buffer<RG8>.Load linear: 34.106ms 1.055x
Buffer<RG8>.Load random: 34.477ms 1.044x
Buffer<RGBA8>.Load uniform: 2.452ms 14.680x
Buffer<RGBA8>.Load linear: 35.773ms 1.006x
Buffer<RGBA8>.Load random: 35.996ms 1.000x
Buffer<R16f>.Load uniform: 1.491ms 24.148x
Buffer<R16f>.Load linear: 34.077ms 1.056x
Buffer<R16f>.Load random: 34.463ms 1.044x
Buffer<RG16f>.Load uniform: 1.916ms 18.785x
Buffer<RG16f>.Load linear: 34.229ms 1.052x
Buffer<RG16f>.Load random: 34.597ms 1.040x
Buffer<RGBA16f>.Load uniform: 2.519ms 14.291x
Buffer<RGBA16f>.Load linear: 35.787ms 1.006x
Buffer<RGBA16f>.Load random: 35.996ms 1.000x
Buffer<R32f>.Load uniform: 1.478ms 24.350x
Buffer<R32f>.Load linear: 34.098ms 1.056x
Buffer<R32f>.Load random: 34.353ms 1.048x
Buffer<RG32f>.Load uniform: 1.845ms 19.514x
Buffer<RG32f>.Load linear: 34.138ms 1.054x
Buffer<RG32f>.Load random: 34.495ms 1.044x
Buffer<RGBA32f>.Load uniform: 2.374ms 15.163x
Buffer<RGBA32f>.Load linear: 67.973ms 0.530x
Buffer<RGBA32f>.Load random: 68.054ms 0.529x
ByteAddressBuffer.Load uniform: 21.403ms 1.682x
ByteAddressBuffer.Load linear: 21.906ms 1.643x
ByteAddressBuffer.Load random: 24.336ms 1.479x
ByteAddressBuffer.Load2 uniform: 45.620ms 0.789x
ByteAddressBuffer.Load2 linear: 55.815ms 0.645x
ByteAddressBuffer.Load2 random: 48.744ms 0.738x
ByteAddressBuffer.Load3 uniform: 52.929ms 0.680x
ByteAddressBuffer.Load3 linear: 79.057ms 0.455x
ByteAddressBuffer.Load3 random: 93.636ms 0.384x
ByteAddressBuffer.Load4 uniform: 68.510ms 0.525x
ByteAddressBuffer.Load4 linear: 114.561ms 0.314x
ByteAddressBuffer.Load4 random: 209.280ms 0.172x
ByteAddressBuffer.Load2 unaligned uniform: 45.640ms 0.789x
ByteAddressBuffer.Load2 unaligned linear: 55.802ms 0.645x
ByteAddressBuffer.Load2 unaligned random: 48.717ms 0.739x
ByteAddressBuffer.Load4 unaligned uniform: 68.685ms 0.524x
ByteAddressBuffer.Load4 unaligned linear: 115.244ms 0.312x
ByteAddressBuffer.Load4 unaligned random: 210.358ms 0.171x
StructuredBuffer<float>.Load uniform: 1.116ms 32.267x
StructuredBuffer<float>.Load linear: 34.094ms 1.056x
StructuredBuffer<float>.Load random: 34.092ms 1.056x
StructuredBuffer<float2>.Load uniform: 1.569ms 22.942x
StructuredBuffer<float2>.Load linear: 34.143ms 1.054x
StructuredBuffer<float2>.Load random: 34.125ms 1.055x
StructuredBuffer<float4>.Load uniform: 2.087ms 17.245x
StructuredBuffer<float4>.Load linear: 67.959ms 0.530x
StructuredBuffer<float4>.Load random: 67.950ms 0.530x
cbuffer{float4} load uniform: 1.298ms 27.733x
cbuffer{float4} load linear: 798.703ms 0.045x
cbuffer{float4} load random: 324.356ms 0.111x
Texture2D<R8>.Load uniform: 1.962ms 18.351x
Texture2D<R8>.Load linear: 34.027ms 1.058x
Texture2D<R8>.Load random: 34.029ms 1.058x
Texture2D<RG8>.Load uniform: 1.994ms 18.054x
Texture2D<RG8>.Load linear: 34.334ms 1.048x
Texture2D<RG8>.Load random: 34.102ms 1.056x
Texture2D<RGBA8>.Load uniform: 2.247ms 16.018x
Texture2D<RGBA8>.Load linear: 36.077ms 0.998x
Texture2D<RGBA8>.Load random: 35.930ms 1.002x
Texture2D<R16F>.Load uniform: 2.021ms 17.814x
Texture2D<R16F>.Load linear: 34.040ms 1.057x
Texture2D<R16F>.Load random: 34.021ms 1.058x
Texture2D<RG16F>.Load uniform: 2.020ms 17.822x
Texture2D<RG16F>.Load linear: 34.308ms 1.049x
Texture2D<RG16F>.Load random: 34.095ms 1.056x
Texture2D<RGBA16F>.Load uniform: 2.199ms 16.372x
Texture2D<RGBA16F>.Load linear: 36.074ms 0.998x
Texture2D<RGBA16F>.Load random: 68.064ms 0.529x
Texture2D<R32F>.Load uniform: 2.014ms 17.869x
Texture2D<R32F>.Load linear: 34.042ms 1.057x
Texture2D<R32F>.Load random: 34.028ms 1.058x
Texture2D<RG32F>.Load uniform: 1.981ms 18.166x
Texture2D<RG32F>.Load linear: 34.320ms 1.049x
Texture2D<RG32F>.Load random: 67.948ms 0.530x
Texture2D<RGBA32F>.Load uniform: 2.064ms 17.440x
Texture2D<RGBA32F>.Load linear: 67.974ms 0.530x
Texture2D<RGBA32F>.Load random: 68.049ms 0.529x
```

**Typed loads:** Maxwell doesn't coalesce any typed loads. Dimensions (1d/2d/4d) and channel widths (8b/16b/32b) don't directly affect performance. All up to 64 bit loads are full rate. 128 bit loads are half rate (only RGBA32). Best bytes per cycle rate can be achieved by 64+ bit loads (RGBA16, RG32, RGBA32).

**Raw (ByteAddressBuffer) loads:** Oddly we see no coalescing here either. CUDA code shows big performance improvement with similar linear access pattern. All 1d raw loads are as fast as typed buffer loads. However NV doesn't seem to emit wide raw loads either. 2d is exactly 2x slower, 3d is 3x slower and 4d is 4x slower than 1d. NVIDIA supports 64 bit and 128 wide raw loads in CUDA, see: https://devblogs.nvidia.com/parallelforall/cuda-pro-tip-increase-performance-with-vectorized-memory-access/. Wide loads in CUDA however require memory alignment (8/16 bytes). My test case is perfectly aligned, but HLSL ByteAddressBuffer.Load4() specification only requires alignment of 4. In general case it's hard to prove alignment of 16 (in my code there's an explicit multiply address by 16).

**Structured buffer loads:** Structured buffer loads guarantee natural alignment. Nvidia has full rate 1d and 2d structured buffer loads. But 4d loads (128 bit) are half rate as usual.

**Texture loads:** Similar performance as typed buffer loads. Random access of wide formats tends to be slightly slower (but my 2d random produces different access pattern than 1d).

**Cbuffer loads:** Nvidia Maxwell (and newer GPUs) have a special constant buffer hardware unit. Uniform address constant buffer loads are up to 32x faster (warp width) than standard memory loads. However non-uniform constant buffer loads are dead slow. Nvidia CUDA documents tell us that constant buffer load gets serialized for each unique address. Thus we can see up to 32x performance drop compared to best case. But in my test case (each lane = different address), we se up to 200x slow down. This result tells us that there's likely a small constant buffer cache on each SM, and if your access pattern is bad enough, this cache starts to trash badly. Unfortunately Nvidia doesn't provide us a public document describing best practices to avoid this pitfall.

**Uniform address optimization:** New Nvidia drivers introduced a shader compiler based uniform address load optimization for loops. This speeds up the loads in these cases by up to 28x (close to 32x theoretical warp width). This new optimization is awesome, because previously Nvidia had to fully lean to their constant buffer hardware for good uniform load performance. As we all know constant buffers are very limited (vec4 arrays only and 64KB size limit). See "Uniform Address Load Investigation" chapter for more info.

**Suggestions:** Prefer 64+ bit typed loads (RGBA16, RG32, RGBA32). ByteAddressBuffer wide loads and coalescing doesn't seem to work in DirectX. Uniform address loads (inside loop using loop index) have a fast path. Use it whenever possible. This results in similar performance as using constant buffer hardware, but supports all buffer and texture types. No size or alignment limitations!

### NVidia Pascal (GTX 1070 Ti)
```markdown
Buffer<R8>.Load uniform: 0.845ms 36.835x
Buffer<R8>.Load linear: 29.335ms 1.061x
Buffer<R8>.Load random: 28.981ms 1.074x
Buffer<RG8>.Load uniform: 1.151ms 27.036x
Buffer<RG8>.Load linear: 30.267ms 1.028x
Buffer<RG8>.Load random: 29.359ms 1.060x
Buffer<RGBA8>.Load uniform: 1.534ms 20.286x
Buffer<RGBA8>.Load linear: 31.214ms 0.997x
Buffer<RGBA8>.Load random: 31.118ms 1.000x
Buffer<R16f>.Load uniform: 0.808ms 38.516x
Buffer<R16f>.Load linear: 28.943ms 1.075x
Buffer<R16f>.Load random: 29.870ms 1.042x
Buffer<RG16f>.Load uniform: 1.119ms 27.803x
Buffer<RG16f>.Load linear: 29.458ms 1.056x
Buffer<RG16f>.Load random: 29.904ms 1.041x
Buffer<RGBA16f>.Load uniform: 1.467ms 21.207x
Buffer<RGBA16f>.Load linear: 31.222ms 0.997x
Buffer<RGBA16f>.Load random: 30.223ms 1.030x
Buffer<R32f>.Load uniform: 0.847ms 36.746x
Buffer<R32f>.Load linear: 30.240ms 1.029x
Buffer<R32f>.Load random: 28.963ms 1.074x
Buffer<RG32f>.Load uniform: 1.087ms 28.615x
Buffer<RG32f>.Load linear: 30.391ms 1.024x
Buffer<RG32f>.Load random: 29.475ms 1.056x
Buffer<RGBA32f>.Load uniform: 1.434ms 21.706x
Buffer<RGBA32f>.Load linear: 59.394ms 0.524x
Buffer<RGBA32f>.Load random: 57.593ms 0.540x
ByteAddressBuffer.Load uniform: 18.151ms 1.714x
ByteAddressBuffer.Load linear: 18.451ms 1.686x
ByteAddressBuffer.Load random: 21.305ms 1.461x
ByteAddressBuffer.Load2 uniform: 41.123ms 0.757x
ByteAddressBuffer.Load2 linear: 40.461ms 0.769x
ByteAddressBuffer.Load2 random: 49.244ms 0.632x
ByteAddressBuffer.Load3 uniform: 44.836ms 0.694x
ByteAddressBuffer.Load3 linear: 65.966ms 0.472x
ByteAddressBuffer.Load3 random: 77.712ms 0.400x
ByteAddressBuffer.Load4 uniform: 58.439ms 0.532x
ByteAddressBuffer.Load4 linear: 97.260ms 0.320x
ByteAddressBuffer.Load4 random: 174.779ms 0.178x
ByteAddressBuffer.Load2 unaligned uniform: 41.147ms 0.756x
ByteAddressBuffer.Load2 unaligned linear: 40.483ms 0.769x
ByteAddressBuffer.Load2 unaligned random: 55.911ms 0.557x
ByteAddressBuffer.Load4 unaligned uniform: 58.126ms 0.535x
ByteAddressBuffer.Load4 unaligned linear: 99.081ms 0.314x
ByteAddressBuffer.Load4 unaligned random: 179.514ms 0.173x
StructuredBuffer<float>.Load uniform: 0.887ms 35.091x
StructuredBuffer<float>.Load linear: 29.878ms 1.042x
StructuredBuffer<float>.Load random: 29.408ms 1.058x
StructuredBuffer<float2>.Load uniform: 1.141ms 27.279x
StructuredBuffer<float2>.Load linear: 30.575ms 1.018x
StructuredBuffer<float2>.Load random: 28.985ms 1.074x
StructuredBuffer<float4>.Load uniform: 1.523ms 20.436x
StructuredBuffer<float4>.Load linear: 58.493ms 0.532x
StructuredBuffer<float4>.Load random: 58.546ms 0.532x
cbuffer{float4} load uniform: 1.390ms 22.394x
cbuffer{float4} load linear: 684.120ms 0.045x
cbuffer{float4} load random: 273.085ms 0.114x
Texture2D<R8>.Load uniform: 1.627ms 19.125x
Texture2D<R8>.Load linear: 28.924ms 1.076x
Texture2D<R8>.Load random: 28.923ms 1.076x
Texture2D<RG8>.Load uniform: 1.378ms 22.577x
Texture2D<RG8>.Load linear: 29.041ms 1.072x
Texture2D<RG8>.Load random: 28.938ms 1.075x
Texture2D<RGBA8>.Load uniform: 1.563ms 19.914x
Texture2D<RGBA8>.Load linear: 30.666ms 1.015x
Texture2D<RGBA8>.Load random: 30.334ms 1.026x
Texture2D<R16F>.Load uniform: 1.313ms 23.704x
Texture2D<R16F>.Load linear: 28.961ms 1.074x
Texture2D<R16F>.Load random: 28.968ms 1.074x
Texture2D<RG16F>.Load uniform: 1.360ms 22.883x
Texture2D<RG16F>.Load linear: 29.048ms 1.071x
Texture2D<RG16F>.Load random: 28.926ms 1.076x
Texture2D<RGBA16F>.Load uniform: 1.501ms 20.729x
Texture2D<RGBA16F>.Load linear: 30.649ms 1.015x
Texture2D<RGBA16F>.Load random: 57.629ms 0.540x
Texture2D<R32F>.Load uniform: 1.384ms 22.477x
Texture2D<R32F>.Load linear: 28.955ms 1.075x
Texture2D<R32F>.Load random: 28.968ms 1.074x
Texture2D<RG32F>.Load uniform: 1.408ms 22.101x
Texture2D<RG32F>.Load linear: 29.056ms 1.071x
Texture2D<RG32F>.Load random: 57.672ms 0.540x
Texture2D<RGBA32F>.Load uniform: 1.538ms 20.232x
Texture2D<RGBA32F>.Load linear: 57.653ms 0.540x
Texture2D<RGBA32F>.Load random: 57.557ms 0.541x
```
**NVIDIA Pascal** results (ratios) are identical to Maxwell. See Maxwell for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between Maxwell and Pascal.

### NVIDIA Kepler (600/700 series)
```markdown
Buffer<R8>.Load uniform: 3.073ms 62.440x
Buffer<R8>.Load linear: 195.662ms 0.981x
Buffer<R8>.Load random: 197.022ms 0.974x
Buffer<RG8>.Load uniform: 3.227ms 59.465x
Buffer<RG8>.Load linear: 195.179ms 0.983x
Buffer<RG8>.Load random: 196.785ms 0.975x
Buffer<RGBA8>.Load uniform: 3.598ms 53.329x
Buffer<RGBA8>.Load linear: 193.676ms 0.991x
Buffer<RGBA8>.Load random: 191.866ms 1.000x
Buffer<R16f>.Load uniform: 3.031ms 63.308x
Buffer<R16f>.Load linear: 195.622ms 0.981x
Buffer<R16f>.Load random: 197.009ms 0.974x
Buffer<RG16f>.Load uniform: 3.025ms 63.434x
Buffer<RG16f>.Load linear: 195.135ms 0.983x
Buffer<RG16f>.Load random: 196.860ms 0.975x
Buffer<RGBA16f>.Load uniform: 3.443ms 55.728x
Buffer<RGBA16f>.Load linear: 193.744ms 0.990x
Buffer<RGBA16f>.Load random: 191.929ms 1.000x
Buffer<R32f>.Load uniform: 2.970ms 64.605x
Buffer<R32f>.Load linear: 195.751ms 0.980x
Buffer<R32f>.Load random: 197.141ms 0.973x
Buffer<RG32f>.Load uniform: 3.175ms 60.425x
Buffer<RG32f>.Load linear: 195.351ms 0.982x
Buffer<RG32f>.Load random: 196.911ms 0.974x
Buffer<RGBA32f>.Load uniform: 3.621ms 52.985x
Buffer<RGBA32f>.Load linear: 350.658ms 0.547x
Buffer<RGBA32f>.Load random: 350.633ms 0.547x
ByteAddressBuffer.Load uniform: 3.758ms 51.055x
ByteAddressBuffer.Load linear: 191.898ms 1.000x
ByteAddressBuffer.Load random: 216.928ms 0.884x
ByteAddressBuffer.Load2 uniform: 4.682ms 40.977x
ByteAddressBuffer.Load2 linear: 390.852ms 0.491x
ByteAddressBuffer.Load2 random: 442.053ms 0.434x
ByteAddressBuffer.Load3 uniform: 572.822ms 0.335x
ByteAddressBuffer.Load3 linear: 568.316ms 0.338x
ByteAddressBuffer.Load3 random: 570.361ms 0.336x
ByteAddressBuffer.Load4 uniform: 752.691ms 0.255x
ByteAddressBuffer.Load4 linear: 758.795ms 0.253x
ByteAddressBuffer.Load4 random: 763.638ms 0.251x
ByteAddressBuffer.Load2 unaligned uniform: 4.199ms 45.692x
ByteAddressBuffer.Load2 unaligned linear: 391.542ms 0.490x
ByteAddressBuffer.Load2 unaligned random: 442.574ms 0.434x
ByteAddressBuffer.Load4 unaligned uniform: 752.793ms 0.255x
ByteAddressBuffer.Load4 unaligned linear: 758.698ms 0.253x
ByteAddressBuffer.Load4 unaligned random: 763.679ms 0.251x
StructuredBuffer<float>.Load uniform: 3.103ms 61.827x
StructuredBuffer<float>.Load linear: 195.674ms 0.981x
StructuredBuffer<float>.Load random: 196.991ms 0.974x
StructuredBuffer<float2>.Load uniform: 3.301ms 58.120x
StructuredBuffer<float2>.Load linear: 195.167ms 0.983x
StructuredBuffer<float2>.Load random: 196.749ms 0.975x
StructuredBuffer<float4>.Load uniform: 3.846ms 49.882x
StructuredBuffer<float4>.Load linear: 350.461ms 0.547x
StructuredBuffer<float4>.Load random: 350.494ms 0.547x
cbuffer{float4} load uniform: 4.478ms 42.844x
cbuffer{float4} load linear: 9217.404ms 0.021x
cbuffer{float4} load random: 3333.476ms 0.058x
Texture2D<R8>.Load uniform: 3.384ms 56.695x
Texture2D<R8>.Load linear: 202.197ms 0.949x
Texture2D<R8>.Load random: 204.327ms 0.939x
Texture2D<RG8>.Load uniform: 3.731ms 51.424x
Texture2D<RG8>.Load linear: 198.542ms 0.966x
Texture2D<RG8>.Load random: 211.881ms 0.906x
Texture2D<RGBA8>.Load uniform: 4.306ms 44.558x
Texture2D<RGBA8>.Load linear: 196.088ms 0.978x
Texture2D<RGBA8>.Load random: 195.847ms 0.980x
Texture2D<R16F>.Load uniform: 3.419ms 56.118x
Texture2D<R16F>.Load linear: 202.264ms 0.949x
Texture2D<R16F>.Load random: 204.311ms 0.939x
Texture2D<RG16F>.Load uniform: 3.673ms 52.243x
Texture2D<RG16F>.Load linear: 198.553ms 0.966x
Texture2D<RG16F>.Load random: 211.917ms 0.905x
Texture2D<RGBA16F>.Load uniform: 4.115ms 46.626x
Texture2D<RGBA16F>.Load linear: 196.084ms 0.978x
Texture2D<RGBA16F>.Load random: 350.561ms 0.547x
Texture2D<R32F>.Load uniform: 3.517ms 54.547x
Texture2D<R32F>.Load linear: 202.339ms 0.948x
Texture2D<R32F>.Load random: 204.392ms 0.939x
Texture2D<RG32F>.Load uniform: 3.705ms 51.783x
Texture2D<RG32F>.Load linear: 198.537ms 0.966x
Texture2D<RG32F>.Load random: 350.591ms 0.547x
Texture2D<RGBA32F>.Load uniform: 4.028ms 47.637x
Texture2D<RGBA32F>.Load linear: 350.589ms 0.547x
Texture2D<RGBA32F>.Load random: 350.519ms 0.547x
```

**NVIDIA Kepler** results (ratios) are identical to Maxwell & Pascal. See Maxwell for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between Kepler, Maxwell and Pascal.

### NVidia Volta (Titan V)
```markdown
Buffer<R8>.Load uniform: 2.241ms 8.139x
Buffer<R8>.Load linear: 14.806ms 1.232x
Buffer<R8>.Load random: 16.514ms 1.104x
Buffer<RG8>.Load uniform: 4.576ms 3.985x
Buffer<RG8>.Load linear: 16.397ms 1.112x
Buffer<RG8>.Load random: 16.707ms 1.092x
Buffer<RGBA8>.Load uniform: 5.155ms 3.538x
Buffer<RGBA8>.Load linear: 16.726ms 1.090x
Buffer<RGBA8>.Load random: 18.236ms 1.000x
Buffer<R16f>.Load uniform: 2.807ms 6.497x
Buffer<R16f>.Load linear: 14.771ms 1.235x
Buffer<R16f>.Load random: 16.857ms 1.082x
Buffer<RG16f>.Load uniform: 4.128ms 4.418x
Buffer<RG16f>.Load linear: 16.155ms 1.129x
Buffer<RG16f>.Load random: 15.140ms 1.205x
Buffer<RGBA16f>.Load uniform: 4.747ms 3.841x
Buffer<RGBA16f>.Load linear: 17.517ms 1.041x
Buffer<RGBA16f>.Load random: 17.727ms 1.029x
Buffer<R32f>.Load uniform: 2.630ms 6.935x
Buffer<R32f>.Load linear: 17.341ms 1.052x
Buffer<R32f>.Load random: 15.922ms 1.145x
Buffer<RG32f>.Load uniform: 4.769ms 3.824x
Buffer<RG32f>.Load linear: 15.745ms 1.158x
Buffer<RG32f>.Load random: 15.801ms 1.154x
Buffer<RGBA32f>.Load uniform: 4.772ms 3.822x
Buffer<RGBA32f>.Load linear: 29.343ms 0.621x
Buffer<RGBA32f>.Load random: 29.427ms 0.620x
ByteAddressBuffer.Load uniform: 8.948ms 2.038x
ByteAddressBuffer.Load linear: 8.722ms 2.091x
ByteAddressBuffer.Load random: 10.403ms 1.753x
ByteAddressBuffer.Load2 uniform: 10.132ms 1.800x
ByteAddressBuffer.Load2 linear: 11.406ms 1.599x
ByteAddressBuffer.Load2 random: 10.999ms 1.658x
ByteAddressBuffer.Load3 uniform: 12.638ms 1.443x
ByteAddressBuffer.Load3 linear: 13.708ms 1.330x
ByteAddressBuffer.Load3 random: 14.081ms 1.295x
ByteAddressBuffer.Load4 uniform: 15.421ms 1.183x
ByteAddressBuffer.Load4 linear: 26.412ms 0.690x
ByteAddressBuffer.Load4 random: 18.078ms 1.009x
ByteAddressBuffer.Load2 unaligned uniform: 11.076ms 1.647x
ByteAddressBuffer.Load2 unaligned linear: 11.474ms 1.589x
ByteAddressBuffer.Load2 unaligned random: 12.227ms 1.492x
ByteAddressBuffer.Load4 unaligned uniform: 15.817ms 1.153x
ByteAddressBuffer.Load4 unaligned linear: 25.894ms 0.704x
ByteAddressBuffer.Load4 unaligned random: 18.138ms 1.005x
StructuredBuffer<float>.Load uniform: 6.606ms 2.761x
StructuredBuffer<float>.Load linear: 6.555ms 2.782x
StructuredBuffer<float>.Load random: 9.063ms 2.012x
StructuredBuffer<float2>.Load uniform: 8.332ms 2.189x
StructuredBuffer<float2>.Load linear: 8.545ms 2.134x
StructuredBuffer<float2>.Load random: 7.271ms 2.508x
StructuredBuffer<float4>.Load uniform: 8.890ms 2.051x
StructuredBuffer<float4>.Load linear: 9.650ms 1.890x
StructuredBuffer<float4>.Load random: 9.677ms 1.885x
cbuffer{float4} load uniform: 1.381ms 13.202x
cbuffer{float4} load linear: 320.961ms 0.057x
cbuffer{float4} load random: 150.072ms 0.122x
Texture2D<R8>.Load uniform: 4.481ms 4.070x
Texture2D<R8>.Load linear: 15.953ms 1.143x
Texture2D<R8>.Load random: 15.058ms 1.211x
Texture2D<RG8>.Load uniform: 4.594ms 3.970x
Texture2D<RG8>.Load linear: 14.838ms 1.229x
Texture2D<RG8>.Load random: 14.938ms 1.221x
Texture2D<RGBA8>.Load uniform: 5.140ms 3.548x
Texture2D<RGBA8>.Load linear: 14.915ms 1.223x
Texture2D<RGBA8>.Load random: 15.031ms 1.213x
Texture2D<R16F>.Load uniform: 5.748ms 3.173x
Texture2D<R16F>.Load linear: 15.321ms 1.190x
Texture2D<R16F>.Load random: 15.044ms 1.212x
Texture2D<RG16F>.Load uniform: 4.609ms 3.957x
Texture2D<RG16F>.Load linear: 14.918ms 1.222x
Texture2D<RG16F>.Load random: 14.851ms 1.228x
Texture2D<RGBA16F>.Load uniform: 5.182ms 3.519x
Texture2D<RGBA16F>.Load linear: 14.915ms 1.223x
Texture2D<RGBA16F>.Load random: 29.841ms 0.611x
Texture2D<R32F>.Load uniform: 4.462ms 4.087x
Texture2D<R32F>.Load linear: 15.615ms 1.168x
Texture2D<R32F>.Load random: 15.519ms 1.175x
Texture2D<RG32F>.Load uniform: 4.585ms 3.977x
Texture2D<RG32F>.Load linear: 16.651ms 1.095x
Texture2D<RG32F>.Load random: 29.710ms 0.614x
Texture2D<RGBA32F>.Load uniform: 5.163ms 3.532x
Texture2D<RGBA32F>.Load linear: 29.970ms 0.608x
Texture2D<RGBA32F>.Load random: 29.358ms 0.621x
```

**NVIDIA Volta** results (ratios) of most common load/sample operations are identical to Pascal. However there are some huge changes raw load performance. Raw loads: 1d ~2x faster, 2d-4d ~4x faster (slightly more on 3d and 4d). Nvidia definitely seems to now use a faster direct memory path for raw loads. Raw loads are now the best choice on Nvidia hardware (which is a direct opposite of their last gen hardware). Independent studies of Volta architecture show that their raw load L1$ latency also dropped from 85 cycles (Pascal) down to 28 cycles (Volta). This should makes raw loads even more viable in real applications. My benchmark measures only throughput, so latency improvement isn't visible.

**Uniform address optimization:** Uniform address optimization no longer affects StructuredBuffers. My educated guess is that StructuredBuffers (like raw buffers) now use the same lower latency direct memory path. Nvidia most likely hasn't yet implemented uniform address optimization for these new memory operations. Another curiosity is that Volta also has much lower performance advantage in the uniform address optimized cases (versus any other Nvidia GPU, including Turing).

### NVidia Turing (RTX 2080 Ti)
```
Buffer<R8>.Load uniform: 0.703ms 23.287x
Buffer<R8>.Load linear: 16.179ms 1.011x
Buffer<R8>.Load random: 15.435ms 1.060x
Buffer<RG8>.Load uniform: 0.881ms 18.567x
Buffer<RG8>.Load linear: 15.983ms 1.024x
Buffer<RG8>.Load random: 17.044ms 0.960x
Buffer<RGBA8>.Load uniform: 1.336ms 12.247x
Buffer<RGBA8>.Load linear: 16.825ms 0.973x
Buffer<RGBA8>.Load random: 16.364ms 1.000x
Buffer<R16f>.Load uniform: 0.662ms 24.729x
Buffer<R16f>.Load linear: 15.431ms 1.060x
Buffer<R16f>.Load random: 15.916ms 1.028x
Buffer<RG16f>.Load uniform: 0.870ms 18.811x
Buffer<RG16f>.Load linear: 16.861ms 0.971x
Buffer<RG16f>.Load random: 16.384ms 0.999x
Buffer<RGBA16f>.Load uniform: 1.331ms 12.296x
Buffer<RGBA16f>.Load linear: 15.892ms 1.030x
Buffer<RGBA16f>.Load random: 15.949ms 1.026x
Buffer<R32f>.Load uniform: 0.651ms 25.143x
Buffer<R32f>.Load linear: 15.438ms 1.060x
Buffer<R32f>.Load random: 16.851ms 0.971x
Buffer<RG32f>.Load uniform: 1.369ms 11.953x
Buffer<RG32f>.Load linear: 15.440ms 1.060x
Buffer<RG32f>.Load random: 15.917ms 1.028x
Buffer<RGBA32f>.Load uniform: 1.348ms 12.141x
Buffer<RGBA32f>.Load linear: 30.695ms 0.533x
Buffer<RGBA32f>.Load random: 32.514ms 0.503x
ByteAddressBuffer.Load uniform: 7.013ms 2.333x
ByteAddressBuffer.Load linear: 6.308ms 2.594x
ByteAddressBuffer.Load random: 7.347ms 2.227x
ByteAddressBuffer.Load2 uniform: 9.510ms 1.721x
ByteAddressBuffer.Load2 linear: 16.912ms 0.968x
ByteAddressBuffer.Load2 random: 9.715ms 1.684x
ByteAddressBuffer.Load3 uniform: 14.700ms 1.113x
ByteAddressBuffer.Load3 linear: 19.200ms 0.852x
ByteAddressBuffer.Load3 random: 14.804ms 1.105x
ByteAddressBuffer.Load4 uniform: 18.228ms 0.898x
ByteAddressBuffer.Load4 linear: 43.493ms 0.376x
ByteAddressBuffer.Load4 random: 32.616ms 0.502x
ByteAddressBuffer.Load2 unaligned uniform: 9.549ms 1.714x
ByteAddressBuffer.Load2 unaligned linear: 16.901ms 0.968x
ByteAddressBuffer.Load2 unaligned random: 9.719ms 1.684x
ByteAddressBuffer.Load4 unaligned uniform: 18.218ms 0.898x
ByteAddressBuffer.Load4 unaligned linear: 41.476ms 0.395x
ByteAddressBuffer.Load4 unaligned random: 32.081ms 0.510x
StructuredBuffer<float>.Load uniform: 6.535ms 2.504x
StructuredBuffer<float>.Load linear: 6.706ms 2.440x
StructuredBuffer<float>.Load random: 6.911ms 2.368x
StructuredBuffer<float2>.Load uniform: 8.057ms 2.031x
StructuredBuffer<float2>.Load linear: 16.874ms 0.970x
StructuredBuffer<float2>.Load random: 8.374ms 1.954x
StructuredBuffer<float4>.Load uniform: 15.491ms 1.056x
StructuredBuffer<float4>.Load linear: 20.327ms 0.805x
StructuredBuffer<float4>.Load random: 18.112ms 0.903x
cbuffer{float4} load uniform: 0.834ms 19.616x
cbuffer{float4} load linear: 328.935ms 0.050x
cbuffer{float4} load random: 125.135ms 0.131x
Texture2D<R8>.Load uniform: 0.746ms 21.929x
Texture2D<R8>.Load linear: 16.173ms 1.012x
Texture2D<R8>.Load random: 15.400ms 1.063x
Texture2D<RG8>.Load uniform: 1.043ms 15.691x
Texture2D<RG8>.Load linear: 15.421ms 1.061x
Texture2D<RG8>.Load random: 15.400ms 1.063x
Texture2D<RGBA8>.Load uniform: 1.876ms 8.725x
Texture2D<RGBA8>.Load linear: 16.462ms 0.994x
Texture2D<RGBA8>.Load random: 16.461ms 0.994x
Texture2D<R16F>.Load uniform: 0.741ms 22.092x
Texture2D<R16F>.Load linear: 16.253ms 1.007x
Texture2D<R16F>.Load random: 16.222ms 1.009x
Texture2D<RG16F>.Load uniform: 1.053ms 15.546x
Texture2D<RG16F>.Load linear: 15.440ms 1.060x
Texture2D<RG16F>.Load random: 16.148ms 1.013x
Texture2D<RGBA16F>.Load uniform: 1.890ms 8.659x
Texture2D<RGBA16F>.Load linear: 16.125ms 1.015x
Texture2D<RGBA16F>.Load random: 31.047ms 0.527x
Texture2D<R32F>.Load uniform: 0.746ms 21.930x
Texture2D<R32F>.Load linear: 16.403ms 0.998x
Texture2D<R32F>.Load random: 16.638ms 0.983x
Texture2D<RG32F>.Load uniform: 1.060ms 15.441x
Texture2D<RG32F>.Load linear: 15.439ms 1.060x
Texture2D<RG32F>.Load random: 31.903ms 0.513x
Texture2D<RGBA32F>.Load uniform: 1.888ms 8.668x
Texture2D<RGBA32F>.Load linear: 31.525ms 0.519x
Texture2D<RGBA32F>.Load random: 32.783ms 0.499x
```

**NVIDIA Turing** results (ratios) of most common load/sample operations are identical Volta. Except wide raw buffer load performance is closer to Maxwell/Pascal. In Volta, Nvidia used one large 128KB shared L1$ (freely configurable between groupshared mem and L1$), while in Turing they have 96KB shared L1$ which can be configured only as 64/32 or 32/64. This benchmark seems to point out that this halves their L1$ bandwidth for raw loads.

**Uniform address optimization:** Like Volta, the new uniform address optimization no longer affects StructuredBuffers. My educated guess is that StructuredBuffers (like raw buffers) now use the same lower latency direct memory path. Nvidia most likely hasn't yet implemented uniform address optimization for these new memory operations. Turing uniform address optimization performance however (in other cases) returns to similar 20x+ figures than Maxwell/Pascal.


### NVidia Turing (RTX 2060) Dx12+Dxc
```markdown
Buffer<R8>.Load uniform: 42.546ms 1.418ms 0.024ms 0.974x
Buffer<R8>.Load linear: 42.429ms 1.414ms 0.024ms 0.976x
Buffer<R8>.Load random: 42.231ms 1.408ms 0.024ms 0.981x
Buffer<RG8>.Load uniform: 42.098ms 1.403ms 0.028ms 0.984x
Buffer<RG8>.Load linear: 41.970ms 1.399ms 0.024ms 0.987x
Buffer<RG8>.Load random: 41.749ms 1.392ms 0.026ms 0.992x
Buffer<RGBA8>.Load uniform: 42.109ms 1.404ms 0.027ms 0.984x
Buffer<RGBA8>.Load linear: 41.704ms 1.390ms 0.024ms 0.993x
Buffer<RGBA8>.Load random: 41.431ms 1.381ms 0.023ms 1.000x
Buffer<R16f>.Load uniform: 41.301ms 1.377ms 0.023ms 1.003x
Buffer<R16f>.Load linear: 41.118ms 1.371ms 0.017ms 1.008x
Buffer<R16f>.Load random: 41.085ms 1.369ms 0.018ms 1.008x
Buffer<RG16f>.Load uniform: 41.019ms 1.367ms 0.017ms 1.010x
Buffer<RG16f>.Load linear: 40.915ms 1.364ms 0.013ms 1.013x
Buffer<RG16f>.Load random: 40.896ms 1.363ms 0.013ms 1.013x
Buffer<RGBA16f>.Load uniform: 41.213ms 1.374ms 0.013ms 1.005x
Buffer<RGBA16f>.Load linear: 41.009ms 1.367ms 0.012ms 1.010x
Buffer<RGBA16f>.Load random: 40.921ms 1.364ms 0.011ms 1.012x
Buffer<R32f>.Load uniform: 40.977ms 1.366ms 0.014ms 1.011x
Buffer<R32f>.Load linear: 41.016ms 1.367ms 0.013ms 1.010x
Buffer<R32f>.Load random: 41.044ms 1.368ms 0.016ms 1.009x
Buffer<RG32f>.Load uniform: 41.188ms 1.373ms 0.017ms 1.006x
Buffer<RG32f>.Load linear: 41.307ms 1.377ms 0.023ms 1.003x
Buffer<RG32f>.Load random: 41.455ms 1.382ms 0.022ms 0.999x
Buffer<RGBA32f>.Load uniform: 82.315ms 2.744ms 0.050ms 0.503x
Buffer<RGBA32f>.Load linear: 82.720ms 2.757ms 0.046ms 0.501x
Buffer<RGBA32f>.Load random: 83.249ms 2.775ms 0.042ms 0.498x
ByteAddressBuffer.Load uniform: 24.914ms 0.830ms 0.013ms 1.663x
ByteAddressBuffer.Load linear: 27.733ms 0.924ms 0.010ms 1.494x
ByteAddressBuffer.Load random: 25.408ms 0.847ms 0.008ms 1.631x
ByteAddressBuffer.Load2 uniform: 29.087ms 0.970ms 0.008ms 1.424x
ByteAddressBuffer.Load2 linear: 45.967ms 1.532ms 0.011ms 0.901x
ByteAddressBuffer.Load2 random: 29.715ms 0.990ms 0.008ms 1.394x
ByteAddressBuffer.Load3 uniform: 36.015ms 1.200ms 0.010ms 1.150x
ByteAddressBuffer.Load3 linear: 48.069ms 1.602ms 0.010ms 0.862x
ByteAddressBuffer.Load3 random: 40.612ms 1.354ms 0.009ms 1.020x
ByteAddressBuffer.Load4 uniform: 42.966ms 1.432ms 0.012ms 0.964x
ByteAddressBuffer.Load4 linear: 114.037ms 3.801ms 0.050ms 0.363x
ByteAddressBuffer.Load4 random: 88.896ms 2.963ms 0.058ms 0.466x
ByteAddressBuffer.Load2 unaligned uniform: 29.944ms 0.998ms 0.024ms 1.384x
ByteAddressBuffer.Load2 unaligned linear: 47.366ms 1.579ms 0.044ms 0.875x
ByteAddressBuffer.Load2 unaligned random: 30.523ms 1.017ms 0.026ms 1.357x
ByteAddressBuffer.Load4 unaligned uniform: 44.288ms 1.476ms 0.042ms 0.935x
ByteAddressBuffer.Load4 unaligned linear: 117.805ms 3.927ms 0.101ms 0.352x
ByteAddressBuffer.Load4 unaligned random: 92.137ms 3.071ms 0.075ms 0.450x
StructuredBuffer<float>.Load uniform: 21.173ms 0.706ms 0.018ms 1.957x
StructuredBuffer<float>.Load linear: 24.887ms 0.830ms 0.015ms 1.665x
StructuredBuffer<float>.Load random: 23.340ms 0.778ms 0.016ms 1.775x
StructuredBuffer<float2>.Load uniform: 23.389ms 0.780ms 0.015ms 1.771x
StructuredBuffer<float2>.Load linear: 26.705ms 0.890ms 0.018ms 1.551x
StructuredBuffer<float2>.Load random: 25.050ms 0.835ms 0.015ms 1.654x
StructuredBuffer<float4>.Load uniform: 45.781ms 1.526ms 0.026ms 0.905x
StructuredBuffer<float4>.Load linear: 59.790ms 1.993ms 0.034ms 0.693x
StructuredBuffer<float4>.Load random: 53.258ms 1.775ms 0.027ms 0.778x
cbuffer{float4} load uniform: 25.805ms 0.860ms 0.015ms 1.606x
cbuffer{float4} load linear: 1052.681ms 35.089ms 0.402ms 0.039x
cbuffer{float4} load random: 600.614ms 20.020ms 0.306ms 0.069x
Texture2D<R8>.Load uniform: 41.035ms 1.368ms 0.027ms 1.010x
Texture2D<R8>.Load linear: 40.913ms 1.364ms 0.024ms 1.013x
Texture2D<R8>.Load random: 40.795ms 1.360ms 0.013ms 1.016x
Texture2D<RG8>.Load uniform: 40.818ms 1.361ms 0.012ms 1.015x
Texture2D<RG8>.Load linear: 40.847ms 1.362ms 0.011ms 1.014x
Texture2D<RG8>.Load random: 40.785ms 1.360ms 0.013ms 1.016x
Texture2D<RGBA8>.Load uniform: 40.805ms 1.360ms 0.011ms 1.015x
Texture2D<RGBA8>.Load linear: 41.333ms 1.378ms 0.009ms 1.002x
Texture2D<RGBA8>.Load random: 41.057ms 1.369ms 0.009ms 1.009x
Texture2D<R16F>.Load uniform: 40.666ms 1.356ms 0.009ms 1.019x
Texture2D<R16F>.Load linear: 40.812ms 1.360ms 0.008ms 1.015x
Texture2D<R16F>.Load random: 40.786ms 1.360ms 0.011ms 1.016x
Texture2D<RG16F>.Load uniform: 40.809ms 1.360ms 0.015ms 1.015x
Texture2D<RG16F>.Load linear: 41.111ms 1.370ms 0.012ms 1.008x
Texture2D<RG16F>.Load random: 41.019ms 1.367ms 0.021ms 1.010x
Texture2D<RGBA16F>.Load uniform: 41.404ms 1.380ms 0.024ms 1.001x
Texture2D<RGBA16F>.Load linear: 42.212ms 1.407ms 0.025ms 0.982x
Texture2D<RGBA16F>.Load random: 82.685ms 2.756ms 0.089ms 0.501x
Texture2D<R32F>.Load uniform: 42.099ms 1.403ms 0.050ms 0.984x
Texture2D<R32F>.Load linear: 42.544ms 1.418ms 0.052ms 0.974x
Texture2D<R32F>.Load random: 42.721ms 1.424ms 0.057ms 0.970x
Texture2D<RG32F>.Load uniform: 43.266ms 1.442ms 0.056ms 0.958x
Texture2D<RG32F>.Load linear: 43.760ms 1.459ms 0.055ms 0.947x
Texture2D<RG32F>.Load random: 86.674ms 2.889ms 0.086ms 0.478x
Texture2D<RGBA32F>.Load uniform: 87.993ms 2.933ms 0.060ms 0.471x
Texture2D<RGBA32F>.Load linear: 89.194ms 2.973ms 0.025ms 0.465x
Texture2D<RGBA32F>.Load random: 89.533ms 2.984ms 0.022ms 0.463x
Texture2D<R8>.Sample(nearest) uniform: 45.503ms 1.517ms 0.014ms 0.911x
Texture2D<R8>.Sample(nearest) linear: 45.477ms 1.516ms 0.012ms 0.911x
Texture2D<R8>.Sample(nearest) random: 45.390ms 1.513ms 0.011ms 0.913x
Texture2D<RG8>.Sample(nearest) uniform: 45.568ms 1.519ms 0.010ms 0.909x
Texture2D<RG8>.Sample(nearest) linear: 45.669ms 1.522ms 0.011ms 0.907x
Texture2D<RG8>.Sample(nearest) random: 45.633ms 1.521ms 0.011ms 0.908x
Texture2D<RGBA8>.Sample(nearest) uniform: 47.668ms 1.589ms 0.012ms 0.869x
Texture2D<RGBA8>.Sample(nearest) linear: 48.155ms 1.605ms 0.013ms 0.860x
Texture2D<RGBA8>.Sample(nearest) random: 48.043ms 1.601ms 0.014ms 0.862x
Texture2D<R16F>.Sample(nearest) uniform: 45.443ms 1.515ms 0.013ms 0.912x
Texture2D<R16F>.Sample(nearest) linear: 45.389ms 1.513ms 0.012ms 0.913x
Texture2D<R16F>.Sample(nearest) random: 45.382ms 1.513ms 0.010ms 0.913x
Texture2D<RG16F>.Sample(nearest) uniform: 45.530ms 1.518ms 0.010ms 0.910x
Texture2D<RG16F>.Sample(nearest) linear: 45.730ms 1.524ms 0.012ms 0.906x
Texture2D<RG16F>.Sample(nearest) random: 45.593ms 1.520ms 0.012ms 0.909x
Texture2D<RGBA16F>.Sample(nearest) uniform: 47.595ms 1.587ms 0.015ms 0.870x
Texture2D<RGBA16F>.Sample(nearest) linear: 48.407ms 1.614ms 0.018ms 0.856x
Texture2D<RGBA16F>.Sample(nearest) random: 90.192ms 3.006ms 0.038ms 0.459x
Texture2D<R32F>.Sample(nearest) uniform: 45.930ms 1.531ms 0.023ms 0.902x
Texture2D<R32F>.Sample(nearest) linear: 46.137ms 1.538ms 0.025ms 0.898x
Texture2D<R32F>.Sample(nearest) random: 46.184ms 1.539ms 0.022ms 0.897x
Texture2D<RG32F>.Sample(nearest) uniform: 46.440ms 1.548ms 0.024ms 0.892x
Texture2D<RG32F>.Sample(nearest) linear: 46.688ms 1.556ms 0.020ms 0.887x
Texture2D<RG32F>.Sample(nearest) random: 91.388ms 3.046ms 0.033ms 0.453x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 91.601ms 3.053ms 0.029ms 0.452x
Texture2D<RGBA32F>.Sample(nearest) linear: 91.665ms 3.055ms 0.029ms 0.452x
Texture2D<RGBA32F>.Sample(nearest) random: 91.513ms 3.050ms 0.028ms 0.453x
Texture2D<R8>.Sample(bilinear) uniform: 46.322ms 1.544ms 0.015ms 0.894x
Texture2D<R8>.Sample(bilinear) linear: 46.185ms 1.540ms 0.014ms 0.897x
Texture2D<R8>.Sample(bilinear) random: 46.078ms 1.536ms 0.015ms 0.899x
Texture2D<RG8>.Sample(bilinear) uniform: 46.202ms 1.540ms 0.013ms 0.897x
Texture2D<RG8>.Sample(bilinear) linear: 46.313ms 1.544ms 0.014ms 0.895x
Texture2D<RG8>.Sample(bilinear) random: 46.112ms 1.537ms 0.016ms 0.898x
Texture2D<RGBA8>.Sample(bilinear) uniform: 48.008ms 1.600ms 0.018ms 0.863x
Texture2D<RGBA8>.Sample(bilinear) linear: 48.692ms 1.623ms 0.018ms 0.851x
Texture2D<RGBA8>.Sample(bilinear) random: 48.411ms 1.614ms 0.017ms 0.856x
Texture2D<R16F>.Sample(bilinear) uniform: 45.681ms 1.523ms 0.018ms 0.907x
Texture2D<R16F>.Sample(bilinear) linear: 45.741ms 1.525ms 0.012ms 0.906x
Texture2D<R16F>.Sample(bilinear) random: 45.624ms 1.521ms 0.012ms 0.908x
Texture2D<RG16F>.Sample(bilinear) uniform: 45.744ms 1.525ms 0.010ms 0.906x
Texture2D<RG16F>.Sample(bilinear) linear: 45.987ms 1.533ms 0.016ms 0.901x
Texture2D<RG16F>.Sample(bilinear) random: 45.889ms 1.530ms 0.015ms 0.903x
Texture2D<RGBA16F>.Sample(bilinear) uniform: 47.870ms 1.596ms 0.014ms 0.865x
Texture2D<RGBA16F>.Sample(bilinear) linear: 48.595ms 1.620ms 0.015ms 0.853x
Texture2D<RGBA16F>.Sample(bilinear) random: 90.672ms 3.022ms 0.027ms 0.457x
Texture2D<R32F>.Sample(bilinear) uniform: 46.160ms 1.539ms 0.017ms 0.898x
Texture2D<R32F>.Sample(bilinear) linear: 46.187ms 1.540ms 0.017ms 0.897x
Texture2D<R32F>.Sample(bilinear) random: 46.286ms 1.543ms 0.017ms 0.895x
Texture2D<RG32F>.Sample(bilinear) uniform: 46.592ms 1.553ms 0.014ms 0.889x
Texture2D<RG32F>.Sample(bilinear) linear: 46.834ms 1.561ms 0.014ms 0.885x
Texture2D<RG32F>.Sample(bilinear) random: 91.524ms 3.051ms 0.023ms 0.453x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 182.679ms 6.089ms 0.039ms 0.227x
Texture2D<RGBA32F>.Sample(bilinear) linear: 182.764ms 6.092ms 0.037ms 0.227x
Texture2D<RGBA32F>.Sample(bilinear) random: 270.850ms 9.028ms 0.067ms 0.153x
```

### NVidia Ampere (RTX 3090)
```
Buffer<R8>.Load uniform: 0.691ms 15.067x
Buffer<R8>.Load linear: 6.324ms 1.647x
Buffer<R8>.Load random: 7.773ms 1.340x
Buffer<RG8>.Load uniform: 0.717ms 14.529x
Buffer<RG8>.Load linear: 6.334ms 1.644x
Buffer<RG8>.Load random: 7.843ms 1.328x
Buffer<RGBA8>.Load uniform: 0.842ms 12.372x
Buffer<RGBA8>.Load linear: 7.419ms 1.404x
Buffer<RGBA8>.Load random: 10.414ms 1.000x
Buffer<R16f>.Load uniform: 0.651ms 15.991x
Buffer<R16f>.Load linear: 6.328ms 1.646x
Buffer<R16f>.Load random: 6.824ms 1.526x
Buffer<RG16f>.Load uniform: 0.722ms 14.426x
Buffer<RG16f>.Load linear: 6.845ms 1.521x
Buffer<RG16f>.Load random: 9.893ms 1.053x
Buffer<RGBA16f>.Load uniform: 0.891ms 11.690x
Buffer<RGBA16f>.Load linear: 7.490ms 1.390x
Buffer<RGBA16f>.Load random: 7.536ms 1.382x
Buffer<R32f>.Load uniform: 0.676ms 15.409x
Buffer<R32f>.Load linear: 7.352ms 1.416x
Buffer<R32f>.Load random: 9.929ms 1.049x
Buffer<RG32f>.Load uniform: 0.767ms 13.578x
Buffer<RG32f>.Load linear: 6.349ms 1.640x
Buffer<RG32f>.Load random: 6.842ms 1.522x
Buffer<RGBA32f>.Load uniform: 0.973ms 10.705x
Buffer<RGBA32f>.Load linear: 14.504ms 0.718x
Buffer<RGBA32f>.Load random: 13.037ms 0.799x
ByteAddressBuffer.Load uniform: 7.217ms 1.443x
ByteAddressBuffer.Load linear: 6.009ms 1.733x
ByteAddressBuffer.Load random: 5.433ms 1.917x
ByteAddressBuffer.Load2 uniform: 10.077ms 1.033x
ByteAddressBuffer.Load2 linear: 7.871ms 1.323x
ByteAddressBuffer.Load2 random: 7.259ms 1.435x
ByteAddressBuffer.Load3 uniform: 10.867ms 0.958x
ByteAddressBuffer.Load3 linear: 10.198ms 1.021x
ByteAddressBuffer.Load3 random: 10.597ms 0.983x
ByteAddressBuffer.Load4 uniform: 12.582ms 0.828x
ByteAddressBuffer.Load4 linear: 15.811ms 0.659x
ByteAddressBuffer.Load4 random: 12.665ms 0.822x
ByteAddressBuffer.Load2 unaligned uniform: 9.054ms 1.150x
ByteAddressBuffer.Load2 unaligned linear: 7.347ms 1.417x
ByteAddressBuffer.Load2 unaligned random: 7.258ms 1.435x
ByteAddressBuffer.Load4 unaligned uniform: 12.581ms 0.828x
ByteAddressBuffer.Load4 unaligned linear: 15.790ms 0.660x
ByteAddressBuffer.Load4 unaligned random: 12.666ms 0.822x
StructuredBuffer<float>.Load uniform: 5.889ms 1.768x
StructuredBuffer<float>.Load linear: 4.689ms 2.221x
StructuredBuffer<float>.Load random: 4.648ms 2.241x
StructuredBuffer<float2>.Load uniform: 6.670ms 1.561x
StructuredBuffer<float2>.Load linear: 6.513ms 1.599x
StructuredBuffer<float2>.Load random: 5.817ms 1.790x
StructuredBuffer<float4>.Load uniform: 7.168ms 1.453x
StructuredBuffer<float4>.Load linear: 9.839ms 1.058x
StructuredBuffer<float4>.Load random: 9.253ms 1.125x
cbuffer{float4} load uniform: 1.126ms 9.245x
cbuffer{float4} load linear: 280.222ms 0.037x
cbuffer{float4} load random: 98.995ms 0.105x
Texture2D<R8>.Load uniform: 0.676ms 15.409x
Texture2D<R8>.Load linear: 6.335ms 1.644x
Texture2D<R8>.Load random: 6.310ms 1.650x
Texture2D<RG8>.Load uniform: 0.815ms 12.776x
Texture2D<RG8>.Load linear: 6.338ms 1.643x
Texture2D<RG8>.Load random: 6.324ms 1.647x
Texture2D<RGBA8>.Load uniform: 0.973ms 10.705x
Texture2D<RGBA8>.Load linear: 9.430ms 1.104x
Texture2D<RGBA8>.Load random: 12.498ms 0.833x
Texture2D<R16F>.Load uniform: 0.709ms 14.697x
Texture2D<R16F>.Load linear: 6.337ms 1.644x
Texture2D<R16F>.Load random: 6.314ms 1.649x
Texture2D<RG16F>.Load uniform: 0.778ms 13.382x
Texture2D<RG16F>.Load linear: 9.417ms 1.106x
Texture2D<RG16F>.Load random: 12.493ms 0.834x
Texture2D<RGBA16F>.Load uniform: 1.024ms 10.170x
Texture2D<RGBA16F>.Load linear: 17.148ms 0.607x
Texture2D<RGBA16F>.Load random: 25.050ms 0.416x
Texture2D<R32F>.Load uniform: 0.740ms 14.066x
Texture2D<R32F>.Load linear: 9.774ms 1.065x
Texture2D<R32F>.Load random: 12.493ms 0.834x
Texture2D<RG32F>.Load uniform: 0.863ms 12.064x
Texture2D<RG32F>.Load linear: 17.484ms 0.596x
Texture2D<RG32F>.Load random: 25.180ms 0.414x
Texture2D<RGBA32F>.Load uniform: 1.176ms 8.859x
Texture2D<RGBA32F>.Load linear: 25.574ms 0.407x
Texture2D<RGBA32F>.Load random: 25.952ms 0.401x
Texture2D<R8>.Sample(nearest) uniform: 12.506ms 0.833x
Texture2D<R8>.Sample(nearest) linear: 12.513ms 0.832x
Texture2D<R8>.Sample(nearest) random: 13.423ms 0.776x
Texture2D<RG8>.Sample(nearest) uniform: 12.867ms 0.809x
Texture2D<RG8>.Sample(nearest) linear: 12.884ms 0.808x
Texture2D<RG8>.Sample(nearest) random: 15.190ms 0.686x
Texture2D<RGBA8>.Sample(nearest) uniform: 13.018ms 0.800x
Texture2D<RGBA8>.Sample(nearest) linear: 12.530ms 0.831x
Texture2D<RGBA8>.Sample(nearest) random: 13.568ms 0.768x
Texture2D<R16F>.Sample(nearest) uniform: 13.230ms 0.787x
Texture2D<R16F>.Sample(nearest) linear: 12.514ms 0.832x
Texture2D<R16F>.Sample(nearest) random: 14.266ms 0.730x
Texture2D<RG16F>.Sample(nearest) uniform: 13.395ms 0.777x
Texture2D<RG16F>.Sample(nearest) linear: 13.051ms 0.798x
Texture2D<RG16F>.Sample(nearest) random: 13.401ms 0.777x
Texture2D<RGBA16F>.Sample(nearest) uniform: 13.421ms 0.776x
Texture2D<RGBA16F>.Sample(nearest) linear: 12.902ms 0.807x
Texture2D<RGBA16F>.Sample(nearest) random: 26.066ms 0.400x
Texture2D<R32F>.Sample(nearest) uniform: 12.870ms 0.809x
Texture2D<R32F>.Sample(nearest) linear: 13.069ms 0.797x
Texture2D<R32F>.Sample(nearest) random: 12.508ms 0.833x
Texture2D<RG32F>.Sample(nearest) uniform: 13.945ms 0.747x
Texture2D<RG32F>.Sample(nearest) linear: 12.524ms 0.832x
Texture2D<RG32F>.Sample(nearest) random: 26.621ms 0.391x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 26.447ms 0.394x
Texture2D<RGBA32F>.Sample(nearest) linear: 26.258ms 0.397x
Texture2D<RGBA32F>.Sample(nearest) random: 25.757ms 0.404x
Texture2D<R8>.Sample(bilinear) uniform: 12.508ms 0.833x
Texture2D<R8>.Sample(bilinear) linear: 13.029ms 0.799x
Texture2D<R8>.Sample(bilinear) random: 12.507ms 0.833x
Texture2D<RG8>.Sample(bilinear) uniform: 12.510ms 0.832x
Texture2D<RG8>.Sample(bilinear) linear: 13.034ms 0.799x
Texture2D<RG8>.Sample(bilinear) random: 13.032ms 0.799x
Texture2D<RGBA8>.Sample(bilinear) uniform: 12.514ms 0.832x
Texture2D<RGBA8>.Sample(bilinear) linear: 13.060ms 0.797x
Texture2D<RGBA8>.Sample(bilinear) random: 12.520ms 0.832x
Texture2D<R16F>.Sample(bilinear) uniform: 12.507ms 0.833x
Texture2D<R16F>.Sample(bilinear) linear: 12.514ms 0.832x
Texture2D<R16F>.Sample(bilinear) random: 12.509ms 0.833x
Texture2D<RG16F>.Sample(bilinear) uniform: 13.034ms 0.799x
Texture2D<RG16F>.Sample(bilinear) linear: 12.516ms 0.832x
Texture2D<RG16F>.Sample(bilinear) random: 12.522ms 0.832x
Texture2D<RGBA16F>.Sample(bilinear) uniform: 12.508ms 0.833x
Texture2D<RGBA16F>.Sample(bilinear) linear: 12.533ms 0.831x
Texture2D<RGBA16F>.Sample(bilinear) random: 24.840ms 0.419x
Texture2D<R32F>.Sample(bilinear) uniform: 12.507ms 0.833x
Texture2D<R32F>.Sample(bilinear) linear: 12.516ms 0.832x
Texture2D<R32F>.Sample(bilinear) random: 12.510ms 0.832x
Texture2D<RG32F>.Sample(bilinear) uniform: 12.510ms 0.832x
Texture2D<RG32F>.Sample(bilinear) linear: 12.526ms 0.831x
Texture2D<RG32F>.Sample(bilinear) random: 24.839ms 0.419x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 49.561ms 0.210x
Texture2D<RGBA32F>.Sample(bilinear) linear: 49.592ms 0.210x
Texture2D<RGBA32F>.Sample(bilinear) random: 74.230ms 0.140x
```

**NVIDIA Ampere** results (ratios) of most common load/sample operations look similar to Turing.

**Sampler ratios (NEW!):** New tests for sampler ratios show that Ampere has half rate bilinear RG32F and quarter rate bilinear RGBA32F. Nearest filtering is full rate, except for RGBA32F which is half rate (similar to RGBA32F texture loads). In Turing and Ampere RGBA32/float4 buffer loads are full rate.

### NVidia Ampere (RTX 3060) Dx12+Dxc
```markdown
Buffer<R8>.Load uniform: 19.301ms 0.643ms 0.001ms 1.493x
Buffer<R8>.Load linear: 19.310ms 0.644ms 0.002ms 1.493x
Buffer<R8>.Load random: 22.015ms 0.734ms 0.001ms 1.309x
Buffer<RG8>.Load uniform: 19.656ms 0.655ms 0.052ms 1.466x
Buffer<RG8>.Load linear: 19.562ms 0.652ms 0.035ms 1.473x
Buffer<RG8>.Load random: 19.599ms 0.653ms 0.037ms 1.470x
Buffer<RGBA8>.Load uniform: 20.206ms 0.674ms 0.001ms 1.426x
Buffer<RGBA8>.Load linear: 20.247ms 0.675ms 0.001ms 1.423x
Buffer<RGBA8>.Load random: 28.820ms 0.961ms 0.036ms 1.000x
Buffer<R16f>.Load uniform: 19.200ms 0.640ms 0.001ms 1.501x
Buffer<R16f>.Load linear: 19.360ms 0.645ms 0.002ms 1.489x
Buffer<R16f>.Load random: 19.289ms 0.643ms 0.001ms 1.494x
Buffer<RG16f>.Load uniform: 19.642ms 0.655ms 0.078ms 1.467x
Buffer<RG16f>.Load linear: 19.385ms 0.646ms 0.000ms 1.487x
Buffer<RG16f>.Load random: 28.574ms 0.952ms 0.003ms 1.009x
Buffer<RGBA16f>.Load uniform: 20.290ms 0.676ms 0.001ms 1.420x
Buffer<RGBA16f>.Load linear: 20.368ms 0.679ms 0.003ms 1.415x
Buffer<RGBA16f>.Load random: 20.234ms 0.674ms 0.001ms 1.424x
Buffer<R32f>.Load uniform: 19.200ms 0.640ms 0.001ms 1.501x
Buffer<R32f>.Load linear: 19.365ms 0.645ms 0.002ms 1.488x
Buffer<R32f>.Load random: 28.627ms 0.954ms 0.002ms 1.007x
Buffer<RG32f>.Load uniform: 19.243ms 0.641ms 0.001ms 1.498x
Buffer<RG32f>.Load linear: 19.273ms 0.642ms 0.004ms 1.495x
Buffer<RG32f>.Load random: 19.234ms 0.641ms 0.002ms 1.498x
Buffer<RGBA32f>.Load uniform: 38.357ms 1.279ms 0.040ms 0.751x
Buffer<RGBA32f>.Load linear: 38.228ms 1.274ms 0.001ms 0.754x
Buffer<RGBA32f>.Load random: 38.096ms 1.270ms 0.001ms 0.757x
ByteAddressBuffer.Load uniform: 24.101ms 0.803ms 0.078ms 1.196x
ByteAddressBuffer.Load linear: 26.086ms 0.870ms 0.000ms 1.105x
ByteAddressBuffer.Load random: 24.122ms 0.804ms 0.052ms 1.195x
ByteAddressBuffer.Load2 uniform: 28.951ms 0.965ms 0.086ms 0.996x
ByteAddressBuffer.Load2 linear: 29.716ms 0.991ms 0.085ms 0.970x
ByteAddressBuffer.Load2 random: 27.587ms 0.920ms 0.102ms 1.045x
ByteAddressBuffer.Load3 uniform: 32.907ms 1.097ms 0.130ms 0.876x
ByteAddressBuffer.Load3 linear: 35.248ms 1.175ms 0.141ms 0.818x
ByteAddressBuffer.Load3 random: 33.923ms 1.131ms 0.001ms 0.850x
ByteAddressBuffer.Load4 uniform: 38.487ms 1.283ms 0.062ms 0.749x
ByteAddressBuffer.Load4 linear: 49.946ms 1.665ms 0.113ms 0.577x
ByteAddressBuffer.Load4 random: 38.990ms 1.300ms 0.093ms 0.739x
ByteAddressBuffer.Load2 unaligned uniform: 29.002ms 0.967ms 0.069ms 0.994x
ByteAddressBuffer.Load2 unaligned linear: 29.620ms 0.987ms 0.088ms 0.973x
ByteAddressBuffer.Load2 unaligned random: 27.663ms 0.922ms 0.097ms 1.042x
ByteAddressBuffer.Load4 unaligned uniform: 39.116ms 1.304ms 0.101ms 0.737x
ByteAddressBuffer.Load4 unaligned linear: 48.755ms 1.625ms 0.081ms 0.591x
ByteAddressBuffer.Load4 unaligned random: 38.488ms 1.283ms 0.078ms 0.749x
StructuredBuffer<float>.Load uniform: 17.312ms 0.577ms 0.033ms 1.665x
StructuredBuffer<float>.Load linear: 22.315ms 0.744ms 0.001ms 1.292x
StructuredBuffer<float>.Load random: 22.470ms 0.749ms 0.001ms 1.283x
StructuredBuffer<float2>.Load uniform: 18.260ms 0.609ms 0.001ms 1.578x
StructuredBuffer<float2>.Load linear: 22.315ms 0.744ms 0.001ms 1.292x
StructuredBuffer<float2>.Load random: 22.369ms 0.746ms 0.000ms 1.288x
StructuredBuffer<float4>.Load uniform: 20.777ms 0.693ms 0.001ms 1.387x
StructuredBuffer<float4>.Load linear: 22.985ms 0.766ms 0.047ms 1.254x
StructuredBuffer<float4>.Load random: 22.442ms 0.748ms 0.001ms 1.284x
cbuffer{float4} load uniform: 26.342ms 0.878ms 0.052ms 1.094x
cbuffer{float4} load linear: 884.881ms 29.496ms 0.233ms 0.033x
cbuffer{float4} load random: 568.427ms 18.948ms 0.177ms 0.051x
Texture2D<R8>.Load uniform: 20.487ms 0.683ms 0.001ms 1.407x
Texture2D<R8>.Load linear: 20.420ms 0.681ms 0.001ms 1.411x
Texture2D<R8>.Load random: 20.442ms 0.681ms 0.002ms 1.410x
Texture2D<RG8>.Load uniform: 20.385ms 0.679ms 0.001ms 1.414x
Texture2D<RG8>.Load linear: 20.358ms 0.679ms 0.002ms 1.416x
Texture2D<RG8>.Load random: 20.539ms 0.685ms 0.001ms 1.403x
Texture2D<RGBA8>.Load uniform: 21.413ms 0.714ms 0.000ms 1.346x
Texture2D<RGBA8>.Load linear: 29.704ms 0.990ms 0.002ms 0.970x
Texture2D<RGBA8>.Load random: 40.703ms 1.357ms 0.089ms 0.708x
Texture2D<R16F>.Load uniform: 20.507ms 0.684ms 0.001ms 1.405x
Texture2D<R16F>.Load linear: 20.424ms 0.681ms 0.001ms 1.411x
Texture2D<R16F>.Load random: 20.470ms 0.682ms 0.002ms 1.408x
Texture2D<RG16F>.Load uniform: 20.440ms 0.681ms 0.000ms 1.410x
Texture2D<RG16F>.Load linear: 29.577ms 0.986ms 0.000ms 0.974x
Texture2D<RG16F>.Load random: 40.110ms 1.337ms 0.001ms 0.719x
Texture2D<RGBA16F>.Load uniform: 21.624ms 0.721ms 0.001ms 1.333x
Texture2D<RGBA16F>.Load linear: 54.502ms 1.817ms 0.005ms 0.529x
Texture2D<RGBA16F>.Load random: 80.639ms 2.688ms 0.009ms 0.357x
Texture2D<R32F>.Load uniform: 20.586ms 0.686ms 0.001ms 1.400x
Texture2D<R32F>.Load linear: 29.941ms 0.998ms 0.004ms 0.963x
Texture2D<R32F>.Load random: 40.446ms 1.348ms 0.044ms 0.713x
Texture2D<RG32F>.Load uniform: 20.643ms 0.688ms 0.001ms 1.396x
Texture2D<RG32F>.Load linear: 54.445ms 1.815ms 0.002ms 0.529x
Texture2D<RG32F>.Load random: 80.666ms 2.689ms 0.005ms 0.357x
Texture2D<RGBA32F>.Load uniform: 40.736ms 1.358ms 0.000ms 0.707x
Texture2D<RGBA32F>.Load linear: 80.360ms 2.679ms 0.001ms 0.359x
Texture2D<RGBA32F>.Load random: 80.761ms 2.692ms 0.048ms 0.357x
Texture2D<R8>.Sample(nearest) uniform: 40.360ms 1.345ms 0.001ms 0.714x
Texture2D<R8>.Sample(nearest) linear: 40.364ms 1.345ms 0.000ms 0.714x
Texture2D<R8>.Sample(nearest) random: 40.397ms 1.347ms 0.002ms 0.713x
Texture2D<RG8>.Sample(nearest) uniform: 40.372ms 1.346ms 0.001ms 0.714x
Texture2D<RG8>.Sample(nearest) linear: 40.719ms 1.357ms 0.039ms 0.708x
Texture2D<RG8>.Sample(nearest) random: 40.383ms 1.346ms 0.001ms 0.714x
Texture2D<RGBA8>.Sample(nearest) uniform: 40.392ms 1.346ms 0.001ms 0.714x
Texture2D<RGBA8>.Sample(nearest) linear: 40.717ms 1.357ms 0.036ms 0.708x
Texture2D<RGBA8>.Sample(nearest) random: 40.848ms 1.362ms 0.061ms 0.706x
Texture2D<R16F>.Sample(nearest) uniform: 40.351ms 1.345ms 0.001ms 0.714x
Texture2D<R16F>.Sample(nearest) linear: 40.479ms 1.349ms 0.001ms 0.712x
Texture2D<R16F>.Sample(nearest) random: 40.421ms 1.347ms 0.002ms 0.713x
Texture2D<RG16F>.Sample(nearest) uniform: 40.566ms 1.352ms 0.036ms 0.710x
Texture2D<RG16F>.Sample(nearest) linear: 41.461ms 1.382ms 0.101ms 0.695x
Texture2D<RG16F>.Sample(nearest) random: 41.515ms 1.384ms 0.131ms 0.694x
Texture2D<RGBA16F>.Sample(nearest) uniform: 40.834ms 1.361ms 0.055ms 0.706x
Texture2D<RGBA16F>.Sample(nearest) linear: 40.519ms 1.351ms 0.002ms 0.711x
Texture2D<RGBA16F>.Sample(nearest) random: 80.786ms 2.693ms 0.085ms 0.357x
Texture2D<R32F>.Sample(nearest) uniform: 40.357ms 1.345ms 0.001ms 0.714x
Texture2D<R32F>.Sample(nearest) linear: 40.712ms 1.357ms 0.042ms 0.708x
Texture2D<R32F>.Sample(nearest) random: 40.478ms 1.349ms 0.000ms 0.712x
Texture2D<RG32F>.Sample(nearest) uniform: 40.826ms 1.361ms 0.080ms 0.706x
Texture2D<RG32F>.Sample(nearest) linear: 40.496ms 1.350ms 0.002ms 0.712x
Texture2D<RG32F>.Sample(nearest) random: 80.505ms 2.683ms 0.009ms 0.358x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 80.725ms 2.691ms 0.004ms 0.357x
Texture2D<RGBA32F>.Sample(nearest) linear: 80.660ms 2.689ms 0.077ms 0.357x
Texture2D<RGBA32F>.Sample(nearest) random: 80.280ms 2.676ms 0.003ms 0.359x
Texture2D<R8>.Sample(bilinear) uniform: 40.363ms 1.345ms 0.001ms 0.714x
Texture2D<R8>.Sample(bilinear) linear: 40.565ms 1.352ms 0.035ms 0.710x
Texture2D<R8>.Sample(bilinear) random: 40.389ms 1.346ms 0.002ms 0.714x
Texture2D<RG8>.Sample(bilinear) uniform: 40.369ms 1.346ms 0.001ms 0.714x
Texture2D<RG8>.Sample(bilinear) linear: 40.491ms 1.350ms 0.000ms 0.712x
Texture2D<RG8>.Sample(bilinear) random: 40.825ms 1.361ms 0.080ms 0.706x
Texture2D<RGBA8>.Sample(bilinear) uniform: 40.745ms 1.358ms 0.060ms 0.707x
Texture2D<RGBA8>.Sample(bilinear) linear: 40.716ms 1.357ms 0.037ms 0.708x
Texture2D<RGBA8>.Sample(bilinear) random: 40.715ms 1.357ms 0.036ms 0.708x
Texture2D<R16F>.Sample(bilinear) uniform: 40.375ms 1.346ms 0.001ms 0.714x
Texture2D<R16F>.Sample(bilinear) linear: 40.483ms 1.349ms 0.001ms 0.712x
Texture2D<R16F>.Sample(bilinear) random: 40.405ms 1.347ms 0.002ms 0.713x
Texture2D<RG16F>.Sample(bilinear) uniform: 40.382ms 1.346ms 0.002ms 0.714x
Texture2D<RG16F>.Sample(bilinear) linear: 40.492ms 1.350ms 0.001ms 0.712x
Texture2D<RG16F>.Sample(bilinear) random: 41.360ms 1.379ms 0.111ms 0.697x
Texture2D<RGBA16F>.Sample(bilinear) uniform: 41.048ms 1.368ms 0.115ms 0.702x
Texture2D<RGBA16F>.Sample(bilinear) linear: 40.525ms 1.351ms 0.002ms 0.711x
Texture2D<RGBA16F>.Sample(bilinear) random: 80.319ms 2.677ms 0.002ms 0.359x
Texture2D<R32F>.Sample(bilinear) uniform: 40.381ms 1.346ms 0.002ms 0.714x
Texture2D<R32F>.Sample(bilinear) linear: 40.492ms 1.350ms 0.001ms 0.712x
Texture2D<R32F>.Sample(bilinear) random: 40.483ms 1.349ms 0.001ms 0.712x
Texture2D<RG32F>.Sample(bilinear) uniform: 40.393ms 1.346ms 0.003ms 0.714x
Texture2D<RG32F>.Sample(bilinear) linear: 40.960ms 1.365ms 0.084ms 0.704x
Texture2D<RG32F>.Sample(bilinear) random: 80.945ms 2.698ms 0.058ms 0.356x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 161.137ms 5.371ms 0.130ms 0.179x
Texture2D<RGBA32F>.Sample(bilinear) linear: 160.190ms 5.340ms 0.001ms 0.180x
Texture2D<RGBA32F>.Sample(bilinear) random: 245.120ms 8.171ms 0.001ms 0.118x
``` 

### Intel Gen9 (HD 630 / i7 6700K)
```markdown
Buffer<R8>.Load uniform: 48.527ms 5.955x
Buffer<R8>.Load linear: 243.487ms 1.187x
Buffer<R8>.Load random: 286.351ms 1.009x
Buffer<RG8>.Load uniform: 49.022ms 5.895x
Buffer<RG8>.Load linear: 242.316ms 1.193x
Buffer<RG8>.Load random: 288.927ms 1.000x
Buffer<RGBA8>.Load uniform: 48.962ms 5.902x
Buffer<RGBA8>.Load linear: 244.140ms 1.184x
Buffer<RGBA8>.Load random: 288.981ms 1.000x
Buffer<R16f>.Load uniform: 49.989ms 5.781x
Buffer<R16f>.Load linear: 242.649ms 1.191x
Buffer<R16f>.Load random: 287.790ms 1.004x
Buffer<RG16f>.Load uniform: 48.921ms 5.907x
Buffer<RG16f>.Load linear: 243.826ms 1.185x
Buffer<RG16f>.Load random: 286.305ms 1.009x
Buffer<RGBA16f>.Load uniform: 48.855ms 5.915x
Buffer<RGBA16f>.Load linear: 242.278ms 1.193x
Buffer<RGBA16f>.Load random: 288.235ms 1.003x
Buffer<R32f>.Load uniform: 49.272ms 5.865x
Buffer<R32f>.Load linear: 241.286ms 1.198x
Buffer<R32f>.Load random: 286.946ms 1.007x
Buffer<RG32f>.Load uniform: 48.587ms 5.948x
Buffer<RG32f>.Load linear: 242.442ms 1.192x
Buffer<RG32f>.Load random: 287.429ms 1.005x
Buffer<RGBA32f>.Load uniform: 48.562ms 5.951x
Buffer<RGBA32f>.Load linear: 241.818ms 1.195x
Buffer<RGBA32f>.Load random: 287.268ms 1.006x
ByteAddressBuffer.Load uniform: 15.647ms 18.469x
ByteAddressBuffer.Load linear: 49.962ms 5.784x
ByteAddressBuffer.Load random: 51.418ms 5.620x
ByteAddressBuffer.Load2 uniform: 13.941ms 20.728x
ByteAddressBuffer.Load2 linear: 93.546ms 3.089x
ByteAddressBuffer.Load2 random: 140.016ms 2.064x
ByteAddressBuffer.Load3 uniform: 19.754ms 14.629x
ByteAddressBuffer.Load3 linear: 168.581ms 1.714x
ByteAddressBuffer.Load3 random: 312.721ms 0.924x
ByteAddressBuffer.Load4 uniform: 13.932ms 20.743x
ByteAddressBuffer.Load4 linear: 175.224ms 1.649x
ByteAddressBuffer.Load4 random: 340.677ms 0.848x
ByteAddressBuffer.Load2 unaligned uniform: 15.152ms 19.072x
ByteAddressBuffer.Load2 unaligned linear: 99.901ms 2.893x
ByteAddressBuffer.Load2 unaligned random: 145.827ms 1.982x
ByteAddressBuffer.Load4 unaligned uniform: 16.249ms 17.784x
ByteAddressBuffer.Load4 unaligned linear: 199.205ms 1.451x
ByteAddressBuffer.Load4 unaligned random: 378.326ms 0.764x
StructuredBuffer<float>.Load uniform: 14.309ms 20.195x
StructuredBuffer<float>.Load linear: 50.181ms 5.759x
StructuredBuffer<float>.Load random: 51.750ms 5.584x
StructuredBuffer<float2>.Load uniform: 13.856ms 20.856x
StructuredBuffer<float2>.Load linear: 94.388ms 3.062x
StructuredBuffer<float2>.Load random: 141.301ms 2.045x
StructuredBuffer<float4>.Load uniform: 13.493ms 21.417x
StructuredBuffer<float4>.Load linear: 175.457ms 1.647x
StructuredBuffer<float4>.Load random: 340.806ms 0.848x
cbuffer{float4} load uniform: 13.443ms 21.497x
cbuffer{float4} load linear: 242.860ms 1.190x
cbuffer{float4} load random: 285.850ms 1.011x
Texture2D<R8>.Load uniform: 24.519ms 11.786x
Texture2D<R8>.Load linear: 97.392ms 2.967x
Texture2D<R8>.Load random: 97.824ms 2.954x
Texture2D<RG8>.Load uniform: 24.376ms 11.855x
Texture2D<RG8>.Load linear: 97.068ms 2.977x
Texture2D<RG8>.Load random: 97.767ms 2.956x
Texture2D<RGBA8>.Load uniform: 24.509ms 11.791x
Texture2D<RGBA8>.Load linear: 101.171ms 2.856x
Texture2D<RGBA8>.Load random: 101.069ms 2.859x
Texture2D<R16F>.Load uniform: 24.874ms 11.618x
Texture2D<R16F>.Load linear: 97.947ms 2.950x
Texture2D<R16F>.Load random: 97.385ms 2.967x
Texture2D<RG16F>.Load uniform: 24.324ms 11.881x
Texture2D<RG16F>.Load linear: 98.257ms 2.941x
Texture2D<RG16F>.Load random: 97.672ms 2.959x
Texture2D<RGBA16F>.Load uniform: 24.408ms 11.840x
Texture2D<RGBA16F>.Load linear: 101.515ms 2.847x
Texture2D<RGBA16F>.Load random: 195.229ms 1.480x
Texture2D<R32F>.Load uniform: 24.677ms 11.710x
Texture2D<R32F>.Load linear: 97.829ms 2.954x
Texture2D<R32F>.Load random: 97.614ms 2.960x
Texture2D<RG32F>.Load uniform: 24.859ms 11.625x
Texture2D<RG32F>.Load linear: 97.809ms 2.955x
Texture2D<RG32F>.Load random: 194.397ms 1.487x
Texture2D<RGBA32F>.Load uniform: 24.660ms 11.719x
Texture2D<RGBA32F>.Load linear: 243.432ms 1.187x
Texture2D<RGBA32F>.Load random: 195.579ms 1.478x
 
NOTE: Intel result not directly comparable with other GPUs. I had to reduce workload size to avoid TDR.
```

**Typed loads:** All typed loads have same identical performance. Dimensions (1d/2d/4d) and channel widths (8b/16b/32b) don't affect performance. Intel compiler has a fast path for uniform address loads. It improves performance by up to 6x. Linear typed loads do not coalesce. Best bytes per cycle rate can be achieved by widest RGBA32 loads.

**Raw (ByteAddressBuffer) loads:** Intel raw buffer loads are significantly faster compared to similar typed loads. 1d raw load is 5x faster than any typed load. 2d linear raw load is 2.5x faster than typed loads. 4d linear raw load is 40% faster than typed loads. 2d/4d random raw loads are around 2x slower compared to linear ones (could be coalescing or something else). 3d raw load performance matches 4d. Alignment doesn't seem to matter. Uniform address raw loads also use the same compiler fast path as typed loads (6x gain).

**Structured buffer loads:** Performance is identical to similar raw buffer loads.

**Cbuffer loads:** 22x faster than normal load for fully uniform address. Linear/random access performance identical to typed buffer loads. Raw/structured buffers are up to 2x faster if you have linear/random access pattern.

**Texture loads:** All formats perform similarly, except the widest RGBA32 (half speed linear). Uniform address texture loads are 4x faster than linear. There's certainly something fishy going on, as Texture2D loads are generally 2x+ faster than same format buffer loads. Maybe I am hitting some bank conflict case or Intel is swizzling the buffer layout.

**Suggestions:** When using typed buffers, prefer widest loads (RGBA32). Raw buffers are significantly faster than typed buffers. 

**Uniform address optimization:** Uniform address loads are very fast (both raw and typed). Intel has confirmed that their compiler uses a wave shuffle trick to speed up uniform loads inside loops. See "Uniform Address Load Investigation" chapter for more info. 


### Intel Gen11 (Iris Plus / i7-1065g7)
```markdown
Buffer<R8>.Load uniform: 58.213ms 11.630x
Buffer<R8>.Load linear: 591.619ms 1.144x
Buffer<R8>.Load random: 699.948ms 0.967x
Buffer<RG8>.Load uniform: 59.530ms 11.373x
Buffer<RG8>.Load linear: 598.979ms 1.130x
Buffer<RG8>.Load random: 678.129ms 0.998x
Buffer<RGBA8>.Load uniform: 59.296ms 11.418x
Buffer<RGBA8>.Load linear: 571.312ms 1.185x
Buffer<RGBA8>.Load random: 677.040ms 1.000x
Buffer<R16f>.Load uniform: 58.108ms 11.651x
Buffer<R16f>.Load linear: 571.071ms 1.186x
Buffer<R16f>.Load random: 677.930ms 0.999x
Buffer<RG16f>.Load uniform: 58.052ms 11.663x
Buffer<RG16f>.Load linear: 575.332ms 1.177x
Buffer<RG16f>.Load random: 675.883ms 1.002x
Buffer<RGBA16f>.Load uniform: 58.724ms 11.529x
Buffer<RGBA16f>.Load linear: 571.145ms 1.185x
Buffer<RGBA16f>.Load random: 676.597ms 1.001x
Buffer<R32f>.Load uniform: 57.779ms 11.718x
Buffer<R32f>.Load linear: 570.898ms 1.186x
Buffer<R32f>.Load random: 676.160ms 1.001x
Buffer<RG32f>.Load uniform: 57.770ms 11.720x
Buffer<RG32f>.Load linear: 571.226ms 1.185x
Buffer<RG32f>.Load random: 677.745ms 0.999x
Buffer<RGBA32f>.Load uniform: 58.759ms 11.522x
Buffer<RGBA32f>.Load linear: 571.372ms 1.185x
Buffer<RGBA32f>.Load random: 676.695ms 1.001x
ByteAddressBuffer.Load uniform: 98.943ms 6.843x
ByteAddressBuffer.Load linear: 254.749ms 2.658x
ByteAddressBuffer.Load random: 378.516ms 1.789x
ByteAddressBuffer.Load2 uniform: 68.931ms 9.822x
ByteAddressBuffer.Load2 linear: 456.746ms 1.482x
ByteAddressBuffer.Load2 random: 762.950ms 0.887x
ByteAddressBuffer.Load3 uniform: 77.403ms 8.747x
ByteAddressBuffer.Load3 linear: 839.961ms 0.806x
ByteAddressBuffer.Load3 random: 1706.975ms 0.397x
ByteAddressBuffer.Load4 uniform: 63.715ms 10.626x
ByteAddressBuffer.Load4 linear: 868.385ms 0.780x
ByteAddressBuffer.Load4 random: 1796.999ms 0.377x
ByteAddressBuffer.Load2 unaligned uniform: 78.052ms 8.674x
ByteAddressBuffer.Load2 unaligned linear: 487.732ms 1.388x
ByteAddressBuffer.Load2 unaligned random: 787.569ms 0.860x
ByteAddressBuffer.Load4 unaligned uniform: 79.889ms 8.475x
ByteAddressBuffer.Load4 unaligned linear: 995.681ms 0.680x
ByteAddressBuffer.Load4 unaligned random: 2015.342ms 0.336x
StructuredBuffer<float>.Load uniform: 100.075ms 6.765x
StructuredBuffer<float>.Load linear: 251.827ms 2.689x
StructuredBuffer<float>.Load random: 366.612ms 1.847x
StructuredBuffer<float2>.Load uniform: 69.021ms 9.809x
StructuredBuffer<float2>.Load linear: 447.962ms 1.511x
StructuredBuffer<float2>.Load random: 741.070ms 0.914x
StructuredBuffer<float4>.Load uniform: 62.209ms 10.883x
StructuredBuffer<float4>.Load linear: 868.643ms 0.779x
StructuredBuffer<float4>.Load random: 1798.563ms 0.376x
cbuffer{float4} load uniform: 63.908ms 10.594x
cbuffer{float4} load linear: 859.170ms 0.788x
cbuffer{float4} load random: 1815.643ms 0.373x
Texture2D<R8>.Load uniform: 57.693ms 11.735x
Texture2D<R8>.Load linear: 229.955ms 2.944x
Texture2D<R8>.Load random: 230.291ms 2.940x
Texture2D<RG8>.Load uniform: 57.835ms 11.706x
Texture2D<RG8>.Load linear: 230.142ms 2.942x
Texture2D<RG8>.Load random: 230.195ms 2.941x
Texture2D<RGBA8>.Load uniform: 58.916ms 11.492x
Texture2D<RGBA8>.Load linear: 230.623ms 2.936x
Texture2D<RGBA8>.Load random: 230.788ms 2.934x
Texture2D<R16F>.Load uniform: 60.521ms 11.187x
Texture2D<R16F>.Load linear: 229.671ms 2.948x
Texture2D<R16F>.Load random: 229.764ms 2.947x
Texture2D<RG16F>.Load uniform: 57.673ms 11.739x
Texture2D<RG16F>.Load linear: 230.141ms 2.942x
Texture2D<RG16F>.Load random: 230.311ms 2.940x
Texture2D<RGBA16F>.Load uniform: 58.287ms 11.616x
Texture2D<RGBA16F>.Load linear: 230.076ms 2.943x
Texture2D<RGBA16F>.Load random: 459.294ms 1.474x
Texture2D<R32F>.Load uniform: 57.614ms 11.751x
Texture2D<R32F>.Load linear: 234.894ms 2.882x
Texture2D<R32F>.Load random: 229.711ms 2.947x
Texture2D<RG32F>.Load uniform: 57.674ms 11.739x
Texture2D<RG32F>.Load linear: 230.058ms 2.943x
Texture2D<RG32F>.Load random: 459.470ms 1.474x
Texture2D<RGBA32F>.Load uniform: 58.323ms 11.608x
Texture2D<RGBA32F>.Load linear: 573.734ms 1.180x
Texture2D<RGBA32F>.Load random: 459.263ms 1.474x
Texture2D<R8>.Sample(nearest) uniform: 229.959ms 2.944x
Texture2D<R8>.Sample(nearest) linear: 229.704ms 2.947x
Texture2D<R8>.Sample(nearest) random: 232.381ms 2.913x
Texture2D<RG8>.Sample(nearest) uniform: 230.455ms 2.938x
Texture2D<RG8>.Sample(nearest) linear: 230.214ms 2.941x
Texture2D<RG8>.Sample(nearest) random: 230.276ms 2.940x
Texture2D<RGBA8>.Sample(nearest) uniform: 231.480ms 2.925x
Texture2D<RGBA8>.Sample(nearest) linear: 231.062ms 2.930x
Texture2D<RGBA8>.Sample(nearest) random: 240.466ms 2.816x
Texture2D<R16F>.Sample(nearest) uniform: 229.897ms 2.945x
Texture2D<R16F>.Sample(nearest) linear: 231.593ms 2.923x
Texture2D<R16F>.Sample(nearest) random: 229.666ms 2.948x
Texture2D<RG16F>.Sample(nearest) uniform: 230.391ms 2.939x
Texture2D<RG16F>.Sample(nearest) linear: 230.027ms 2.943x
Texture2D<RG16F>.Sample(nearest) random: 230.098ms 2.942x
Texture2D<RGBA16F>.Sample(nearest) uniform: 230.577ms 2.936x
Texture2D<RGBA16F>.Sample(nearest) linear: 230.086ms 2.943x
Texture2D<RGBA16F>.Sample(nearest) random: 459.828ms 1.472x
Texture2D<R32F>.Sample(nearest) uniform: 229.827ms 2.946x
Texture2D<R32F>.Sample(nearest) linear: 231.692ms 2.922x
Texture2D<R32F>.Sample(nearest) random: 229.751ms 2.947x
Texture2D<RG32F>.Sample(nearest) uniform: 230.528ms 2.937x
Texture2D<RG32F>.Sample(nearest) linear: 230.021ms 2.943x
Texture2D<RG32F>.Sample(nearest) random: 460.311ms 1.471x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 230.903ms 2.932x
Texture2D<RGBA32F>.Sample(nearest) linear: 573.964ms 1.180x
Texture2D<RGBA32F>.Sample(nearest) random: 460.377ms 1.471x
Texture2D<R8>.Sample(bilinear) uniform: 229.860ms 2.945x
Texture2D<R8>.Sample(bilinear) linear: 229.663ms 2.948x
Texture2D<R8>.Sample(bilinear) random: 229.689ms 2.948x
Texture2D<RG8>.Sample(bilinear) uniform: 230.469ms 2.938x
Texture2D<RG8>.Sample(bilinear) linear: 230.000ms 2.944x
Texture2D<RG8>.Sample(bilinear) random: 230.095ms 2.942x
Texture2D<RGBA8>.Sample(bilinear) uniform: 230.668ms 2.935x
Texture2D<RGBA8>.Sample(bilinear) linear: 230.157ms 2.942x
Texture2D<RGBA8>.Sample(bilinear) random: 240.543ms 2.815x
Texture2D<R16F>.Sample(bilinear) uniform: 229.871ms 2.945x
Texture2D<R16F>.Sample(bilinear) linear: 229.663ms 2.948x
Texture2D<R16F>.Sample(bilinear) random: 229.777ms 2.947x
Texture2D<RG16F>.Sample(bilinear) uniform: 230.454ms 2.938x
Texture2D<RG16F>.Sample(bilinear) linear: 230.009ms 2.944x
Texture2D<RG16F>.Sample(bilinear) random: 234.252ms 2.890x
Texture2D<RGBA16F>.Sample(bilinear) uniform: 230.580ms 2.936x
Texture2D<RGBA16F>.Sample(bilinear) linear: 344.679ms 1.964x
Texture2D<RGBA16F>.Sample(bilinear) random: 460.104ms 1.471x
Texture2D<R32F>.Sample(bilinear) uniform: 230.189ms 2.941x
Texture2D<R32F>.Sample(bilinear) linear: 229.679ms 2.948x
Texture2D<R32F>.Sample(bilinear) random: 229.726ms 2.947x
Texture2D<RG32F>.Sample(bilinear) uniform: 460.443ms 1.470x
Texture2D<RG32F>.Sample(bilinear) linear: 459.809ms 1.472x
Texture2D<RG32F>.Sample(bilinear) random: 689.533ms 0.982x
Texture2D<RGBA32F>.Sample(bilinear) uniform: 919.711ms 0.736x
Texture2D<RGBA32F>.Sample(bilinear) linear: 918.780ms 0.737x
Texture2D<RGBA32F>.Sample(bilinear) random: 919.250ms 0.737x
```

**Intel Gen11** results (ratios) of most common load/sample operations look similar to Gen 9, except raw loads are no longer 5x faster, instead they are 2.5x faster. The uniform address speedup seems to be around 2x higher. Maybe SIMD16 mode used instead of SIMD8?

**Sampler ratios (NEW!):** New tests for sampler ratios show that Gen11 has half rate bilinear RG32F and quarter rate bilinear RGBA32F. Nearest filtering is full rate, except for RGBA32F which is half rate (similar to RGBA32F texture loads). 

## Contact

Send private message to @SebAaltonen at Twitter. We can discuss via company emails later.

## License

PerfTest is released under the MIT license. See [LICENSE.md](LICENSE.md) for full text.
