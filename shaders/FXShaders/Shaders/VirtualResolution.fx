#include "FXShaders/Common.fxh"

#ifndef VIRTUAL_RESOLUTION_UPFILTER
#define VIRTUAL_RESOLUTION_UPFILTER POINT
#endif

#ifndef VIRTUAL_RESOLUTION_DOWNFILTER
#define VIRTUAL_RESOLUTION_DOWNFILTER LINEAR
#endif

#ifndef VIRTUAL_RESOLUTION_DYNAMIC
#define VIRTUAL_RESOLUTION_DYNAMIC 1
#endif

#ifndef VIRTUAL_RESOLUTION_WIDTH
#define VIRTUAL_RESOLUTION_WIDTH BUFFER_WIDTH
#endif

#ifndef VIRTUAL_RESOLUTION_HEIGHT
#define VIRTUAL_RESOLUTION_HEIGHT BUFFER_HEIGHT
#endif

namespace FXShaders
{

uniform uint iScaleMode <
	ui_label = "Scale Mode";
	ui_type = "combo";
	ui_items = "None\0Crop\0Stretch\0";
> = 0;

#if VIRTUAL_RESOLUTION_DYNAMIC

uniform float fResolutionX <
	ui_label = "Virtual Resolution Width";
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_step = 1.0;
> = BUFFER_WIDTH;

uniform float fResolutionY <
	ui_label = "Virtual Resolution Height";
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1.0;
> = BUFFER_HEIGHT;

#define Resolution float2(fResolutionX, fResolutionY)

#else

static const float2 Resolution = float2(
	VIRTUAL_RESOLUTION_WIDTH,
	VIRTUAL_RESOLUTION_HEIGHT);

#endif

static const int ScaleMode_None = 0;
static const int ScaleMode_Crop = 1;
static const int ScaleMode_Stretch = 2;

texture BackBufferTex : COLOR;

sampler BackBufferDownSample {
	Texture = BackBufferTex;
	MinFilter = VIRTUAL_RESOLUTION_DOWNFILTER;
	MagFilter = VIRTUAL_RESOLUTION_DOWNFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#if VIRTUAL_RESOLUTION_DYNAMIC

sampler BackBufferUpSample {
	Texture = BackBufferTex;
	MinFilter = VIRTUAL_RESOLUTION_UPFILTER;
	MagFilter = VIRTUAL_RESOLUTION_UPFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#else

texture DownSampledTex {
	Width = VIRTUAL_RESOLUTION_WIDTH;
	Height = VIRTUAL_RESOLUTION_HEIGHT;
};
sampler DownSampled {
	Texture = DownSampledTex;
	MinFilter = VIRTUAL_RESOLUTION_UPFILTER;
	MagFilter = VIRTUAL_RESOLUTION_UPFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#endif

bool Crop(float2 uv)
{
	float ar_real = GetAspectRatio();
	float ar_virtual = Resolution.x / Resolution.y;

	//crop horizontally or vertically? (true = x, false = y)
	bool x_or_y = ar_real > ar_virtual;
	
	float ar_result = x_or_y
		? ar_virtual / ar_real
		: ar_real / ar_virtual;
	
	float mask = (1.0 - (ar_result)) * 0.5;
	
	return x_or_y
		? uv.x > mask && uv.x < 1.0 - mask
		: uv.y > mask && uv.y < 1.0 - mask;
}

float2 Stretch(float2 uv)
{
	float ar_real = GetAspectRatio();
	float ar_virtual = Resolution.x / Resolution.y;
	
	bool x_or_y = ar_real > ar_virtual;

	float ar_result = x_or_y
		? (ar_virtual / ar_real)
		: (ar_real / ar_virtual);
	
	float scale = 1.0 / ar_result;

	return x_or_y
		? ScaleCoord(uv, float2(scale, 1.0))
		: ScaleCoord(uv, float2(1.0, scale));
}

float4 DownSamplePS(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	#if VIRTUAL_RESOLUTION_DYNAMIC
		float2 scale = Resolution * GetPixelSize();
		float4 color = tex2D(BackBufferDownSample, ScaleCoord(uv, 1.0 / scale));
		
		return color;
	#else
		return tex2D(BackBufferDownSample, uv);
	#endif
}

float4 UpSamplePS(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float crop = iScaleMode == ScaleMode_Crop ? Crop(uv) : 1.0;
	
	uv = iScaleMode == ScaleMode_Stretch ? Stretch(uv) : uv;

	#if VIRTUAL_RESOLUTION_DYNAMIC
		float2 scale = Resolution * GetPixelSize();
		uv = ScaleCoord(uv, scale);

		float4 color = tex2D(BackBufferUpSample, uv);
	#else
		float4 color = tex2D(DownSampled, uv);
	#endif
	
	color *= crop;

	return color;
}

technique VirtualResolution
{
	pass DownSample
	{
		VertexShader = ScreenVS;
		PixelShader = DownSamplePS;

		#if !VIRTUAL_RESOLUTION_DYNAMIC
			RenderTarget = DownSampledTex;
		#endif
	}
	pass UpSample
	{
		VertexShader = ScreenVS;
		PixelShader = UpSamplePS;
	}
}

} // Namespace.
