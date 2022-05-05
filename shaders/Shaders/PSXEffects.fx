#include "ReShade.fxh"

uniform int iResolutionFactor <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 40;
	ui_step = 1;
	ui_label = "Resolution Factor [PSX Effects]";
> = 1;

uniform int iColorDepth <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 255;
	ui_step = 1;
	ui_label = "Color Depth [PSX Effects]";
> = 32;

uniform int iSubtractFade <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 1000;
	ui_step = 1;
	ui_label = "Subtraction Fade [PSX Effects]";
> = 0;

uniform int iMaxDarkness <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 100;
	ui_step  = 1;
	ui_label = "Saturated Diffuse [PSX Effects]";
> = 20;

uniform float fFavorRed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Darken / Darks Favor [PSX Effects]";
> = 0.0;

uniform bool bUseScanlines <
	ui_type = "boolean";
	ui_label = "Use Scanlines [PSX Effects]";
> = false;

uniform bool bVertScanlines <
	ui_type = "boolean";
	ui_label = "Vertical Scanlines [PSX Effects]";
> = true;

uniform int iScanlinesIntensity <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 100;
	ui_step = 1;
	ui_label = "Scanlines Intensity [PSX Effects]";
> = 5;

uniform bool bUseDithering <
	ui_type = "boolean";
	ui_label = "Enable Dithering [PSX Effects]";
> = false;

uniform float fDitherThreshold <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.001;
	ui_label = "Dither Threshold [PSX Effects]";
> = 1.0;

uniform int iDitherIntensity <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 100;
	ui_step = 1;
	ui_label = "Dither Intensity [PSX Effects]";
> = 5;

uniform int iDitherTex <
	ui_type = "combo";
	ui_items = "2x2 \0 3x3 \0 4x4 \0 8x8 \0";
	ui_label = "Dither Texture [PSX Effects]";
> = 0;

uniform bool bUseOGDitheringSize <
	ui_type = "boolean";
	ui_label = "DEBUG - Use Original Dithering Pattern Size [PSX Effects]";
> = false;

uniform bool bUseGammaSpace <
	ui_type = "boolean";
	ui_label = "DEBUG - Use Gamma Space [PSX Effects]";
> = false;

uniform bool bUseTrueScreenRes <
	ui_type = "boolean";
	ui_label = "DEBUG - Use Screen Res [PSX Effects]";
> = false;

uniform int iVertexInaccuracy <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 100;
	ui_step = 1;
	ui_label = "DEBUG - Vertex Inaccuracy [PSX Effects]";
> = 30;

texture2D texPattern2  <string source = "PSXEffects/Dither2x2.png";> { Width = 2; Height = 2; Format = RGBA8;};
texture2D texPattern3  <string source = "PSXEffects/Dither3x3.png";> { Width = 3; Height = 3; Format = RGBA8;};
texture2D texPattern4  <string source = "PSXEffects/Dither4x4.png";> { Width = 4; Height = 4; Format = RGBA8;};
texture2D texPattern8  <string source = "PSXEffects/Dither8x8.png";> { Width = 8; Height = 8; Format = RGBA8;};

sampler2D _DitherTex
{
	Texture = texPattern8;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Repeat;
	AddressV = Repeat;
};

sampler2D _DitherTex2
{
	Texture = texPattern2;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = Repeat;
	AddressV = Repeat;
};

sampler2D _DitherTex3
{
	Texture = texPattern3;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = Repeat;
	AddressV = Repeat;
};

sampler2D _DitherTex4
{
	Texture = texPattern4;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = Repeat;
	AddressV = Repeat;
};

sampler2D _DitherTex8
{
	Texture = texPattern8;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Repeat;
	AddressV = Repeat;
};

float4 PixelSnap(float4 pos)
{
	float _VertexSnappingDetail = (float)iVertexInaccuracy / (float)iResolutionFactor;
	float2 hpc = ReShade::ScreenSize.xy * 0.75f;
	_VertexSnappingDetail /= 8;
	float2 pixelPos = round((pos.xy / pos.w) * hpc / _VertexSnappingDetail)*_VertexSnappingDetail;
	pos.xy = pixelPos / hpc * pos.w;
	return pos;
}

struct PSX_VSOUT
{
	float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

PSX_VSOUT VS_PSX(in uint id : SV_VertexID)
{
	PSX_VSOUT o;
	o.uv.x = (id == 2) ? 2.0 : 0.0;
    o.uv.y = (id == 1) ? 2.0 : 0.0;    
	o.pos = float4(o.uv.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);	
	//o.pos.xy = floor(o.pos.xy/o.pos.w*(1./0.015))*0.015*o.pos.w;
    o.pos = PixelSnap(o.pos);
	return o;
}

float LinearRgbToLuminance(float3 linearRgb)
{
    return dot(linearRgb, float3(0.2126729f,  0.7151522f, 0.0721750f));
}

float GammaToLinearSpaceExact(float value)
{
    if (value <= 0.04045F)
        return value / 12.92F;
    else if (value < 1.0F)
        return pow((value + 0.055F)/1.055F, 2.4F);
    else
        return pow(value, 2.2F);
}

float3 GammaToLinearSpace(float3 sRGB)
{
    // Approximate version from http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html?m=1
    //return sRGB * (sRGB * (sRGB * 0.305306011h + 0.682171111h) + 0.012522878h);

    // Precise version, useful for debugging.
    return float3(GammaToLinearSpaceExact(sRGB.r), GammaToLinearSpaceExact(sRGB.g), GammaToLinearSpaceExact(sRGB.b));
}

float4 PS_PSXEffectsPost(in PSX_VSOUT i) : SV_Target
{
	float _DarkMax = (float)iMaxDarkness / 100;
	float _SubtractFade = (float)iSubtractFade / 100;
	float _DitherIntensity = (float)iDitherIntensity / 100;
	float _DitherThreshold = fDitherThreshold;
	float _ColorDepth = iColorDepth;
	float _FavorRed = fFavorRed;
	float _Dithering = 0.0;
	
	if (bUseDithering){
		_Dithering = 1.0;
	} else {
		_Dithering = 0.0;
	}
	
	float _SLDirection = 0.0;
	
	if (bVertScanlines){
		_SLDirection = 1.0;
	} else {
		_SLDirection = 0.0;
	}
	
	float _Scanlines = 0.0;
	
	if (bUseScanlines){
		_Scanlines = 1.0;
	} else {
		_Scanlines = 0.0;
	}
	
	float _ScanlineIntensity = (float)iScanlinesIntensity / 100;
	
	float2 pixel_center = floor(i.pos.xy / iResolutionFactor) * iResolutionFactor + iResolutionFactor * 0.5;
	pixel_center /= ReShade::ScreenSize.xy;
	
	if (__RENDERER__ >= 0x10000){
		pixel_center.y = 1-pixel_center.y;
	}
	
	float4 col = tex2D(ReShade::BackBuffer, pixel_center);
	
	col.rgb = floor(col.rgb * _ColorDepth) / _ColorDepth;
	//Scanlines
	float sl = floor(i.uv.x*ReShade::ScreenSize.x % 2) * _SLDirection + floor(i.uv.y*ReShade::ScreenSize.y % 2) * (1 - _SLDirection);
	col.rgb += _Scanlines * sl * _ScanlineIntensity;
	
	float luma = LinearRgbToLuminance(col.rgb);
	
	if (bUseGammaSpace){
		luma = LinearRgbToLuminance(GammaToLinearSpace(saturate(col.rgb)));
	}
	
	float2 res = i.uv*ReShade::ScreenSize.xy - 0.5;
	
	float ditherSize = 6;
	
	if (iDitherTex <= 0){
		ditherSize = 2;
	} else if (iDitherTex == 1){
		ditherSize = 3;
	} else if (iDitherTex == 2){
		ditherSize = 4;
	} else if (iDitherTex == 3){
		ditherSize = 8;
	} else {
		ditherSize = 6;
	}
	
	float2 psx_uv = res/((float)iResolutionFactor*ditherSize);
	
	if (bUseOGDitheringSize){
		ditherSize = 6;
		psx_uv = i.uv*(ReShade::ScreenSize.x/ditherSize);
	}
	
	if (bUseTrueScreenRes){
		psx_uv = pixel_center*(ReShade::ScreenSize.x/ditherSize);
	}
	
	float dither = tex2D(_DitherTex, i.uv*(ReShade::ScreenSize.x/6)).a;
	
	if (iDitherTex <= 0){
		dither = tex2D(_DitherTex2, psx_uv).a;
	} else if (iDitherTex == 1){
		dither = tex2D(_DitherTex3, psx_uv).a;
	} else if (iDitherTex == 2){
		dither = tex2D(_DitherTex4, psx_uv).a;
	} else if (iDitherTex == 3){
		dither = tex2D(_DitherTex8, psx_uv).a;
	} else {
		dither = tex2D(_DitherTex, i.uv*(ReShade::ScreenSize.x/6)).a;
	}

	col.rgb += (luma < dither / _DitherThreshold ? _DitherIntensity : 0) * _Dithering;
	col.rgb -= (3 - col.rgb) * _SubtractFade;
	col.rgb -= _FavorRed * ((1 - col.rgb) * 0.25);
	col.r += _FavorRed * ((0.5 - col.rgb) * 0.1);

	return col;
}

technique PSXEffects
{
	pass PSXEffects_Post
	{
		VertexShader = VS_PSX;
		PixelShader = PS_PSXEffectsPost;
	}
}