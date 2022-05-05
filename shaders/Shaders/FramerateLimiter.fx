/*
	The world's worst framerate limiter.

	This is an example of how to capture frames at specific time points and use
	them later.
*/

//#region Preprocessor

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

//#endregion

//#region Uniforms

uniform float Framerate
<
	__UNIFORM_SLIDER_FLOAT1

	ui_tooltip = "Default: 30.0";
	ui_min = 1.0;
	ui_max = 144.0;
> = 30.0;

uniform float Timer <source = "timer";>;

//#endregion

//#region Textures

texture LastBackBufferTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler LastBackBuffer { Texture = LastBackBufferTex; };

texture LastTimeTex { Format = R32F; };
sampler LastTime { Texture = LastTimeTex; };

texture LastTimeTempTex { Format = R32F; };
sampler LastTimeTemp { Texture = LastTimeTempTex; };

//#endregion

//#region Functions

bool should_write(sampler LastTime)
{
	float last_time = tex2Dfetch(LastTime, 0).x;
	float target = rcp(Framerate) * 1000.0;

	return Timer - last_time > target;
}

//#endregion

//#region Shaders

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return tex2D(LastBackBuffer, uv);
}

float4 SaveLastBackBufferPS(
	float4 p: SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	if (should_write(LastTime))
		return tex2D(ReShade::BackBuffer, uv);

	discard;
}

float4 CopyLastTimeToTempPS(
	float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return tex2Dfetch(LastTime, 0);
}

float4 SaveLastTimePS(float4 p: SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	if (should_write(LastTimeTemp))
		return Timer;

	discard;
}

//#endregion

//#region Technique

technique FramerateLimiter
{
	pass SaveLastBackBuffer
	{
		VertexShader = PostProcessVS;
		PixelShader = SaveLastBackBufferPS;
		RenderTarget = LastBackBufferTex;
	}
	pass CopyLastTimeToTemp
	{
		VertexShader = PostProcessVS;
		PixelShader = CopyLastTimeToTempPS;
		RenderTarget = LastTimeTempTex;
	}
	pass SaveLastTime
	{
		VertexShader = PostProcessVS;
		PixelShader = SaveLastTimePS;
		RenderTarget = LastTimeTex;
	}
	pass Main
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
	}
}

//#endregion