/////////////////////////////////////////////////////////
// RealisticLongExposure.fx by SirCobra
// Version 0.2
// You can find descriptions and my other shaders here: https://github.com/LordKobra/CobraFX
// --------Description---------
// It will take the games input for a defined amount of seconds to create the final image, just as a camera would do in real life.
/////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************   UI & Defines   *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

// Shader Start
#include "Reshade.fxh"
uniform float timer < source = "timer"; > ;

//Namespace Everything
namespace RealisticLongExposure {

	//ui
	uniform float RealExposureDuration <
		ui_type = "slider";
		ui_min = 0.1; ui_max = 120;
		ui_step = 0.1;
		ui_tooltip = "Exposure Duration in seconds.";
	> = 1;
	uniform bool StartExposure <
		ui_tooltip = "Click to start the Exposure Process. It will run for the given amount of seconds and then freeze. Tip: Bind this to a hotkey to use it conveniently.";
	> = false;
	uniform bool ShowGreenOnFinish <
		ui_tooltip = "Display a green dot at the top to signalize the exposure has finished and entered preview mode.";
	> = false;
	uniform float ISO <
		ui_type = "slider";
		ui_min = 100; ui_max = 1600;
		ui_step = 1;
		ui_tooltip = "ISO. 100 is normalized to the game. 1600 is 16 times the sensitivity.";
	> = 100;
	uniform float Threshold <
		ui_type = "slider";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
		ui_tooltip = "Disables ISO scaling for values below Threshold to avoid average game brightness to bleed into the long exposure. 0 means black, 1 is white (maximum luminosity).";
	> = 0;
	uniform float ThresholdSmoothness <
		ui_type = "slider";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
		ui_tooltip = "A higher smoothness will create a soft gradient between the luminosity affected and not affected by the ISO. Useful when a sharp seperation is not possible.";
	> = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////////
//*************************************                       ****************************************//
//*************************************  Textures & Samplers  ****************************************//
//*************************************                       ****************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	texture texExposureReal{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
	texture texExposureRealCopy{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
	texture texTimer{ Width = 2; Height = 1; Format = R32F; };
	texture texTimerCopy{ Width = 2; Height = 1; Format = R32F; };

	sampler2D samplerExposure{ Texture = texExposureReal; };
	sampler2D samplerExposureCopy{ Texture = texExposureRealCopy; };
	sampler2D samplerTimer{ Texture = texTimer; };
	sampler2D samplerTimerCopy{ Texture = texTimerCopy; };

////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//*************************************** Helper Functions *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

	float encodeTimer(float value)
	{
		float maxval = 8388608; // 2^23
		float texval = value / maxval;
		return texval;
	}

	float decodeTimer(float value)
	{
		float maxval = 8388608; // 2^23
		float texval = value * maxval;
		return texval;
	}

	// show the green dot signalizing the frame is finished
	float4 show_green(float2 texcoord, float4 fragment)
	{
		float2 c = float2(0.5, 0.06);
		float range = 0.02;
		if (sqrt((c.x - texcoord.x) * (c.x - texcoord.x) + (c.y - texcoord.y) * (c.y - texcoord.y)) < range)
		{
			fragment = float4(0.5, 1, 0.5, 1);
		}
		return fragment;
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//***************************************                  *******************************************//
	//***************************************       Main       *******************************************//
	//***************************************                  *******************************************//
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	float4 getExposure(float4 rgbval, bool e)
	{
		float brightness = (rgbval.r + rgbval.g + rgbval.b) / 3;
		float enc = (brightness > Threshold) ? 1 + (ISO / 100 - 1) * smoothstep(Threshold, min(Threshold+ThresholdSmoothness, 1), brightness ) : 1; // -> apply smooth function?
		float dec = 1;
		rgbval.rgb = e ? enc * rgbval.rgb / 14400 : dec * rgbval.rgb;
		return rgbval;
	}

	void long_exposure(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		float start_time = decodeTimer(tex2D(samplerTimer, float2(0.25, 0.5)).r);
		float framecounter = decodeTimer(tex2D(samplerTimer, float2(0.75, 0.5)).r);
		float4 rgbval = tex2D(ReShade::BackBuffer, texcoord);
		fragment = tex2D(samplerExposureCopy, texcoord);
		rgbval = getExposure(rgbval, true); //rgbval / 14400 * ((ISO-100) / 100 +1);//(pow(2.71828,(rgbval.r+rgbval.g+rgbval.b/768))-1);
		// during exposure
		// active: add rgb
		// inactive: keep current
		// after exposure reset so it is ready for activation
		if (StartExposure)
		{
			if (abs(timer - start_time) < 1000 * RealExposureDuration)
			{
				fragment.rgb += rgbval.rgb;
			}
		}
		else
		{
			fragment = float4(0, 0, 0, 1);
		}
	}

	void copy_exposure(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		fragment = tex2D(samplerExposure, texcoord);
	}

	// TIMER
	void update_timer(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target)
	{
		// timer 2x1
		// value 1: starting point - modified while shader offline - frozen on activation of StartExposure
		// value 2: framecounter - 0 while offline - counting up while online
		float start_time = decodeTimer(tex2D(samplerTimerCopy, float2(0.25, 0.5)).r);
		float framecounter = decodeTimer(tex2D(samplerTimerCopy, float2(0.75, 0.5)).r);
		float new_value;
		if (texcoord.x < 0.5)
		{
			new_value = StartExposure ? start_time : timer;
		}
		else
		{
			if (abs(timer - start_time) < 1000 * RealExposureDuration)
			{
				new_value = StartExposure ? framecounter + 1 : 0;
			}
			else
			{
				new_value = StartExposure ? framecounter : 0;
			}
		}
		fragment = encodeTimer(new_value);
	}

	void copy_timer(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float fragment : SV_Target)
	{
		fragment = tex2D(samplerTimer, texcoord).r;
	}

	// OUTPUT
	void downsample_exposure(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		float4 exposure_rgb = tex2D(samplerExposure, texcoord);
		float4 game_rgb = tex2D(ReShade::BackBuffer, texcoord);
		float start_time = decodeTimer(tex2D(samplerTimer, float2(0.25, 0.5)).r);
		float framecounter = decodeTimer(tex2D(samplerTimer, float2(0.75, 0.5)).r);
		float4 result = float4(0, 0, 0, 1);
		if (StartExposure && framecounter)
		{
			result.rgb = exposure_rgb.rgb * (14400 / framecounter);
			result = getExposure(result, false);
		}
		else
		{
			result = game_rgb;
		}
		fragment = ((int)ShowGreenOnFinish * (int)StartExposure * (int)(timer - start_time > 1000 * RealExposureDuration)) ? show_green(texcoord, result) : result;
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//***************************************                  *******************************************//
	//***************************************     Pipeline     *******************************************//
	//***************************************                  *******************************************//
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	technique RealLongExposure
	{
		pass longExposure { VertexShader = PostProcessVS; PixelShader = long_exposure; RenderTarget = texExposureReal; }
		pass copyExposure { VertexShader = PostProcessVS; PixelShader = copy_exposure; RenderTarget = texExposureRealCopy; }
		pass updateTimer { VertexShader = PostProcessVS; PixelShader = update_timer; RenderTarget = texTimer; }
		pass copyTimer { VertexShader = PostProcessVS; PixelShader = copy_timer; RenderTarget = texTimerCopy; }
		pass downsampleExposure { VertexShader = PostProcessVS; PixelShader = downsample_exposure; }
	}
}
