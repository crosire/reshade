//#region Includes

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

//#endregion

//#region Uniforms

uniform float DofScale
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Scale";
	ui_tooltip =
		"If this is empty, nag @luluco250 in the ReShade Discord channel.\n"
		"\nDefault: 3.0";
	ui_category = "Appearance";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 3.0;

uniform float FocusTime
<
	__UNIFORM_DRAG_FLOAT1

	ui_label = "Time";
	ui_tooltip =
		"If this is empty, nag @luluco250 in the ReShade Discord channel.\n"
		"\nDefault: 350.0";
	ui_category = "Focus";
	ui_min = 0.0;
	ui_max = 2000.0;
	ui_step = 10.0;
> = 350.0;

uniform float2 FocusPoint
<
	__UNIFORM_DRAG_FLOAT2

	ui_label = "Point";
	ui_tooltip =
		"If this is empty, nag @luluco250 in the ReShade Discord channel.\n"
		"\nDefault: 0.5 0.5";
	ui_category = "Focus";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
> = float2(0.5, 0.5);

uniform float FrameTime <source = "frametime";>;

//#endregion

//#region Textures

texture FocalDOF_Focus { Format = R32F; };
sampler Focus { Texture = FocalDOF_Focus; };

texture FocalDOF_LastFocus { Format = R32F; };
sampler LastFocus { Texture = FocalDOF_LastFocus; };

//#endregion

//#region Shaders

void GetFocusVS(
	uint id : SV_VERTEXID,
	out float4 p : SV_POSITION,
	out float2 uv : TEXCOORD0,
	out float focus : TEXCOORD1)
{
	PostProcessVS(id, p, uv);
	
	float last = tex2Dfetch(LastFocus, 0).x;
	focus = ReShade::GetLinearizedDepth(FocusPoint);
	focus = lerp(last, focus, FrameTime / FocusTime);
	focus = saturate(focus);
}

void ReadFocusVS(
	uint id : SV_VERTEXID,
	out float4 p : SV_POSITION,
	out float2 uv : TEXCOORD0,
	out float focus : TEXCOORD1)
{
	PostProcessVS(id, p, uv);
	focus = tex2Dfetch(Focus, 0).x;
}

float4 GetFocusPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD0,
	float focus : TEXCOORD1) : SV_TARGET
{
	return focus;
}

float4 SaveFocusPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD0,
	float focus : TEXCOORD1) : SV_TARGET
{
	return focus;
}

float4 MainPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD0,
	float focus : TEXCOORD1) : SV_TARGET
{
	float depth = ReShade::GetLinearizedDepth(uv);
	float scale = abs(depth - focus) * DofScale;

	#define FETCH(off) exp(\
		tex2D(ReShade::BackBuffer, uv + ReShade::PixelSize * off * scale))

	static const float2 offsets[] =
	{
		float2(0.0, 1.0),
		float2(0.75, 0.75),
		float2(1.0, 0.0),
		float2(0.75, -0.75),
		float2(0.0, -1.0),
		float2(-0.75, -0.75),
		float2(-1.0, 0.0),
		float2(-0.75, 0.75)
	};

	float4 color = FETCH(0.0);

	[unroll]
	for (int i = 0; i < 8; ++i)
		color += FETCH(offsets[i]);
	color /= 9;

	#undef FETCH

	return log(color);
}

//#endregion

//#region Technique

technique FocalDOF
{
	pass GetFocus
	{
		VertexShader = GetFocusVS;
		PixelShader = GetFocusPS;
		RenderTarget = FocalDOF_Focus;
	}
	pass SaveFocus
	{
		VertexShader = ReadFocusVS;
		PixelShader = SaveFocusPS;
		RenderTarget = FocalDOF_LastFocus;
	}
	pass Main
	{
		VertexShader = ReadFocusVS;
		PixelShader = MainPS;
	}
}

//#endregion