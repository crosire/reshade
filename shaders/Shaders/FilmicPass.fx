/**
 * FilmicPass
 *
 * Applies some common color adjustments to mimic a more cinema-like look.
 */

#include "ReShadeUI.fxh"

uniform float Strength < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.05; ui_max = 1.5;
	ui_toolip = "Strength of the color curve altering";
> = 0.85;

uniform float Fade < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 0.6;
	ui_tooltip = "Decreases contrast to imitate faded image";
> = 0.4;
uniform float Contrast < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
> = 1.0;
uniform float Linearization < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.5; ui_max = 2.0;
> = 0.5;
uniform float Bleach < __UNIFORM_SLIDER_FLOAT1
	ui_min = -0.5; ui_max = 1.0;
	ui_tooltip = "More bleach means more contrasted and less colorful image";
> = 0.0;
uniform float Saturation < __UNIFORM_SLIDER_FLOAT1
	ui_min = -1.0; ui_max = 1.0;
> = -0.15;

uniform float RedCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;
uniform float GreenCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;
uniform float BlueCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;
uniform float BaseCurve < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.5;

uniform float BaseGamma < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.7; ui_max = 2.0;
	ui_tooltip = "Gamma Curve";
> = 1.0;
uniform float EffectGamma < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 0.65;
uniform float EffectGammaR < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;
uniform float EffectGammaG < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;
uniform float EffectGammaB < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
> = 1.0;

uniform float3 LumCoeff <
> = float3(0.212656, 0.715158, 0.072186);

#include "ReShade.fxh"

float3 FilmPass(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	float3 B = tex2D(ReShade::BackBuffer, texcoord).rgb;	
	float3 G = B;
	float3 H = 0.01;
 
	B = saturate(B);
	B = pow(B, Linearization);
	B = lerp(H, B, Contrast);
 
	float A = dot(B.rgb, LumCoeff);
	float3 D = A;
 
	B = pow(abs(B), 1.0 / BaseGamma);
 
	float a = RedCurve;
	float b = GreenCurve;
	float c = BlueCurve;
	float d = BaseCurve;
 
	float y = 1.0 / (1.0 + exp(a / 2.0));
	float z = 1.0 / (1.0 + exp(b / 2.0));
	float w = 1.0 / (1.0 + exp(c / 2.0));
	float v = 1.0 / (1.0 + exp(d / 2.0));
 
	float3 C = B;
 
	D.r = (1.0 / (1.0 + exp(-a * (D.r - 0.5))) - y) / (1.0 - 2.0 * y);
	D.g = (1.0 / (1.0 + exp(-b * (D.g - 0.5))) - z) / (1.0 - 2.0 * z);
	D.b = (1.0 / (1.0 + exp(-c * (D.b - 0.5))) - w) / (1.0 - 2.0 * w);
 
	D = pow(abs(D), 1.0 / EffectGamma);
 
	float3 Di = 1.0 - D;
 
	D = lerp(D, Di, Bleach);
 
	D.r = pow(abs(D.r), 1.0 / EffectGammaR);
	D.g = pow(abs(D.g), 1.0 / EffectGammaG);
	D.b = pow(abs(D.b), 1.0 / EffectGammaB);
 
	if (D.r < 0.5)
		C.r = (2.0 * D.r - 1.0) * (B.r - B.r * B.r) + B.r;
	else
		C.r = (2.0 * D.r - 1.0) * (sqrt(B.r) - B.r) + B.r;
 
	if (D.g < 0.5)
		C.g = (2.0 * D.g - 1.0) * (B.g - B.g * B.g) + B.g;
	else
		C.g = (2.0 * D.g - 1.0) * (sqrt(B.g) - B.g) + B.g;

	if (D.b < 0.5)
		C.b = (2.0 * D.b - 1.0) * (B.b - B.b * B.b) + B.b;
	else
		C.b = (2.0 * D.b - 1.0) * (sqrt(B.b) - B.b) + B.b;
 
	float3 F = lerp(B, C, Strength);
 
	F = (1.0 / (1.0 + exp(-d * (F - 0.5))) - v) / (1.0 - 2.0 * v);
 
	float r2R = 1.0 - Saturation;
	float g2R = 0.0 + Saturation;
	float b2R = 0.0 + Saturation;
 
	float r2G = 0.0 + Saturation;
	float g2G = (1.0 - Fade) - Saturation;
	float b2G = (0.0 + Fade) + Saturation;
 
	float r2B = 0.0 + Saturation;
	float g2B = (0.0 + Fade) + Saturation;
	float b2B = (1.0 - Fade) - Saturation;
 
	float3 iF = F;
 
	F.r = (iF.r * r2R + iF.g * g2R + iF.b * b2R);
	F.g = (iF.r * r2G + iF.g * g2G + iF.b * b2G);
	F.b = (iF.r * r2B + iF.g * g2B + iF.b * b2B);
 
	float N = dot(F.rgb, LumCoeff);
	float3 Cn = F;
 
	if (N < 0.5)
		Cn = (2.0 * N - 1.0) * (F - F * F) + F;
	else
		Cn = (2.0 * N - 1.0) * (sqrt(F) - F) + F;
 
	Cn = pow(max(Cn,0), 1.0 / Linearization);
 
	float3 Fn = lerp(B, Cn, Strength);
	return Fn;
}

technique FilmicPass
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FilmPass;
	}
}
