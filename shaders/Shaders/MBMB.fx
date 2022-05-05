/*
	Mouse-Based Motion Blur by luluco250
*/

#include "ReShade.fxh"

//Macros////////////////////////////////////////////////////////////////////////////////////////////

// How many samples/taps to take from the image.
// Too low will make the blur less smooth.
// Too high can be very bad for performance.
#ifndef MBMB_SAMPLES
#define MBMB_SAMPLES 20
#endif

// Use gaussian blur or simple linear blurring?
#ifndef MBMB_GAUSSIAN
#define MBMB_GAUSSIAN 1
#endif

// Depth flags, they do exactly what they're named after.
#define AFFECT_INTENSITY 1
#define AFFECT_DISTORTION 2

// To use these flags, you define MBMB_DEPTH_FLAGS in
// this fashion: 
//
// MBMB_DEPTH_FLAGS=0
// MBMB_DEPTH_FLAGS=AFFECT_INTENSITY
// MBMB_DEPTH_FLAGS=AFFECT_DISTORTION
// MBMB_DEPTH_FLAGS=AFFECT_INTENSITY | AFFECT_DISTORTION
//
// Note the '|' OR operator, this combines the flags.
#ifndef MBMB_DEPTH_FLAGS
#define MBMB_DEPTH_FLAGS 0
#endif

// Smoothes out mouse movement, but uses two textures and passes.
#ifndef MBMB_SMOOTHING
#define MBMB_SMOOTHING 0
#endif

// Simulates HDR, which makes highlights keep
// their brightness even after blurring.
#ifndef MBMB_HDR
#define MBMB_HDR 0
#endif

//Uniforms//////////////////////////////////////////////////////////////////////////////////////////

uniform float fSensitivity <
	ui_label   = "Sensitivity";
	ui_tooltip = "This is multiplied to the mouse speed.\n"
	             "Lower values will help smooth out motion "
				 "as the mouse speed can be a bit \"jumpy\" "
				 "at low motions.\n"
				 "Higher values will instead increase it even "
				 "more, increasing the overall \"instensity\" "
				 "of the effect.\n"
	             "\nDefault: 0.1";
	ui_type    = "drag";
	ui_min     = 0.0;
	ui_max     = 100.0;
	ui_step    = 0.01;
> = 0.1;

uniform float fDistortion <
	ui_label   = "Distortion";
	ui_tooltip = "How much to distort the blurring to "
	             "simulate the look of real vector-based "
				 "motion blur.\n"
				 "Can look weird with values too high "
				 "but also smoothes out the blurring, "
				 "preventing \"straight lines\".\n"
	             "\nDefault: 1.0";
	ui_type    = "drag";
	ui_min     = 0.0;
	ui_max     = 10.0;
	ui_step    = 0.01;
> = 1.0;

uniform float2 f2MinMaxSpeed <
	ui_label   = "Min/Max Speed";
	ui_tooltip = "The first value tells the shader where "
	             "to cut off the mouse speed, so it only "
				 "blurs when you move the mouse at a speed "
				 "higher than it.\n"
				 "The second value specifies the maximum "
				 "speed allowed from the mouse.\n"
				 "You can disable this by setting the "
				 "first value to 0.0 and the second to a "
				 "very high value, like 100.0.\n"
	             "\nDefault: (0.1, 10.0)";
	ui_type    = "drag";
	ui_min     = 0.0;
	ui_max     = 100.0;
	ui_step    = 0.01;
> = float2(0.1, 10.0);

#if MBMB_DEPTH_FLAGS > 0
uniform float fDepthInfluence <
	ui_label   = "Depth Influence";
	ui_tooltip = "How much depth affects the effect.\n"
	             "Values lower than 1.0 will reduce "
				 "how much depth has an effect on the "
				 "intensity and distortion of the effect.\n"
	             "Values above 1.0 will increase it's effect.\n"
				 "It is recommended to test thoroughly to "
				 "find a good balance.\n"
	             "\nDefault: 1.0";
	ui_type    = "drag";
	ui_min     = 0.0;
	ui_max     = 10.0;
	ui_step    = 0.001;
> = 1.0;
#endif

#if MBMB_GAUSSIAN
uniform float fBlur_Sigma <
	ui_label   = "Blur Sigma";
	ui_tooltip = "How much to blur the image.\n"
	             "Too much will reduce the overall "
				 "quality of the blurring.\n"
				 "While too little will reduce the "
				 "amount of blurring overall.\n"
	             "\nDefault: 4.0";
	ui_type    = "drag";
	ui_min     = 1.0;
	ui_max     = 100.0;
	ui_step    = 0.01;
> = 4.0;
#endif

#if MBMB_SMOOTHING
uniform float fSmoothing <
	ui_label   = "Smoothing";
	ui_tooltip = "Default: 10.0";
	ui_type    = "drag";
	ui_min     = 0.01;
	ui_max     = 100.0;
	ui_step    = 0.01;
> = 10.0;
#endif

#if MBMB_HDR
uniform float fHDRBrightness <
	ui_label   = "HDR Brightness";
	ui_tooltip = "Default: 3.0";
	ui_type    = "drag";
	ui_min     = 1.0;
	ui_max     = 10.0;
	ui_step    = 0.01;
> = 3.0;
#endif

#if MBMB_SMOOTHING
uniform float fFrameTime <source = "frametime";>;
#endif

uniform float3 f3MouseDelta <source = "mousedelta";>;

//Textures//////////////////////////////////////////////////////////////////////////////////////////

sampler2D sBackBuffer {
	Texture     = ReShade::BackBufferTex;
	//SRGBTexture = true;
	MinFilter   = LINEAR;
	MagFilter   = LINEAR;
	MipFilter   = LINEAR;
	AddressU    = CLAMP;
	AddressV    = CLAMP;
};

#if MBMB_SMOOTHING
texture2D tMBMB_Mouse {
	Format = RG16F;
};
sampler2D sMouse {
	Texture   = tMBMB_Mouse;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU  = BORDER;
	AddressV  = BORDER;
};

texture2D tMBMB_LastMouse {
	Format = RG16F;
};
sampler2D sLastMouse {
	Texture   = tMBMB_LastMouse;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU  = BORDER;
	AddressV  = BORDER;
};
#endif

#if MBMB_HDR
texture2D tMBMB_HDR {
	Width  = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D sHDR {
	Texture   = tMBMB_HDR;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
};
#endif

//Functions/////////////////////////////////////////////////////////////////////////////////////////

// https://en.wikipedia.org/wiki/Gaussian_blur#Mathematics
float gaussian1D(float x, float sigma) {
	return (1.0 / sqrt(2.0 * 3.1415 * (sigma * sigma))) * exp(-((x * x) / (2.0 * (sigma * sigma))));
}

// Better than regular clamping for a number of reasons.
// More specifically it keeps the overall 'form' of the vector,
// normal clamping distorts it.
float2 clamp_magnitude(float2 v, float2 min_max) {
	if (v.x == 0.0 && v.y == 0.0) {
		return 0.0;
	} else {
		float mag = length(v);
		return (mag < min_max.x) ? 0.0 : (v / mag) * min(mag, min_max.y);
	}
}

float3 reinhard(float3 c) {
	return c / (1.0 + c);
}

float3 inv_reinhard(float3 c, float max_c) {
	return (c / max(1.0 - c, max_c));
}

//Shader////////////////////////////////////////////////////////////////////////////////////////////

#if MBMB_HDR
float4 PS_MakeHDR(
	float4 position : SV_POSITION,
	float2 uv       : TEXCOORD
) : SV_TARGET {
	return float4(pow(tex2D(sBackBuffer, uv).rgb, fHDRBrightness), 1.0);
}
#endif

float4 PS_MBMB(
	float4 position : SV_POSITION,
	float2 uv       : TEXCOORD
) : SV_TARGET {
#if MBMB_SMOOTHING
	float2 speed = tex2Dfetch(sMouse, (int4)0).xy * fSensitivity;
#else
	float2 speed = f3MouseDelta.xy * fSensitivity;
#endif

	float2 mms = f2MinMaxSpeed;
	mms.y = mms.y <= 0.0 ? 10000.0 : mms.y;
	speed = clamp_magnitude(speed, mms);

#if MBMB_DEPTH_FLAGS > 0
	float depth = ReShade::GetLinearizedDepth(uv);
	depth = lerp(1.0, depth * fDepthInfluence, saturate(fDepthInfluence));
#endif

	// This tells the direction and how much to blur the image.
	float2 ps = ReShade::PixelSize * speed;

	float d = fDistortion;
#if MBMB_DEPTH_FLAGS & AFFECT_DISTORTION
	d *= depth;
#endif

	// Distortion stuff, makes it look like real vector motion blur.
	float2 distort = 1.0 - abs(uv - 0.5) * 2.0;
	ps *= lerp(1.0, distort.yx * d, saturate(d));

#if MBMB_DEPTH_FLAGS & AFFECT_INTENSITY
	ps = lerp(0.0, ps, depth);
#endif

#if MBMB_HDR
#define sColor sHDR
#else
#define sColor sBackBuffer
#endif

#if MBMB_GAUSSIAN
	// We'll use color.a for accumulation.
	float4 color = 0.0;

	// The usual blurring algorithm, gather various points
	// at a certain offset and then blend them.
	for (int i = -MBMB_SAMPLES / 2; i <= MBMB_SAMPLES / 2; ++i) {
		float offset = i;
		color += float4(tex2D(sColor, uv + ps * offset).rgb, 1.0) * gaussian1D(offset, fBlur_Sigma);
	}
	color.rgb /= color.a;
#else
	float3 color = 0.0;

	// Much simpler blurring, similar to box blur.
	for (int i = -MBMB_SAMPLES / 2; i <= MBMB_SAMPLES / 2; ++i)
		color += tex2D(sColor, uv + ps * i).rgb;
	color /= MBMB_SAMPLES;
#endif

#if MBMB_HDR
	color.rgb = pow(color.rgb, 1.0 / fHDRBrightness);
#endif

	return float4(color.rgb, 1.0);

#undef sColor
}

#if MBMB_SMOOTHING
float4 PS_SaveMouse(
	float4 position : SV_POSITION,
	float2 uv       : TEXCOORD
) : SV_TARGET {
	float2 mouse = f3MouseDelta.xy;
	float2 last  = tex2Dfetch(sLastMouse, (int4)0).xy;
	mouse = lerp(last, mouse, saturate(fFrameTime / fSmoothing));
	return float4(mouse, 0.0, 1.0);
}

float4 PS_SaveLast(
	float4 position : SV_POSITION,
	float2 uv       : TEXCOORD
) : SV_TARGET {
	return float4(tex2Dfetch(sMouse, (int4)0).xy, 0.0, 1.0);
}
#endif

//Technique/////////////////////////////////////////////////////////////////////////////////////////

technique MBMB {
#if MBMB_HDR
	pass MakeHDR {
		VertexShader = PostProcessVS;
		PixelShader  = PS_MakeHDR;
		RenderTarget = tMBMB_HDR;
	}
#endif
	pass MBMB {
		VertexShader    = PostProcessVS;
		PixelShader     = PS_MBMB;
		//SRGBWriteEnable = true;
	}

	#if MBMB_SMOOTHING
	pass SaveMouse {
		VertexShader = PostProcessVS;
		PixelShader  = PS_SaveMouse;
		RenderTarget = tMBMB_Mouse;
	}
	pass SaveLast {
		VertexShader = PostProcessVS;
		PixelShader  = PS_SaveLast;
		RenderTarget = tMBMB_LastMouse;
	}
	#endif
}
