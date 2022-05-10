#include "ReShade.fxh"

uniform float Tuning_Sharp <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Sharpness [CRT-Sim]";
> = 0.2;

uniform float4 Tuning_Persistence <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Persistence [CRT-Sim]";
> = float4(0.1,0.0,0.0,1.0);

uniform float Tuning_Bleed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Composite Bleeding [CRT-Sim]";
> = 0.5;

uniform float Tuning_Artifacts <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Composite Artifacts [CRT-Sim]";
> = 0.5;

uniform float NTSCLerp <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "NTSC Scanlines [CRT-Sim]";
> = 1.0;

uniform bool animate_artifacts <
	ui_type = "boolean";
	ui_label = "Animate Artifacts [CRT-Sim]";
> = true;

uniform float InputSizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Sim]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 256.0;

uniform float InputSizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Sim]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 224.0;

uniform float BloomScalar <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.05;
	ui_label = "Bloom [CRT-Sim]";
> = 0.1;

uniform float BloomPower <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 4.0;
	ui_step = 0.1;
	ui_label = "Bloom Power [CRT-Sim]";
> = 1.0;

uniform float CRTMask_Scale <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "CRT Mask Scale [CRT-Sim]";
> = 0.25;

uniform float Tuning_Satur <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Saturation [CRT-Sim]";
> = 1.0;

uniform float Tuning_Mask_Brightness <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Mask Brightness [CRT-Sim]";
> = 0.5;

uniform float Tuning_Mask_Opacity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Mask Opacity [CRT-Sim]"; 
> = 0.3;

uniform float Tuning_Overscan <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Overscan [CRT-Sim]";
> = 0.95;

uniform float Tuning_Barrel <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Barrel Distortion [CRT-Sim]";
> = 0.25;

#define InputSize float2(InputSizeX,InputSizeY)
#define RcpScrWidth float2(1.0 / (float)InputSizeX, 0.0)
#define RcpScrHeight float2(0.0, 1.0 / (float)InputSizeY)

static const float2 BloomScale = float2(0.025,0.025);

texture NTSCArtifactTex <source="crtsim/artifacts.png";> { Width=256; Height=224;};
sampler2D NTSCArtifactSampler { Texture=NTSCArtifactTex; MinFilter=LINEAR; MagFilter=LINEAR; };

texture shadowMaskSamplerTex <source="crtsim/mask.png";> { Width=64; Height=32;};
sampler2D shadowMaskSampler { Texture=shadowMaskSamplerTex; MinFilter=LINEAR; MagFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

texture2D CRTSim_prevFrameTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
sampler2D prevFrameSampler { Texture = CRTSim_prevFrameTex; MinFilter = LINEAR;  MagFilter = LINEAR;  MipFilter = LINEAR; };

texture target0_crtsim
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D compFrameSampler { Texture = target0_crtsim; AddressU = BORDER; AddressV = BORDER;};

texture target1_crtsim
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D PreBloomBufferSampler { Texture = target1_crtsim; };

texture targetB_crtsim
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D curFrameSampler { Texture = targetB_crtsim; AddressU = BORDER; AddressV = BORDER;};

texture target2_crtsim
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D DownsampleBufferSampler { Texture = target2_crtsim; };

texture target3_crtsim
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};
sampler2D UpsampledBufferSampler { Texture = target3_crtsim; };

uniform int FrameCount < source = "framecount"; >;

#define mod2(x,y) (x-y*floor(x/y))

float Brightness(float4 InVal)
{
	return dot(InVal, float4(0.299, 0.587, 0.114, 0.0));
}
	

float4 PS_CRTSim_Stock(float4 vpos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
	float4 c=tex2D(ReShade::BackBuffer, tex);
	return c;
}

float4 PS_CRTSim_Composite(float4 vpos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 NTSCArtifact1 = tex2D(NTSCArtifactSampler, uv);
	float4 NTSCArtifact2 = tex2D(NTSCArtifactSampler, uv + RcpScrHeight);
	float4 NTSCArtifact = lerp(NTSCArtifact1, NTSCArtifact2, NTSCLerp);
	
	float2 LeftUV = uv - RcpScrWidth;
	float2 RightUV = uv + RcpScrWidth;
	
	float4 Cur_Left = tex2D(curFrameSampler, LeftUV);
	float4 Cur_Local = tex2D(curFrameSampler, uv);
	float4 Cur_Right = tex2D(curFrameSampler, RightUV);
	
	float4 TunedNTSC = NTSCArtifact * Tuning_Artifacts;
		
	// Note: The "persistence" and "bleed" parameters have some overlap, but they are not redundant.
	// "Persistence" affects bleeding AND trails. (Scales the sum of the previous value and its scaled neighbors.)
	// "Bleed" only affects bleeding. (Scaling of neighboring previous values.)
	
	float4 Prev_Left = tex2D(prevFrameSampler, LeftUV);
	float4 Prev_Local = tex2D(prevFrameSampler, uv);
	float4 Prev_Right = tex2D(prevFrameSampler, RightUV);
	
	// Apply NTSC artifacts based on differences in luma between local pixel and neighbors..
	Cur_Local =
		saturate(Cur_Local +
		(((Cur_Left - Cur_Local) + (Cur_Right - Cur_Local)) * TunedNTSC));
	
	float curBrt = Brightness(Cur_Local);
	float offset = 0;
	
	// Step left and right looking for changes in luma that would produce a ring or halo on this pixel due to undershooting/overshooting.
	// (Note: It would probably be more accurate to look at changes in luma between pixels at a distance of N and N+1,
	// as opposed to 0 and N as done here, but this works pretty well and is a little cheaper.)
	
	float SharpWeight[3] =
	{
		1.0, -0.3162277, 0.1
	};
	
	for (int i = 0; i < 3; ++i)
	{
		float2 StepSize = (float2(1.0/InputSizeX,0) * (float(i + 1)));
		float4 neighborleft = tex2D(curFrameSampler, uv - StepSize);
		float4 neighborright = tex2D(curFrameSampler, uv + StepSize);
		
		float NBrtL = Brightness(neighborleft);
		float NBrtR = Brightness(neighborright);
		offset += ((((curBrt - NBrtL) + (curBrt - NBrtR))) * SharpWeight[i]);
	}
	
	// Apply the NTSC artifacts to the unsharp offset as well.
	Cur_Local = saturate(Cur_Local + (offset * Tuning_Sharp * lerp(float4(1,1,1,1), NTSCArtifact, Tuning_Artifacts)));
	
	// Take the max here because adding is overkill; bleeding should only brighten up dark areas, not blow out the whole screen.
	Cur_Local = saturate(max(Cur_Local, Tuning_Persistence * (1.0 / (1.0 + (2.0 * Tuning_Bleed))) * (Prev_Local + ((Prev_Left + Prev_Right) * Tuning_Bleed))));
	
	return Cur_Local;
}

float3 PS_CRTSim_Lastframe(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
	return tex2D(ReShade::BackBuffer, uv).rgb;
}

static const float2 Poisson0 = float2(0.000000, 0.000000);
static const float2 Poisson1 = float2(0.000000, 1.000000);
static const float2 Poisson2 = float2(0.000000, -1.000000);
static const float2 Poisson3 = float2(-0.866025, 0.500000);
static const float2 Poisson4 = float2(-0.866025, -0.500000);
static const float2 Poisson5 = float2(0.866025, 0.500000);
static const float2 Poisson6 = float2(0.866025, -0.500000);

static const float InvNumSamples = 1.0f / 7.0;

float4 PS_CRTSim_Bloom_Downsample(float4 vpos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 bloom = float4(0,0,0,0);
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson0 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson1 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson2 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson3 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson4 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson5 * BloomScale));
	bloom += tex2D(PreBloomBufferSampler, uv + (Poisson6 * BloomScale));
	bloom *= InvNumSamples;
	return bloom;
}

float4 PS_CRTSim_Bloom_Upsample(float4 vpos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	// Swap X and Y for this one to reduce artifacts in sampling.
	
	float4 bloom = float4(0,0,0,0);
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson0.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson1.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson2.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson3.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson4.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson5.yx * BloomScale));
	bloom += tex2D(DownsampleBufferSampler, uv + (Poisson6.yx * BloomScale));
	bloom *= InvNumSamples;
	return bloom;
}

float4 ColorPow(float4 InColor, float InPower)
{
	// This method preserves color better.
	float4 RefLuma = float4(0.299, 0.587, 0.114, 0.0);
	float ActLuma = dot(InColor, RefLuma);
	float4 ActColor = InColor / ActLuma;
	float PowLuma = pow(ActLuma, InPower);
	float4 PowColor = ActColor * PowLuma;
	return PowColor;
}

float4 PS_CRTSim_Present(float4 vpos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 PreBloom = tex2D(PreBloomBufferSampler, uv);
	float4 Blurred = tex2D(UpsampledBufferSampler, uv);
	
	return PreBloom + (ColorPow(Blurred, BloomPower) * BloomScalar);
}

float4 PS_CRTSim_Screen(float4 vpos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float2 ScaledUV = uv;
	
	float2 scanuv = frac(uv / CRTMask_Scale * 100.0);
	float3 scantex = tex2D(shadowMaskSampler, scanuv).rgb;
	
	scantex += Tuning_Mask_Brightness;			// adding looks better
	scantex = lerp(float3(1,1,1), scantex, Tuning_Mask_Opacity);

	// Apply overscan after scanline sampling is done.
	float2 overscanuv = (ScaledUV * Tuning_Overscan) - ((Tuning_Overscan - 1.0) * 0.5);
	
	// Curve UVs for composite texture inwards to garble things a bit.
	overscanuv = overscanuv - float2(0.5,0.5);
	float rsq = (overscanuv.x*overscanuv.x) + (overscanuv.y*overscanuv.y);
	overscanuv = overscanuv + (overscanuv * (Tuning_Barrel * rsq)) + float2(0.5,0.5);
		
	float3 comptex = tex2D(ReShade::BackBuffer, overscanuv).rgb;

	float4 emissive = float4(comptex * scantex, 1);
	float desat = dot(float4(0.299, 0.587, 0.114, 0.0), emissive);
	emissive = lerp(float4(desat,desat,desat,1), emissive, Tuning_Satur);
	
	return emissive;
}

technique CRTSim {

	pass Pass1_Base
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Stock;
		RenderTarget = target0_crtsim;
		RenderTarget = targetB_crtsim;
	}

	pass Pass2_Composite
	{	
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Composite;
		RenderTarget = target1_crtsim;
	}
	
	pass Pass2_1_LastFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Lastframe;
		RenderTarget = CRTSim_prevFrameTex;
	}
	
	pass Pass3_BloomDownsample
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Bloom_Downsample;
		RenderTarget = target2_crtsim;
	}
	
	pass Pass4_BloomUpsample
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Bloom_Upsample;
		RenderTarget = target3_crtsim;
	}
	
	pass Pass5_Present
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Present;
	}
	
	pass Pass6_CRTScreen
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CRTSim_Screen;
	}
}