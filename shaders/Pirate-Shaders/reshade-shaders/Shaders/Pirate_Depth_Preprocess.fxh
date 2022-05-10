//===================================================================================================================
#if DEPTH_USE_RESHADE_SETTINGS
#undef PixelSize
#include "ReShade.fxh"
#define PixelSize  	float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#endif
//===================================================================================================================
#if (DEPTH_USE_RESHADE_SETTINGS == 0)
uniform bool DEPTH_INVERT <
	ui_label = "Depth - Invert";
	> = false;
uniform bool DEPTH_INVERT_Y <
	ui_label = "Depth - Invert Up/down";
	ui_tooltip = "Inverts Y, happens on Unity games.";
	> = false;
uniform bool DEPTH_USE_FARPLANE <
	ui_label = "Depth - Use Farplane / Logarithm";
	> = false;
uniform float DEPTH_FARPLANE <
	ui_label = "Depth - Farplane";
	ui_tooltip = "Controls the depth curve. 1.0 is the same as turning farplane off. Some games like GTAV use numbers under 1.0.";
	> = 200.0;
#endif
uniform bool DEPTH_DEBUG <
	ui_label = "Depth - Debug";
	ui_tooltip = "Shows depth, you want close objects to be black and far objects to be white for things to work properly.";
	> = false;
uniform int FOV <
	ui_label = "FoV";
	ui_type = "drag";
	ui_min = 10; ui_max = 90;
	> = 75;
uniform bool FOV_DEBUG <
	ui_label = "FoV - Turn on to configure";
	ui_tooltip = "Change FoV until flat surfaces like walls and ground show an uniform shade.";
	> = false;
//===================================================================================================================
texture2D	texDepth : DEPTH;
sampler2D	SamplerDepth {Texture = texDepth; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

texture2D	TexNormalDepth {Width = BUFFER_WIDTH * DEPTH_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DEPTH_TEXTURE_QUALITY; Format = RGBA16; MipLevels = DEPTH_AO_MIPLEVELS;};
sampler2D	SamplerND {Texture = TexNormalDepth; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
//===================================================================================================================
#define pi 3.14159265359f
#define threepitwo 4.71238898038f
#define depthlog depth / 2.3

float GetDepth(float2 coords)
{
	#if DEPTH_USE_RESHADE_SETTINGS
	return saturate(ReShade::GetLinearizedDepth(coords));
	#else
	if (DEPTH_INVERT_Y) coords.y = 1.0 - coords.y;
	float depth = tex2D(SamplerDepth, coords.xy).x;
	if (DEPTH_USE_FARPLANE)	depth /= DEPTH_FARPLANE - depth * DEPTH_FARPLANE + depth;
	if (DEPTH_INVERT) depth = 1.0 - depth;
	return saturate(depth);
	#endif
}

float3 EyeVector(float3 vec)
{
	vec.xy = vec.xy * 2.0 - 1.0;
	vec.x -= vec.x * (1.0 - vec.z) * sin(radians(FOV));
	vec.y -= vec.y * (1.0 - vec.z) * sin(radians(FOV * (PixelSize.y / PixelSize.x)));
	return vec;
}

float4 GetNormalDepth(float2 coords)
{
	const float2 offsety = float2(0.0, PixelSize.y);
  	const float2 offsetx = float2(PixelSize.x, 0.0);
	
	float pointdepth = GetDepth(coords);
	#define NORMAL_MODE 2
	
	#if (NORMAL_MODE == 0) 
	float3 p = EyeVector(float3(coords, pointdepth));
	float3 py1 = EyeVector(float3(coords + offsety, GetDepth(coords + offsety))) - p;
  	float3 px1 = EyeVector(float3(coords + offsetx, GetDepth(coords + offsetx))) - p;
	#elif (NORMAL_MODE == 1) 
	float3 py1 = EyeVector(float3(coords + offsety, GetDepth(coords + offsety))) - EyeVector(float3(coords - offsety, GetDepth(coords - offsety)));
  	float3 px1 = EyeVector(float3(coords + offsetx, GetDepth(coords + offsetx))) - EyeVector(float3(coords - offsetx, GetDepth(coords - offsetx)));
	#elif (NORMAL_MODE == 2)
	float3 p = EyeVector(float3(coords, pointdepth));
	float3 py1 = EyeVector(float3(coords + offsety, GetDepth(coords + offsety))) - p;
	float3 py2 = p - EyeVector(float3(coords - offsety, GetDepth(coords - offsety)));
  	float3 px1 = EyeVector(float3(coords + offsetx, GetDepth(coords + offsetx))) - p;
	float3 px2 = p - EyeVector(float3(coords - offsetx, GetDepth(coords - offsetx)));
	py1 = lerp(py1, py2, abs(py1.z) > abs(py2.z));
	px1 = lerp(px1, px2, abs(px1.z) > abs(px2.z));
	#endif
  
	float3 normal = cross(py1, px1);
	normal = (normalize(normal) + 1.0) * 0.5;
  
  	return float4(normal, pointdepth);
}
//===================================================================================================================
float4 PS_DepthPrePass(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GetNormalDepth(texcoord);
}
//===================================================================================================================
technique Pirate_DepthPreProcess
{
	pass DepthPre
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_DepthPrePass;
		RenderTarget = TexNormalDepth;
	}
}