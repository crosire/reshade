
//Gaussian Bloom by Ioxa
//Version 1.0 for ReShade 3.0

//Settings
#if !defined GaussBloomTexScale
	#define GaussBloomTexScale 0.5
#endif

uniform int GaussianBloomRadius
<
	ui_type = "drag";
	ui_min = 0; ui_max = 4;
	ui_tooltip = "Adjustment for the bloom radius. Values greater than 1.00 will increase the radius.";
> = 2;

uniform float GaussianBloomOffset
<
	ui_type = "drag";
	ui_min = 1.00; ui_max = 10.00;
	ui_tooltip = "Additional adjustment for the bloom radius. Values greater than 1.00 will increase the radius.";
	ui_step = 0.20;
> = 2.00;

uniform float Threshold
<
	ui_type = "drag";
	ui_min = 0.1; ui_max = 1.0;
	ui_tooltip = "";
> = 0.800;

uniform float3 BloomTint <
	ui_type = "color";
> = float3(1.0,1.0,1.0);

uniform float Exposure
<
	ui_type = "drag";
	ui_min = 1.000; ui_max = 20.000;
	ui_tooltip = "";
> = 3.50;

uniform float GaussianBloomSaturation
<
	ui_type = "drag";
	ui_min = 0.00; ui_max = 2.00;
	ui_tooltip = "";
> = 0.500;

uniform float DitherStrength
<
	ui_type = "drag";
	ui_min = 0.000; ui_max = 1.000;
	ui_tooltip = "";
> = 1.00;

uniform float GaussianBloomStrength
<
	ui_type = "drag";
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Adjusts the strength of the effect";
> = 0.30;

#include "ReShade.fxh"

texture GaussianBloomTex { Width = BUFFER_WIDTH*GaussBloomTexScale; Height = BUFFER_HEIGHT*GaussBloomTexScale; Format = RGBA8; };
texture GaussianBloomTex2 { Width = BUFFER_WIDTH*GaussBloomTexScale; Height = BUFFER_HEIGHT*GaussBloomTexScale; Format = RGBA8; };

sampler GaussianBloomSampler { Texture = GaussianBloomTex; AddressW = BORDER; AddressU = BORDER; AddressV = BORDER;};
sampler GaussianBloomSampler2 { Texture = GaussianBloomTex2; AddressW = BORDER; AddressU = BORDER; AddressV = BORDER;};

float3 GaussianBloomFinal(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{

	float3 blur = tex2D(GaussianBloomSampler, texcoord / GaussianBloomOffset).rgb;
		
	blur.rgb = lerp(dot(blur.rgb,0.333333),blur.rgb,GaussianBloomSaturation);
	
	if(DitherStrength)
	{
	float sine = sin(dot(texcoord, float2(12.9898,78.233)));
	float noise = frac(sine * 43758.5453 + texcoord.x) * 0.012 - 0.006;
	blur.rgb += (float3(-noise, noise, -noise)*DitherStrength);
	}
	
	float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;
	orig = lerp(orig, (1.0 - ((1.0 - orig) * (1.0 - blur))), GaussianBloomStrength);
	
	return saturate(orig);
}

float3 GaussianBloomBrightPass(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	
	float3 color = tex2D( ReShade::BackBuffer, texcoord * GaussianBloomOffset).rgb;
	
	color.rgb *= ( 1.0f + ( color.rgb / ( Threshold * Threshold )) );
	color.rgb *= Exposure;
    color.rgb -= (5.0f);

    color.rgb = max( color.rgb, 0.0f );

    //color.rgb /= ( 10.0 + color.rgb );

	color.rgb *= BloomTint;
		
	return color;
}

float3 GaussianBloom1(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{

float3 blur = tex2D(GaussianBloomSampler, texcoord).rgb;
	
if(GaussianBloomRadius == 0)	
{
	float offset[4] = { 0.0, 1.1824255238, 3.0293122308, 5.0040701377 };
	float weight[4] = { 0.39894, 0.2959599993, 0.0045656525, 0.00000149278686458842 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 4; ++i)
	{
		blur += tex2D(GaussianBloomSampler, texcoord + float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler, texcoord - float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
	}
}	

if(GaussianBloomRadius == 1)	
{
	float offset[6] = { 0.0, 1.4584295168, 3.40398480678, 5.3518057801, 7.302940716, 9.2581597095 };
	float weight[6] = { 0.13298, 0.23227575, 0.1353261595, 0.0511557427, 0.01253922, 0.0019913644 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 6; ++i)
	{
		blur += tex2D(GaussianBloomSampler, texcoord + float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler, texcoord - float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
	}
}	

if(GaussianBloomRadius == 2)	
{
	float offset[11] = { 0.0, 1.4895848401, 3.4757135714, 5.4618796741, 7.4481042327, 9.4344079746, 11.420811147, 13.4073334, 15.3939936778, 17.3808101174, 19.3677999584 };
	float weight[11] = { 0.06649, 0.1284697563, 0.111918249, 0.0873132676, 0.0610011113, 0.0381655709, 0.0213835661, 0.0107290241, 0.0048206869, 0.0019396469, 0.0006988718 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 11; ++i)
	{
		blur += tex2D(GaussianBloomSampler, texcoord + float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler, texcoord - float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
	}
}	

if(GaussianBloomRadius == 3)	
{
	float offset[15] = { 0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4401038149, 21.43402885, 23.4279736431, 25.4219399344, 27.4159294386 };
	float weight[15] = { 0.0443266667, 0.0872994708, 0.0820892038, 0.0734818355, 0.0626171681, 0.0507956191, 0.0392263968, 0.0288369812, 0.0201808877, 0.0134446557, 0.0085266392, 0.0051478359, 0.0029586248, 0.0016187257, 0.0008430913 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 15; ++i)
	{
		blur += tex2D(GaussianBloomSampler, texcoord + float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler, texcoord - float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
	}
}

if(GaussianBloomRadius == 4)	
{
	float offset[18] = { 0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4661974725, 21.4627427973, 23.4592916956, 25.455844494, 27.4524015179, 29.4489630909, 31.445529535, 33.4421011704 };
	float weight[18] = { 0.033245, 0.0659162217, 0.0636705814, 0.0598194658, 0.0546642566, 0.0485871646, 0.0420045997, 0.0353207015, 0.0288880982, 0.0229808311, 0.0177815511, 0.013382297, 0.0097960001, 0.0069746748, 0.0048301008, 0.0032534598, 0.0021315311, 0.0013582974 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 18; ++i)
	{
		blur += tex2D(GaussianBloomSampler, texcoord + float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler, texcoord - float2(0.0, offset[i] * ReShade::PixelSize.y)).rgb * weight[i];
	}
}	

	return (blur);
}

float3 GaussianBloom2(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{

float3 blur = tex2D(GaussianBloomSampler2, texcoord).rgb;

if(GaussianBloomRadius == 1)	
{
	float offset[6] = { 0.0, 1.4584295168, 3.40398480678, 5.3518057801, 7.302940716, 9.2581597095 };
	float weight[6] = { 0.13298, 0.23227575, 0.1353261595, 0.0511557427, 0.01253922, 0.0019913644 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 6; ++i)
	{
		blur += tex2D(GaussianBloomSampler2, texcoord + float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler2, texcoord - float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
	}
}	

if(GaussianBloomRadius == 2)	
{
	float offset[11] = { 0.0, 1.4895848401, 3.4757135714, 5.4618796741, 7.4481042327, 9.4344079746, 11.420811147, 13.4073334, 15.3939936778, 17.3808101174, 19.3677999584 };
	float weight[11] = { 0.06649, 0.1284697563, 0.111918249, 0.0873132676, 0.0610011113, 0.0381655709, 0.0213835661, 0.0107290241, 0.0048206869, 0.0019396469, 0.0006988718 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 11; ++i)
	{
		blur += tex2D(GaussianBloomSampler2, texcoord + float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler2, texcoord - float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
	}
}	

if(GaussianBloomRadius == 3)	
{
	float offset[15] = { 0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4401038149, 21.43402885, 23.4279736431, 25.4219399344, 27.4159294386 };
	float weight[15] = { 0.0443266667, 0.0872994708, 0.0820892038, 0.0734818355, 0.0626171681, 0.0507956191, 0.0392263968, 0.0288369812, 0.0201808877, 0.0134446557, 0.0085266392, 0.0051478359, 0.0029586248, 0.0016187257, 0.0008430913 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 15; ++i)
	{
		blur += tex2D(GaussianBloomSampler2, texcoord + float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler2, texcoord - float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
	}
}

if(GaussianBloomRadius == 4)	
{
	float offset[18] = { 0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4661974725, 21.4627427973, 23.4592916956, 25.455844494, 27.4524015179, 29.4489630909, 31.445529535, 33.4421011704 };
	float weight[18] = { 0.033245, 0.0659162217, 0.0636705814, 0.0598194658, 0.0546642566, 0.0485871646, 0.0420045997, 0.0353207015, 0.0288880982, 0.0229808311, 0.0177815511, 0.013382297, 0.0097960001, 0.0069746748, 0.0048301008, 0.0032534598, 0.0021315311, 0.0013582974 };
	
	blur *= weight[0];
	
	[loop]
	for(int i = 1; i < 18; ++i)
	{
		blur += tex2D(GaussianBloomSampler2, texcoord + float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
		blur += tex2D(GaussianBloomSampler2, texcoord - float2( offset[i] * ReShade::PixelSize.x,0.0)).rgb * weight[i];
	}
}	
	return (blur);
}

technique GaussianBloom
{
	pass BrightPass
	{
		VertexShader = PostProcessVS;
		PixelShader = GaussianBloomBrightPass;
		RenderTarget = GaussianBloomTex;
	}
		
	pass Bloom1
	{
		VertexShader = PostProcessVS;
		PixelShader = GaussianBloom1;
		RenderTarget = GaussianBloomTex2;
	}
	
	pass Bloom2
	{
		VertexShader = PostProcessVS;
		PixelShader = GaussianBloom2;
		RenderTarget = GaussianBloomTex;
	}

	pass Bloom
	{
		VertexShader = PostProcessVS;
		PixelShader = GaussianBloomFinal;
	}

}
