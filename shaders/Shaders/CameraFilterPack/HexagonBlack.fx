#include "ReShade.fxh"

uniform float Size <
	ui_type = "drag";
	ui_min = 0.2;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Size [Hexagon Black]";
	ui_tooltip = "Size of the Hexagons";
> = 1.0;

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

float hexDist(float2 a, float2 b){
	float2 p = abs(b-a);
	float s = 0.5;
	float c = 0.8660254;

	float diagDist = s*p.x + c*p.y;
	return max(diagDist, p.x)/c;
}

float2 nearestHex(float s, float2 st){
	float h = 0.5*s;
	float r = 0.8660254*s;
	float b = s + 2.0*h;
	float a = 2.0*r;
	float m = h/r;

	float2 sect = st/float2(2.0*r, h+s);
	float2 sectPxl = fmod(st, float2(2.0*r, h+s));

	float aSection = fmod(floor(sect.y), 2.0);

	float2 coord = floor(sect);
		if(aSection > 0.0){
			if(sectPxl.y < (h-sectPxl.x*m)){
				coord -= 1.0;
			}
			else if(sectPxl.y < (-h + sectPxl.x*m)){
			coord.y -= 1.0;
		}
	} else {
		if(sectPxl.x > r){
			if(sectPxl.y < (2.0*h - sectPxl.x * m)){
				coord.y -= 1.0;
			}
		} else {
			if(sectPxl.y < (sectPxl.x*m)){
				coord.y -= 1.0;
			} else {
				coord.x -= 1.0;
			}
		}
	}

	float xoff = fmod(coord.y, 2.0)*r;
	return float2(coord.x*2.0*r-xoff, coord.y*(h+s))+float2(r*2.0, s);
	
}

float4 PS_HexagonBlack(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 uv 		= texcoord.xy;
	float   s 		= Size * ReShade::ScreenSize.x/160.0;
	float2 nearest 	= nearestHex(s, texcoord.xy * ReShade::ScreenSize.xy);
	float4 texel 	= tex2D(ReShade::BackBuffer, nearest/ReShade::ScreenSize.xy);
	float dist 		= hexDist(texcoord.xy * ReShade::ScreenSize.xy, nearest);
	float luminance = (texel.r + texel.g + texel.b)/3.0;
	float interiorSize = s;
	float interior = 1.0 - smoothstep(interiorSize-1.0, interiorSize, dist);

	return float4(texel.rgb*interior, 1.0);	

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique HexagonBlack {
	pass HexagonBlack {
		VertexShader=PostProcessVS;
		PixelShader=PS_HexagonBlack;
	}
}