//#region Includes

#include "ReShade.fxh"

//#endregion

//#region Preprocessor

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

//#endregion

//#region Uniforms

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

#else

static const float fResolutionX = VIRTUAL_RESOLUTION_WIDTH;
static const float fResolutionY = VIRTUAL_RESOLUTION_HEIGHT;

#endif

#define f2Resolution float2(fResolutionX, fResolutionY)

//#endregion

//#region Textures

sampler sBackBuffer_Down {
	Texture = ReShade::BackBufferTex;
	MinFilter = VIRTUAL_RESOLUTION_DOWNFILTER;
	MagFilter = VIRTUAL_RESOLUTION_DOWNFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#if VIRTUAL_RESOLUTION_DYNAMIC

sampler sBackBuffer_Up {
	Texture = ReShade::BackBufferTex;
	MinFilter = VIRTUAL_RESOLUTION_UPFILTER;
	MagFilter = VIRTUAL_RESOLUTION_UPFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#else

texture tVirtualResolution_DownSampled {
	Width = VIRTUAL_RESOLUTION_WIDTH;
	Height = VIRTUAL_RESOLUTION_HEIGHT;
};
sampler sDownSampled {
	Texture = tVirtualResolution_DownSampled;
	MinFilter = VIRTUAL_RESOLUTION_UPFILTER;
	MagFilter = VIRTUAL_RESOLUTION_UPFILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

#endif

//#endregion

//#region Functions

float2 scaleUV(float2 uv, float2 scale) {
	return (uv - 0.5) * scale + 0.5;
}

float ruleOfThree(float a, float b, float c) {
	return (b * c) / a;
}

bool Crop(float2 uv) {
	static const float ar_real = ReShade::AspectRatio;
	float ar_virtual = fResolutionX / fResolutionY;

	//crop horizontally or vertically? (true = x, false = y)
	bool x_or_y = ar_real > ar_virtual;
	float ar_result = x_or_y ? (ar_virtual / ar_real) : 
							   (ar_real / ar_virtual);
	
	float mask = (1.0 - (ar_result)) * 0.5;
	return x_or_y ? (uv.x > mask && uv.x < 1.0 - mask) : 
					(uv.y > mask && uv.y < 1.0 - mask);
}

float2 Stretch(float2 uv) {
	static const float ar_real = ReShade::AspectRatio;
	float ar_virtual = fResolutionX / fResolutionY;
	
	bool x_or_y = ar_real > ar_virtual;
	float ar_result = x_or_y
		? (ar_virtual / ar_real)
		: (ar_real / ar_virtual);
	
	float scale = 1.0 / ar_result;
	return x_or_y
		? scaleUV(uv, float2(scale, 1.0))
		: scaleUV(uv, float2(1.0, scale));
}

//#endregion

//#region Shaders

float4 PS_DownSample(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	#if VIRTUAL_RESOLUTION_DYNAMIC

	float2 scale = f2Resolution * ReShade::PixelSize;
	float3 col = tex2D(sBackBuffer_Down, scaleUV(uv, 1.0 / scale)).rgb;
	return float4(col, 1.0);

	#else

	return tex2D(sBackBuffer_Down, uv);

	#endif
}

float4 PS_UpSample(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	#if VIRTUAL_RESOLUTION_DYNAMIC

	float2 scale = f2Resolution * ReShade::PixelSize;
	uv = scaleUV(uv, scale);
	uv = iScaleMode == 2 ? Stretch(uv) : uv;

	float3 col = tex2D(sBackBuffer_Up, uv).rgb;

	col *= iScaleMode == 1 ? Crop(uv) : 1.0;

	return float4(col, 1.0);

	#else

	uv = iScaleMode == 2 ? Stretch(uv) : uv;
	float crop = iScaleMode == 1 ? Crop(uv) : 1.0;
	return tex2D(sDownSampled, uv) * crop;

	#endif
}

//#endregion

//#region Technique

technique VirtualResolution {
	pass DownSample {
		VertexShader = PostProcessVS;
		PixelShader = PS_DownSample;

		#if !VIRTUAL_RESOLUTION_DYNAMIC
		RenderTarget = tVirtualResolution_DownSampled;
		#endif
	}
	pass UpSample {
		VertexShader = PostProcessVS;
		PixelShader = PS_UpSample;
	}
}

//#endregion