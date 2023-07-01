SamplerState s0 : register(s0);
Texture2D<float4> t0 : register(t0);
RWTexture2D<float4> dest : register(u0);

cbuffer cb0 : register(b0)
{
	float2 texel; // 1.0 / dimension
}

[RootSignature(
	"RootConstants(num32BitConstants=2, b0),"
	"DescriptorTable(SRV(t0, numDescriptors = 1)),"
	"DescriptorTable(UAV(u0, numDescriptors = 1)),"
	"DescriptorTable(Sampler(s0, numDescriptors = 1))")]

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
	float2 uv = texel * (tid.xy + 0.5);

	// Sample input texture and write result to the destination mipmap level
	dest[tid.xy] = t0.SampleLevel(s0, uv, 0);
}
