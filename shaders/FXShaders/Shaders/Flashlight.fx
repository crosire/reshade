/*
	Flashlight shader by luluco250

	MIT Licensed.
*/

#include "ReShade.fxh"

#ifndef FLASHLIGHT_NO_DEPTH
#define FLASHLIGHT_NO_DEPTH 0
#endif

#ifndef FLASHLIGHT_NO_TEXTURE
#define FLASHLIGHT_NO_TEXTURE 0
#endif

#ifndef FLASHLIGHT_NO_BLEND_FIX
#define FLASHLIGHT_NO_BLEND_FIX 0
#endif

uniform float uBrightness <
	ui_label = "Brightness";
	ui_tooltip =
		"Flashlight halo brightness.\n"
		"\nDefault: 10.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.01;
> = 10.0;

uniform float uSize <
	ui_label = "Size";
	ui_tooltip =
		"Flashlight halo size in pixels.\n"
		"\nDefault: 420.0";
	ui_type = "drag";
	ui_min = 10.0;
	ui_max = 1000.0;
	ui_step = 1.0;
> = 420.0;

uniform float3 uColor <
	ui_label = "Color";
	ui_tooltip =
		"Flashlight halo color.\n"
		"\nDefault: R:255 G:230 B:200";
	ui_type = "color";
> = float3(255, 230, 200) / 255.0;

#if !FLASHLIGHT_NO_DEPTH
uniform float uDistance <
	ui_label = "Distance";
	ui_tooltip =
		"The distance that the flashlight can illuminate.\n"
		"Only works if the game has depth buffer access.\n"
		"\nDefault: 0.1";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 0.1;
#endif

sampler2D sColor {
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
	MinFilter = POINT;
	MagFilter = POINT;
};

#define nsin(x) (sin(x) * 0.5 + 0.5)

float4 PS_Flashlight(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	float2 res = ReShade::ScreenSize;
	float2 coord = uv * res;

	float halo = distance(coord, res * 0.5);
	float flashlight = uSize - min(halo, uSize);
	flashlight /= uSize;
	
	#if !FLASHLIGHT_NO_TEXTURE
	// Add some texture to the halo by using some sin lines + reduce intensity
	// when nearing the center of the halo.
	float defects = sin(flashlight * 30.0) * 0.5 + 0.5;
	defects = lerp(defects, 1.0, flashlight * 2.0);

	static const float contrast = 0.125;

	defects = 0.5 * (1.0 - contrast) + defects * contrast;
	flashlight *= defects * 4.0;
	#else
	flashlight *= 2.0;
	#endif

	#if !FLASHLIGHT_NO_DEPTH
	float depth = 1.0 - ReShade::GetLinearizedDepth(uv);
	depth = pow(abs(depth), 1.0 / uDistance);
	flashlight *= depth;
	#endif

	float3 colored_flashlight = flashlight * uColor;
	colored_flashlight *= colored_flashlight * colored_flashlight;

	float3 result = 1.0 + colored_flashlight * uBrightness;

	float3 color = tex2D(sColor, uv).rgb;
	color *= result;

	#if !FLASHLIGHT_NO_BLEND_FIX

	// Add some minimum amount of light to very dark pixels.	
	color = max(color, (result - 1.0) * 0.001);
	
	#endif

	return float4(color, 1.0);
}

technique Flashlight {
	pass {
		VertexShader = PostProcessVS;
		PixelShader = PS_Flashlight;
		SRGBWriteEnable = true;
	}
}
