// ++++++++++++++++++++++++++++++++++++++++++++++++++++++
// *** PPFX SSDO from the Post-Processing Suite 1.03.29 for ReShade
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

// ** SSDO **
#ifndef pSSDOSamplePrecision
#define		pSSDOSamplePrecision		RGBA16F // SSDO Sample Precision - The texture format of the source texture used to calculate the effect. RGBA8 is generally too low, RGBA16F should be the sweet-spot. RGBA32F is overkill and heavily kills your FPS.
#endif

#ifndef pSSDOLOD
#define		pSSDOLOD					1		// SSDO LOD - The resolution which the effect is calculated in. 1 = full-screen, 2 = half, 4 = quarter, 8 = one-eighth
#endif

#ifndef pSSDOFilterScale
#define		pSSDOFilterScale			1.0		// SSDO Filter Scale Factor - Resolution control for the filter where noise the technique produces gets removed. Performance-affective. 0.5 means half resolution, 0.25 = quarter res,  1 = full-res. etc. Values above 1.0 yield a downsampled blur which doesn't make sense and is not recommended. | 0.1 - 4.0
#endif

#ifndef qSSDOFilterPrecision
#define		qSSDOFilterPrecision		RGBA16	// SSDO Filter Precision - The texture format used when filtering out the SSDO's noise. Use this to prevent banding artifacts that you may see in combination with very high ssdoIntensity values. RGBA16F, RGBA32F or, standard, RGBA8. Strongly suggest the latter to keep high framerates.
#endif

uniform float pSSDOIntensity <
    ui_label = "Intensity";
    ui_tooltip = "The intensity curve applied to the effect. High values may produce banding when used along with RGBA8 FilterPrecision.\nAs increasing the precision to RGBA16F will heavily affect performance, rather combine Intensity and Amount if you want high visibility.";
    ui_type = "slider";
    ui_min = 0.001;
    ui_max = 20.0;
    ui_step = 0.001;
> = 2.0;

uniform float pSSDOAmount <
    ui_label = "Amount";
    ui_tooltip = "A multiplier applied to occlusion/lighting factors when they are calculated. High values increase the effect's visibilty but may expose artifacts and noise.";
    ui_type = "slider";
    ui_min = 0.001;
    ui_max = 20.0;
    ui_step = 0.001;
> = 1.0;

uniform float pSSDOAOMix <
    ui_label = "Ambient Occlusion Mix";
    ui_tooltip = "Ambient occlusion darkens geometry if it is occluded relative to the camera's point of view. This simulates the fact that 'blocked' light can't reach geometry behind the blocker.\nThis results in diffuse, blurry shadows. | This value controls the darkening amount.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 1.0;

uniform float pSSDOILMix <
    ui_label = "Indirect Lighting Mix";
    ui_tooltip = "Geometry that is exposed to the static light source (specified below with ssdoLightPos) gets brightened. This value controls the brightening amount.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 1.0;

uniform float pSSDOBounceMultiplier <
    ui_label = "Indirect Bounce Color Multiplier";
    ui_tooltip = "SSDO includes an indirect bounce of light which means that colors of objects may interact with each other. This value controls the effects' visibility.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 1.0;

uniform float pSSDOBounceSaturation <
    ui_label = "Indirect Bounce Color Saturation";
    ui_tooltip = "High values may look strange.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 20.0;
    ui_step = 0.001;
> = 3.0;

uniform int pSSDOSampleAmount <
    ui_label = "Sample Count";
    ui_tooltip = "The amount of samples taken to accumulate SSDO. Affects quality, reduces noise and almost linearly affects performance. Current high-end systems should max out at ~32 samples at Full HD to reach desirable framerates.";
    ui_type = "slider";
    ui_min = 2;
    ui_max = 256;
    ui_step = 1;
> = 16;

uniform float pSSDOSampleRange <
    ui_label = "Sample Range";
    ui_tooltip = "Maximum 3D-distance (in pixels) for occluders to occlude geometry. High values reduce cache coherence, lead to cache misses and thus decrease performance so keep this below ~150.\nYou may prevent this performance drop by increasing Source LOD.";
    ui_type = "slider";
    ui_min = 4.0;
    ui_max = 400.0;
    ui_step = 0.001;
> = 50.5;

uniform int pSSDOSourceLOD <
    ui_label = "Source LOD";
    ui_tooltip = "The Mipmap-level of the source texture used to calculate the occlusion/indirect light. 0 = full resolution, 1 = half-axis resolution, 2 = quarter-axis resolution etc.\nCombined with high SampleRange-values, this may improve performance with a slight loss of quality.";
    ui_type = "slider";
    ui_min = 0;
    ui_max = 3;
    ui_step = 1;
> = 0;

uniform int pSSDOBounceLOD <
    ui_label = "Bounce LOD";
    ui_tooltip = "The Mipmap-level of the color texture used to calculate the light bounces. 0 = full resolution, 1 = half-axis resolution, 2 = quarter-axis resolution etc.\nCombined with high SampleRange-values, this may improve performance with a slight loss of quality.";
    ui_type = "slider";
    ui_min = 0;
    ui_max = 3;
    ui_step = 1;
> = 0;

uniform float pSSDOFilterRadius <
    ui_label = "Filter Radius";
    ui_tooltip = "The blur radius that is used to filter out the noise the technique produces. Don't push this too high, everything between 8 - 24 is recommended (depending from SampleAmount, SampleRange, Intensity and Amount).";
    ui_type = "slider";
    ui_min = 2.0;
    ui_max = 100.0;
    ui_step = 1.0;
> = 12.0;

uniform int pSSDOFilterStep <
    ui_label = "Filter Step";
    ui_tooltip = "A performance optimization for the filter, introducing interleaved sampling. Leave it at 1 and rather use FilterScale to improve the filter performance.";
    ui_type = "slider";
    ui_min = 1;
    ui_max = 16;
    ui_step = 1;
> = 1;

uniform float3 pSSDOLightPos <
    ui_label = "Light Position";
    ui_tooltip = "The view-space position of the virtual light-source used to calculate occlusion in XYZ-axis'. (0.0,0.0,1.0) means the light is located on the camera.\nValues should sum 1.0 together. High Z-axis-values recommended.";
    ui_type = "slider";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.1;
> = float3(0.0,0.2,0.8);

uniform int pSSDOMixMode <
    ui_label = "Mix Mode";
    ui_type = "combo";
    ui_items = "Multiply (visually best imo)\0Additive (use with very low Amount & Intensity), generally not recommended.\0";
> = 0;

uniform int pSSDODebugMode <
    ui_label = "Filter Step";
    ui_type = "combo";
    ui_items = "Debug-mode off\0Outputs the filtered SSDO component\0Shows you the raw, noisy SSDO right after scattering the occlusion/lighting\0";
> = 0;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   TEXTURES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** ESSENTIALS ***
texture texColorLOD { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; MipLevels = 4; };
texture texDepth : DEPTH;

// *** FX RTs ***
texture texViewSpace
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = pSSDOSamplePrecision;
	MipLevels = 4;
};
texture texSSDOA
{
	Width = BUFFER_WIDTH/2.0*pSSDOLOD;
	Height = BUFFER_HEIGHT/2.0*pSSDOLOD;
	Format = qSSDOFilterPrecision;
};
texture texSSDOB
{
	Width = BUFFER_WIDTH*pSSDOFilterScale;
	Height = BUFFER_HEIGHT*pSSDOFilterScale;
	Format = qSSDOFilterPrecision;
};
texture texSSDOC
{
	Width = BUFFER_WIDTH*pSSDOFilterScale;
	Height = BUFFER_HEIGHT*pSSDOFilterScale;
	Format = qSSDOFilterPrecision;
};

// *** EXTERNAL TEXTURES ***
texture texNoise < source = "mcnoise.png"; >
{
	Width = 1080;
	Height = 1080;
	#define NOISE_SCREENSCALE float2((BUFFER_WIDTH/pow(2.0,pSSDOLOD))/1920.0,(BUFFER_HEIGHT/pow(2.0,pSSDOLOD))/1080.0)
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   SAMPLERS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** ESSENTIALS ***
sampler SamplerColorLOD
{
	Texture = texColorLOD;
};

sampler2D SamplerDepth
{
	Texture = texDepth;
};

// *** FX RTs ***
sampler SamplerViewSpace
{
	Texture = texViewSpace;
	MipFilter = NONE;
};
sampler SamplerSSDOA
{
	Texture = texSSDOA;
};
sampler SamplerSSDOB
{
	Texture = texSSDOB;
};
sampler SamplerSSDOC
{
	Texture = texSSDOC;
};

// *** EXTERNAL TEXTURES ***
sampler SamplerNoise
{
	Texture = texNoise;
	MipFilter = NONE;
	MinFilter = NONE;
	MagFilter = NONE;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   VARIABLES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static const float2 pxSize = float2(BUFFER_RCP_WIDTH,BUFFER_RCP_HEIGHT);
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

float4 viewSpace(float2 txCoords)
{
	const float2 offsetS = txCoords+float2(0.0,1.0)*pxSize;
	const float2 offsetE = txCoords+float2(1.0,0.0)*pxSize;
	const float depth = linearDepth(txCoords);
	const float depthS = linearDepth(offsetS);
	const float depthE = linearDepth(offsetE);
	
	const float3 vsNormal = cross(normalize(float3(offsetS-txCoords,depthS-depth)),normalize(float3(offsetE-txCoords,depthE-depth)));
	return float4(vsNormal.x,-vsNormal.y,-vsNormal.z,depth);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   EFFECTS   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// *** SSDO ***
	#define SSDO_CONTRIB_RANGE (pSSDOSampleRange*pxSize.y)
	#define SSDO_BLUR_DEPTH_DISCONTINUITY_THRESH_MULTIPLIER 0.05
	
	// SSDO - Scatter Illumination
	float4 FX_SSDOScatter( float2 txCoords )
	{
		const float	sourceAxisDiv = pow(2.0,pSSDOSourceLOD);
		const float2	texelSize = pxSize*pow(2.0,pSSDOSourceLOD);
		const float4	vsOrig = tex2D(SamplerViewSpace,txCoords);
		float3	ssdo = 0.0;
		
		const float2	randomDir = tex2D(SamplerNoise,frac(txCoords*NOISE_SCREENSCALE)).xy;
		
		[loop]
		for (float offs=1.0;offs<=pSSDOSampleAmount;offs++)
		{
			const float2 fetchCoords = txCoords+(frac(randomDir*11.139795*offs)*2.0-1.0)*offs*(pSSDOSampleRange/(pSSDOSampleAmount*sourceAxisDiv))*texelSize;
			const float4 vsFetch = tex2Dlod(SamplerViewSpace,float4(fetchCoords,0,pSSDOSourceLOD));
			float3 albedoFetch = tex2Dlod(SamplerColorLOD,float4(fetchCoords,0,pSSDOBounceLOD)).xyz;
			albedoFetch = lerp(dot(albedoFetch,lumaCoeff),albedoFetch,pSSDOBounceSaturation);
			albedoFetch /= max(1.0,max(albedoFetch.x,max(albedoFetch.y,albedoFetch.z)));
			
			const float3 dirVec = float3(fetchCoords.x-txCoords.x,txCoords.y-fetchCoords.y,vsOrig.w-vsFetch.w);
			const float illuminationFact = dot(normalize(dirVec),pSSDOLightPos);
			const float visibility = (illuminationFact > 0.0) ? max(0.0,sign(dot(normalize(float3(dirVec.xy,abs(dirVec.z))),vsOrig.xyz))) : 1.0;
			const float normalDiff = length(vsFetch.xyz-vsOrig.xyz);
			if (illuminationFact < 0.0) albedoFetch = 1.0-albedoFetch;
			ssdo += clamp(sign(illuminationFact),-pSSDOILMix,pSSDOAOMix) * (1.0-albedoFetch*pSSDOBounceMultiplier) * visibility * (max(0.0,SSDO_CONTRIB_RANGE-length(dirVec))/SSDO_CONTRIB_RANGE) * sign(max(0.0,normalDiff-0.3));
		}
		
		return float4(saturate(0.5-(ssdo/pSSDOSampleAmount*pSSDOAmount)*smoothstep(0.1,0.3,1.0-vsOrig.w)),vsOrig.w);
	}

	// Depth-Bilateral Gaussian Blur - Horizontal
	float4 FX_BlurBilatH( float2 txCoords, float radius )
	{
		const float	texelSize = pxSize.x/pSSDOFilterScale;
		float4	pxInput = tex2D(SamplerSSDOB,txCoords);
		pxInput.xyz *= 0.5;
		float	sampleSum = 0.5;
		const float	weightDiv = 1.0+2.0/radius;
		
		[loop]
		for (float hOffs=1.5; hOffs<radius; hOffs+=2.0*pSSDOFilterStep)
		{
			const float weight = 1.0/pow(abs(weightDiv),hOffs*hOffs/radius);
			float2 fetchCoords = txCoords;
			fetchCoords.x += texelSize * hOffs;
			float4 fetch = tex2Dlod(SamplerSSDOB, float4(fetchCoords, 0.0, 0.0));
			float contribFact = max(0.0,sign(SSDO_CONTRIB_RANGE*SSDO_BLUR_DEPTH_DISCONTINUITY_THRESH_MULTIPLIER-abs(pxInput.w-fetch.w))) * weight;
			pxInput.xyz+=fetch.xyz * contribFact;
			sampleSum += contribFact;
			fetchCoords = txCoords;
			fetchCoords.x -= texelSize * hOffs;
			fetch = tex2Dlod(SamplerSSDOB, float4(fetchCoords, 0.0, 0.0));
			contribFact = max(0.0,sign(SSDO_CONTRIB_RANGE*SSDO_BLUR_DEPTH_DISCONTINUITY_THRESH_MULTIPLIER-abs(pxInput.w-fetch.w))) * weight;
			pxInput.xyz+=fetch.xyz * contribFact;
			sampleSum += contribFact;
		}
		pxInput.xyz /= sampleSum;
		
		return pxInput;
	}
	
	// Depth-Bilateral Gaussian Blur - Vertical
	float3 FX_BlurBilatV( float2 txCoords, float radius )
	{
		const float	texelSize = pxSize.y/pSSDOFilterScale;
		float4	pxInput = tex2D(SamplerSSDOC,txCoords);
		pxInput.xyz *= 0.5;
		float	sampleSum = 0.5;
		const float	weightDiv = 1.0+2.0/radius;
		
		[loop]
		for (float vOffs=1.5; vOffs<radius; vOffs+=2.0*pSSDOFilterStep)
		{
			float weight = 1.0/pow(abs(weightDiv),vOffs*vOffs/radius);
			float2 fetchCoords = txCoords;
			fetchCoords.y += texelSize * vOffs;
			float4 fetch = tex2Dlod(SamplerSSDOC, float4(fetchCoords, 0.0, 0.0));
			float contribFact = max(0.0,sign(SSDO_CONTRIB_RANGE*SSDO_BLUR_DEPTH_DISCONTINUITY_THRESH_MULTIPLIER-abs(pxInput.w-fetch.w))) * weight;
			pxInput.xyz+=fetch.xyz * contribFact;
			sampleSum += contribFact;
			fetchCoords = txCoords;
			fetchCoords.y -= texelSize * vOffs;
			fetch = tex2Dlod(SamplerSSDOC, float4(fetchCoords, 0.0, 0.0));
			contribFact = max(0.0,sign(SSDO_CONTRIB_RANGE*SSDO_BLUR_DEPTH_DISCONTINUITY_THRESH_MULTIPLIER-abs(pxInput.w-fetch.w))) * weight;
			pxInput.xyz+=fetch.xyz * contribFact;
			sampleSum += contribFact;
		}
		pxInput /= sampleSum;
		
		return pxInput.xyz;
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
float4 PS_SetOriginal(VS_OUTPUT_POST IN) : COLOR
{
    return tex2D(ReShade::BackBuffer,IN.txcoord.xy);
}

// *** SSDO ***
	float4 PS_SSDOViewSpace(VS_OUTPUT_POST IN) : COLOR
	{
		return viewSpace(IN.txcoord.xy);
	}

	float4 PS_SSDOScatter(VS_OUTPUT_POST IN) : COLOR
	{
		return FX_SSDOScatter(IN.txcoord.xy);
	}
	
	float4 PS_SSDOBlurScale(VS_OUTPUT_POST IN) : COLOR
	{
		return tex2D(SamplerSSDOA,IN.txcoord.xy);
	}

	float4 PS_SSDOBlurH(VS_OUTPUT_POST IN) : COLOR
	{
		return FX_BlurBilatH(IN.txcoord.xy,pSSDOFilterRadius/pSSDOFilterStep*pSSDOFilterScale);
	}

	float4 PS_SSDOBlurV(VS_OUTPUT_POST IN) : COLOR
	{
		return float4(FX_BlurBilatV(IN.txcoord.xy,pSSDOFilterRadius/pSSDOFilterStep*pSSDOFilterScale).xyz,1.0);
	}
	
	float4 PS_SSDOMix(VS_OUTPUT_POST IN) : COLOR
	{
    float3 SSDO_MIX_MODE;
		if (pSSDOMixMode == 1)
			SSDO_MIX_MODE = pow(abs(tex2D(SamplerSSDOB,IN.txcoord.xy).xyz*2.0),pSSDOIntensity)-1.0;
		else
			SSDO_MIX_MODE = pow(abs(tex2D(SamplerSSDOB,IN.txcoord.xy).xyz*2.0),pSSDOIntensity);
		
		if (pSSDODebugMode == 1)
		{
      if (pSSDOMixMode == 1)
        return float4(saturate(0.5 + SSDO_MIX_MODE),1.0);
      else
        return float4(saturate(0.5 * SSDO_MIX_MODE),1.0);
			return float4(saturate(0.5 * SSDO_MIX_MODE),1.0);
		}
		else if (pSSDODebugMode == 2)
			return float4(tex2D(SamplerSSDOA,IN.txcoord.xy).xyz,1.0);
		else
		{
      if (pSSDOMixMode == 1)
        return float4(saturate(tex2D(ReShade::BackBuffer,IN.txcoord.xy).xyz + SSDO_MIX_MODE),1.0);
      else
        return float4(saturate(tex2D(ReShade::BackBuffer,IN.txcoord.xy).xyz * SSDO_MIX_MODE),1.0);
    }
	}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++   TECHNIQUES   +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

technique PPFXSSDO < ui_label = "PPFX SSDO"; ui_tooltip = "Screen Space Directional Occlusion | Ambient Occlusion simulates diffuse shadows/self-shadowing of geometry.\nIndirect Lighting brightens objects that are exposed to a certain 'light source' you may specify in the parameters below.\nThis approach takes directional information into account and simulates indirect light bounces, approximating global illumination."; >
{
	pass setOriginal
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SetOriginal;
		RenderTarget0 = texColorLOD;
	}
	
	pass ssdoViewSpace
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOViewSpace;
		RenderTarget0 = texViewSpace;
	}
		
	pass ssdoScatter
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOScatter;
		RenderTarget0 = texSSDOA;
	}
		
	pass ssdoBlurScale
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOBlurScale;
		RenderTarget0 = texSSDOB;
	}
		
	pass ssdoBlurH
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOBlurH;
		RenderTarget0 = texSSDOC;
	}
		
	pass ssdoBlurV
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOBlurV;
		RenderTarget0 = texSSDOB;
	}
		
	pass ssdoMix
	{
		VertexShader = VS_PostProcess;
		PixelShader = PS_SSDOMix;
	}
}