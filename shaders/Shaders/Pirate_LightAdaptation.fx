//===================================================================================================================
#include 	"Pirate_Global.fxh" //That evil corporation~
#include	"Pirate_Lumachroma.fxh"
//===================================================================================================================
uniform float LIGHTADAPT_SPEED <
	ui_label = "Light Adaptation - Speed";
	ui_tooltip = "Rate of adaptation per frame, 1.0 = 100% per frame.";
	> = 0.004;
uniform float LIGHTADAPT_CURVE <
	ui_label = "Light Adaptation - midtones";
	ui_tooltip = "Affects midtones based on average luma of the screen.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 10.0;
	> = 0.1;
uniform float LIGHTADAPT_LEVELS <
	ui_label = "Light Adaptation - Level";
	ui_tooltip = "Affects black and white ranges, 1.0 is the same as an equalizer effect on image editors.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.1;
uniform float LIGHTADAPT_EXPOSURE <
	ui_label = "Light Adaptation - Exposure";
	ui_tooltip = "Controls auto exposure of the image.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.1;
uniform float LIGHTADAPT_SPREAD <
	ui_label = "Light Adaptation - Detector Spread";
	ui_tooltip = "If you're getting a consistent min or max light values, the detection might be hitting an UI pixel, lower this value slightly until the debug view starts moving normally.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 1.0;
uniform bool LIGHTADAPT_DEBUG <
	ui_label = "Light Adaptation - Debug";
	ui_tooltip = "Shows debug bars. Top bar is the original curve, bottom is adapted curve. Red values are bellow average, green above average.";
	> = false;

//===================================================================================================================

texture2D	TexLightAdapt {Width = 1; Height = 1; Format = RGBA32F;};
sampler2D	SamplerLightAdapt {Texture = TexLightAdapt; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
texture2D	TexLightAdaptLF {Width = 1; Height = 1; Format = RGBA32F;};
sampler2D	SamplerLightAdaptLF {Texture = TexLightAdaptLF; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

//===================================================================================================================

float4 PS_LightAdaptGather(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	const float2 taps[13]=
	{
		float2(0.0, 0.0),
		float2(-0.4, -0.4),
		float2(-0.4, 0.4),
		float2(0.4, -0.4),
		float2(0.4, 0.4),
		float2(-0.4, 0.0),
		float2(0.4, 0.0),
		float2(0.0, -0.4),
		float2(0.0, 0.4),
		float2(0.25, 0.25),
		float2(-0.25, 0.25),
		float2(0.25, -0.25),
		float2(-0.25,- 0.25)
	};
	
	float maxlight, minlight, averagelight;
	minlight = 1.0;
	
	for(int i=0; i < 13; i++)
	{
		float tap = LumaChroma(tex2D(SamplerColor, 0.5 + taps[i] * LIGHTADAPT_SPREAD)).w;
		maxlight = max(maxlight, tap);
		minlight = min(minlight, tap);
		averagelight += tap;
	}
	averagelight /= 13;
	averagelight -= (maxlight + minlight) / 2;
	float4 lastframe = tex2D(SamplerLightAdaptLF, float2(0.5, 0.5));
	minlight = lerp(lastframe.r, minlight, LIGHTADAPT_SPEED);
	maxlight = lerp(lastframe.g, maxlight, LIGHTADAPT_SPEED);
	averagelight = lerp(lastframe.b, averagelight, LIGHTADAPT_SPEED);
	return float4(minlight, maxlight, averagelight, 1.0);
}

float4 PS_LightAdapt(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	float4 gather = tex2D(SamplerLightAdapt, float2(0.5, 0.5)); // Min, max, average
	float flatav = 0.5 - (gather.r + gather.g) / 2;
	flatav *= LIGHTADAPT_EXPOSURE;
	if (LIGHTADAPT_DEBUG) {
		float b = gather.b + (gather.r + gather.g) / 2;
		[branch] if (texcoord.y < 0.01) {
			if (texcoord.x <= gather.r) {
				return float4(0.0, 0.0, 0.0, 1.0);
			}
			if (texcoord.x <= b) {
				return float4(1.0, 0.0, 0.0, 1.0);
			}
			if (texcoord.x <= gather.g) {
				return float4(0.0, 1.0, 0.0, 1.0);
			}
			return float4(0.0, 0.0, 0.0, 1.0);
		}
		[branch] if (texcoord.y < 0.02) {
			if (texcoord.x <= lerp(gather.r, 0.0, LIGHTADAPT_LEVELS) + flatav) {
				return float4(0.0, 0.0, 0.0, 1.0);
			}
			if (texcoord.x <= lerp(b, pow(saturate(b), 1.0 + gather.b), LIGHTADAPT_CURVE) + flatav) {
				return float4(1.0, 0.0, 0.0, 1.0);
			}
			if (texcoord.x <= lerp(gather.g, 1.0, LIGHTADAPT_LEVELS) + flatav) {
				return float4(0.0, 1.0, 0.0, 1.0);
			}
			return float4(0.0, 0.0, 0.0, 1.0);
		}
	}
	float4 lc = LumaChroma(ret);
	lc.w += flatav;
	lc.w = lerp(lc.w, smoothstep(gather.r, gather.g, lc.w), LIGHTADAPT_LEVELS);
	//lc.w = lerp(lc.w, pow(saturate(lc.w), 1.0 + gather.b), LIGHTADAPT_CURVE);
	lc.w = pow(saturate(lc.w), 1.0 + gather.b * LIGHTADAPT_CURVE);
	return float4(lc.rgb * lc.w, 1.0);
}

float4 PS_LightAdaptLF(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return tex2D(SamplerLightAdapt, float2(0.5, 0.5));
}

//===================================================================================================================
//===================================================================================================================

technique Pirate_LightAdaptation
{
	pass LightAdaptationGather
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_LightAdaptGather;
		RenderTarget = TexLightAdapt;
	}
	pass LightAdaptationEffect
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_LightAdapt;
	}
	pass LightAdaptationLF
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_LightAdaptLF;
		RenderTarget = TexLightAdaptLF;
	}
}