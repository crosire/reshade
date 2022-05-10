#pragma once


#define RES_WIDTH BUFFER_WIDTH
#define RES_HEIGHT BUFFER_HEIGHT

#define BUFFER_SIZE int2(RES_WIDTH,RES_HEIGHT)
#define BUFFER_SIZE3 int3(RES_WIDTH,RES_HEIGHT,RESHADE_DEPTH_LINEARIZATION_FAR_PLANE)
#define min3(v) min(min(v.x,v.y),v.z)
#define max3(v) max(max(v.x,v.y),v.z)
#define mean3(v) ((v.x+v.y+v.z)/3)
#define pow2(x) (x*x)
#define diff3(v1,v2) any(v1.xyz-v2.xyz)

#define getTex2D(s,c) tex2Dlod(s,float4(c,0,0))
#define getTex2Dlod(s,c,l) tex2Dlod(s,float4(c,0,l))
#define getColor(c) tex2D(ReShade::BackBuffer,c)
#define getColorLod(c,l) tex2D(colorSampler,c,l)
#define getDepth(c) ReShade::GetLinearizedDepth(c)

namespace DH
{

//////// COLOR SPACE
	float RGBCVtoHUE(in float3 RGB, in float C, in float V) {
	    float3 Delta = (V - RGB) / C;
	    Delta.rgb -= Delta.brg;
	    Delta.rgb += float3(2,4,6);
	    Delta.brg = step(V, RGB) * Delta.brg;
	    float H;
	    H = max(Delta.r, max(Delta.g, Delta.b));
	    return frac(H / 6);
	}

	float3 RGBtoHSL(in float3 RGB) {
	    float3 HSL = 0;
	    float U, V;
	    U = -min(RGB.r, min(RGB.g, RGB.b));
	    V = max(RGB.r, max(RGB.g, RGB.b));
	    HSL.z = ((V - U) * 0.5);
	    float C = V + U;
	    if (C != 0)
	    {
	      	HSL.x = RGBCVtoHUE(RGB, C, V);
	      	HSL.y = C / (1 - abs(2 * HSL.z - 1));
	    }
	    return HSL;
	}
	  
	float3 HUEtoRGB(in float H) 
	{
		float R = abs(H * 6 - 3) - 1;
		float G = 2 - abs(H * 6 - 2);
		float B = 2 - abs(H * 6 - 4);
		return saturate(float3(R,G,B));
	}
	  
	float3 HSLtoRGB(in float3 HSL)
	{
	    float3 RGB = HUEtoRGB(HSL.x);
	    float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
	    return (RGB - 0.5) * C + HSL.z;
	}


//////// DISTANCES

	float distance2(float2 p1,float2 p2) {
		float2 diff = p1-p2;
		return dot(diff,diff);
	}
	
	float distance2(float3 p1,float3 p2) {
		float3 diff = p1-p2;
		return dot(diff,diff);
	}

//////// COORDS

	float2 getPixelSize() {
		return 1.0/BUFFER_SIZE;
	} 

	float3 getWorldPosition(float2 coords,float depth,int3 bufferSize) {
		float3 result = float3((coords-0.5)*depth,depth);
		result *= bufferSize;
		return result;
	}
	
	float3 getWorldPosition(float3 coords,int3 bufferSize) {
		return getWorldPosition(coords.xy,coords.z,bufferSize);
	}
	
	float3 getWorldPosition(float2 coords,float depth) {
		return getWorldPosition(coords,depth,int3(BUFFER_WIDTH,BUFFER_HEIGHT,RESHADE_DEPTH_LINEARIZATION_FAR_PLANE));
	}
	
	float3 getWorldPosition(float3 coords) {
		return getWorldPosition(coords.xy,coords.z,int3(BUFFER_WIDTH,BUFFER_HEIGHT,RESHADE_DEPTH_LINEARIZATION_FAR_PLANE));
	}
	
	float3 getWorldPosition(float2 coords,int3 bufferSize) {
		float depth = getDepth(coords);
		return getWorldPosition(coords,depth,bufferSize);
	}
	
	float3 getWorldPosition(float2 coords) {
		float depth = getDepth(coords);
		return getWorldPosition(coords,depth);
	}
	
	float2 getScreenPosition(float3 wp,int3 bufferSize) {
		float3 result = wp/bufferSize;
		result /= result.z;
		return result.xy+0.5;
	}

//////// NORMALS
	float3 computeNormal(float2 coords, int3 samplerSize)
	{
		float3 offset = float3(ReShade::PixelSize.xy, 0.0)/10;
		
		float3 posCenter = getWorldPosition(coords,samplerSize);
		float3 posNorth  = getWorldPosition(coords - offset.zy,samplerSize);
		float3 posEast   = getWorldPosition(coords + offset.xz,samplerSize);
		float deformRatio = distance2(coords,float2(0.5,0.5));
		return float3((coords-0.5)/6,0.5)+normalize(cross(posCenter - posNorth, posCenter - posEast));
		//return float3(0,0,0.5)+normalize(cross(posCenter - posNorth, posCenter - posEast));
	}

	void saveNormal(out float4 outNormal,float3 normal) {
		outNormal = float4(normal/2+0.5,1);
	}

	float3 loadNormal(sampler s,float2 coords) {
		float4 normalColor = tex2Dlod(s,float4(coords,0,0));
		return (normalColor.xyz-0.5)*2;
	}
}
