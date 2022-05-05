  //========//
 // Macros //
//========//

#define NONE 0
#define SRGB 1
#define CUSTOM 2

#ifndef HEX_LENS_FLARE_BLUR_SAMPLES
#define HEX_LENS_FLARE_BLUR_SAMPLES 16
#endif

#ifndef HEX_LENS_FLARE_DOWNSCALE
#define HEX_LENS_FLARE_DOWNSCALE 4
#endif

#ifndef HEX_LENS_FLARE_GAMMA_MODE
#define HEX_LENS_FLARE_GAMMA_MODE SRGB
#endif

#ifndef HEX_LENS_FLARE_DEBUG
#define HEX_LENS_FLARE_DEBUG 0
#endif

  //===========//
 // Constants //
//===========//

#if defined(__RESHADE_FXC__)
	float GetAspectRatio() {
		return BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
	}
	float2 GetPixelSize() {
		return float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	}
	float2 GetScreenSize() {
		return float2(BUFFER_WIDTH, BUFFER_HEIGHT);
	}
	
	#define cAspectRatio GetAspectRatio()
	#define cPixelSize GetPixelSize()
	#define cScreenSize GetScreenSize()
#else
	static const float cAspectRatio = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
	static const float2 cPixelSize = float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	static const float2 cScreenSize = float2(BUFFER_WIDTH, BUFFER_HEIGHT);
#endif

static const float c2PI = 3.14159 * 2.0;

  //==========//
 // Uniforms //
//==========//

uniform float uIntensity <
	ui_label = "Intensity";
	ui_category = "Lens Flare";
	ui_tooltip = "Default: 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.001;
> = 1.0;

uniform float uThreshold <
	ui_label = "Threshold";
	ui_category = "Lens Flare";
	ui_tooltip = "Default: 0.99";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 0.95;

uniform float uScale <
	ui_label = "Scale";
	ui_category = "Lens Flare";
	ui_tooltip = "Default: 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 1.0;

uniform float3 uColor0 <
	ui_label = "#1";
	ui_category = "Colors";
	ui_tooltip = "Default: R:147 G:255 B:0";
	ui_type = "color";
> = float3(147, 255, 0) / 255.0;

uniform float3 uColor1 <
	ui_label = "#2";
	ui_category = "Colors";
	ui_tooltip = "Default: R:66 G:151 B:255";
	ui_type = "color";
> = float3(66, 151, 255) / 255.0;

uniform float3 uColor2 <
	ui_label = "#3";
	ui_category = "Colors";
	ui_tooltip = "Default: R:255 G:147 B:0";
	ui_type = "color";
> = float3(255, 147, 0) / 255.0;

uniform float3 uColor3 <
	ui_label = "#4";
	ui_category = "Colors";
	ui_tooltip = "Default: R:100 G:236 B:255";
	ui_type = "color";
> = float3(100, 236, 255) / 255.0;

#if HEX_LENS_FLARE_DEBUG

uniform int uDebugOptions <
	ui_label = "Debug Options";
	ui_category = "Debug";
	ui_tooltip = "Default: None";
	ui_type = "combo";
	ui_items = "None\0Display Texture\0";
> = 0;

#endif

  //==========//
 // Textures //
//==========//

texture2D tHexLensFlare_Color : COLOR;
sampler2D sColor {
	Texture = tHexLensFlare_Color;
	#if HEX_LENS_FLARE_GAMMA_MODE == SRGB
	SRGBTexture = true;
	#endif
	AddressU = BORDER;
	AddressV = BORDER;
};

texture2D tHexLensFlare_Prepare {
	Width = BUFFER_WIDTH / HEX_LENS_FLARE_DOWNSCALE;
	Height = BUFFER_HEIGHT / HEX_LENS_FLARE_DOWNSCALE;
	Format = RGBA16F;
};
sampler2D sPrepare {
	Texture = tHexLensFlare_Prepare;
};

texture2D tHexLensFlare_VerticalBlur {
	Width = BUFFER_WIDTH / HEX_LENS_FLARE_DOWNSCALE;
	Height = BUFFER_HEIGHT / HEX_LENS_FLARE_DOWNSCALE;
	Format = RGBA16F;
};
sampler2D sVerticalBlur {
	Texture = tHexLensFlare_VerticalBlur;
};

texture2D tHexLensFlare_DiagonalBlur {
	Width = BUFFER_WIDTH / HEX_LENS_FLARE_DOWNSCALE;
	Height = BUFFER_HEIGHT / HEX_LENS_FLARE_DOWNSCALE;
	Format = RGBA16F;
};
sampler2D sDiagonalBlur {
	Texture = tHexLensFlare_DiagonalBlur;
};

texture2D tHexLensFlare_RhomboidBlur {
	Width = BUFFER_WIDTH / HEX_LENS_FLARE_DOWNSCALE;
	Height = BUFFER_HEIGHT / HEX_LENS_FLARE_DOWNSCALE;
	Format = RGBA16F;
};
sampler2D sRhomboidBlur {
	Texture = tHexLensFlare_RhomboidBlur;
};

  //===========//
 // Functions //
//===========//

// Scale a coordinate.
// uv: Coordinate
// s: Scale
// c: Center
float2 scale(float2 uv, float2 s, float2 c) {
	return (uv - c) * s + c;
}

float2 scale(float2 uv, float2 s) {
	return scale(uv, s, 0.5);
}

float3 blur(sampler2D sp, float2 uv, float2 dir) {
	float4 color = 0.0;

	dir *= HEX_LENS_FLARE_DOWNSCALE * uScale;
	uv += dir * 0.5;

	[unroll]
	for (int i = 0; i < HEX_LENS_FLARE_BLUR_SAMPLES; ++i)
		color += float4(tex2D(sp, uv + dir * i).rgb, 1.0);

	return color.rgb / color.a;
}

float get_light(sampler2D sp, float2 uv, float t) {
	return step(t, dot(tex2D(sp, uv).rgb, 0.333));
}

  //=========//
 // Shaders //
//=========//

void VS_PostProcess(
	uint id : SV_VERTEXID,
	out float4 position : SV_POSITION,
	out float2 uv : TEXCOORD
) {
	uv.x = (id == 2) ? 2.0 : 0.0;
	uv.y = (id == 1) ? 2.0 : 0.0;
	position = float4(
		uv * float2(2.0, -2.0) + float2(-1.0, 1.0),
		0.0,
		1.0
	);
}

float4 PS_Prepare(
	float4 position : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	uv = 1.0 - uv;

	float3 color = 0.0;
	color += get_light(sColor, uv, uThreshold) * uColor0;
	color += get_light(sColor, scale(uv, 3.0), uThreshold) * uColor1;
	color += get_light(sColor, scale(uv, 9.0), uThreshold) * uColor2;
	color += get_light(sColor, scale(1.0 - uv, 0.666), uThreshold) * uColor3;

	return float4(color, 1.0);
}

float4 PS_VerticalBlur(
	float4 position : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float2 dir = cPixelSize * float2(cos(c2PI / 2), sin(c2PI / 2));
	float3 color = blur(sPrepare, uv, dir);

	return float4(color, 1.0);
}

float4 PS_DiagonalBlur(
	float4 position : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float2 dir = cPixelSize * float2(cos(-c2PI / 6), sin(-c2PI / 6));
	float3 color = blur(sPrepare, uv, dir);
	color += tex2D(sVerticalBlur, uv).rgb;

	return float4(color, 1.0);
}

float4 PS_RhomboidBlur(
	float4 position : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float2 dir1 = cPixelSize * float2(cos(-c2PI / 6), sin(-c2PI / 6));
	float3 color1 = blur(sVerticalBlur, uv, dir1);

	float2 dir2 = cPixelSize * float2(cos(-5 * c2PI / 6), sin(-5 * c2PI / 6));
	float3 color2 = blur(sDiagonalBlur, uv, dir2);

	float3 color = (color1 + color2) * 0.5;
	return float4(color, 1.0);
}

float4 PS_Blend(
	float4 position : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float3 color = tex2D(sColor, uv).rgb;
	float3 result = tex2D(sRhomboidBlur, uv).rgb;

	color = 1.0 - (1.0 - color) * (1.0 - result * uIntensity);

	#if HEX_LENS_FLARE_DEBUG

	if (uDebugOptions == 1)
		color = result;

	#endif

	return float4(color, 1.0);
}

  //===========//
 // Technique //
//===========//

technique HexLensFlare {
	pass Prepare {
		VertexShader = VS_PostProcess;
		PixelShader = PS_Prepare;
		RenderTarget = tHexLensFlare_Prepare;
	}
	pass VerticalBlur {
		VertexShader = VS_PostProcess;
		PixelShader = PS_VerticalBlur;
		RenderTarget = tHexLensFlare_VerticalBlur;
	}
	pass DiagonalBlur {
		VertexShader = VS_PostProcess;
		PixelShader = PS_DiagonalBlur;
		RenderTarget = tHexLensFlare_DiagonalBlur;
	}
	pass RhomboidBlur {
		VertexShader = VS_PostProcess;
		PixelShader = PS_RhomboidBlur;
		RenderTarget = tHexLensFlare_RhomboidBlur;
	}
	pass Blend {
		VertexShader = VS_PostProcess;
		PixelShader = PS_Blend;

		#if HEX_LENS_FLARE_GAMMA_MODE == SRGB
		SRGBWriteEnable = true;
		#endif
	}
}
