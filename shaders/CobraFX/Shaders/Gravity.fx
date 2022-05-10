/////////////////////////////////////////////////////////
// Gravity.fx by SirCobra
// Version 0.1
// currently VERY resource-intensive
// This effect lets pixels gravitate towards the bottom in a 3D environment.
// It uses a custom seed (currently the Mandelbrot set) to determine the intensity of each pixel.
// Make sure to also test out the texture-RNG variant with the picture "gravityrng.png" provided in Textures.
// You can replace the texture with your own picture, as long as it is 1920x1080, RGBA8 and has the same name.
// Only the red-intensity is taken. So either use red images or greyscale images.
// The effect can be applied to a specific area like a DoF shader. The basic methods for this were taken with permission
// from https://github.com/FransBouma/OtisFX/blob/master/Shaders/Emphasize.fx
// Thanks to Blue1992Shader for optimizing the code
// Thanks to FransBouma, Lord of Lunacy and Annihlator for advise on my first shader :)
/////////////////////////////////////////////////////////

//render targets don't work with dx9
#if (__RENDERER__ >= 0xa000)
	#define GRAVITY_COMPUTE 1
#else
	#define GRAVITY_COMPUTE 0
#endif

#if GRAVITY_COMPUTE != 0

//
// UI
//

uniform float GravityIntensity <
	ui_type = "drag";
ui_min = 0.000; ui_max = 1.000;
ui_step = 0.01;
ui_tooltip = "Gravity strength. Higher values look cooler but increase the computation time by a lot!";
> = 0.5;
uniform float GravityRNG <
	ui_type = "drag";
ui_min = 0.01; ui_max = 0.99;
ui_step = 0.02;
ui_tooltip = "Changes the RNG for each pixel.";
> = 75;
uniform bool useImage <
	ui_tooltip = "Changes the RNG to the input image called gravityrng.png located in Textures. You can change the image for your own seed as long as the name and resolution stay the same.";
> = false;
uniform bool allowOverlapping <
	ui_tooltip = "This way the effect does not get hidden behind other objects.";
> = false;
uniform float FocusDepth <
	ui_type = "drag";
ui_min = 0.000; ui_max = 1.000;
ui_step = 0.001;
ui_tooltip = "Manual focus depth of the point which has the focus. Range from 0.0, which means camera is the focus plane, till 1.0 which means the horizon is focus plane.";
> = 0.026;
uniform float FocusRangeDepth <
	ui_type = "drag";
ui_min = 0.0; ui_max = 1.000;
ui_step = 0.001;
ui_tooltip = "The depth of the range around the manual focus depth which should be emphasized. Outside this range, de-emphasizing takes place";
> = 0.001;
uniform float FocusEdgeDepth <
	ui_type = "drag";
ui_min = 0.000; ui_max = 1.000;
ui_tooltip = "The depth of the edge of the focus range. Range from 0.00, which means no depth, so at the edge of the focus range, the effect kicks in at full force,\ntill 1.00, which means the effect is smoothly applied over the range focusRangeEdge-horizon.";
ui_step = 0.001;
> = 0.050;
uniform bool Spherical <
	ui_tooltip = "Enables Emphasize in a sphere around the focus-point instead of a 2D plane";
> = false;
uniform int Sphere_FieldOfView <
	ui_type = "drag";
ui_min = 1; ui_max = 180;
ui_tooltip = "Specifies the estimated Field of View you are currently playing with. Range from 1, which means 1 Degree, till 180 which means 180 Degree (half the scene).\nNormal games tend to use values between 60 and 90.";
> = 75;
uniform float Sphere_FocusHorizontal <
	ui_type = "drag";
ui_min = 0; ui_max = 1;
ui_tooltip = "Specifies the location of the focuspoint on the horizontal axis. Range from 0, which means left screen border, till 1 which means right screen border.";
> = 0.5;
uniform float Sphere_FocusVertical <
	ui_type = "drag";
ui_min = 0; ui_max = 1;
ui_tooltip = "Specifies the location of the focuspoint on the vertical axis. Range from 0, which means upper screen border, till 1 which means bottom screen border.";
> = 0.5;
uniform float3 BlendColor <
	ui_type = "color";
ui_tooltip = "Specifies the blend color to blend with the greyscale. in (Red, Green, Blue). Use dark colors to darken further away objects";
> = float3(0.55, 1.0, 0.95);
uniform float EffectFactor <
	ui_type = "drag";
ui_min = 0.0; ui_max = 1.0;
ui_tooltip = "Specifies the factor the desaturation is applied. Range from 0.0, which means the effect is off (normal image), till 1.0 which means the desaturated parts are\nfull greyscale (or color blending if that's enabled)";
> = 1.0;
uniform float3 FilterColor <
	ui_type = "color";
ui_tooltip = "addDesc";
> = float3(0.55, 1.0, 0.95);
uniform float FilterRange <
	ui_type = "drag";
ui_min = 0; ui_max = 1.74;
ui_tooltip = "addDesc";
> = 0.5;

#include "Reshade.fxh"

#ifndef M_PI
#define M_PI 3.1415927
#endif

//
// textures
//
namespace alt {

	texture texGravityDistanceMap{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R16F; };
	texture texGravityCurrentSeed{ Format = R16F; };
	texture texGravitySeedMapExt < source = "gravityrng.png"; > { Width = 1920; Height = 1080; Format = RGBA8; };

	// raw depth, CoC,  GravitySeed, reserved
	texture texGravityBuf{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	sampler2D SamplerGravityBuf{ Texture = texGravityBuf; };
	//
	// samplers
	//

	sampler2D SamplerGravityDistanceMap{ Texture = texGravityDistanceMap; };
	sampler2D SamplerGravityCurrentSeed{ Texture = texGravityCurrentSeed; };
	sampler2D SamplerGravitySeedMapExt{ Texture = texGravitySeedMapExt; };
	//
	// code
	//
	// Calculate Focus Intensity
	float CalculateDepthDiffCoC(float2 texcoord : TEXCOORD)
	{
		float4 col_val = tex2D(ReShade::BackBuffer, texcoord);
		const float scenedepth = ReShade::GetLinearizedDepth(texcoord);
		const float scenefocus = FocusDepth;
		const float desaturateFullRange = FocusRangeDepth + FocusEdgeDepth;
		float depthdiff;
		texcoord.x = (texcoord.x - Sphere_FocusHorizontal)*ReShade::ScreenSize.x;
		texcoord.y = (texcoord.y - Sphere_FocusVertical)*ReShade::ScreenSize.y;
		const float degreePerPixel = Sphere_FieldOfView / ReShade::ScreenSize.x;
		const float fovDifference = sqrt((texcoord.x*texcoord.x) + (texcoord.y*texcoord.y))*degreePerPixel;
		depthdiff = Spherical ? sqrt((scenedepth*scenedepth) + (scenefocus*scenefocus) - (2 * scenedepth*scenefocus*cos(fovDifference*(2 * M_PI / 360)))) : depthdiff = abs(scenedepth - scenefocus);
		float coc_val = (1 - saturate((depthdiff > desaturateFullRange) ? 1.0 : smoothstep(FocusRangeDepth, desaturateFullRange, depthdiff)));
		return ((distance(col_val.rgb,FilterColor.rgb) < FilterRange) ? coc_val : 0.0);
	}

	//calculate Mandelbrot Seed
	//inspired by http://nuclear.mutantstargoat.com/articles/sdr_fract/
	float mandelbrotRNG(float2 texcoord: TEXCOORD)
	{
		float2 center = float2(0.675, 0.46); // an interesting center at the mandelbrot for our zoom
		float zoom = 0.033*GravityRNG; // smaller numbers increase zoom
		float aspect = ReShade::ScreenSize.x / ReShade::ScreenSize.y; // format to screenspace
		float2 z, c;
		c.x = aspect * (texcoord.x - 0.5) * zoom - center.x;
		c.y = (texcoord.y - 0.5) * zoom - center.y;
		int i;
		z = c;

		for (i = 0; i < 100; i++)
		{
			float x = z.x*z.x - z.y*z.y + c.x;
			float y = 2 * z.x*z.y + c.y;
			if ((x*x + y * y) > 4.0) break;
			z.x = x;
			z.y = y;
		}

		float intensity = 1.0;
		return saturate(((intensity * (i == 100 ? 0.0 : float(i)) / 100) - 0.8) / 0.22);
	}
	// Calculates the maximum Distance Map
	// For every pixel in GravityIntensity: If GravityIntensity*mandelbrot > j*offset.y : set new real max distance
	float distance_main(float2 texcoord: TEXCOORD)
	{
		float real_max_distance = 0.0;
		float2 offset = float2(0.0, BUFFER_RCP_HEIGHT);
		int iterations = round(min(texcoord.y, GravityIntensity) * BUFFER_HEIGHT);
		int j;

		for (j = 0; j < iterations; j++)
		{

			// 4. RNG
			float rng_value = tex2Dlod(SamplerGravityBuf, float4(texcoord - j * offset, 0, 1)).b;
			float tex_distance_max = GravityIntensity * rng_value;
			if ((tex_distance_max) > (j * offset.y))
			{
				real_max_distance = j * offset.y; // new max threshold
			}
		}
		return  real_max_distance;
	}



	// Applies Gravity to the Pixels recursively
	float4 Gravity_main(float4 vpos, float2 texcoord : TEXCOORD)
	{
		float real_max_distance = tex2Dfetch(SamplerGravityDistanceMap, vpos.xy).r;
		int iterations = round(real_max_distance * BUFFER_HEIGHT);

		vpos.z = 0;
		float4 sample_pos = vpos;
		for (float depth = tex2Dfetch(SamplerGravityBuf, vpos.xy).x;
			vpos.z < iterations; ++vpos.z, --vpos.y)
		{
			float4 samp = tex2Dfetch(SamplerGravityBuf, vpos.xy);
			samp.w *= samp.z;

			[flatten]
			if (!any(samp <= float4(depth-allowOverlapping, 0.01, 0.05, vpos.z))) {
				sample_pos = vpos;
				sample_pos.z /= samp.w;
				depth = samp.x;
			}
		}

		float4 colFragment = tex2Dfetch(ReShade::BackBuffer, sample_pos.xy);
		return lerp(colFragment, float4(BlendColor, 1.0), sample_pos.z * EffectFactor);
	}

	float rng_delta()
	{
		float2 old_rng = tex2Dfetch(SamplerGravityCurrentSeed, (0).xx).x;
		float new_rng = GravityRNG + useImage * 0.01 + GravityIntensity;
		return old_rng.x - new_rng;
	}

	void vs_rng_generate(uint vid : SV_VERTEXID, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD)
	{
		PostProcessVS(vid, pos, uv);
		pos.xy *= abs(rng_delta()) > 0.005;
	}

	//RNG MAP
	void rng_generate(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		float value = tex2D(SamplerGravitySeedMapExt, texcoord).r;
		value = saturate((value - 1 + GravityRNG) / GravityRNG);
		fragment = useImage ? value : mandelbrotRNG(texcoord);
	}


	void vs_dist_generate(uint vid : SV_VERTEXID, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD)
	{
		PostProcessVS(vid, pos, uv);
		pos.xy *= abs(rng_delta()) > 0.005;
	}

	//DISTANCE MAP
	void dist_generate(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target)
	{
		fragment = distance_main(texcoord);
	}

	//COC + SEED
	void coc_generate(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		vpos.w = 0;
		fragment.x = -ReShade::GetLinearizedDepth(texcoord);
		fragment.y = CalculateDepthDiffCoC(texcoord);
		fragment.zw = GravityIntensity * fragment.y*BUFFER_HEIGHT;
	}
	void rng_update_seed(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target)
	{
		fragment = GravityRNG + useImage * 0.01 + GravityIntensity;
	}
	//MAIN FUNCTION
	void gravity_func(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 outFragment : SV_Target)
	{
		vpos.w = 0;
		outFragment = Gravity_main(vpos, texcoord);
	}

	// PRECALC
	void vs_rng_generate2(uint vid : SV_VERTEXID, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD)
	{
		PostProcessVS(vid, pos, uv);
		pos.xy *= true;
	}
	void vs_dist_generate2(uint vid : SV_VERTEXID, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD)
	{
		PostProcessVS(vid, pos, uv);
		pos.xy *= true;
	}
#define ENABLE_RED (1 << 0)
#define ENABLE_GREEN (1 << 1)
#define ENABLE_BLUE (1 << 2)
#define ENABLE_ALPHA (1 << 3)

	technique PreGravity < hidden = true; enabled = true; timeout = 2000; >
	{
		pass GenerateRNG { VertexShader = vs_rng_generate2; PixelShader = rng_generate; RenderTarget = texGravityBuf; RenderTargetWriteMask = ENABLE_BLUE; }
		pass GenerateDistance { VertexShader = vs_dist_generate2; PixelShader = dist_generate; RenderTarget = texGravityDistanceMap; }
		pass GenerateCoC { VertexShader = PostProcessVS; PixelShader = coc_generate; RenderTarget = texGravityBuf; RenderTargetWriteMask = ENABLE_RED | ENABLE_GREEN | ENABLE_ALPHA; }
	}

	technique Gravity
	{
		pass GenerateRNG { VertexShader = vs_rng_generate; PixelShader = rng_generate; RenderTarget = texGravityBuf; RenderTargetWriteMask = ENABLE_BLUE; }
		// dist to max scather point.
		pass GenerateDistance { VertexShader = vs_dist_generate; PixelShader = dist_generate; RenderTarget = texGravityDistanceMap; }

		// also populate x with raw depth.
		pass GenerateCoC { VertexShader = PostProcessVS; PixelShader = coc_generate; RenderTarget = texGravityBuf; RenderTargetWriteMask = ENABLE_RED | ENABLE_GREEN | ENABLE_ALPHA; }

		pass UpdateRNGSeed { VertexShader = PostProcessVS; PixelShader = rng_update_seed; RenderTarget = texGravityCurrentSeed; }

		pass ApplyGravity { VertexShader = PostProcessVS; PixelShader = gravity_func; }
	}
}
/* About the Pipeline:
	We generate the RNGMap interactively. It's our intensity function.
	Then comes a map which generates the maximum distance a pixel has to search only based on RNGMap and GravityStrength.
	Then we update the current settings. Only if the settings change the above steps have to be executed again.
	Then we apply Gravity including the DepthMap and colours.
*/

#endif // Shader End