//PS1 Hardware Dithering & Color Precision Truncation Function
//by ompu co | Sam Blye (c) 2020

uniform bool useDither <
	ui_type = "boolean";
	ui_label = "Use PS1 Dithering";
> = 0;

#include "ReShade.fxh"

//PS1 dither table from PSYDEV SDK documentation
static const int4x4 psx_dither_table = float4x4
(
	0,      8,     2,      10,
	12,     4,     14,     6,
	3,      11,    1,      9,
	15,     7,     13,     5
);

texture target0_psx
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};

sampler psx0 { Texture = target0_psx; };

float3 DitherCrunch(float3 col, int2 p){
	col*=255.0; //extrapolate 16bit color float to 16bit integer space
	if(useDither==1) 
	{
		int dither = psx_dither_table[p.x % 4][p.y % 4];
		col += (dither / 2.0 - 4.0); //dithering process as described in PSYDEV SDK documentation
	}
	col = lerp((uint3(col) & 0xf8), 0xf8, step(0xf8,col));
	//truncate to 5bpc precision via bitwise AND operator, and limit value max to prevent wrapping.
	//PS1 colors in default color mode have a maximum integer value of 248 (0xf8)
	col /= 255; //bring color back to floating point number space
	return col;
}

float4 PS_PSXDitherStart(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	return tex2D(ReShade::BackBuffer,texcoord);
}

float4 PS_PSXDitherBlit(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 samp = tex2D(psx0,texcoord);
	int2 scp = int2(texcoord * tex2Dsize(psx0,0));
	float3 col = DitherCrunch(samp,scp);
	return float4(col.rgb,1.0);
}

technique PSXDither_OMPUCO
{	
	pass p0
	{	
		RenderTarget = target0_psx;
		VertexShader = PostProcessVS;
		PixelShader = PS_PSXDitherStart;
	}
	
	pass p1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_PSXDitherBlit;
	}
}