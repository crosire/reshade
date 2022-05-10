#include "ReShade.fxh"

/*
   SGENPT-mul - Sega Genesis Pseudo Transparency muler Shader - v8b
   
   2011-2020 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is 
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [ArtifactColors]";
> = 320.0;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [ArtifactColors]";
> = 240.0;

uniform int SGPT_BLEND_OPTION <
	ui_type = "combo";
	ui_items = "OFF\0VL\0CB\0CB-S\0Both\0Both 2\0Both-S";
	ui_label = "Blend Option [SGENPT-mul]";
> = 0;

uniform float SGPT_BLEND_LEVEL <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.1;
	ui_label = "Both Blend Level [SGENPT-mul]";
> = 1.0;

uniform int SGPT_ADJUST_VIEW <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 1;
	ui_step = 1;
	ui_label = "Adjust View [SGENPT-mul]";
> = 0;

uniform int SGPT_LINEAR_GAMMA <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 1;
	ui_step = 1;
	ui_label = "Use Linear Gamma [SGENPT-mul]";
> = 1;

#define display_size float2(display_sizeX,display_sizeY)

#define display_size_pixel float2(1.0/display_sizeX,1.0/display_sizeY)

#define GAMMA_EXP			(SGPT_LINEAR_GAMMA+1.0)
#define GAMMA_IN(color)		pow(color, float3(GAMMA_EXP, GAMMA_EXP, GAMMA_EXP))
#define GAMMA_OUT(color)	pow(color, float3(1.0 / GAMMA_EXP, 1.0 / GAMMA_EXP, 1.0 / GAMMA_EXP))

static const float3 Y = float3(0.2126, 0.7152, 0.0722);

float3 min_s(float3 central, float3 adj1, float3 adj2) {return min(central, max(adj1, adj2));}
float3 max_s(float3 central, float3 adj1, float3 adj2) {return max(central, min(adj1, adj2));}

float4 PS_SGENPT(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target {
	float4 outp;
	
	float2 dx = float2(1.0, 0.0)*display_size_pixel;
	float2 dy = float2(0.0, 1.0)*display_size_pixel;

	// Reading the texels.
	float3 C = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord    ).xyz);
	float3 L = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord -dx).xyz);
	float3 R = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord +dx).xyz);
	float3 U = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord -dy).xyz);
	float3 D = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord +dy).xyz);
	float3 UL = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord -dx -dy).xyz);
	float3 UR = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord +dx -dy).xyz);
	float3 DL = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord -dx +dy).xyz);
	float3 DR = GAMMA_IN(tex2D(ReShade::BackBuffer, txcoord +dx +dy).xyz);

	float3 color = C;

	//  Get min/max samples
	float3 min_sample = min_s(C, L, R);
	float3 max_sample = max_s(C, L, R);

	float diff = dot(max(max(C, L), max(C, R)) - min(min(C, L), min(C, R)), Y);

	if (SGPT_BLEND_OPTION == 1) // Only Vertical Lines
	{
		min_sample = max_s(min_sample, min_s(C, DL, DR), min_s(C, UL, UR));
		max_sample = min_s(max_sample, max_s(C, DL, DR), max_s(C, UL, UR));

		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.25*( 1.0 - diff )*(L + R);
	}
	else if (SGPT_BLEND_OPTION == 2) // Only Checkerboard
	{
		min_sample = max(min_sample, min_s(C, U, D));
		max_sample = min(max_sample, max_s(C, U, D));

		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.125*( 1.0 - diff )*(L + R + U + D);
	}
	else if (SGPT_BLEND_OPTION == 3) // Only Checkerboard - Soft
	{
		min_sample = min_s(min_sample, U, D);
		max_sample = max_s(max_sample, U, D);

		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.125*( 1.0 - diff )*(L + R + U + D);
	}
	else if (SGPT_BLEND_OPTION == 4) // VL-CB
	{
		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.25*( 1.0 - diff )*(L + R);
	}
	else if (SGPT_BLEND_OPTION == 5) // VL-CB-2
	{
		min_sample = min_s(min_sample, U, D);
		max_sample = max_s(max_sample, U, D);

		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.25*( 1.0 - diff )*(L + R);
	}
	else if (SGPT_BLEND_OPTION == 6) // VL-CB-Soft
	{
		min_sample = min(min_sample, min(min_s(D, DL, DR), min_s(U, UL, UR)));
		max_sample = max(max_sample, max(max_s(D, DL, DR), max_s(U, UL, UR)));

		diff *= (1.0 - SGPT_BLEND_LEVEL);

		color = 0.5*( 1.0 + diff )*C + 0.25*( 1.0 - diff )*(L + R);
	}

	color = clamp(color, min_sample, max_sample);

	color = lerp(color, float3(dot(abs(C-color), float3(1.0, 1.0, 1.0)),dot(abs(C-color), float3(1.0, 1.0, 1.0)),dot(abs(C-color), float3(1.0, 1.0, 1.0))), SGPT_ADJUST_VIEW);

	return float4(GAMMA_OUT(color), 1.0);
}

technique SGENPT_MUL {

	pass SGENPT_MUL {
		VertexShader=PostProcessVS;
		PixelShader=PS_SGENPT;
	}
}