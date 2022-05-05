#include "ReShade.fxh"

#ifndef DB_PALETTE_NAME
	#define DB_PALETTE_NAME "Famiclone.png"
#endif

#ifndef DB_BAYER_TEX
	#define DB_BAYER_TEX "bayer8x8.png"
#endif

uniform bool _CSIZE <
	ui_type = "boolean";
	ui_label = "Custom Screen Size [Darkbringer Retro]";
> = true;

uniform float screenX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_step = 1.0;
	ui_label = "Screen Width [Darkbringer Retro]";
> = 160.0;

uniform float screenY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1.0;
	ui_label = "Screen Height [Darkbringer Retro]";
> = 200.0;

uniform bool _ARC <
	ui_type = "boolean";
	ui_label = "Aspect Ratio Bars [Darkbringer Retro]";
> = true;

uniform float screenStretchX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Stretch Width [Darkbringer Retro]";
> = 2.0;

uniform float screenStretchY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Screen Stretch Height [Darkbringer Retro]";
> = 1.0;

uniform bool _BIT1 <
	ui_type = "boolean";
	ui_label = "1-Bit Mode [Darkbringer Retro]";
> = false;

uniform bool _DITHER <
	ui_type = "boolean";
	ui_label = "Enable Dithering [Darkbringer Retro]";
> = true;

uniform float _ColorBleed <
	ui_type = "drag";
	ui_min = -1.5;
	ui_max = 1.5;
	ui_step = 0.001;
	ui_label = "Dither Bleed [Darkbringer Retro]";
> = 0.25;

uniform float _ColorShift <
	ui_type = "drag";
	ui_min = -1.5;
	ui_max = 1.5;
	ui_step = 0.001;
	ui_label = "Dither Brightness [Darkbringer Retro]";
> = 0.29;

/*
uniform bool _PALETTE <
	ui_type = "boolean";
	ui_label = "Enable Palette [Darkbringer Retro]";
> = true;
*/

uniform bool _HORLINES <
	ui_type = "boolean";
	ui_label = "Horizontal Lines [Darkbringer Retro]";
> = false;

uniform bool _VERTLINES <
	ui_type = "boolean";
	ui_label = "Vertical Lines [Darkbringer Retro]";
> = false;

uniform float _LinesMult <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.5;
	ui_step = 0.001;
	ui_label = "Line Shade [Darkbringer Retro]";
> = 0.8;

static const float _Dim = 16.0;
static const float _ScaleRG = 0.05859375;
static const float _Offset = 0.001953125;

texture texDBPAL < source = "DB/" DB_PALETTE_NAME; > { Width = 256; Height = 16; Format = RGBA32F; };
sampler2D _Lut2D { Texture = texDBPAL; MinFilter=POINT; MagFilter=POINT; };
texture texDBBAYER  < source = "DB/" DB_BAYER_TEX; > { Width = 8; Height = 8; Format = RGBA8; };
sampler2D _BayerTex { Texture = texDBBAYER; MinFilter=POINT; MagFilter=POINT; };

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

float closestPerm( int x, int y)
{			

	float resDither = floor( (tex2Dgrad(_BayerTex, float2(x / 8.0, y / 8.0), ddx(8), ddy(8)).r)*255.0);

	float limit = 0.0;
	if (x < 8)
	{
		limit = (resDither + 1) / 64.0;
	}
	return limit;
}

float closestPermLum(int x, int y, float c0, float lumShift)
{
	float limit = closestPerm(x, y);

	if (c0 < limit) return 1.0-lumShift;
	return 1.0;
}

float find_closest(int x, int y, float c0)
{
	float limit = closestPerm(x, y);

	if (c0 < limit)
	return 0.0;
	return 1.0;
}

float2 Get2DUV(float4 c)
{
	//c.g = 1 - saturate(c.g);

	float b = floor(c.b * _Dim * _Dim);
	float by = floor(b / _Dim);
	float bx = floor(b - by * _Dim);

	float2 uv = c.rg * _ScaleRG + _Offset;
	uv += float2(bx, by) / _Dim;

	return uv;
}

float4 PS_Darkbringer(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 _MainTex_TexelSize = float4(1.0/ReShade::ScreenSize.x, 1.0/ReShade::ScreenSize.y, ReShade::ScreenSize.x, ReShade::ScreenSize.y);
	float4 _Lut2D_TexelSize = float4(1.0/256, 1.0/16, 256, 16);
	
	float2 newScreenSize = float2(screenX,screenY);
	float2 screenStretching = float2(screenStretchX,screenStretchY);
	
	float2 xy = floor(uv*_MainTex_TexelSize.zw);
	if (_CSIZE) {
		if (_ARC) 
		{
			float2 cSC = uv*_MainTex_TexelSize.zw;
			float nWidth = (newScreenSize.x*screenStretching.x / newScreenSize.y*screenStretching.y)*_MainTex_TexelSize.w;
			float horDistance = (_MainTex_TexelSize.z - nWidth) / 2;
			if (_ARC && (horDistance > 0 &&
			(cSC.x<horDistance || cSC.x>(_MainTex_TexelSize.z - horDistance))))
			return float4(0, 0, 0, 0);
		}
					
		float dnm = (((_MainTex_TexelSize.z) / (_MainTex_TexelSize.w)) / screenStretching.x) / ((newScreenSize.x / newScreenSize.y));
		if ((((_MainTex_TexelSize.z) / (_MainTex_TexelSize.w)) / screenStretching.x) != ((newScreenSize.x / newScreenSize.y)))
		newScreenSize.x = floor(newScreenSize.x * dnm);
		xy = floor(uv * newScreenSize.xy);

		uv.x = xy.x / newScreenSize.x; //Floating point precision error avoidance.
		uv.y = xy.y / newScreenSize.y;

	} else {
		newScreenSize.xy = _MainTex_TexelSize.zw;
		screenStretching.xy = 1;
	}

	if (_BIT1)
	{
		float4 lum = float4(0.299, 0.587, 0.114, 0.0);
		float grayscale = dot(tex2D(ReShade::BackBuffer, uv), lum);
		int x = floor(fmod(xy.x, 8.0));
		int y = floor(fmod(xy.y, 8.0));
		float bit1 = find_closest(x, y, grayscale);
		return float4(bit1, bit1, bit1, 1);
	}

	float3 rgb = tex2D(ReShade::BackBuffer, uv).rgb;
						
	if (_DITHER)
	{
		float4 lum = float4(0.299, 0.587, 0.114, 0.0);
		float grayscale = dot(tex2D(ReShade::BackBuffer, uv), lum);

		int x = floor(fmod(xy.x, 8.0));
		int y = floor(fmod(xy.y, 8.0));
		rgb = rgb*(closestPermLum(x, y, grayscale, _ColorBleed) + _ColorShift);
	}
	if (_HORLINES)
	{
		if (fmod(floor(uv.y / (1 / (newScreenSize.y*screenStretching.y))), 2) != 0)
		rgb *= (_LinesMult);
	}
	if (_VERTLINES)
	{
		if (fmod(floor(uv.x / (1 / (newScreenSize.x*screenStretching.x))), 2) != 0)
		rgb *= (_LinesMult);
	}

	/*
	if (_PALETTE)
	{
		rgb = clamp(rgb, 0, 1);
		float bScale = (_Lut2D_TexelSize.w - 1.0) / (1.0*_Lut2D_TexelSize.w);
		float bOffset = 1.0 / (2.0 * _Lut2D_TexelSize.w);
		float3 uvRGB = clamp(rgb.rgb * bScale + bOffset, 0, 1);
		rgb = tex2Dgrad(_Lut2D, Get2DUV(float4(rgb, 1)), ddx(256), ddy(16)).rgb;
	}
	*/
				
	return  float4(rgb, 1.0);

}

technique Darkbringer
{
	pass Darkbringer_Retro
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Darkbringer;
	}
}