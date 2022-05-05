#define _ScreenParams float2(BUFFER_WIDTH, BUFFER_HEIGHT)

#ifndef Pattern_Texture_Size
	#define Pattern_Texture_Size		64		//X size of Palette texture [0 to X]
#endif

#ifndef texPatternName
	#define texPatternName "pattern1.png"
#endif
	
#include "ReShade.fxh"

texture2D texPattern  <string source = "Dithering/Patterns/" texPatternName;> { Width = Pattern_Texture_Size;Height = Pattern_Texture_Size;Format = RGBA8;};

sampler2D _PatternTex
{
	Texture = texPattern;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Repeat;
	AddressV = Repeat;
};

float4 PS_SimpleDither(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	
	float4 col = tex2D(ReShade::BackBuffer, texcoord);
	col = step(tex2D(_PatternTex, _ScreenParams.xy*texcoord/8.), tex2D(ReShade::BackBuffer,texcoord));

	return col;

}

technique DitheringSimple
{
	pass SimpleDither
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_SimpleDither;
	}
}