#include "ReShade.fxh"

uniform int pipcolor <
	ui_type = "combo";
	ui_items = "Amber\0Blue\0Green\0Green (FO4)\0White";
	ui_label = "PipBoy Color [Pipboy 3000]";
> = 0;

uniform int colors <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 256;
	ui_label = "Display Colors [Pipboy Green Fallout4]";
> = 256;

float4 PS_PipBoy3000 (float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  float3 ink = float3(1.0, 0.713, 0.258);
  
  if (pipcolor == 1){
	ink = float3(0.180, 0.811, 1.0);  
  } else if (pipcolor == 2){
	ink = float3(0.101, 1.0, 0.501); 
  } else if (pipcolor == 3){
	ink = float3(0.090, 1.000, 0.080);  
  } else if (pipcolor == 4){
	ink = float3(0.752, 1.0, 1.0);  
  }
  
  float3 c11 = tex2D(ReShade::BackBuffer, texcoord).xyz;
  float lct = floor(colors*length(c11))/colors;
  return float4(lct*ink,1);
}

technique PipBoy
{
   pass PipBoy3000
   {
     VertexShader = PostProcessVS;
     PixelShader  = PS_PipBoy3000;
   }  
}