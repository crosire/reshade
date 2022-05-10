#include "ReShade.fxh"

uniform float3 ConvergeX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Horizontal Convergence [Converge]";
	ui_tooltip = "Convergence on the X Axis";
> = float3(0.0, 0.0, 0.0);

uniform float3 ConvergeY <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Vertical Convergence [Converge]";
	ui_tooltip = "Convergence on the Y Axis";
> = float3(0.0, 0.0, 0.0);

uniform float3 RadialConvergeX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Radial Horizontal Convergence [Converge]";
	ui_tooltip = "Radial convergence on the X Axis";
> = float3(0.0, 0.0, 0.0);

uniform float3 RadialConvergeY <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Radial Vertical Convergence [Converge]";
	ui_tooltip = "Radial convergence on the Y Axis";
> = float3(0.0, 0.0, 0.0);


//DEV NOTE: Even if they have those options, best to leave them to same or the best looking, since ReShade can't go that deep on injection
uniform float2 ScreenDims <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 100000.0;
	ui_step = 1.0;
	ui_label = "Screen Dimensions [Converge]";
	ui_tooltip = "Should be your display resolution";
> = float2(320.0,240.0);

uniform float2 TargetDims <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 100000.0;
	ui_step = 1.0;
	ui_label = "Target Dimensions [Converge]";
	ui_tooltip = "Should be your desired or game resolution";
> = float2(320.0,240.0);


float4 PS_Deconverge(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target0
{
	vpos.xy /= ReShade::ScreenSize.xy;
	vpos.y = 1.0f - texcoord.y; // flip y
	vpos.xy -= 0.5f; // center
	vpos.xy *= 2.0f; // zoom

	texcoord += 0.5f / TargetDims; // half texel offset correction (DX9)

	// imaginary texel dimensions independed from screen dimension, but ratio
	float2 TexelDims = (1.0f / 1024);

	// center coordinates
	texcoord.x -= 0.5f;
	texcoord.y -= 0.5f;

	// radial converge offset to "translate" the most outer pixel as thay would be translated by the linar converge with the same amount
	float2 radialConvergeOffset = 2.0f;

	// radial converge
	texcoord.x *= 1.0f + RadialConvergeX * TexelDims.x * radialConvergeOffset.x;
	texcoord.y *= 1.0f + RadialConvergeY * TexelDims.y * radialConvergeOffset.y;

	// un-center coordinates
	texcoord.x += 0.5f;
	texcoord.y += 0.5f;

	// linear converge
	texcoord.x += ConvergeX * TexelDims.x;
	texcoord.y += ConvergeY * TexelDims.y;

	float3 col = tex2D(ReShade::BackBuffer,texcoord).rgb;

	return float4(col,1.0);
}

technique Deconverge_MAME
{
	pass DeconvergeMAME
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Deconverge;
	}
}