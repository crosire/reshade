/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg

   	Advanced Depth of Field "ADoF"
    by Marty McFly / P.Gilcher
    part of qUINT shader library for ReShade 4

    Copyright (c) Pascal Gilcher / Marty McFly. All rights reserved.

=============================================================================*/

/*=============================================================================
	Preprocessor settings
=============================================================================*/

//------------------------------------------------------------------
//Enables partial occlusion of bokeh disc at screen corners
#ifndef ADOF_OPTICAL_VIGNETTE_ENABLE
 #define ADOF_OPTICAL_VIGNETTE_ENABLE	           0	  //[0 or 1]
#endif
//------------------------------------------------------------------
//Enables chromatic aberration at bokeh shape borders.
#ifndef ADOF_CHROMATIC_ABERRATION_ENABLE
 #define ADOF_CHROMATIC_ABERRATION_ENABLE          1	  //[0 or 1]
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform bool bADOF_AutofocusEnable <
    ui_type = "bool";
    ui_label = "Enable Autofocus";
    ui_tooltip = "Enables automated focus calculation.";
    ui_category = "Focusing";
> = true;

uniform float2 fADOF_AutofocusCenter <
    ui_type = "drag";
    ui_min = 0.0; ui_max = 1.0;
    ui_label = "Autofocus Center";
    ui_tooltip = "X and Y coordinates of autofocus center. Axes start from upper left screen corner.";
    ui_category = "Focusing";
> = float2(0.5, 0.5);

uniform float fADOF_AutofocusRadius <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_label = "Autofocus sample radius";
    ui_tooltip = "Radius of area contributing to focus calculation.";
    ui_category = "Focusing";
> = 0.6;

uniform float fADOF_AutofocusSpeed <
    ui_type = "drag";
    ui_min = 0.05;
    ui_max = 1.0;
    ui_label = "Autofocus Adjustment Speed";
    ui_tooltip = "Adjustment speed of autofocus on focus change";
    ui_category = "Focusing";
> = 0.1;

uniform float fADOF_ManualfocusDepth <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_label = "Manual focus depth";
    ui_tooltip = "Manually adjusted static focus depth, disable autofocus to use it.";
    ui_category = "Focusing";
> = 0.001;

uniform float fADOF_NearBlurCurve <
    ui_type = "drag";
    ui_min = 0.5;
    ui_max = 6.0;
    ui_label = "Near blur curve";
    ui_category = "Focusing";
> = 6.0;

uniform float fADOF_FarBlurCurve <
    ui_type = "drag";
    ui_min = 0.5;
    ui_max = 6.0;
    ui_label = "Far blur curve";
    ui_category = "Focusing";
> = 1.5;

uniform float fADOF_HyperFocus <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_label = "Hyperfocal depth distance";
    ui_category = "Focusing";
> = 0.10;

uniform float fADOF_RenderResolutionMult <
    ui_type = "drag";
    ui_min = 0.5;
    ui_max = 1.0;
    ui_label = "Size Scale";
    ui_tooltip = "Resolution Scale of bokeh blur. 0.5 means 1/2 screen width and height.";
    ui_category = "Blur & Quality";
> = 0.5;

uniform float fADOF_ShapeRadius <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 100.0;
    ui_label = "Bokeh Maximal Blur Size";
    ui_tooltip = "Blur size of areas entirely out of focus.";
    ui_category = "Blur & Quality";
> = 20.5;

uniform float fADOF_SmootheningAmount <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 20.0;
    ui_label = "Gaussian blur width";
    ui_tooltip = "Width of gaussian blur after bokeh filter.";
    ui_category = "Blur & Quality";
> = 4.0;

uniform float fADOF_BokehIntensity <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_label = "Bokeh Intensity";
    ui_tooltip = "Intensity of bokeh discs.";
    ui_category = "Bokeh";
> = 0.3;

uniform int iADOF_BokehMode <
    ui_type = "slider";
    ui_min = 0;
    ui_max = 3;
    ui_label = "Bokeh highlight type";
    ui_tooltip = "Different methods to emphasize bokeh sprites";
    ui_category = "Bokeh";
> = 2;

uniform int iADOF_ShapeVertices <
    ui_type = "drag";
    ui_min = 3;
    ui_max = 9;
    ui_label = "Bokeh shape vertices";
    ui_tooltip = "Vertices of bokeh kernel. 5 = pentagon, 6 = hexagon etc.";
    ui_category = "Bokeh";
> = 6;

uniform int iADOF_ShapeQuality <
    ui_type = "drag";
    ui_min = 2;
    ui_max = 25;
    ui_label = "Bokeh shape quality";
    ui_category = "Bokeh";
> = 5;

uniform float fADOF_ShapeCurvatureAmount <
    ui_type = "drag";
    ui_min = -1.0;
    ui_max = 1.0;
    ui_label = "Bokeh shape roundness";
    ui_tooltip = "Roundness of bokeh kernel. 1.0 = circle, 0.0 = polygon.";
    ui_category = "Bokeh";
> = 1.0;

uniform float fADOF_ShapeRotation <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 360.0;
    ui_label = "Bokeh shape rotation";
    ui_category = "Bokeh";
> = 0.0;

uniform float fADOF_ShapeAnamorphRatio <
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_label = "Bokeh shape aspect ratio";
    ui_category = "Bokeh";
> = 1.0;

#if(ADOF_OPTICAL_VIGNETTE_ENABLE != 0)
    uniform float fADOF_ShapeVignetteCurve <
        ui_type = "drag";
        ui_min = 0.5;
        ui_max = 2.5;
        ui_label = "Bokeh shape vignette curve";
        ui_category = "Bokeh";
    > = 0.75;

    uniform float fADOF_ShapeVignetteAmount <
        ui_type = "drag";
        ui_min = 0.0;
        ui_max = 2.0;
        ui_label = "Bokeh shape vignette amount";
        ui_category = "Bokeh";
    > = 1.0;
#endif

#if(ADOF_CHROMATIC_ABERRATION_ENABLE != 0)
    uniform float fADOF_ShapeChromaAmount <
        ui_type = "drag";
        ui_min = -1.0;
        ui_max = 1.0;
        ui_label = "Shape chromatic aberration amount";
        ui_category = "Chromatic Aberration";
    > = -0.1;

    uniform int iADOF_ShapeChromaMode <
        ui_type = "drag";
        ui_min = 0;
        ui_max = 2;
        ui_label = "Shape chromatic aberration type";
        ui_category = "Chromatic Aberration";
    > = 2;
#endif

/*=============================================================================
	Textures, Samplers, Globals
=============================================================================*/

#define RESHADE_QUINT_COMMON_VERSION_REQUIRE    202
#define RESHADE_QUINT_EFFECT_DEPTH_REQUIRE      //effect requires depth access
#include "qUINT_common.fxh"

#define DISCRADIUS_RESOLUTION_BOUNDARY_LOWER 	0.25//1.0	//used for blending blurred scene.
#define DISCRADIUS_RESOLUTION_BOUNDARY_UPPER 	2.0//6.0	//used for blending blurred scene.
#define DISCRADIUS_RESOLUTION_BOUNDARY_CURVE    0.5		//used for blending blurred scene.
#define FPS_HAND_BLUR_CUTOFF_DIST		0.3353		//fps hand depth (x10.000), change if you perceive blurred fps weapons.
#define FPS_HAND_BLUR_CUTOFF_CHECK		0		//blur = max if depth > hand depth, else 0, useful for tweaking above param
#define GAUSSIAN_BUILDUP_MULT			4.0		//value of x -> gaussian reaches max radius at |CoC| == 1/x

texture2D ADOF_FocusTex 	    { Format = R16F; };
texture2D ADOF_FocusTexPrev     { Format = R16F; };

sampler2D sADOF_FocusTex	    { Texture = ADOF_FocusTex; };
sampler2D sADOF_FocusTexPrev	{ Texture = ADOF_FocusTexPrev; };

texture2D CommonTex0 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; };
sampler2D sCommonTex0	{ Texture = CommonTex0;	};

texture2D CommonTex1 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; };
sampler2D sCommonTex1	{ Texture = CommonTex1;	};

/*=============================================================================
	Vertex Shader
=============================================================================*/

struct ADOF_VSOUT
{
	float4   vpos           : SV_Position;
    float4   txcoord        : TEXCOORD0;
    float4   offset0        : TEXCOORD1;
    float2x2 offsmat        : TEXCOORD2;

};

ADOF_VSOUT VS_ADOF(in uint id : SV_VertexID)
{
    ADOF_VSOUT OUT;

    OUT.txcoord.x = (id == 2) ? 2.0 : 0.0;
    OUT.txcoord.y = (id == 1) ? 2.0 : 0.0;
    OUT.txcoord.zw = OUT.txcoord.xy / fADOF_RenderResolutionMult;
    OUT.vpos = float4(OUT.txcoord.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    //can't precompute vertices directly, not enough registers in sm3
    sincos(6.2831853 / iADOF_ShapeVertices, OUT.offsmat._21, OUT.offsmat._22);
    OUT.offsmat._11 = OUT.offsmat._22;
    OUT.offsmat._12 = -OUT.offsmat._21;

    sincos(radians(fADOF_ShapeRotation), OUT.offset0.x, OUT.offset0.y);
    OUT.offset0.zw = mul(OUT.offset0.xy, OUT.offsmat);

    return OUT;
}

/*=============================================================================
	Functions
=============================================================================*/

float GetLinearDepth(float2 coords)
{
	return qUINT::linear_depth(coords);
}

float CircleOfConfusion(float2 texcoord, bool aggressiveLeakReduction)
{
	float2 depthdata; //x - linear scene depth, y - linear scene focus
	float scenecoc;   //blur value, signed by position relative to focus plane

    depthdata.x = GetLinearDepth(texcoord.xy);

	[branch]
	if(aggressiveLeakReduction)
	{
        float3 neighbourOffsets = float3(qUINT::PIXEL_SIZE.xy, 0);
        //sadly, flipped depth buffers etc don't allow for gather or linearizing in batch
        float4 neighbourDepths = float4(GetLinearDepth(texcoord.xy - neighbourOffsets.xz), //left
                                        GetLinearDepth(texcoord.xy + neighbourOffsets.xz), //right
                                        GetLinearDepth(texcoord.xy - neighbourOffsets.zy), //top
                                        GetLinearDepth(texcoord.xy + neighbourOffsets.zy));//bottom

		float neighbourMin = min(min(neighbourDepths.x,neighbourDepths.y),min(neighbourDepths.z,neighbourDepths.w));
		depthdata.x = lerp(min(neighbourMin, depthdata.x), depthdata.x, 0.001);
	}

	depthdata.y = tex2D(sADOF_FocusTex, texcoord.xy).x;
	float handdepth = depthdata.x;

	depthdata.xy = saturate(depthdata.xy / fADOF_HyperFocus);

	[branch]
	if(depthdata.x < depthdata.y)
	{
		scenecoc = depthdata.x / depthdata.y - 1.0;
		scenecoc *= exp2(-0.5*fADOF_NearBlurCurve*fADOF_NearBlurCurve);
	}
	else
	{
		scenecoc = (depthdata.x - depthdata.y)/((depthdata.y * exp2(fADOF_FarBlurCurve*fADOF_FarBlurCurve)) - depthdata.y);
	        scenecoc = saturate(scenecoc);
	}

#if(FPS_HAND_BLUR_CUTOFF_CHECK != 0)
	scenecoc = (handdepth < FPS_HAND_BLUR_CUTOFF_DIST * 1e-4) ? 0.0 : 1.0;
#else //FPS_HAND_BLUR_CUTOFF_CHECK
	scenecoc = (handdepth < FPS_HAND_BLUR_CUTOFF_DIST * 1e-4) ? 0.0 : scenecoc;
#endif //FPS_HAND_BLUR_CUTOFF_CHECK

	return scenecoc;
}

void ShapeRoundness(inout float2 sampleOffset, in float roundness)
{
	sampleOffset *= (1.0-roundness) + rsqrt(dot(sampleOffset,sampleOffset))*roundness;
}

void OpticalVignette(in float2 sampleOffset, in float2 centerVec, inout float sampleWeight)
{
	sampleOffset -= centerVec; //scaled by vignette intensity
	sampleWeight *= saturate(3.333 - dot(sampleOffset,sampleOffset) * 1.666); //notsosmoothstep to avoid aliasing
}

float2 CoC2BlurRadius(float CoC)
{
	return float2(fADOF_ShapeAnamorphRatio, qUINT::ASPECT_RATIO.y) * CoC * fADOF_ShapeRadius * 6e-4;
}

/*=============================================================================
	Pixel Shaders
=============================================================================*/

void PS_CopyBackBuffer(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
    color = tex2D(qUINT::sBackBufferTex, IN.txcoord.xy);
}

void PS_ReadFocus(in ADOF_VSOUT IN, out float focus : SV_Target0)
{
    float scenefocus = 0.0;

    [branch]
	if(bADOF_AutofocusEnable == true)
	{
		float samples = 10.0;
		float weightsum = 1e-6;

		for(float xcoord = 0.0; xcoord < samples; xcoord++)
		for(float ycoord = 0.0; ycoord < samples; ycoord++)
		{
			float2 sampleOffset = (float2(xcoord,ycoord) + 0.5) / samples;
			sampleOffset = sampleOffset * 2.0 - 1.0;
			sampleOffset *= fADOF_AutofocusRadius;
			sampleOffset += (fADOF_AutofocusCenter - 0.5);

			float sampleWeight = saturate(1.2 * exp2(-dot(sampleOffset,sampleOffset)*4.0));

			float tempfocus = GetLinearDepth(sampleOffset * 0.5 + 0.5);
			sampleWeight *= rcp(tempfocus + 0.001);

			sampleWeight *= saturate(tempfocus > FPS_HAND_BLUR_CUTOFF_DIST * 1e-4); //remove fps hands from focus calculations

			scenefocus += tempfocus * sampleWeight;
			weightsum += sampleWeight;
		}
		scenefocus /= weightsum;
	}
	else
	{
		scenefocus = fADOF_ManualfocusDepth * fADOF_ManualfocusDepth;
	}

    float prevscenefocus = tex2D(sADOF_FocusTexPrev, 0.5).x;
    float adjustmentspeed = fADOF_AutofocusSpeed * fADOF_AutofocusSpeed;
    adjustmentspeed *= prevscenefocus > scenefocus ? 2.0 : 1.0;

    focus = lerp(prevscenefocus, scenefocus, saturate(adjustmentspeed));
}

void PS_CopyFocus(in ADOF_VSOUT IN, out float focus : SV_Target0)
{
    focus = tex2D(sADOF_FocusTex, IN.txcoord.xy).x;
}

void PS_CoC(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
    color           = tex2D(qUINT::sBackBufferTex, IN.txcoord.xy);

	static const float2 sampleOffsets[4] = {    float2( 1.5, 0.5) * qUINT::PIXEL_SIZE.xy,
		                                        float2( 0.5,-1.5) * qUINT::PIXEL_SIZE.xy,
				                                float2(-1.5,-0.5) * qUINT::PIXEL_SIZE.xy,
				                                float2(-0.5, 1.5) * qUINT::PIXEL_SIZE.xy};

	float centerDepth = GetLinearDepth(IN.txcoord.xy);
    float4 sampleCoord = 0.0;
    float3 neighbourOffsets = float3(qUINT::PIXEL_SIZE.xy, 0);
    float4 coccolor = 0.0;

	[loop]
	for(int i=0; i<4; i++)
	{
		sampleCoord.xy = IN.txcoord.xy + sampleOffsets[i];

		float3 sampleColor = tex2Dlod(qUINT::sBackBufferTex, sampleCoord).rgb;

        float4 sampleDepths = float4(GetLinearDepth(sampleCoord.xy + neighbourOffsets.xz),  //right
                                     GetLinearDepth(sampleCoord.xy - neighbourOffsets.xz),  //left
                                     GetLinearDepth(sampleCoord.xy + neighbourOffsets.zy),  //bottom
                                     GetLinearDepth(sampleCoord.xy - neighbourOffsets.zy)); //top

        float sampleDepthMin = min(min(sampleDepths.x,sampleDepths.y),min(sampleDepths.z,sampleDepths.w));

		sampleColor /= 1.0 + max(max(sampleColor.r, sampleColor.g), sampleColor.b);

		float sampleWeight = saturate(sampleDepthMin * rcp(centerDepth) + 1e-3);
		coccolor += float4(sampleColor.rgb * sampleWeight, sampleWeight);
	}

	coccolor.rgb /= coccolor.a;
	coccolor.rgb /= 1.0 - max(coccolor.r, max(coccolor.g, coccolor.b));

	color.rgb = lerp(color.rgb, coccolor.rgb, saturate(coccolor.w * 8.0));
	color.w = CircleOfConfusion(IN.txcoord.xy, 1);
    color.w = saturate(color.w * 0.5 + 0.5);
}

void unpack_hdr(inout float3 color)
{
    color = color * rcp(1.2 - saturate(color));    
}

void pack_hdr(inout float3 color)
{
    color = 1.2 * color * rcp(color + 1.0);
}

float4 PS_DoF_Main(in ADOF_VSOUT IN) : SV_Target0
{
	if(max(IN.txcoord.z,IN.txcoord.w) > 1.01) discard;

	float4 BokehSum, BokehMax;
	BokehMax		           = tex2D(sCommonTex0, IN.txcoord.zw);
    BokehSum                   = BokehMax;
	float weightSum 		   = 1.0;
	float CoC 			       = abs(BokehSum.w * 2.0 - 1.0);
	float2 bokehRadiusScaled   = CoC2BlurRadius(CoC);
	float nRings 			   = lerp(1.0,iADOF_ShapeQuality,saturate(CoC)) + (dot(IN.vpos.xy,1) % 2) * 0.5;

	if(bokehRadiusScaled.x < DISCRADIUS_RESOLUTION_BOUNDARY_LOWER * qUINT::PIXEL_SIZE.x) return BokehSum;

	bokehRadiusScaled /= nRings;
	CoC /= nRings;

#if (ADOF_OPTICAL_VIGNETTE_ENABLE != 0)
	float2 centerVec = IN.txcoord.zw - 0.5;
	float centerDist = sqrt(dot(centerVec,centerVec));
	float vignette = pow(centerDist, fADOF_ShapeVignetteCurve) * fADOF_ShapeVignetteAmount;
	centerVec = centerVec / centerDist * vignette;
	weightSum *= saturate(3.33 - vignette * 2.0);
	BokehSum *= weightSum;
	BokehMax *= weightSum;
#endif

    int densityScale = max(1, 6 - iADOF_ShapeVertices);

	[loop]
    for (int iVertices = 0; iVertices < iADOF_ShapeVertices && iVertices < 10; iVertices++)
    {
        [loop]
        for(float iRings = 1; iRings <= nRings && iRings < 26; iRings++)
        {
            [loop]
            for(float iSamplesPerRing = 0; iSamplesPerRing < iRings * densityScale && iSamplesPerRing < 26*2; iSamplesPerRing++)
            {
                float x = iSamplesPerRing/(iRings * densityScale);
                float a = x * x * (3.0 - 2.0 * x);
                float l = 2.55 * rcp(iADOF_ShapeVertices * iADOF_ShapeVertices * 0.4 - 1.0);
                x = lerp(x, (1.0 + l) * x - a * l, fADOF_ShapeCurvatureAmount);

                float2 sampleOffset = lerp(IN.offset0.xy,IN.offset0.zw, x);
           
                ShapeRoundness(sampleOffset,fADOF_ShapeCurvatureAmount);

                float4 sampleBokeh 	= tex2Dlod(sCommonTex0, float4(IN.txcoord.zw + sampleOffset.xy * (bokehRadiusScaled * iRings),0,0));
                float sampleWeight	= saturate(1e6 * (abs(sampleBokeh.a * 2.0 - 1.0) - CoC * (float)iRings) + 1.0);

#if (ADOF_OPTICAL_VIGNETTE_ENABLE != 0)
                OpticalVignette(sampleOffset.xy * iRings/nRings, centerVec, sampleWeight);
#endif
                sampleBokeh.rgb *= sampleWeight;
                weightSum 		+= sampleWeight;
                BokehSum 		+= sampleBokeh;
                BokehMax 		= max(BokehMax,sampleBokeh);
            }
        }

        IN.offset0.xy = IN.offset0.zw;
        IN.offset0.zw = mul(IN.offset0.zw, IN.offsmat);
    }

   // return lerp(BokehSum / weightSum, BokehMax, fADOF_BokehIntensity * saturate(CoC*nRings*2.0));

   float4 ret = 0;

   BokehSum /= weightSum;

   unpack_hdr(BokehSum.rgb);
   unpack_hdr(BokehMax.rgb);

   	if(iADOF_BokehMode == 0)
	{
		ret = lerp(BokehSum, BokehMax, fADOF_BokehIntensity * saturate(CoC * nRings * 2.0));
	}	
	else if(iADOF_BokehMode == 1)
	{		

		float maxlum = dot(float3(0.3, 0.59, 0.11), BokehMax.rgb);
		float avglum = dot(float3(0.3, 0.59, 0.11), BokehSum.rgb);

		ret = BokehSum * lerp(avglum, maxlum, fADOF_BokehIntensity * saturate(CoC * nRings * 2.0)) / avglum;
	}
	else if(iADOF_BokehMode == 2)
	{
		float maxlum = dot(float3(0.3, 0.59, 0.11), BokehMax.rgb);
		float avglum = dot(float3(0.3, 0.59, 0.11), BokehSum.rgb);

		float bokehweight = max(0, maxlum - avglum);
		bokehweight = bokehweight * fADOF_BokehIntensity * 2.0;
		bokehweight *= bokehweight;

		ret = BokehSum + BokehMax * saturate(bokehweight * CoC * nRings);
	}
	else if(iADOF_BokehMode == 3)
	{
		float maxlum = dot(float3(0.3, 0.59, 0.11), BokehMax.rgb);
		float avglum = dot(float3(0.3, 0.59, 0.11), BokehSum.rgb);

		float bokehweight = maxlum - avglum;
		ret = lerp(BokehSum, BokehMax, saturate(bokehweight * CoC * nRings * fADOF_BokehIntensity));
	}

	pack_hdr(ret.rgb);

	return ret;
}

void PS_DoF_Combine(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
	float4 blurredColor = tex2D(sCommonTex1, IN.txcoord.xy * fADOF_RenderResolutionMult);
	float4 originalColor  = tex2D(qUINT::sBackBufferTex, IN.txcoord.xy);

	float CoC 		= abs(CircleOfConfusion(IN.txcoord.xy, 0));
	float bokehRadiusPixels = CoC2BlurRadius(CoC).x * BUFFER_WIDTH;

    #define linearstep(a,b,x) saturate((x-a)/(b-a))
	float blendWeight = linearstep(DISCRADIUS_RESOLUTION_BOUNDARY_LOWER, DISCRADIUS_RESOLUTION_BOUNDARY_UPPER, bokehRadiusPixels);
	      blendWeight = pow(blendWeight,DISCRADIUS_RESOLUTION_BOUNDARY_CURVE);

	color.rgb      = lerp(originalColor.rgb, blurredColor.rgb, blendWeight);
	color.a        = saturate(CoC * GAUSSIAN_BUILDUP_MULT) * 0.5 + 0.5;
}

void PS_DoF_Gauss1(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
	float4 centerTap = tex2D(sCommonTex0, IN.txcoord.xy);
    float CoC = abs(centerTap.a * 2.0 - 1.0);

    float nSteps 		= floor(CoC * (fADOF_SmootheningAmount + 0.0));
	float expCoeff 		= -2.0 * rcp(nSteps * nSteps + 1e-3); //sigma adjusted for blur width
	float2 blurAxisScaled 	= float2(1,0) * qUINT::PIXEL_SIZE.xy;

	float4 gaussianSum = 0.0;
	float  gaussianSumWeight = 1e-3;

	for(float iStep = -nSteps; iStep <= nSteps; iStep++)
	{
		float currentWeight = exp(iStep * iStep * expCoeff);
		float currentOffset = 2.0 * iStep - 0.5; //Sample between texels to double blur width at no cost

		float4 currentTap = tex2Dlod(sCommonTex0, float4(IN.txcoord.xy + blurAxisScaled.xy * currentOffset, 0, 0));
		currentWeight *= saturate(abs(currentTap.a * 2.0 - 1.0) - CoC * 0.25); //bleed fix

		gaussianSum += currentTap * currentWeight;
		gaussianSumWeight += currentWeight;
	}

	gaussianSum /= gaussianSumWeight;

	color.rgb = lerp(centerTap.rgb, gaussianSum.rgb, saturate(gaussianSumWeight));
    color.a = centerTap.a;
}

void PS_DoF_Gauss2(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
	float4 centerTap = tex2D(sCommonTex1, IN.txcoord.xy);
    float CoC = abs(centerTap.a * 2.0 - 1.0);

	float nSteps 		= min(50,floor(CoC * (fADOF_SmootheningAmount + 0.0)));
	float expCoeff 		= -2.0 * rcp(nSteps * nSteps + 1e-3); //sigma adjusted for blur width
	float2 blurAxisScaled 	= float2(0,1) * qUINT::PIXEL_SIZE.xy;

	float4 gaussianSum = 0.0;
	float  gaussianSumWeight = 1e-3;

	for(float iStep = -nSteps; iStep <= nSteps; iStep++)
	{
		float currentWeight = exp(iStep * iStep * expCoeff);
		float currentOffset = 2.0 * iStep - 0.5; //Sample between texels to double blur width at no cost

		float4 currentTap = tex2Dlod(sCommonTex1, float4(IN.txcoord.xy + blurAxisScaled.xy * currentOffset, 0, 0));
		currentWeight *= saturate(abs(currentTap.a * 2.0 - 1.0) - CoC * 0.25); //bleed fix

		gaussianSum += currentTap * currentWeight;
		gaussianSumWeight += currentWeight;
	}

	gaussianSum /= gaussianSumWeight;

	color.rgb = lerp(centerTap.rgb, gaussianSum.rgb, saturate(gaussianSumWeight));
    color.a = centerTap.a;
}

#if (ADOF_CHROMATIC_ABERRATION_ENABLE != 0)
void PS_DoF_ChromaticAberration(in ADOF_VSOUT IN, out float4 color : SV_Target0)
{
	float4 colorVals[5];
    float3 neighbourOffsets = float3(qUINT::PIXEL_SIZE.xy, 0);

    colorVals[0] = tex2D(sCommonTex0, IN.txcoord.xy);                       //C
    colorVals[1] = tex2D(sCommonTex0, IN.txcoord.xy - neighbourOffsets.xz); //L
    colorVals[2] = tex2D(sCommonTex0, IN.txcoord.xy - neighbourOffsets.zy); //T
    colorVals[3] = tex2D(sCommonTex0, IN.txcoord.xy + neighbourOffsets.xz); //R
    colorVals[4] = tex2D(sCommonTex0, IN.txcoord.xy + neighbourOffsets.zy); //B

	float CoC 			= abs(colorVals[0].a * 2.0 - 1.0);
	float2 bokehRadiusScaled	= CoC2BlurRadius(CoC);

	float4 vGradTwosided = float4(dot(colorVals[0].rgb - colorVals[1].rgb, 1),	 //C - L
	                              dot(colorVals[0].rgb - colorVals[2].rgb, 1),	 //C - T
				                  dot(colorVals[3].rgb - colorVals[0].rgb, 1),	 //R - C
				                  dot(colorVals[4].rgb - colorVals[0].rgb, 1)); 	 //B - C

	float2 vGrad = min(vGradTwosided.xy, vGradTwosided.zw);

	float vGradLen = sqrt(dot(vGrad,vGrad)) + 1e-6;
	vGrad = vGrad / vGradLen * saturate(vGradLen * 32.0) * bokehRadiusScaled * 0.125 * fADOF_ShapeChromaAmount;

	float4 chromaVals[3];

	chromaVals[0] = colorVals[0];
	chromaVals[1] = tex2D(sCommonTex0, IN.txcoord.xy + vGrad);
	chromaVals[2] = tex2D(sCommonTex0, IN.txcoord.xy - vGrad);

	chromaVals[1].rgb = lerp(chromaVals[0].rgb, chromaVals[1].rgb, saturate(4.0 * abs(chromaVals[1].w)));
	chromaVals[2].rgb = lerp(chromaVals[0].rgb, chromaVals[2].rgb, saturate(4.0 * abs(chromaVals[2].w)));

	uint3 chromaMode = (uint3(0,1,2) + iADOF_ShapeChromaMode.xxx) % 3;

	color.rgb = float3(chromaVals[chromaMode.x].r,
		           chromaVals[chromaMode.y].g,
			   chromaVals[chromaMode.z].b);
	color.a = 1.0;
}
#endif

/*=============================================================================
	Techniques
=============================================================================*/

technique ADOF
< ui_tooltip = "                         >> qUINT::ADOF <<\n\n"
               "ADOF is a bokeh depth of field shader.\n"
               "It blurs the scene in front of and behind the focus plane\n"
               "to simulate the behaviour of real lenses. A multitude of features\n"
               "allows to simulate various types of bokeh blur that cameras produce.\n"
               "\nADOF is written by Marty McFly / Pascal Gilcher"; >
{
   /* pass
    {
        VertexShader = VS_ADOF;
        PixelShader = PS_CopyBackBuffer;
        RenderTarget = texOriginal;
    }*/
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader = PS_ReadFocus;
        RenderTarget = ADOF_FocusTex;
    }
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader = PS_CopyFocus;
        RenderTarget = ADOF_FocusTexPrev;
    }
    pass
	{
		VertexShader = VS_ADOF;
		PixelShader  = PS_CoC;
        RenderTarget = CommonTex0;
	}
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_Main;
        RenderTarget = CommonTex1;
    }
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_Combine;
        RenderTarget = CommonTex0;
    }
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_Gauss1;
        RenderTarget = CommonTex1;
    }      
#if(ADOF_CHROMATIC_ABERRATION_ENABLE != 0)
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_Gauss2;
        RenderTarget = CommonTex0;
    }
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_ChromaticAberration;
    }
#else
    pass
    {
        VertexShader = VS_ADOF;
        PixelShader  = PS_DoF_Gauss2;
    }
#endif
}
