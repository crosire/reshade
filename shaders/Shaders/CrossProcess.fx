/*
	Coded by Gilcher Pascal aka Marty McFly
	Amateur port by Insomnia 
*/	

uniform float CrossContrast <
	ui_type = "drag";
	ui_min = 0.50; ui_max = 2.0;
	ui_tooltip = "Contrast";
> = 1.0;
uniform float CrossSaturation <
	ui_type = "drag";
	ui_min = 0.50; ui_max = 2.00;
	ui_tooltip = "Saturation";
> = 1.0;
uniform float CrossBrightness <
	ui_type = "drag";
	ui_min = -1.000; ui_max = 1.000;
	ui_tooltip = "Brightness";
> = 0.0;
uniform float CrossAmount <
	ui_type = "drag";
	ui_min = 0.05; ui_max = 1.50;
	ui_tooltip = "Cross Amount";
> = 0.50;


#include "ReShade.fxh"

float3 CrossPass(float4 position : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	
	float2 CrossMatrix [3] = {
		float2 (1.03, 0.04),
		float2 (1.09, 0.01),
		float2 (0.78, 0.13),
 		};

	float3 image1 = color.rgb;
	float3 image2 = color.rgb;
	float gray = dot(float3(0.5,0.5,0.5), image1);  
	image1 = lerp (gray, image1,CrossSaturation);
	image1 = lerp (0.35, image1,CrossContrast);
	image1 +=CrossBrightness;
	image2.r = image1.r * CrossMatrix[0].x + CrossMatrix[0].y;
	image2.g = image1.g * CrossMatrix[1].x + CrossMatrix[1].y;
	image2.b = image1.b * CrossMatrix[2].x + CrossMatrix[2].y;
	color.rgb = lerp(image1, image2, CrossAmount);
	
	return color;
}


technique Cross
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = CrossPass;
	}
}