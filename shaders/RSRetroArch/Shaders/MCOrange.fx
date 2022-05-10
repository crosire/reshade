#include "ReShade.fxh"

uniform int colors <
	ui_type = "drag";
	ui_min = 8;
	ui_max = 256;
	ui_tooltip = "Display Colors [MC Orange]";
> = 256;

float4 PS_MCOrange (float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  float3 ink = float3(0.57, 0.28, 0.0);  
  float3 c11 = tex2D(ReShade::BackBuffer, texcoord).xyz;
  float lct = floor(colors*length(c11))/colors;
  return float4(lct*ink,1);
}

technique MCOrange
{
   pass MCAmber0
   {
     VertexShader = PostProcessVS;
     PixelShader  = PS_MCOrange;
   }  
}
