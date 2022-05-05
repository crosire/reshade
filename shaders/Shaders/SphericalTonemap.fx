/*
		Coded by Gilcher Pascal aka Marty McFly
		Amateur port by Insomnia 
*/	

uniform float sphericalAmount <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_label = "Spherical amount";
	ui_tooltip = "Amount of spherical tonemapping applied...sort of";
> = 1.0;



#include "ReShade.fxh"

float3 SphericalPass(float4 position : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	
	float3 signedColor = color.rgb * 2.0 - 1.0;
	float3 sphericalColor = sqrt(1.0 - signedColor.rgb * signedColor.rgb);
	sphericalColor = sphericalColor * 0.5 + 0.5;
	sphericalColor *= color.rgb;
	color.rgb += sphericalColor.rgb * sphericalAmount;
	color.rgb *= 0.95;
	
	return color.rgb;
}


technique SphericalTonemap
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = SphericalPass;
	}
}