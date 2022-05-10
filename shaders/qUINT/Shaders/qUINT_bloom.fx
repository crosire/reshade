/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg

    Simple Bloom
    by Marty McFly / P.Gilcher
    part of qUINT shader library for ReShade 4

    Copyright (c) Pascal Gilcher / Marty McFly. All rights reserved.

=============================================================================*/

/*=============================================================================
	Preprocessor settings
=============================================================================*/

#ifndef SAMPLE_HQ
 #define SAMPLE_HQ 0
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform float BLOOM_INTENSITY <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 10.00;
	ui_label = "Bloom Intensity";
	ui_tooltip = "Scales bloom brightness.";
> = 1.2;

uniform float BLOOM_CURVE <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 10.00;
	ui_label = "Bloom Curve";
	ui_tooltip = "Higher values limit bloom to bright light sources only.";
> = 1.5;

uniform float BLOOM_SAT <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 5.00;
	ui_label = "Bloom Saturation";
	ui_tooltip = "Adjusts the color strength of the bloom effect";
> = 2.0;
/*
uniform float BLOOM_DIRT <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_label = "Lens Dirt Amount";
	ui_tooltip = "Applies a dirt mask on top of the original bloom.";
> = 0.0;
*/
uniform float BLOOM_LAYER_MULT_1 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 1 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.05;
uniform float BLOOM_LAYER_MULT_2 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 2 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.05;
uniform float BLOOM_LAYER_MULT_3 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 3 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.05;
uniform float BLOOM_LAYER_MULT_4 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 4 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.1;
uniform float BLOOM_LAYER_MULT_5 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 5 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.5;
uniform float BLOOM_LAYER_MULT_6 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 6 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.01;
uniform float BLOOM_LAYER_MULT_7 <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Layer 7 Intensity";
	ui_tooltip = "Intensity of this bloom layer. 1 is sharpest layer, 7 the most blurry.";
> = 0.01;
uniform float BLOOM_ADAPT_STRENGTH <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_label = "Bloom Scene Adaptation Sensitivity";
	ui_tooltip = "Amount of adaptation applied, 0 means same exposure for all scenes, 1 means complete autoexposure.";
> = 0.5;
uniform float BLOOM_ADAPT_EXPOSURE <
	ui_type = "drag";
	ui_min = -5.00; ui_max = 5.00;
	ui_label = "Bloom Scene Exposure Bias";
	ui_tooltip = "qUINT bloom employs eye adaptation to tune bloom intensity for scene differences.\nThis parameter adjusts the final scene exposure.";
> = 0.0;
uniform float BLOOM_ADAPT_SPEED <
	ui_type = "drag";
	ui_min = 0.50; ui_max = 10.00;
	ui_label = "Bloom Scene Adaptation Speed";
	ui_tooltip = "Eye adaptation data is created by exponential moving average with last frame data.\nThis parameter controls the adjustment speed.\nHigher parameters let the image adjust more quickly.";
> = 2.0;
uniform bool BLOOM_ADAPT_MODE <
    ui_label = "Adapt bloom only";
> = false;
uniform float BLOOM_TONEMAP_COMPRESSION <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 10.00;
	ui_label = "Bloom Tonemap Compression";
	ui_tooltip = "Lower values compress a larger color range.";
> = 4.0;

/*=============================================================================
	Textures, Samplers, Globals
=============================================================================*/

#define RESHADE_QUINT_COMMON_VERSION_REQUIRE 200
#include "qUINT_common.fxh"

#define INT_LOG2(v)   (((v >> 1) != 0) + ((v >> 2) != 0) + ((v >> 3) != 0) + ((v >> 4) != 0) + ((v >> 5) != 0) + ((v >> 6) != 0) + ((v >> 7) != 0) + ((v >> 8) != 0) + ((v >> 9) != 0) + ((v >> 10) != 0) + ((v >> 11) != 0) + ((v >> 12) != 0) + ((v >> 13) != 0) + ((v >> 14) != 0) + ((v >> 15) != 0) + ((v >> 16) != 0))

//static const int BloomTex7_LowestMip = int(log(BUFFER_HEIGHT/128) / log(2)) + 1;
static const int BloomTex7_LowestMip = INT_LOG2(BUFFER_HEIGHT/128);

texture2D MXBLOOM_BloomTexSource 	{ Width = BUFFER_WIDTH/2; 	Height = BUFFER_HEIGHT/2;    Format = RGBA16F;};
texture2D MXBLOOM_BloomTex1 		{ Width = BUFFER_WIDTH/2; 	Height = BUFFER_HEIGHT/2;    Format = RGBA16F;};
texture2D MXBLOOM_BloomTex2			{ Width = BUFFER_WIDTH/4;  	Height = BUFFER_HEIGHT/4;    Format = RGBA16F;};
texture2D MXBLOOM_BloomTex3 		{ Width = BUFFER_WIDTH/8;  	Height = BUFFER_HEIGHT/8;    Format = RGBA16F;};
texture2D MXBLOOM_BloomTex4 		{ Width = BUFFER_WIDTH/16;  Height = BUFFER_HEIGHT/16;   Format = RGBA16F;};
texture2D MXBLOOM_BloomTex5 		{ Width = BUFFER_WIDTH/32;  Height = BUFFER_HEIGHT/32;   Format = RGBA16F;};
texture2D MXBLOOM_BloomTex6 		{ Width = BUFFER_WIDTH/64;  Height = BUFFER_HEIGHT/64;   Format = RGBA16F;};
texture2D MXBLOOM_BloomTex7 		{ Width = BUFFER_WIDTH/128; Height = BUFFER_HEIGHT/128;  Format = RGBA16F; MipLevels = BloomTex7_LowestMip;};
texture2D MXBLOOM_BloomTexAdapt		{ Format = R16F; };

sampler2D sMXBLOOM_BloomTexSource	{ Texture = MXBLOOM_BloomTexSource;	};
sampler2D sMXBLOOM_BloomTex1		{ Texture = MXBLOOM_BloomTex1;		};
sampler2D sMXBLOOM_BloomTex2		{ Texture = MXBLOOM_BloomTex2;		};
sampler2D sMXBLOOM_BloomTex3		{ Texture = MXBLOOM_BloomTex3;		};
sampler2D sMXBLOOM_BloomTex4		{ Texture = MXBLOOM_BloomTex4;		};
sampler2D sMXBLOOM_BloomTex5		{ Texture = MXBLOOM_BloomTex5;		};
sampler2D sMXBLOOM_BloomTex6		{ Texture = MXBLOOM_BloomTex6;		};
sampler2D sMXBLOOM_BloomTex7		{ Texture = MXBLOOM_BloomTex7;		};
sampler2D sMXBLOOM_BloomTexAdapt	{ Texture = MXBLOOM_BloomTexAdapt;	};

/*=============================================================================
	Functions
=============================================================================*/

float4 downsample(sampler2D tex, float2 tex_size, float2 uv)
{
	float4 offset_uv = 0;

	float2 kernel_small_offsets = float2(2.0,2.0) / tex_size;
	float2 kernel_large_offsets = float2(4.0,4.0) / tex_size;

	float4 kernel_center = tex2D(tex, uv);

	float4 kernel_small = 0;

	offset_uv.xy = uv + kernel_small_offsets;
	kernel_small += tex2Dlod(tex, offset_uv); //++
	offset_uv.x = uv.x - kernel_small_offsets.x;
	kernel_small += tex2Dlod(tex, offset_uv); //-+
	offset_uv.y = uv.y - kernel_small_offsets.y;
	kernel_small += tex2Dlod(tex, offset_uv); //--
	offset_uv.x = uv.x + kernel_small_offsets.x;
	kernel_small += tex2Dlod(tex, offset_uv); //+-

#if SAMPLE_HQ == 0
	return kernel_center / 5.0	
	      + kernel_small / 5.0;
#else
	float4 kernel_large_1 = 0;

	offset_uv.xy = uv + kernel_large_offsets;
	kernel_large_1 += tex2Dlod(tex, offset_uv); //++
	offset_uv.x = uv.x - kernel_large_offsets.x;
	kernel_large_1 += tex2Dlod(tex, offset_uv); //-+
	offset_uv.y = uv.y - kernel_large_offsets.y;
	kernel_large_1 += tex2Dlod(tex, offset_uv); //--
	offset_uv.x = uv.x + kernel_large_offsets.x;
	kernel_large_1 += tex2Dlod(tex, offset_uv); //+-

	float4 kernel_large_2 = 0;

	offset_uv.xy = uv;
	offset_uv.x += kernel_large_offsets.x;
	kernel_large_2 += tex2Dlod(tex, offset_uv); //+0
	offset_uv.x -= kernel_large_offsets.x * 2.0;
	kernel_large_2 += tex2Dlod(tex, offset_uv); //-0
	offset_uv.x = uv.x;
	offset_uv.y += kernel_large_offsets.y;
	kernel_large_2 += tex2Dlod(tex, offset_uv); //0+
	offset_uv.y -= kernel_large_offsets.y * 2.0;
	kernel_large_2 += tex2Dlod(tex, offset_uv); //0-

	return kernel_center * 0.5 / 4.0		
	     + kernel_small  * 0.5 / 4.0	
	     + kernel_large_1 * 0.125 / 4.0
	     + kernel_large_2 * 0.25 / 4.0;
#endif
}

float3 Upsample(sampler2D tex, float2 texel_size, float2 uv)
{
	float4 offset_uv = 0;

	float4 kernel_small_offsets;
	kernel_small_offsets.xy = 1.5 * texel_size;
	kernel_small_offsets.zw = kernel_small_offsets.xy * 2;

	float3 kernel_center = tex2D(tex, uv).rgb;

	float3 kernel_small_1 = 0;

	offset_uv.xy = uv.xy - kernel_small_offsets.xy;
	kernel_small_1 += tex2Dlod(tex, offset_uv).rgb; //--
	offset_uv.x += kernel_small_offsets.z;
	kernel_small_1 += tex2Dlod(tex, offset_uv).rgb; //+-
	offset_uv.y += kernel_small_offsets.w;
	kernel_small_1 += tex2Dlod(tex, offset_uv).rgb; //++
	offset_uv.x -= kernel_small_offsets.z;
	kernel_small_1 += tex2Dlod(tex, offset_uv).rgb; //-+

#if SAMPLE_HQ == 0
	return kernel_center / 5.0
	     + kernel_small_1 / 5.0;
#else
	float3 kernel_small_2 = 0;

	offset_uv.xy = uv.xy + float2(kernel_small_offsets.x, 0);
	kernel_small_2 += tex2Dlod(tex, offset_uv).rgb; //+0
	offset_uv.x -= kernel_small_offsets.z;
	kernel_small_2 += tex2Dlod(tex, offset_uv).rgb; //-0
	offset_uv.xy = uv.xy + float2(0, kernel_small_offsets.y);
	kernel_small_2 += tex2Dlod(tex, offset_uv).rgb; //0+
	offset_uv.y -= kernel_small_offsets.w;
	kernel_small_2 += tex2Dlod(tex, offset_uv).rgb; //0-

	return kernel_center * 4.0 / 16.0
	     + kernel_small_1 * 1.0 / 16.0
	     + kernel_small_2 * 2.0 / 16.0;
#endif
}

/*=============================================================================
	Pixel Shaders
=============================================================================*/

void PS_BloomPrepass(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 color : SV_Target0)
{
	color = downsample(qUINT::sBackBufferTex, qUINT::SCREEN_SIZE, uv);
	color.w = saturate(dot(color.rgb, 0.333));

	color.rgb = lerp(color.w, color.rgb, BLOOM_SAT);
	color.rgb *= (pow(color.w, BLOOM_CURVE) * BLOOM_INTENSITY * BLOOM_INTENSITY * BLOOM_INTENSITY) / (color.w + 1e-3);
}

void PS_Downsample1(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTexSource, ldexp(qUINT::SCREEN_SIZE, -1.0), uv);
}
void PS_Downsample2(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex1, 		ldexp(qUINT::SCREEN_SIZE, -2.0), uv);
}
void PS_Downsample3(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex2, 		ldexp(qUINT::SCREEN_SIZE, -3.0), uv);
}
void PS_Downsample4(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex3, 		ldexp(qUINT::SCREEN_SIZE, -4.0), uv);
}
void PS_Downsample5(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex4, 		ldexp(qUINT::SCREEN_SIZE, -5.0), uv);
}
void PS_Downsample6(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex5, 		ldexp(qUINT::SCREEN_SIZE, -6.0), uv);
}
void PS_Downsample7(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = downsample(sMXBLOOM_BloomTex6, 		ldexp(qUINT::SCREEN_SIZE, -7.0), uv); 
	bloom.w = lerp(tex2D(sMXBLOOM_BloomTexAdapt, 0).x /*last*/, 
				   bloom.w /*current*/, 
				   saturate(qUINT::FRAME_TIME * 1e-3 * BLOOM_ADAPT_SPEED));
}

void PS_AdaptStoreLast(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float adapt : SV_Target0)
{	adapt = tex2Dlod(sMXBLOOM_BloomTex7, float4(uv.xy,0,BloomTex7_LowestMip)).w;}

void PS_Upsample1(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex7, ldexp(qUINT::PIXEL_SIZE, 7.0), uv) * BLOOM_LAYER_MULT_7, BLOOM_LAYER_MULT_6);}
void PS_Upsample2(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex6, ldexp(qUINT::PIXEL_SIZE, 6.0), uv), BLOOM_LAYER_MULT_5);
}
void PS_Upsample3(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex5, ldexp(qUINT::PIXEL_SIZE, 5.0), uv), BLOOM_LAYER_MULT_4);
}
void PS_Upsample4(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex4, ldexp(qUINT::PIXEL_SIZE, 4.0), uv), BLOOM_LAYER_MULT_3);
}
void PS_Upsample5(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex3, ldexp(qUINT::PIXEL_SIZE, 3.0), uv), BLOOM_LAYER_MULT_2);
}
void PS_Upsample6(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 bloom : SV_Target0)
{	
	bloom = float4(Upsample(sMXBLOOM_BloomTex2, ldexp(qUINT::PIXEL_SIZE, 2.0), uv), BLOOM_LAYER_MULT_1);
}

void PS_Combine(in float4 pos : SV_Position, in float2 uv : TEXCOORD, out float4 color : SV_Target0)
{
	float3 bloom = Upsample(sMXBLOOM_BloomTex1, ldexp(qUINT::PIXEL_SIZE, 1.0), uv);
	bloom /= dot(float4(BLOOM_LAYER_MULT_1, BLOOM_LAYER_MULT_2, BLOOM_LAYER_MULT_3, BLOOM_LAYER_MULT_4), 1) + dot(float3(BLOOM_LAYER_MULT_5, BLOOM_LAYER_MULT_6, BLOOM_LAYER_MULT_7), 1);
	color = tex2D(qUINT::sBackBufferTex, uv);
	
	float adapt = tex2D(sMXBLOOM_BloomTexAdapt, 0).x + 1e-3; // we lerped to 0.5 earlier.
	adapt *= 8;

	//based on suggestion by https://github.com/KarlRamstedt
	if(BLOOM_ADAPT_MODE)
	{
		bloom *= lerp(1, rcp(adapt), BLOOM_ADAPT_STRENGTH); 
		bloom *= exp2(BLOOM_ADAPT_EXPOSURE);
		color.rgb += bloom;
	}
	else 
	{
		color.rgb += bloom;
		color.rgb *= lerp(1, rcp(adapt), BLOOM_ADAPT_STRENGTH); 
		color.rgb *= exp2(BLOOM_ADAPT_EXPOSURE);
	}	

	color.rgb = pow(max(0,color.rgb), BLOOM_TONEMAP_COMPRESSION);
	color.rgb = color.rgb / (1.0 + color.rgb);
	color.rgb = pow(color.rgb, 1.0 / BLOOM_TONEMAP_COMPRESSION);
}

/*=============================================================================
	Techniques
=============================================================================*/

technique Bloom
< ui_tooltip = "                >> qUINT::Bloom <<\n\n"
			   "Bloom is a shader that produces a glow around bright\n"
               "light sources and other emitters on screen.\n"
               "\nBloom is written by Marty McFly / Pascal Gilcher"; >
{
    pass
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_BloomPrepass;
		RenderTarget0 = MXBLOOM_BloomTexSource;
	}

	#define PASS_DOWNSAMPLE(i) pass { VertexShader = PostProcessVS; PixelShader  = PS_Downsample##i; RenderTarget0 = MXBLOOM_BloomTex##i; }

	PASS_DOWNSAMPLE(1)
	PASS_DOWNSAMPLE(2)
	PASS_DOWNSAMPLE(3)
	PASS_DOWNSAMPLE(4)
	PASS_DOWNSAMPLE(5)
	PASS_DOWNSAMPLE(6)
	PASS_DOWNSAMPLE(7)

	pass
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_AdaptStoreLast;
		RenderTarget0 = MXBLOOM_BloomTexAdapt;
	}

	#define PASS_UPSAMPLE(i,j) pass {VertexShader = PostProcessVS;PixelShader  = PS_Upsample##i;RenderTarget0 = MXBLOOM_BloomTex##j;ClearRenderTargets = false;BlendEnable = true;BlendOp = ADD;SrcBlend = ONE;DestBlend = SRCALPHA;}

	PASS_UPSAMPLE(1,6)
	PASS_UPSAMPLE(2,5)
	PASS_UPSAMPLE(3,4)
	PASS_UPSAMPLE(4,3)
	PASS_UPSAMPLE(5,2)
	PASS_UPSAMPLE(6,1)

	pass
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_Combine;
	}
}
