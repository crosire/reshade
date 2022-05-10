#include "ReShade.fxh"


/*
CMYK Halftone Dot Shader

Adapted from Stefan Gustavson's GLSL shader demo for WebGL:
http://webstaff.itn.liu.se/~stegu/OpenGLinsights/shadertutorial.html

Ported to Cg shader language by hunterk

This shader is licensed in the public domain, as per S. Gustavson's original license.
Note: the MIT-licensed noise functions have been purposely removed.
*/

uniform float frequency <
	ui_type = "drag";
	ui_min = 50.0;
	ui_max = 1000.0;
	ui_step = 50.0;
	ui_label = "HalfTone Dot Density [CMYK HalfTone]";
> = 550.0;

sampler2D SamplerColorPoint
{
	Texture = ReShade::BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

float4 PS_cymk_halftone_dot(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    // Distance to nearest point in a grid of
	// (frequency x frequency) points over the unit square
	float2x2 rotation_matrix = float2x2(0.707, 0.707, -0.707, 0.707);
	float2 st2 = mul(rotation_matrix , texcoord);
    float2 nearest = 2.0 * frac(frequency * st2) - 1.0;
    float dist = length(nearest);
    float3 texcolor = tex2D(SamplerColorPoint, texcoord).rgb; // Unrotated coords
    float3 black = float3(0,0,0);
	
	// Perform a rough RGB-to-CMYK conversion
    float4 cmyk;
    cmyk.xyz = 1.0 - texcolor;
    cmyk.w = min(cmyk.x, min(cmyk.y, cmyk.z)); // Create K
	
	float2x2 k_matrix = float2x2(0.707, 0.707, -0.707, 0.707);
	float2 Kst = frequency * (0.48 * (ReShade::ScreenSize / ReShade::ScreenSize)) * mul(k_matrix , texcoord);
    float2 Kuv = 2.0 * frac(Kst) - 1.0;
    float k = step(0.0, sqrt(cmyk.w) - length(Kuv));
	float2x2 c_matrix = float2x2(0.966, 0.259, -0.259, 0.966);
    float2 Cst = frequency * (0.48 * (ReShade::ScreenSize / ReShade::ScreenSize)) * mul(c_matrix , texcoord);
    float2 Cuv = 2.0 * frac(Cst) - 1.0;
    float c = step(0.0, sqrt(cmyk.x) - length(Cuv));
	float2x2 m_matrix = float2x2(0.966, -0.259, 0.259, 0.966);
    float2 Mst = frequency * (0.48 * (ReShade::ScreenSize / ReShade::ScreenSize)) * mul(m_matrix , texcoord);
    float2 Muv = 2.0 * frac(Mst) - 1.0;
    float m = step(0.0, sqrt(cmyk.y) - length(Muv));
    float2 Yst = frequency * (0.48 * (ReShade::ScreenSize / ReShade::ScreenSize)) * texcoord; // 0 deg
    float2 Yuv = 2.0 * frac(Yst) - 1.0;
    float y = step(0.0, sqrt(cmyk.z) - length(Yuv));
	
	float3 rgbscreen = 1.0 - 0.9 * float3(c,m,y);
	rgbscreen = lerp(rgbscreen, black, 0.85 * k);
	
	float afwidth = 2.0 * frequency * max(length(ReShade::ScreenSize / (ReShade::ScreenSize * ReShade::ScreenSize)), length(ReShade::ScreenSize / (ReShade::ScreenSize * ReShade::ScreenSize)));
	float blend = smoothstep(0.0, 1.0, afwidth);
	
    float4 color = float4(lerp(rgbscreen , texcolor, blend), 1.0);
	color = (max(texcolor.r, max(texcolor.g, texcolor.b)) < 0.01) ? float4(0,0,0,0) : color; // make blacks actually black

	return color;
}

technique CMYK_Halftone
{
	pass CMYK
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_cymk_halftone_dot;
	}
}