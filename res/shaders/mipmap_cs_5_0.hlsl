Texture2DArray<float4> t0 : register(t0);
RWTexture2DArray<float4> dest : register(u0);

cbuffer cb0 : register(b0)
{
	int level;
}

float4 reduce(int3 location)
{
	float4 v0 = t0.Load(int4(location, level - 1), int2(0, 0));
	float4 v1 = t0.Load(int4(location, level - 1), int2(0, 1));
	float4 v2 = t0.Load(int4(location, level - 1), int2(1, 0));
	float4 v3 = t0.Load(int4(location, level - 1), int2(1, 1));
	return (v0 + v1 + v2 + v3) * 0.25;
}

[RootSignature(
	"RootConstants(num32BitConstants=1, b0),"
	"DescriptorTable(SRV(t0, numDescriptors = 1, flags = DATA_VOLATILE)),"
	"DescriptorTable(UAV(u0, numDescriptors = 1))")]
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
	// Sample input texture and write result to the destination mipmap level
	dest[tid] = reduce(int3(tid.xy * 2, tid.z));
}
