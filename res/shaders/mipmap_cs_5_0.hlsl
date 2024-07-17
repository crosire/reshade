RWTexture2DArray<float4> mips[7] : register(u0);

cbuffer cb0 : register(b0)
{
	uint num_mips;
}

// Inspired by algorithm described in https://gpuopen.com/manuals/fidelityfx_sdk/fidelityfx_sdk-page_techniques_single-pass-downsampler/#algorithm-structure

float4 reduce(float4 v0, float4 v1, float4 v2, float4 v3)
{
	return (v0 + v1 + v2 + v3) * 0.25;
}
float4 load_and_reduce(uint2 location, uint slice)
{
	float4 v0 = mips[0][uint3(location + uint2(0, 0), slice)];
	float4 v1 = mips[0][uint3(location + uint2(1, 0), slice)];
	float4 v2 = mips[0][uint3(location + uint2(0, 1), slice)];
	float4 v3 = mips[0][uint3(location + uint2(1, 1), slice)];
	return reduce(v0, v1, v2, v3);
}

void store_mip(int mip, uint2 location, uint slice, float4 v)
{
	mips[mip][uint3(location, slice)] = v;
}

uint2 morton_to_xy(uint index)
{
	uint2 xy = uint2(index, index >> 1);
	xy &= 0x55555555;
	xy = (xy ^ (xy >> 1)) & 0x33333333;
	xy = (xy ^ (xy >> 2)) & 0x0F0F0F0F;
	xy = (xy ^ (xy >> 4)) & 0x00FF00FF;
	xy = (xy ^ (xy >> 8)) & 0x0000FFFF;
	return xy;
}

groupshared float4 intermediate[16 * 16];

void downsample_mip_n(const uint mip, uint2 base_location, uint thread_index, uint slice)
{
	GroupMemoryBarrierWithGroupSync();

	const uint group = (1 << ((mip - 2) * 2));

	[branch]
	if ((thread_index % group) == 0)
	{
		float4 v = reduce(
            intermediate[thread_index + group * 0 / 4],
            intermediate[thread_index + group * 1 / 4],
            intermediate[thread_index + group * 2 / 4],
            intermediate[thread_index + group * 3 / 4]);

		intermediate[thread_index] = v;

		store_mip(mip, base_location / (1 << mip), slice, v);
	}
}

void downsample_mip_1_2(uint2 base_location, uint thread_index, uint slice)
{
	// Each thread group downsamples a 64x64 tile, with each thread starting by loading 4x4 pixels
	float4 v[4];
	v[0] = load_and_reduce(base_location + uint2(0, 0), slice);
	v[1] = load_and_reduce(base_location + uint2(2, 0), slice);
	v[2] = load_and_reduce(base_location + uint2(0, 2), slice);
	v[3] = load_and_reduce(base_location + uint2(2, 2), slice);

	// Downsample to 32x32 (each thread stores 2x2 pixels)
	base_location /= 2;
	store_mip(1, base_location + uint2(0, 0), slice, v[0]);
	store_mip(1, base_location + uint2(1, 0), slice, v[1]);
	store_mip(1, base_location + uint2(0, 1), slice, v[2]);
	store_mip(1, base_location + uint2(1, 1), slice, v[3]);

	if (num_mips <= 2)
		return;

	v[0] = reduce(v[0], v[1], v[2], v[3]);

	intermediate[thread_index] = v[0];

	// Downsample to 16x16 (each thread stores 1x1 pixels)
	base_location /= 2;
	store_mip(2, base_location, slice, v[0]);
}
void downsample_mip_3_6(uint2 base_location, uint thread_index, uint slice)
{
	if (num_mips <= 3)
		return;
	// Downsample to 8x8 (every 4th thread stores a pixel)
	downsample_mip_n(3, base_location, thread_index, slice);

	if (num_mips <= 4)
		return;
	// Downsample to 4x4 (every 16th thread stores a pixel)
	downsample_mip_n(4, base_location, thread_index, slice);

	if (num_mips <= 5)
		return;
	// Downsample to 2x2 (every 64th thread stores a pixel)
	downsample_mip_n(5, base_location, thread_index, slice);

	if (num_mips <= 6)
		return;
	// Downsample to 1x1 (every 256th thread stores a pixel)
	downsample_mip_n(6, base_location, thread_index, slice);
}

[RootSignature(
	"RootConstants(num32BitConstants=1, b0),"
	"DescriptorTable(UAV(u0, numDescriptors = 7, flags = DATA_VOLATILE))")]
[numthreads(16 * 16, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint thread_index : SV_GroupIndex)
{
	uint2 base_location = group_id.xy * 64 + morton_to_xy(thread_index) * (64 / 16); // Each 16x16 thread group processes 64x64 pixels

	downsample_mip_1_2(base_location, thread_index, group_id.z);
	downsample_mip_3_6(base_location, thread_index, group_id.z);
}
