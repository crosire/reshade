uniform float _ReduceResolution	<
	ui_type= "drag";
	ui_min = 1.0; ui_max = 6.0;
	ui_label = "Resolution Scale (Dithering Megapack)";
> = 4.0;

#define _ScreenParams float2(BUFFER_WIDTH, BUFFER_HEIGHT)

#ifndef Palette_Texture_Width
	#define Palette_Texture_Width		8.0		//X size of Palette texture [0 to X]
#endif

#ifndef Pattern_Texture_Size
	#define Pattern_Texture_Size		64		//X size of Palette texture [0 to X]
#endif

#ifndef texPalName
	#define texPalName "teletext.png"
#endif

#ifndef texPatternName
	#define texPatternName "pattern1.png"
#endif
	
#include "ReShade.fxh"
	
texture2D texPalette  <string source = "Dithering/Palettes/" texPalName;> { Width = Palette_Texture_Width;Height = 1;Format = RGBA8;};
texture2D texPattern  <string source = "Dithering/Patterns/" texPatternName;> { Width = Pattern_Texture_Size;Height = Pattern_Texture_Size;Format = RGBA8;};

sampler2D _Palette
{
	Texture = texPalette;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler2D _PatternTex
{
	Texture = texPattern;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Repeat;
	AddressV = Repeat;
};

float lengthlum(float3 A, float3 B)
{
	float lumA = dot(A,float3(0.212656, 0.715158, 0.072186));
	float lumB = dot(B,float3(0.212656, 0.715158, 0.072186));
	return abs(lumA - lumB);
}

float3 GetClosest(float3 ref)
{
	float3 old = float3 (100.0*255.0, 100.0*255.0, 100.0*255.0);

	for(float i=1; i<=Palette_Texture_Width; i++)
	{
		float3 pal = tex2D(_Palette, float2(i / Palette_Texture_Width, 0.5)).xyz;
		old = lerp(pal.xyz, old.xyz, step(length(old-ref), length(pal-ref)));	
	}
	return old;
}

float bayer8x8(float2 uv)
{
	return tex2D(_PatternTex, uv/(_ReduceResolution*8.)).r;
}

float4 PS_Dither(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	
	float2 reducedUv = texcoord;

	float3 quantize = float3(1./16., 1./16., 1./16.);

	reducedUv.x = floor((reducedUv.x*_ScreenParams.x)/_ReduceResolution)*(_ReduceResolution/_ScreenParams.x);
	reducedUv.y = floor((reducedUv.y*_ScreenParams.y)/_ReduceResolution)*(_ReduceResolution/_ScreenParams.y);

	float4 col = tex2D(ReShade::BackBuffer, reducedUv);

	float4 originalCol = tex2D(ReShade::BackBuffer, texcoord.xy);

	col += float4((bayer8x8(texcoord*_ScreenParams)-.5)*(quantize), 1.0);

	col = float4(GetClosest(col.xyz), 1.0);

	return col;

}

technique Palette
{
	pass Dither
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Dither;
	}
}