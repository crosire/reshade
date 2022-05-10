#include "ReShade.fxh"

/*
   Downsamples to 16 colors using quantization.
   Uses 4-bit RGBI values for an "EGA"/"Tandy" look
   
   Author: VileR
   License: public domain
*/

// 1.0 is the 'proper' value, 1.2 seems to give better results but brighter
// colors may clip.
#define color_enhance 1.2

sampler2D SourcePointSampler
{
	Texture = ReShade::BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

float3 nearest_rgbi (float3 original) {

	float3 rgbi_palette[16] = {
	  float3(0.0,     0.0,     0.0),
	  float3(0.0,     0.0,     0.66667),
	  float3(0.0,     0.66667, 0.0),
	  float3(0.0,     0.66667, 0.66667),
	  float3(0.66667, 0.0,     0.0),
	  float3(0.66667, 0.0,     0.66667),
	  float3(0.66667, 0.33333, 0.0),
	  float3(0.66667, 0.66667, 0.66667),
	  float3(0.33333, 0.33333, 0.33333),
	  float3(0.33333, 0.33333, 1.0),
	  float3(0.33333, 1.0,     0.33333),
	  float3(0.33333, 1.0,     1.0),
	  float3(1.0,     0.33333, 0.33333),
	  float3(1.0,     0.33333, 1.0),
	  float3(1.0,     1.0,     0.33333),
	  float3(1.0,     1.0,     1.0),
	};

  float dst;
  float min_dst = 2.0;
  int idx = 0;
  for (int i=0; i<16; i++) {
    dst = distance(original, rgbi_palette[i]);
    if (dst < min_dst) {
      min_dst = dst;
      idx = i;
    }
  }
  return rgbi_palette[idx];
}

float4 PS_EGA(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 fragcolor = tex2D(SourcePointSampler, texcoord).rgb;
	return float4(nearest_rgbi(fragcolor*color_enhance), 1);
}

technique EGAfilter
{
	pass EGAfilterPass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_EGA;
	}
}