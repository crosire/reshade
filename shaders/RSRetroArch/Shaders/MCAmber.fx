#include "ReShade.fxh"

uniform int colors <
	ui_type = "drag";
	ui_min = 8;
	ui_max = 256;
	ui_tooltip = "Display Colors [MC Amber]";
> = 256;

float4 PS_MCAmber (float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  float3 ink = float3(0.55, 0.41, 0.0);
  float3 c11 = tex2D(ReShade::BackBuffer, texcoord).xyz;
  float lct = floor(colors*length(c11))/colors;
  return float4(lct*ink,1);
}

technique MCAmber
{
   pass MCAmber0
   {
     VertexShader = PostProcessVS;
     PixelShader  = PS_MCAmber;
   }  
}