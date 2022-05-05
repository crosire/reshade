#include "ReShade.fxh"
#include "Tools.fxh"

#ifndef OVERLAY_NAME
	#define OVERLAY_NAME "neutral_lut.png"
	#undef OVERLAY_WIDTH
	#define OVERLAY_WIDTH 1024
	#undef OVERLAY_HEIGHT
	#define OVERLAY_HEIGHT 32
#else
	#ifndef OVERLAY_WIDTH
		#error "OVERLAY_WIDTH not defined"
	#endif
	#ifndef OVERLAY_HEIGHT
		#error "OVERLAY_HEIGHT not defined"
	#endif
#endif

uniform int2 i2UIOverlayPos <
	ui_type = "drag";
	ui_label = "Overlay Position";
	ui_min = 0; ui_max = BUFFER_WIDTH;
	ui_step = 1;
> = int2(0, 0);

uniform float UIoffset <
	ui_type = "drag";
	ui_label = "offset";
	ui_min = 0.0; ui_max = 5.0;
> = 1.0;

texture texOverlay < source = OVERLAY_NAME; > { Width = OVERLAY_WIDTH; Height = OVERLAY_HEIGHT; Format = RGBA8; };
sampler	samplerOverlay 	{ Texture = texOverlay; };


float3 DrawOverlayPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {

	float3 col = tex2D(ReShade::BackBuffer, texcoord).rgb;
	
	col = Tools::Draw::OverlaySampler(col, samplerOverlay, 1.0, texcoord, i2UIOverlayPos, 1.0);

	return col;
}

technique DrawOverlay {
	pass {VertexShader = PostProcessVS; PixelShader = DrawOverlayPS; /* RenderTarget = BackBuffer */}
}
