/*
	PandaFX version 2.0 for ReShade 3.4.x
	by Jukka Korhonen aka Loadus ~ twitter.com/thatbonsaipanda
	November 2018
	jukka.korhonen@gmail.com
	
	Applies cinematic lens effects and color grading.
	Free licence to copy, modify, tweak and publish but
	if you can, give credit. Thanks. o/
	
	- jP
 */

#include "ReShade.fxh"

namespace PandaFX
{
	// UNIFORMS
	//--------------------------------------------
	uniform bool Enable_Diffusion <
		ui_label = "Enable the lens diffusion effect";
		ui_tooltip = "Enable a light diffusion that emulates the glare of a camera lens.";
	> = true;

	uniform bool Enable_Bleach_Bypass <
		ui_label = "Enable the 'Bleach Bypass' effect";
		ui_tooltip = "Enable a cinematic contrast effect that emulates a bleach bypass on film. Used a lot in war movies and gives the image a grittier feel.";
	> = true;

	uniform bool Enable_Static_Dither <
		ui_label = "Apply static dither";
		ui_tooltip = "Dither the diffusion. Only applies a static dither image texture.";
	> = true;

	uniform bool Enable_Dither <
		ui_label = "Dither the final output";
		ui_tooltip = "Dither the final result of the shader.";
	> = false;

	uniform float Blend_Amount <
		ui_label = "Blend Amount";
		ui_type = "drag";
		ui_min = 0.000;
		ui_max = 1.000;
		ui_step = 0.01;
		ui_tooltip = "Blend the effect with the original image.";
	> = 0.5;

	uniform float Bleach_Bypass_Amount <
		ui_label = "Bleach Bypass Amount";
		ui_type = "drag";
		ui_min = 0.000;
		ui_max = 1.000;
		ui_step = 0.01;
		ui_tooltip = "Adjust the amount of the third diffusion layer.";
	> = 0.5;

	uniform float Contrast_R <
		ui_label = "Contrast (Red)";
		ui_type = "drag";
		ui_min = 0.00001;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Apply contrast to red.";
	> = 0.9;

	uniform float Contrast_G <
		ui_label = "Contrast (Green)";
		ui_type = "drag";
		ui_min = 0.00001;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Apply contrast to green.";
	> = 0.8;

	uniform float Contrast_B <
		ui_label = "Contrast (Blue)";
		ui_type = "drag";
		ui_min = 0.00001;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Apply contrast to blue.";
	> = 0.8;

	uniform float Gamma_R <
		ui_label = "Gamma (Red)";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.00;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to red.";
	> = 1.0;

	uniform float Gamma_G <
		ui_label = "Gamma (Green)";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.00;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to green.";
	> = 1.0;

	uniform float Gamma_B <
		ui_label = "Gamma (Blue)";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.00;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to blue.";
	> = 0.99;

	uniform float Diffusion_1_Amount <
		ui_category = "Diffusion layer 1";
		ui_label = "Amount";
		ui_type = "drag";
		ui_min = 0.000;
		ui_max = 1.000;
		ui_step = 0.01;
		ui_tooltip = "Adjust the amount of the first diffusion layer.";
	> = 0.3;

	uniform float Diffusion_1_Radius <
		ui_category = "Diffusion layer 1";
		ui_label = "Radius";
		ui_type = "drag";
		ui_min = 5.0;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Set the radius of the first diffusion layer.";
	> = 8.0;

	uniform float Diffusion_1_Gamma <
		ui_category = "Diffusion layer 1";
		ui_label = "Gamma";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.0;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to first diffusion layer.";
	> = 0.7;

	uniform float Diffusion_1_Quality <
		ui_category = "Diffusion layer 1";
		ui_label = "Sampling quality";
		ui_type = "drag";
		ui_min = 1.00;
		ui_max = 64.00;
		ui_step = 1.0;
		ui_tooltip = "Set the quality of the first diffusion layer. Number is the divider of how many times the texture size is divided in half. Lower number = higher quality, but more processing needed. (No need to adjust this.)";
	> = 2.0;

	uniform float Diffusion_2_Amount <
		ui_category = "Diffusion layer 2";
		ui_label = "Amount";
		ui_type = "drag";
		ui_min = 0.000;
		ui_max = 1.000;
		ui_step = 0.01;
		ui_tooltip = "Adjust the amount of the second diffusion layer.";
	> = 0.2;

	uniform float Diffusion_2_Radius <
		ui_category = "Diffusion layer 2";
		ui_label = "Radius";
		ui_type = "drag";
		ui_min = 5.0;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Set the radius of the second diffusion layer.";
	> = 5.0;

	uniform float Diffusion_2_Gamma <
		ui_category = "Diffusion layer 2";
		ui_label = "Gamma";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.0;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to second diffusion layer.";
	> = 0.5;

	uniform float Diffusion_2_Quality <
		ui_category = "Diffusion layer 2";
		ui_label = "Sampling quality";
		ui_type = "drag";
		ui_min = 1.00;
		ui_max = 64.00;
		ui_step = 1.0;
		ui_tooltip = "Set the quality of the second diffusion layer. Number is the divider of how many times the texture size is divided in half. Lower number = higher quality, but more processing needed. (No need to adjust this.)";
	> = 16.0;

	uniform float Diffusion_3_Amount <
		ui_category = "Diffusion layer 3";
		ui_label = "Amount";
		ui_type = "drag";
		ui_min = 0.000;
		ui_max = 1.000;
		ui_step = 0.01;
		ui_tooltip = "Adjust the amount of the third diffusion layer.";
	> = 1.0;

	uniform float Diffusion_3_Radius <
		ui_category = "Diffusion layer 3";
		ui_label = "Radius";
		ui_type = "drag";
		ui_min = 5.0;
		ui_max = 20.0;
		ui_step = 0.01;
		ui_tooltip = "Set the radius of the third diffusion layer.";
	> = 7.4;

	uniform float Diffusion_3_Gamma <
		ui_category = "Diffusion layer 3";
		ui_label = "Gamma";
		ui_type = "drag";
		ui_min = 0.02;
		ui_max = 5.0;
		ui_step = 0.01;
		ui_tooltip = "Apply Gamma to third diffusion layer.";
	> = 5.0;

	uniform float Diffusion_3_Quality <
		ui_category = "Diffusion layer 3";
		ui_label = "Sampling quality";
		ui_type = "drag";
		ui_min = 1.00;
		ui_max = 64.00;
		ui_step = 1.0;
		ui_tooltip = "Set the quality of the third diffusion layer. Number is the divider of how many times the texture size is divided in half. Lower number = higher quality, but more processing needed. (No need to adjust this.)";
	> = 3.0;

	uniform int framecount < source = "framecount"; >;


	// TEXTURES
	//--------------------------------------------
	texture NoiseTex <source = "hd_noise.png"; > { Width = 1920; Height = 1080; Format = RGBA8; };
	texture prePassLayer { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
	texture blurLayerHorizontal { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBA8; };
	texture blurLayerVertical { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBA8; };
	texture blurLayerHorizontalMedRes { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA8; };
	texture blurLayerVerticalMedRes { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA8; };
	texture blurLayerHorizontalLoRes { Width = BUFFER_WIDTH / 64; Height = BUFFER_HEIGHT / 64; Format = RGBA8; };
	texture blurLayerVerticalLoRes { Width = BUFFER_WIDTH / 64; Height = BUFFER_HEIGHT / 64; Format = RGBA8; };


	// SAMPLERS
	//--------------------------------------------
	sampler2D NoiseSampler { Texture = NoiseTex; };
	sampler2D PFX_PrePassLayer { Texture = prePassLayer; };
	// ------- samplers for large radius blur
	sampler2D PFX_blurHorizontalLayer { Texture = blurLayerHorizontal; };
	sampler2D PFX_blurVerticalLayer { Texture = blurLayerVertical; };
	sampler2D PFX_blurHorizontalLayerMedRes { Texture = blurLayerHorizontalMedRes; };
	sampler2D PFX_blurVerticalLayerMedRes { Texture = blurLayerVerticalMedRes; };
	sampler2D PFX_blurHorizontalLayerLoRes { Texture = blurLayerHorizontalLoRes; };
	sampler2D PFX_blurVerticalLayerLoRes { Texture = blurLayerVerticalLoRes; };


	// FUNCTIONS
	//--------------------------------------------
	float AdjustableSigmoidCurve (float value, float amount) 
	{
		return value < 0.5 ? pow(value, amount) * pow(2.0, amount) * 0.5 
						   : 1.0 - pow(1.0 - value, amount) * pow(2.0, amount) * 0.5;
	}

	float Randomize (float2 coord) 
	{
		return clamp((frac(sin(dot(coord, float2(12.9898, 78.233))) * 43758.5453)), 0.0, 1.0);
	}

	float SigmoidCurve (float value) 
	{
		value = value * 2.0 - 1.0;
		return -value * abs(value) * 0.5 + value + 0.5;	
	}

	float4 BlurH (sampler2D input, float2 uv, float radius, float sampling) 
	{
		float4 avgColor = float4(0.0, 0.0, 0.0, 0.0);
		float width = 1.0 / BUFFER_WIDTH * (sampling + (sampling==0));
		float4 tapCoord = float4(0, 0, 0, 0);
		for (float x = -radius; x <= radius; x++)
		{
			float2 coordinate = uv + float2(x * width, 0.0);
			tapCoord.xy = clamp(coordinate, 0.0, 1.0); 
			float weight = SigmoidCurve(1.0 - (abs(x) / (radius + (radius==0))));
			avgColor.rgb += tex2Dlod(input, tapCoord).rgb * weight;
			avgColor.a += weight;
		}
		
		avgColor.rgb /= (avgColor.a + (avgColor.a==0));
		avgColor.a = 1.0;
		return avgColor;
	}

	float4 BlurV (sampler input, float2 uv, float radius, float sampling) 
	{
		float4 avgColor = float4(0.0, 0.0, 0.0, 0.0);
		float height = 1.0 / BUFFER_HEIGHT * sampling;
		float4 tapCoord = float4(0, 0, 0, 0);
		for (float y = -radius; y <= radius; y++)
		{
			float2 coordinate = uv + float2(0.0, y * height);
			tapCoord.xy = clamp(coordinate, 0.0, 1.0); 
			float weight = SigmoidCurve(1.0 - (abs(y) / (radius + (radius==0))));
			avgColor.rgb += tex2Dlod(input, tapCoord).rgb * weight;
			avgColor.a += weight;
		}
		
		avgColor.rgb /= (avgColor.a + (avgColor.a==0));
		avgColor.a = 1.0;
		return avgColor;
	}



	// SHADERS
	//--------------------------------------------
	void PS_PrePass (float4 pos : SV_Position, 
					 float2 uv : TEXCOORD, 
					 out float4 result : SV_Target) 
	{
		float4 A = tex2D(ReShade::BackBuffer, uv);
		A.r = pow(A.r, Gamma_R);
		A.g = pow(A.g, Gamma_G);
		A.b = pow(A.b, Gamma_B);
		A.r = AdjustableSigmoidCurve(A.r, Contrast_R);
		A.g = AdjustableSigmoidCurve(A.g, Contrast_G);
		A.b = AdjustableSigmoidCurve(A.b, Contrast_B);
		
		// ------- Change color weights of the final render, similar to a printed film
		A.g = A.g * 0.8 + A.b * 0.2;

		float red = clamp(A.r - A.g - A.b, 0.0, 1.0);
		float green = clamp(A.g - A.r - A.b, 0.0, 1.0);
		float blue = clamp(A.b - A.r - A.g, 0.0, 1.0);

		A = A * (1.0 - red * 0.6);
		A = A * (1.0 - green * 0.8);
		A = A * (1.0 - blue * 0.3);

		result = A;
	}


	void PS_HorizontalPass (float4 pos : SV_Position, 
							float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurH(PFX_PrePassLayer, uv, Diffusion_1_Radius, Diffusion_1_Quality);
	}

	void PS_VerticalPass (float4 pos : SV_Position, 
						  float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurV(PFX_blurHorizontalLayer, uv, Diffusion_1_Radius, Diffusion_1_Quality);
	}

	void PS_HorizontalPassMedRes (float4 pos : SV_Position, 
							float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurH(PFX_blurVerticalLayer, uv, Diffusion_2_Radius, Diffusion_2_Quality);
	}

	void PS_VerticalPassMedRes (float4 pos : SV_Position, 
						  float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurV(PFX_blurHorizontalLayerMedRes, uv, Diffusion_2_Radius, Diffusion_2_Quality);
	}

	void PS_HorizontalPassLoRes (float4 pos : SV_Position, 
							float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurH(PFX_blurVerticalLayerMedRes, uv, Diffusion_3_Radius, Diffusion_3_Quality);
	}

	void PS_VerticalPassLoRes (float4 pos : SV_Position, 
						  float2 uv : TEXCOORD, out float4 result : SV_Target) 
	{
		result = BlurV(PFX_blurHorizontalLayerLoRes, uv, Diffusion_3_Radius, Diffusion_3_Quality);
	}

	float4 PS_Composition (float4 vpos : SV_Position, 
							 float2 uv : TEXCOORD) : SV_Target 
	{
		// ------- Create blurred layers for lens diffusion
		float4 blurLayer;
		float4 blurLayerMedRes;
		float4 blurLayerLoRes;

		// ------- Read original image
		float4 A = tex2D(PFX_PrePassLayer, uv);
		float4 O = tex2D(ReShade::BackBuffer, uv);
		float3 rndSample = tex2D(NoiseSampler, uv).rgb;
		
		if (Enable_Diffusion)
		{
			// TODO enable/disable for performance >>
			blurLayer = tex2D(PFX_blurVerticalLayer, uv);
			blurLayerMedRes = tex2D(PFX_blurVerticalLayerMedRes, uv);
			blurLayerLoRes = tex2D(PFX_blurVerticalLayerLoRes, uv);

			// ------- Colorize the blur layers
			blurLayerMedRes = lerp(blurLayerMedRes, dot(0.3333, blurLayerMedRes.rgb), 0.75);
			blurLayerLoRes = lerp(blurLayerLoRes, dot(0.3333, blurLayerLoRes.rgb), 0.75);

			// ------- Set blur layer weights
			blurLayer *= Diffusion_1_Amount;
			blurLayerMedRes *= Diffusion_2_Amount;
			blurLayerLoRes *= Diffusion_3_Amount;
		
			blurLayer = pow(blurLayer, Diffusion_1_Gamma);
			blurLayerMedRes = pow(blurLayerMedRes, Diffusion_2_Gamma);
			blurLayerLoRes = pow(blurLayerLoRes, Diffusion_3_Gamma);

			if (Enable_Static_Dither)
			{
				float3 hd_noise = 1.0 - (rndSample * 0.01);
				blurLayer.rgb = 1.0 - hd_noise * (1.0 - blurLayer.rgb);
				blurLayerMedRes.rgb = 1.0 - hd_noise * (1.0 - blurLayerMedRes.rgb);
				blurLayerLoRes.rgb = 1.0 - hd_noise * (1.0 - blurLayerLoRes.rgb);
			}

			blurLayer = clamp(blurLayer, 0.0, 1.0);
			blurLayerMedRes = clamp(blurLayerMedRes, 0.0, 1.0);
			blurLayerLoRes = clamp(blurLayerLoRes, 0.0, 1.0);

			// ------- Screen blend the blur layers to create lens diffusion
			A.rgb = 1.0 - (1.0 - blurLayer.rgb) * (1.0 - A.rgb);
			A.rgb = 1.0 - (1.0 - blurLayerMedRes.rgb) * (1.0 - A.rgb);
			A.rgb = 1.0 - (1.0 - blurLayerLoRes.rgb) * (1.0 - A.rgb);
		}

		// ------ Compress contrast using Hard Light blending ------
		float Ag = dot(float3(0.3333, 0.3333, 0.3333), A.rgb);
		float4 C = (Ag > 0.5) ? 1 - 2 * (1 - Ag) * (1 - A) : 2 * Ag * A;
		C = pow(C, 0.6);
		A = lerp(A, C, Enable_Bleach_Bypass ? Bleach_Bypass_Amount : 0);

		// ------ Compress to TV levels if needed ------
		// A = A * 0.9373 + 0.0627;
		// ------ Create noise for dark level dither (heavy processing!) ------
		if (Enable_Dither)
		{
			float uvRnd = Randomize(rndSample.rg * framecount);
			float uvRnd2 = Randomize(rndSample.rg * framecount + 1);
			A -= tex2D(NoiseSampler, uv * uvRnd2).r * 0.04;
			A += tex2D(NoiseSampler, uv * uvRnd).r * 0.04;
		}
		return lerp(O, A, Blend_Amount);
	}

	// TECHNIQUES
	//--------------------------------------------
	technique PandaFX 
	{
		pass PreProcess	
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_PrePass;
			RenderTarget = prePassLayer;
		}

		pass HorizontalPass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_HorizontalPass;
			RenderTarget = blurLayerHorizontal;
		}

		pass VerticalPass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_VerticalPass;
			RenderTarget = blurLayerVertical;
		}

		pass HorizontalPassMedRes
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_HorizontalPassMedRes;
			RenderTarget = blurLayerHorizontalMedRes;
		}

		pass VerticalPassMedRes
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_VerticalPassMedRes;
			RenderTarget = blurLayerVerticalMedRes;
		}

		pass HorizontalPassLoRes
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_HorizontalPassLoRes;
			RenderTarget = blurLayerHorizontalLoRes;
		}

		pass VerticalPassLoRes
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_VerticalPassLoRes;
			RenderTarget = blurLayerVerticalLoRes;
		}

		pass CustomPass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_Composition ;
		}
	}
}