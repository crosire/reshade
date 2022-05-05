// ++++++++++++++++++++++++++++++++++++++++++++++++++++++
// *** PPFX Godrays from the Post-Processing Suite 1.03.29 for ReShade
// *** SHADER AUTHOR: Pascal Matthäus ( Euda )
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++
// DEV_NOTES
//+++++++++++++++++++++++++++++
// Updated for compatibility with ReShade 4 and isolated by Marot Satil.

#include "ReShade.fxh"

//+++++++++++++++++++++++++++++
// CUSTOM PARAMETERS
//+++++++++++++++++++++++++++++

// ** GODRAYS **
uniform int pGodraysSampleAmount <
    ui_label = "Sample Amount";
    ui_tooltip = "Effectively the ray's resolution. Low values may look coarse but yield a higher framerate.";
    ui_type = "slider";
    ui_min = 8;
    ui_max = 250;
    ui_step = 1;
> = 64;

uniform float2 pGodraysSource <
    ui_label = "Light Source";
    ui_tooltip = "The vanishing point of the godrays in screen-space. 0.500,0.500 is the middle of your screen.";
    ui_type = "slider";
    ui_min = -0.5;
    ui_max = 1.5;
    ui_step = 0.001;
> = float2(0.5, 0.4);

uniform float pGodraysExposure <
    ui_label = "Exposure";
    ui_tooltip = "Contribution exposure of each single light patch to the final ray. 0.100 should generally be enough.";
    ui_type = "slider";
    ui_min = 0.01;
    ui_max = 1.0;
    ui_step = 0.01;
> = 0.1;

uniform float pGodraysFreq <
    ui_label = "Frequency";
    ui_tooltip = "Higher values result in a higher density of the single rays. '1.000' leads to rays that'll always cover the whole screen. Balance between falloff, samples and this value.";
    ui_type = "slider";
    ui_min = 1.0;
    ui_max = 10.0;
    ui_step = 0.001;
> = 1.2;

uniform float pGodraysThreshold <
    ui_label = "Threshold";
    ui_tooltip = "Pixels darker than this value won't cast rays.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 0.65;

uniform float pGodraysFalloff <
    ui_label = "Falloff";
    ui_tooltip = "Lets the rays' brightness fade/falloff with their distance from the light source specified in 'Light Source'.";
    ui_type = "slider";
    ui_min = 1.0;
    ui_max = 2.0;
    ui_step = 0.001;
> = 1.06;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   TEXTURES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** ESSENTIALS ***
texture texColorGRA { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture texColorGRB { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture texDepth : DEPTH;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   SAMPLERS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** ESSENTIALS ***

sampler SamplerColorGRA
{
	Texture = texColorGRA;
	AddressU = BORDER;
	AddressV = BORDER;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
};

sampler SamplerColorGRB
{
	Texture = texColorGRB;
	AddressU = BORDER;
	AddressV = BORDER;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
};

sampler2D SamplerDepth
{
	Texture = texDepth;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   VARIABLES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static const float3 lumaCoeff = float3(0.2126f,0.7152f,0.0722f);
#define ZNEAR 0.3
#define ZFAR 50.0

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   STRUCTS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

struct VS_OUTPUT_POST
{
	float4 vpos : SV_Position;
	float2 txcoord : TEXCOORD0;
};

struct VS_INPUT_POST
{
	uint id : SV_VertexID;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   HELPERS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

float linearDepth(float2 txCoords)
{
	return (2.0*ZNEAR)/(ZFAR+ZNEAR-tex2D(SamplerDepth,txCoords).x*(ZFAR-ZNEAR));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   EFFECTS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** Godrays ***
	float4 FX_Godrays( float4 pxInput, float2 txCoords )
	{
		float2	stepSize = (txCoords-pGodraysSource) / (pGodraysSampleAmount*pGodraysFreq);
		float3	rayMask = 0.0;
		float	rayWeight = 1.0;
		float	finalWhitePoint = pxInput.w;
		
		[loop]
		for (int i=1;i<(int)pGodraysSampleAmount;i++)
		{
			rayMask += max(0.0, saturate(tex2Dlod(SamplerColorGRB, float4(txCoords-stepSize*(float)i, 0.0, 0.0)).xyz) - pGodraysThreshold) * rayWeight * pGodraysExposure;
			finalWhitePoint += rayWeight * pGodraysExposure;
			rayWeight /= pGodraysFalloff;
		}
		
		rayMask.xyz = dot(rayMask.xyz,lumaCoeff.xyz) / (finalWhitePoint-pGodraysThreshold);
		return float4(pxInput.xyz+rayMask.xyz,finalWhitePoint);
	}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   VERTEX-SHADERS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

VS_OUTPUT_POST VS_PostProcess(VS_INPUT_POST IN)
{
	VS_OUTPUT_POST OUT;
	OUT.txcoord.x = (IN.id == 2) ? 2.0 : 0.0;
	OUT.txcoord.y = (IN.id == 1) ? 2.0 : 0.0;
	OUT.vpos = float4(OUT.txcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	return OUT;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   PIXEL-SHADERS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** Shader Structure ***

// *** Further FX ***
float4 PS_LightFX(VS_OUTPUT_POST IN) : COLOR
{
	float2 pxCoord = IN.txcoord.xy;
	float4 res = tex2D(ReShade::BackBuffer,pxCoord);
	
	res = FX_Godrays(res,pxCoord.xy);
	
	return res;
}

float4 PS_ColorFX(VS_OUTPUT_POST IN) : COLOR
{
	float2 pxCoord = IN.txcoord.xy;
	float4 res = tex2D(SamplerColorGRA,pxCoord);

	return float4(res.xyz,1.0);
}

float4 PS_ImageFX(VS_OUTPUT_POST IN) : COLOR
{
	float2 pxCoord = IN.txcoord.xy;
	float4 res = tex2D(SamplerColorGRB,pxCoord);
	
	return float4(res.xyz,1.0);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   TECHNIQUES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

technique PPFX_Godrays < ui_label = "PPFX Godrays"; ui_tooltip = "Godrays | Lets bright areas cast rays on the screen."; >
{
	pass lightFX
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_LightFX;
		RenderTarget0 = texColorGRA;
	}
	
	pass colorFX
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_ColorFX;
		RenderTarget0 = texColorGRB;
	}
	
	pass imageFX
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_ImageFX;
	}
}