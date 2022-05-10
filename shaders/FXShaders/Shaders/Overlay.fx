#include "ReShade.fxh"

#ifndef OVERLAY_FILENAME
#define OVERLAY_FILENAME "Overlay.png"
#endif

#ifndef OVERLAY_FILTER
#define OVERLAY_FILTER LINEAR
#endif

#ifndef OVERLAY_WIDTH
#define OVERLAY_WIDTH BUFFER_WIDTH
#endif

#ifndef OVERLAY_HEIGHT
#define OVERLAY_HEIGHT BUFFER_HEIGHT
#endif

uniform float fOpacity <
	ui_label = "Opacity";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 1.0;

uniform float fStretch <
	ui_label = "Stretch";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = 1.0;

uniform float2 f2Center <
	ui_label = "Center";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.5, 0.5);

uniform bool bKeepAspectRatio <
	ui_label = "Keep Aspect Ratio";
> = false;

uniform bool bFollowMouse <
	ui_label = "Follow Mouse";
> = false;

uniform float2 f2Mouse <source="mousepoint";>;

static const float2 f2Resolution = float2(OVERLAY_WIDTH, OVERLAY_HEIGHT);
static const float2 f2PixelSize = 1.0 / f2Resolution;

texture tOverlay <
	source=OVERLAY_FILENAME;
> {
	Width = OVERLAY_WIDTH;
	Height = OVERLAY_HEIGHT;
};
sampler sOverlay { 
	Texture = tOverlay;
	MinFilter = OVERLAY_FILTER;
	MagFilter = OVERLAY_FILTER;
	AddressU = BORDER;
	AddressV = BORDER;
};

float2 scale_uv(float2 uv, float2 scale, float2 center) {
	return (uv - center) * scale + center;
}

float4 PS_Overlay(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float2 ar = f2Resolution.x > f2Resolution.y ? float2(f2Resolution.x / f2Resolution.y, 1.0) : float2(1.0, f2Resolution.y / f2Resolution.x);
	float2 corrected = bKeepAspectRatio ? ReShade::ScreenSize * ar : ReShade::ScreenSize;
	float2 stretch = lerp(
		ReShade::ScreenSize * f2PixelSize / (bKeepAspectRatio ? 1.0 : ar),
		corrected * ReShade::PixelSize,
		fStretch
	);

	float2 mouse_pos = saturate(f2Mouse * ReShade::PixelSize);
	float2 uv_overlay;

	if (bFollowMouse)
		uv_overlay = scale_uv(
			uv - mouse_pos + 0.5, 
			stretch, 
			0.5
		);
	else
		uv_overlay = scale_uv(
			uv,
			stretch,
			float2(f2Center.x, 1.0 - f2Center.y)
		);

	float4 original = tex2D(ReShade::BackBuffer, uv);
	float4 overlay = tex2D(sOverlay, uv_overlay);

	float opacity = overlay.a * fOpacity;

	return lerp(original, overlay, opacity);
}

technique Overlay {
	pass {
		VertexShader = PostProcessVS;
		PixelShader = PS_Overlay;
	}
}
