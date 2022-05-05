/*
 Denoising by NVIDIA Corporation
 http://developer.download.nvidia.com/SDK/10/direct3d/Source/Denoising/doc/Denoising.pdf
 
 Copyright 2008 NVIDIA Corporation 

 BY DOWNLOADING THE SOFTWARE AND OTHER AVAILABLE MATERIALS, YOU 
 ("DEVELOPER") AGREE TO BE BOUND BY THE FOLLOWING TERMS AND CONDITIONS 

 The materials available for download to Developers may include software 
 in both sample source ("Source Code") and object code ("Object Code") 
 versions, documentation ("Documentation"), certain art work ("Art 
 Assets") and other materials (collectively, these materials referred to 
 herein as "Materials"). Except as expressly indicated herein, all terms 
 and conditions of this Agreement apply to all of the Materials. 

 Except as expressly set forth herein, NVIDIA owns all of the Materials 
 and makes them available to Developer only under the terms and 
 conditions set forth in this Agreement. 

 License: Subject to the terms of this Agreement, NVIDIA hereby grants to 
 Developer a royalty-free, non-exclusive license to possess and to use 
 the Materials. The following terms apply to the specified type of 
 Material: 

 Source Code: Developer shall have the right to modify and create
 derivative works with the Source Code. Developer shall own any
 derivative works ("Derivatives") it creates to the Source Code, provided
 that Developer uses the Materials in accordance with the terms of this
 Agreement. Developer may distribute the Derivatives, provided that all
 NVIDIA copyright notices and trademarks are used properly and the
 Derivatives include the following statement: "This software contains
 source code provided by NVIDIA Corporation."

 Object Code: Developer agrees not to disassemble, decompile or reverse
 engineer the Object Code versions of any of the Materials. Developer 
 acknowledges that certain of the Materials provided in Object Code
 version may contain third party components that may be subject to
 restrictions, and expressly agrees not to attempt to modify or
 distribute such Materials without first receiving consent from NVIDIA.

 Art Assets: Developer shall have the right to modify and create
 Derivatives of the Art Assets, but may not distribute any of the Art
 Assets or Derivatives created therefrom without NVIDIA�s prior written
 consent.

 Government End Users: If you are acquiring the Software on behalf of any
 unit or agency of the United States Government, the following provisions
 apply. The Government agrees the Software and documentation were
 developed at private expense and are provided with �RESTRICTED
 RIGHTS�. Use, duplication, or disclosure by the Government is subject
 to restrictions as set forth in DFARS 227.7202-1(a) and 227.7202-3(a)
 (1995), DFARS 252.227-7013(c)(1)(ii) (Oct 1988), FAR 12.212(a)(1995),
 FAR 52.227-19, (June 1987) or FAR 52.227-14(ALT III) (June 1987),as
 amended from time to time. In the event that this License, or any part
 thereof, is deemed inconsistent with the minimum rights identified in
 the Restricted Rights provisions, the minimum rights shall prevail. No
 Other License. No rights or licenses are granted by NVIDIA under this
 License, expressly or by implication, with respect to any proprietary
 information or patent, copyright, trade secret or other intellectual
 property right owned or controlled by NVIDIA, except as expressly
 provided in this License. Term: This License is effective until
 terminated. NVIDIA may terminate this Agreement (and with it, all of
 Developer�s right to the Materials) immediately upon written notice
 (which may include email) to Developer, with or without cause.

 Support: NVIDIA has no obligation to support or to continue providing or
 updating any of the Materials.

 No Warranty: THE SOFTWARE AND ANY OTHER MATERIALS PROVIDED BY NVIDIA TO
 DEVELOPER HEREUNDER ARE PROVIDED "AS IS." NVIDIA DISCLAIMS ALL
 WARRANTIES, EXPRESS, IMPLIED OR STATUTORY, INCLUDING, WITHOUT
 LIMITATION, THE IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

 LIMITATION OF LIABILITY: NVIDIA SHALL NOT BE LIABLE TO DEVELOPER,
 DEVELOPER�S CUSTOMERS, OR ANY OTHER PERSON OR ENTITY CLAIMING THROUGH
 OR UNDER DEVELOPER FOR ANY LOSS OF PROFITS, INCOME, SAVINGS, OR ANY
 OTHER CONSEQUENTIAL, INCIDENTAL, SPECIAL, PUNITIVE, DIRECT OR INDIRECT
 DAMAGES (WHETHER IN AN ACTION IN CONTRACT, TORT OR BASED ON A WARRANTY),
 EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF THE
 ESSENTIAL PURPOSE OF ANY LIMITED REMEDY. IN NO EVENT SHALL NVIDIA�S
 AGGREGATE LIABILITY TO DEVELOPER OR ANY OTHER PERSON OR ENTITY CLAIMING
 THROUGH OR UNDER DEVELOPER EXCEED THE AMOUNT OF MONEY ACTUALLY PAID BY
 DEVELOPER TO NVIDIA FOR THE SOFTWARE OR ANY OTHER MATERIALS

 Modified and optimized for ReShade by JPulowski

 Do not distribute without giving credit to the original author(s).

 1.0  - Initial release
*/

#include "ReShadeUI.fxh"

uniform float NoiseLevel < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.01; ui_max = 1.00;
	ui_label = "Noise Level";
	ui_tooltip = "Approximate level of noise in the image.";
> = 0.15;

uniform float LerpCoefficeint < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Lerp Coefficient";
	ui_tooltip = "Amount of blending between the original and the processed image.";
> = 0.8;

uniform float WeightThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Weight Threshold";
> = 0.03;

uniform float CounterThreshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Counter Threshold";
> = 0.05;

uniform float GaussianSigma < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1.0; ui_max = 100.0;
	ui_label = "Gaussian Sigma";
	ui_tooltip = "Controls the additional amount of gaussian blur on the image.";
> = 50.0;

// Preprocessor definitions

// Currently it is not possible to directly integrate preprocessor variables into user interface.
// Add "WindowRadius=3" and "BlockRadius=2" to "PreprocessorDefinitions=" under "[GENERAL]" in your .ini file
// to modify window and block radius from the in-game interface. Don't forget to reload settings after modifying them.

#ifndef WindowRadius
	#define WindowRadius 3	//[1 to 10] Higher values require more performance but might increase the image quality.
#endif

#ifndef BlockRadius
	#define BlockRadius 2	//[1 to 10] (NLM only) For tweaking the amount of noise on edges, higher values require more performance.
#endif

#include "ReShade.fxh"

float3 PS_Denoise_KNN(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_TARGET {
	float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float3 texIJ;
	float weight;
	float3 result = 0.0;
	float counter = 0.0;
	float sum = 0.0;
	
	float iWindowArea = 2.0 * WindowRadius + 1.0;
	iWindowArea *= iWindowArea;

	for (int i = -WindowRadius; i <= WindowRadius; i++) {
		for (int j = -WindowRadius; j <= WindowRadius; j++) {
			texIJ = tex2D(ReShade::BackBuffer, texcoord + ReShade::PixelSize * float2(i, j)).rgb;
			weight = dot(orig - texIJ, orig - texIJ);

			weight = exp(-(weight * rcp(NoiseLevel) + (i * i + j * j) * rcp(GaussianSigma)));
			counter += weight > WeightThreshold;
			
			sum += weight;
			
			result.rgb += texIJ * weight;
		}
	}
            
	result /= sum;
	float lerpQ = (counter > (CounterThreshold * iWindowArea)) ? 1.0 - LerpCoefficeint : LerpCoefficeint;
	result = lerp(result, orig, lerpQ);
	
	return result;
}

float3 PS_Denoise_NLM(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_TARGET {
	float3 result = 0.0;
	float3 texIJb;
	float3 texIJc;
	float counter = 0.0;
	float weight;  
	float sum = 0.0;
    
	float invBlockArea = 2.0 * BlockRadius + 1.0;
	invBlockArea = rcp(invBlockArea * invBlockArea);
    
	for (int i = -WindowRadius; i <= WindowRadius; i++) {
        for (int j = -WindowRadius; j <= WindowRadius; j++) {
				
				weight = 0.0;
				
				for (int n = -BlockRadius; n <= BlockRadius; n++) {
					for (int m = -BlockRadius; m <= BlockRadius; m++) {              
							texIJb = tex2D(ReShade::BackBuffer, texcoord + ReShade::PixelSize * float2(i + n, j + m)).rgb;
							texIJc = tex2D(ReShade::BackBuffer, texcoord + ReShade::PixelSize * float2(    n,     m)).rgb;
							weight = dot(texIJb - texIJc, texIJb - texIJc) + weight;
					}
                }
				texIJc = tex2D(ReShade::BackBuffer, texcoord + ReShade::PixelSize * float2(i, j)).rgb;

				weight *= invBlockArea;
				weight = exp(-(weight * rcp(NoiseLevel) + (i * i + j * j) * rcp(GaussianSigma)));
	            
				counter += weight > WeightThreshold;

				sum += weight;

				result += texIJc * weight;
        }
	}
	
	float iWindowArea = 2.0 * WindowRadius + 1.0;
	iWindowArea *= iWindowArea;

	float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;

    result /= sum;
	float lerpQ = (counter > (CounterThreshold * iWindowArea)) ? 1.0 - LerpCoefficeint : LerpCoefficeint;
	result = lerp(result, orig, lerpQ);
    
	return result;
}

technique KNearestNeighbors
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Denoise_KNN;
	}
}

technique NonLocalMeans
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Denoise_NLM;
	}
}
