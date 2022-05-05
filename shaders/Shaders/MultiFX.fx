/*
	MultiFX by luluco250

	A compilation of common post processing effects combined into a single pass.
*/

#include "ReShade.fxh"

//Macros/////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef MULTIFX_DEBUG
#define MULTIFX_DEBUG 0
#endif

#ifndef MULTIFX_SRGB
#define MULTIFX_SRGB 1
#endif

#ifndef MULTIFX_CA
#define MULTIFX_CA 1
#endif

#ifndef MULTIFX_VIGNETTE
#define MULTIFX_VIGNETTE 1
#endif

#ifndef MULTIFX_FILMGRAIN
#define MULTIFX_FILMGRAIN 1
#endif

#ifndef MULTIFX_SCURVE
#define MULTIFX_SCURVE 1
#endif

#define nsin(x) (sin(x) * 0.5 + 0.5)

//Uniforms///////////////////////////////////////////////////////////////////////////////////////////////////////

#if MULTIFX_CA
uniform float3 f3CA_Intensity <
	ui_label   = "[Chromatic Aberration] Intensity";
	ui_tooltip = "Controls the scale of each color channel in the image. (Red, Green, Blue)";
	ui_type    = "drag";
	ui_min     = 0.0;
	ui_max     = 1.0;
	ui_step    = 0.001;
> = 0.0;
#endif
uniform float fVignette_Intensity <
	ui_label = "[Vignette] Opacity";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 3.0;
	ui_step  = 0.001;
> = 0.0;

uniform float2 f2Vignette_StartEnd <
	ui_label   = "[Vignette] Start/End";
	ui_tooltip = "Controls the range/contrast of vignette by defining start and end offsets.";
	ui_type    = "drag";
	ui_min     =-10.0;
	ui_max     = 10.0;
	ui_step    = 0.001;
> = float2(0.0, 1.0);

uniform float fFilmGrain_Intensity <
	ui_label = "[Film Grain] Intensity";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 1.0;
	ui_step  = 0.001;
> = 0.0;

uniform float fFilmGrain_Speed <
	ui_label = "[Film Grain] Speed";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 100.0;
	ui_step  = 0.001;
> = 1.0;

uniform float fFilmGrain_Mean <
	ui_label = "[Film Grain] Mean";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 1.0;
	ui_step  = 0.001;
> = 0.0;

uniform float fFilmGrain_Variance <
	ui_label = "[Film Grain] Variance";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 1.0;
	ui_step  = 0.001;
> = 0.5;

#if MULTIFX_DEBUG
uniform bool bFilmGrain_ShowNoise <
	ui_label = "[Film Grain] Show Noise";
> = false;
#endif

uniform float fSCurve_Intensity <
	ui_label = "[S-Curve] Intensity";
	ui_type  = "drag";
	ui_min   = 0.0;
	ui_max   = 3.0;
	ui_step  = 0.001;
> = 0.0;

uniform float fTime <source = "timer";>;
/*uniform float fDelta <source = "frametime";>;
uniform float2 f2Mouse <source = "mousepoint";>;*/

//Textures///////////////////////////////////////////////////////////////////////////////////////////////////////

sampler2D sMultiFX_BackBuffer {
	Texture = ReShade::BackBufferTex;
	#if MULTIFX_SRGB
	SRGBTexture = true;
	#endif
};

//Functions//////////////////////////////////////////////////////////////////////////////////////////////////////

float2 scale_uv(float2 uv, float2 scale, float2 center) {
	return (uv - center) * scale + center;
}

float2 scale_uv(float2 uv, float2 scale) {
	return scale_uv(uv, scale, 0.5);
}

float fmod(float a, float b) {
	float c = frac(abs(a / b)) * abs(b);
	return (a < 0.0) ? -c : c;
}

float rand(float2 uv) {
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt = dot(uv, float2(a, b));
	float sn = fmod(dt, 3.1415);
	return frac(sin(sn) * c);
}

float3 blend_dodge(float3 a, float3 b, float w) {
	return lerp(a, a / max(1.0 - b, 0.0000001), w);
}

float gaussian(float z, float u, float o) {
    return (1.0 / (o * sqrt(2.0 * 3.1415))) * exp(-(((z - u) * (z - u)) / (2.0 * (o * o))));
}

float get_lum(float3 color) {
	return max(color.r, max(color.g, color.b));
}

//Effects////////////////////////////////////////////////////////////////////////////////////////////////////////

void ChromaticAberration(out float4 color, float2 uv) {
	color = float4(
		tex2D(sMultiFX_BackBuffer, scale_uv(uv, 1.0 - f3CA_Intensity.r * 0.01)).r,
		tex2D(sMultiFX_BackBuffer, scale_uv(uv, 1.0 - f3CA_Intensity.g * 0.01)).g,
		tex2D(sMultiFX_BackBuffer, scale_uv(uv, 1.0 - f3CA_Intensity.b * 0.01)).b,
		1.0
	);
}

void Vignette(inout float4 color, float2 uv) {
	float vignette = 1.0 - smoothstep(
		f2Vignette_StartEnd.x,
		f2Vignette_StartEnd.y,
		distance(uv, 0.5)
	);
	color = lerp(color, color * vignette, fVignette_Intensity);
}

void FilmGrain(inout float4 color, float2 uv) {
	float t     = fTime * 0.001 * fFilmGrain_Speed;
    float seed  = dot(uv, float2(12.9898, 78.233));
    float noise = frac(sin(seed) * 43758.5453 + t);
	noise = gaussian(noise, fFilmGrain_Mean, fFilmGrain_Variance * fFilmGrain_Variance);

    //float lum   = get_lum(color.rgb);
	//noise = smoothstep(f2FilmGrain_Curve.x, f2FilmGrain_Curve.y, noise);
    
	#if MULTIFX_DEBUG
	if (bFilmGrain_ShowNoise)
		color = noise;
	else
	#endif

	float3 grain = noise * (1.0 - color.rgb);
	color.rgb += grain * fFilmGrain_Intensity * 0.01;

    //color += lerp(noise, color * noise, lum) * fFilmGrain_Intensity;
}

void SCurve(inout float4 color, float2 uv) {
	color = lerp(color, smoothstep(0.0, 1.0, color), fSCurve_Intensity);
}

//Shader/////////////////////////////////////////////////////////////////////////////////////////////////////////

void PS_MultiFX(
	float4 position  : SV_POSITION,
	float2 uv        : TEXCOORD,
	out float4 color : SV_TARGET
) {
	#if MULTIFX_CA
	ChromaticAberration(color, uv);
	#else
	color = tex2D(sMultiFX_BackBuffer, uv);
	#endif
	#if MULTIFX_VIGNETTE
	Vignette(color, uv);
	#endif
	#if MULTIFX_FILMGRAIN
	FilmGrain(color, uv);
	#endif
	#if MULTIFX_SCURVE
	SCurve(color, uv);
	#endif
	color.a = 1.0;
}

//Technique//////////////////////////////////////////////////////////////////////////////////////////////////////

technique MultiFX {
	pass {
		VertexShader = PostProcessVS;
		PixelShader = PS_MultiFX;
		#if MULTIFX_SRGB
		SRGBWriteEnable = true;
		#endif
	}
}
