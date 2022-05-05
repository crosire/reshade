#include "ReShade.fxh"

/*uniform float uBlurScale <
	ui_label = "Blur Scale";
	ui_tooltip = "Default: 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.01;
> = 1.0;*/

uniform float uVignetteScale <
	ui_label = "Vignette Scale";
	ui_tooltip = "Default: 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.001;
> = 1.0;

static const float3 cLuminance = float3(0.2125f, 0.7154f, 0.0721f);

static const int cDownSampleScale = 4;

static const float4 cDownSampleOffset0 = float4(
	BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT,
	BUFFER_RCP_WIDTH, -BUFFER_RCP_HEIGHT
) * 0.5 * cDownSampleScale;

static const float4 cDownSampleOffset1 = float4(
	-BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT,
	-BUFFER_RCP_WIDTH, -BUFFER_RCP_HEIGHT
) * 0.5 * cDownSampleScale;

/*
	cCoeffs meaning:
	last Value = Luma of layer
	First value = Red %
	Second Value = Green %
	Third Value = Blue %
*/

cCoeffs0 = Screen Color
cCoeffs1 = darks Color
cCoeffs2 = dark Bloom color
cCoeffs2 = Bright Bloom color

/*static const float4 cCoeffs0 = float4(0.0, 1.0, 0.0, 0.0);
static const float4 cCoeffs1 = float4(0.3032787, 0.6315789, 0.0, 0.0);
static const float4 cCoeffs2 = float4(0.6967213, 0.3421053, 0.0, 0.0);
static const float4 cCoeffs3 = float4(1.0, 0.0, 0.0, 0.0);*/

uniform float4 cCoeffs0 = float4(0.0, 1.0, 0.0, 0.0);
uniform float4 cCoeffs1 = float4(0.3032787, 0.6315789, 0.0, 0.0);
uniform float4 cCoeffs2 = float4(0.6967213, 0.3421053, 0.0, 0.0);
uniform float4 cCoeffs3 = float4(1.0, 0.0, 0.0, 0.0);

/*static const float4 cCoeffs0 = float4(0.0, 0.3421053, 0.0, 0.0);
static const float4 cCoeffs1 = float4(0.4485981, 0.3947368, 0.0, 0.0);
static const float4 cCoeffs2 = float4(0.7840375, 0.9310345, 0.0, 0.0);
static const float4 cCoeffs3 = float4(1.0, 0.4210526, 0.0, 0.0);*/

/*static const float4 cCoeffs0 = float4(0.0, 0.15, 0.0, 0.0);
static const float4 cCoeffs1 = float4(0.3020134, 0.35, 0.0, 0.0);
static const float4 cCoeffs2 = float4(0.5117371, 0.9827586, 0.0, 0.0);
static const float4 cCoeffs3 = float4(1.0, 0.0, 0.0, 0.0);*/

/*static const float4 cCoeffs0 = float4(1.0, 1.0, 1.0, 0.0);
static const float4 cCoeffs1 = float4(0.3032787, 0.6315789, 0.0, 0.0);
static const float4 cCoeffs2 = float4(0.6967213, 0.3421053, 0.0, 0.0);
static const float4 cCoeffs3 = float4(1.0, 0.0, 0.0, 0.0);*/

uniform float cDesaturation = 0.34;

texture2D tNFSMW_Vignette <
	source = "NFSMW_Vignette.png";
> {
	Width = 64;
	Height = 64;
	//Format = DXT3;
};

texture2D tNFSMW_Scaled {
	Width = BUFFER_WIDTH / cDownSampleScale;
	Height = BUFFER_HEIGHT / cDownSampleScale;
};

sampler2D sColor {
	Texture = ReShade::BackBufferTex;
	//SRGBTexture = true;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler2D sColor_Linear {
	Texture = ReShade::BackBufferTex;
	//SRGBTexture = true;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler2D sScaled {
	Texture = tNFSMW_Scaled;
	//SRGBTexture = true;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler2D sVignette {
	Texture = tNFSMW_Vignette;
	//SRGBTexture = true;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = WRAP;
};

float4 PS_DownScale4x4AlphaLuminance(
	float4 p : SV_POSITION, float2 uv : TEXCOORD
) : SV_TARGET {
	float4 uv0 = uv.xyxy + cDownSampleOffset0;// * uBlurScale;
	float4 uv1 = uv.xyxy + cDownSampleOffset1;// * uBlurScale;
	
	float3 color = 
		tex2D(sColor_Linear, uv0.xy).rgb +
		tex2D(sColor_Linear, uv0.zw).rgb +
		tex2D(sColor_Linear, uv1.xy).rgb +
		tex2D(sColor_Linear, uv1.zw).rgb;
	
	return float4(color, dot(color, cLuminance)) * 0.25;
}

float4 PS_VisualTreatment(
	float4 p : SV_POSITION, float2 uv : TEXCOORD
) : SV_TARGET {
	//float3 vignette = tex2D(sVignette, float2(uv.x, uv.y * uVignetteScale)).rgb;
	//float depth = ReShade::GetLinearizedDepth(uv);
	//float zdist = (1.0 / (1.0 - depth));

	float3 color = tex2D(sColor, uv).rgb;

	float4 scaled_tex = tex2D(sScaled, uv);
	
	float lum = dot(color, cLuminance);

	float4 curve = cCoeffs3 * scaled_tex.w + cCoeffs2;
	curve = curve * scaled_tex.w + cCoeffs1;
	curve = curve * scaled_tex.w + cCoeffs0;

	float3 desat = lum + cDesaturation * (color - lum);

	// Black bloom
	float3 bb = desat * curve.x;
	color = bb + curve.yzw * color;

	return float4(color, 1.0);
}

technique NFSMW {
	pass DownScale {
		VertexShader = PostProcessVS;
		PixelShader = PS_DownScale4x4AlphaLuminance;
		RenderTarget = tNFSMW_Scaled;
		//SRGBWriteEnable = true;
	}
	pass VisualTreatment {
		VertexShader = PostProcessVS;
		PixelShader = PS_VisualTreatment;
		//SRGBWriteEnable = true;
	}
}
