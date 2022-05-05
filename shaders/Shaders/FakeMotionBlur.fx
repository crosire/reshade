/**
 * Copyright (C) 2015 Ganossa (mediehawk@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software with restriction, including without limitation the rights to
 * use and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and the permission notices (this and below) shall
 * be included in all copies or substantial portions of the Software.
 *
 * Permission needs to be specifically granted by the author of the software to any
 * person obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to copy, modify, merge, publish, distribute, and/or
 * sublicense the Software, and subject to the following conditions:
 *
 * The above copyright notice and the permission notices (this and above) shall
 * be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ReShadeUI.fxh"

uniform float mbRecall < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Motion blur intensity";
> = 0.40;
uniform float mbSoftness < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Blur strength of consequential streaks";
> = 1.00;

#include "ReShade.fxh"

texture2D currTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D prevSingleTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D prevTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler2D currColor { Texture = currTex; };
sampler2D prevSingleColor { Texture = prevSingleTex; };
sampler2D prevColor { Texture = prevTex; };

void PS_Combine(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target)
{
	float4 curr = tex2D(currColor, texcoord);
	float4 prevSingle = tex2D(prevSingleColor, texcoord);
	float4 prev = tex2D(prevColor, texcoord);

	float3 diff3 = abs(prevSingle.rgb - curr.rgb) * 2.0f;
	float diff = min(diff3.r + diff3.g + diff3.b, mbRecall);

	const float weight[11] = { 0.082607, 0.040484, 0.038138, 0.034521, 0.030025, 0.025094, 0.020253, 0.015553, 0.011533, 0.008218, 0.005627 };
	prev *= weight[0];

	float pixelBlur = (mbSoftness * 13 * (diff)) * (BUFFER_RCP_WIDTH);
	float pixelBlur2 = (mbSoftness * 11 * (diff)) * (BUFFER_RCP_HEIGHT);

	[unroll]
	for (int z = 1; z < 11; z++)
	{
		prev += tex2D(prevColor, texcoord + float2(z * pixelBlur, 0.0f)) * weight[z];
		prev += tex2D(prevColor, texcoord - float2(z * pixelBlur, 0.0f)) * weight[z];
		prev += tex2D(prevColor, texcoord + float2(0.0f, z * pixelBlur2)) * weight[z];
		prev += tex2D(prevColor, texcoord - float2(0.0f, z * pixelBlur2)) * weight[z];
	}

	color = lerp(curr, prev, diff+0.1);
}

void PS_CopyFrame(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target)
{
	color = tex2D(ReShade::BackBuffer, texcoord);
}
void PS_CopyPreviousFrame(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 prevSingle : SV_Target0, out float4 prev : SV_Target1)
{
	prevSingle = tex2D(currColor, texcoord);
	prev = tex2D(ReShade::BackBuffer, texcoord);
}

technique MotionBlur
{
	pass CopyFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CopyFrame;
		RenderTarget = currTex;
	}

	pass Combine
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Combine;
	}

	pass PrevColor
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CopyPreviousFrame;
		RenderTarget0 = prevSingleTex;
		RenderTarget1 = prevTex;
	}
}
