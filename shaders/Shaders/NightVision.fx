#include "ReShade.fxh"

uniform float iGlobalTime < source = "timer"; >;

float hash(in float n) { return frac(sin(n)*43758.5453123); }

float mod(float x, float y)
{
	return x - y * floor (x/y);
}

float3 PS_Nightvision(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{	
	float2 p = uv;
	
	float2 u = p * 2. - 1.;
	float2 n = u * float2(ReShade::ScreenSize.x / ReShade::ScreenSize.y, 1.0);
	float3 c = tex2D(ReShade::BackBuffer, uv).xyz;

	// flicker, grain, vignette, fade in
	c += sin(hash(iGlobalTime*0.001)) * 0.01;
	c += hash((hash(n.x) + n.y) * iGlobalTime*0.001) * 0.5;
	c *= smoothstep(length(n * n * n * float2(0.0, 0.0)), 1.0, 0.4);
    c *= smoothstep(0.001, 3.5, iGlobalTime*0.001) * 1.5;
	
	c = dot(c, float3(0.2126, 0.7152, 0.0722)) 
	  * float3(0.2, 1.5 - hash(iGlobalTime*0.001) * 0.1,0.4);
	
	return c;
}

technique Nightvision {
	pass Nightvision {
		VertexShader=PostProcessVS;
		PixelShader=PS_Nightvision;
	}
}
