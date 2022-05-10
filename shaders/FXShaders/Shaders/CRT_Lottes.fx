/*
	CRT by Timothy Lottes

	Ported by Lucas Melo (luluco250)

	Refer to the original license in CRT_Lottes.fxh
*/

#include "ReShade.fxh"

//Macros///////////////////////////////////////////////////////////////////////////////

#ifndef CRT_LOTTES_TONE
#define CRT_LOTTES_TONE 1
#endif

#ifndef CRT_LOTTES_CONTRAST
#define CRT_LOTTES_CONTRAST 1
#endif

#ifndef CRT_LOTTES_SATURATION
#define CRT_LOTTES_SATURATION 1
#endif

#ifndef CRT_LOTTES_WARP
#define CRT_LOTTES_WARP 1
#endif

#define NONE 0
#define GRILLE 1
#define GRILLE_LITE 2
#define SHADOW 3

#ifndef CRT_LOTTES_MASK
#define CRT_LOTTES_MASK GRILLE_LITE
#endif

#ifndef CRT_LOTTES_2TAP
#define CRT_LOTTES_2TAP 0
#endif

#ifndef CRT_LOTTES_CUSTOM_RESOLUTION
#define CRT_LOTTES_CUSTOM_RESOLUTION 0
#endif

#ifndef CRT_LOTTES_DOWNSCALE
#define CRT_LOTTES_DOWNSCALE 1
#endif

#ifndef CRT_LOTTES_SMOOTH_DOWNSCALE
#define CRT_LOTTES_SMOOTH_DOWNSCALE 1
#endif

//Uniforms/////////////////////////////////////////////////////////////////////////////

uniform float2 f2Warp <
	ui_label = "Scanline Warp";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 64.0;
	ui_step  = 0.01;
> = float2(48.0, 24.0);

uniform float fThin <
	ui_label = "Scanline Thinness";
	ui_type  = "drag";
	ui_min   = 0.5;
	ui_max   = 1.0;
	ui_step  = 0.001;
> = 0.7;

uniform float fBlur <
	ui_label = "Horizontal Scan Blur";
	ui_type  = "drag";
	ui_min   = 1.0;
	ui_max   = 3.0;
	ui_step  = 0.001;
> = 2.5;

uniform float fMask <
	ui_label = "Shadow Mask";
	ui_type  = "drag";
	ui_min   = 0.25;
	ui_max   = 1.0;
	ui_step  = 0.001;
> = 0.5;

#if CRT_LOTTES_CUSTOM_RESOLUTION
uniform float fWidth <
	ui_label = "Width";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = float(BUFFER_WIDTH);
	ui_step  = 1.0;
> = 640.0;

uniform float fHeight <
	ui_label = "Height";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = float(BUFFER_HEIGHT);
	ui_step  = 1.0;
> = 480.0;

#define f2Resolution float2(fWidth, fHeight)
#define f2Downscale (ReShade::ScreenSize / f2Resolution)
#else

uniform float fDownscale <
	ui_label = "Resolution Downscale";
	ui_type  = "drag";
	ui_min   = 1.0;
	ui_max   = 256.0;
	ui_step  = 0.25;
> = 2.0;

#define fWidth (float(BUFFER_WIDTH) / fDownscale)
#define fHeight (float(BUFFER_HEIGHT) / fDownscale)
#define f2Resolution (ReShade::ScreenSize / fDownscale)
#define f2Downscale fDownscale
#endif

//Setup Header/////////////////////////////////////////////////////////////////////////

#define CRTS_DEBUG 0
#define CRTS_GPU 1
#define CRTS_HLSL 1

#define CRTS_TONE CRT_LOTTES_TONE
#define CRTS_CONTRAST CRT_LOTTES_CONTRAST
#define CRTS_SATURATION CRT_LOTTES_SATURATION
#define CRTS_WARP CRT_LOTTES_WARP

#if CRT_LOTTES_MASK == NONE
	#define CRTS_MASK_NONE 1
#elif CRT_LOTTES_MASK == GRILLE
	#define CRTS_MASK_GRILLE 1
#elif CRT_LOTTES_MASK == GRILLE_LITE
	#define CRTS_MASK_GRILLE_LITE 1
#elif CRT_LOTTES_MASK == SHADOW
	#define CRTS_MASK_SHADOW 1
#endif

#define CRTS_2_TAP CRT_LOTTES_2TAP

#if CRT_LOTTES_DOWNSCALE
sampler2D sBackBuffer_Scale {
	Texture     = ReShade::BackBufferTex;
	SRGBTexture = true;
	#if CRT_LOTTES_SMOOTH_DOWNSCALE
	MinFilter   = LINEAR;
	#else
	MinFilter   = POINT;
	#endif
	MagFilter   = POINT;
};
#endif

sampler2D sBackBuffer_CRT {
	Texture     = ReShade::BackBufferTex;
	SRGBTexture = true;
	MinFilter   = LINEAR;
	MagFilter   = LINEAR;
};

#define CrtsFetch(uv) tex2D(sBackBuffer_CRT, uv).rgb

#include "CRT_Lottes.fxh"

//Functions////////////////////////////////////////////////////////////////////////////

#if CRT_LOTTES_DOWNSCALE
float2 scale_uv(float2 uv, float2 scale, float2 center) {
	return (uv - center) * scale + center;
}

float2 scale_uv(float2 uv, float2 scale) {
	return scale_uv(uv, scale, 0.5);
}
#endif

//Shader///////////////////////////////////////////////////////////////////////////////

#if CRT_LOTTES_DOWNSCALE
void PS_Downscale(
	float4 position  : SV_POSITION,
	float2 uv        : TEXCOORD,
	out float4 color : SV_TARGET
) {
	color = tex2D(sBackBuffer_Scale, scale_uv(uv, f2Downscale));
}

void PS_Upscale(
	float4 position  : SV_POSITION,
	float2 uv        : TEXCOORD,
	out float4 color : SV_TARGET
) {
	color = tex2D(sBackBuffer_Scale, scale_uv(uv, 1.0 / f2Downscale));
}
#endif

void PS_CRT_Lottes(
	float4 position  : SV_POSITION,
	float2 uv        : TEXCOORD,
	out float4 color : SV_TARGET
) {
	float2 coord = uv * ReShade::ScreenSize;
	color.a = 1.0;
	color.rgb = CrtsFilter(
		coord,
		f2Resolution * ReShade::PixelSize,
		f2Resolution * 0.5,
		1.0 / f2Resolution,
		ReShade::PixelSize,
		ReShade::PixelSize * 2.0,
		fHeight,
		1.0 / f2Warp,
		fThin,
		-fBlur,
		fMask,
		CrtsTone(1.0, 0.0, fThin, fMask)
	);
}

//Technique////////////////////////////////////////////////////////////////////////////

technique CRT_Lottes {
	#if CRT_LOTTES_DOWNSCALE
	pass Downscale {
		VertexShader    = PostProcessVS;
		PixelShader     = PS_Downscale;
		SRGBWriteEnable = true;
	}
	pass Upscale {
		VertexShader    = PostProcessVS;
		PixelShader     = PS_Upscale;
		SRGBWriteEnable = true;
	}
	#endif
	pass CRT_Lottes {
		VertexShader    = PostProcessVS;
		PixelShader     = PS_CRT_Lottes;
		SRGBWriteEnable = true;
	}
}
