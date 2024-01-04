[[vk::binding(0)]]
RaytracingAccelerationStructure as : register(t0);
[[vk::binding(1)]]
RWTexture2D<float4> image : register(u0);

struct Payload
{
	float3 color;
};

[shader("raygeneration")]
void main_raygen()
{
	const float2 uv = (float2(DispatchRaysIndex().xy) + float2(0.5, 0.5)) / float2(DispatchRaysDimensions().xy);

	float3 origin = float3(0.0, 0.0, 0.0);
	float3 direction = normalize(float3(uv * 2.0 - 1.0, 1.0));

	RayDesc ray = { origin, 0.1, direction, 1000.0 };
    Payload payload = { float3(0.0, 0.0, 0.0) };

    TraceRay(as, 0, 0xFF, 0, 0, 0, ray, payload);

	image[DispatchRaysIndex().xy] = float4(payload.color, 0.0);
}

[shader("miss")]
void main_miss(inout Payload payload)
{
	payload.color = float3(0.0, 0.0, 0.0);
}

[shader("closesthit")]
void main_closest_hit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	const float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	payload.color = barycentrics;
}
