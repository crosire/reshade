/**
 * Deband shader by haasn
 * https://github.com/mpv-player/mpv/blob/master/video/out/opengl/video_shaders.c
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Modified and optimized for ReShade by JPulowski
 * http://reshade.me/forum/shader-presentation/768-deband
 *
 * Do not distribute without giving credit to the original author(s).
 *
 * 1.0  - Initial release
 * 1.1  - Replaced the algorithm with the one from MPV
 * 1.1a - Minor optimizations
 *        Removed unnecessary lines and replaced them with ReShadeFX intrinsic counterparts
 */

#include "ReShadeUI.fxh"

uniform float Threshold < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0; ui_max = 0.25;
	ui_label = "Cut-Off Threshold";
	ui_tooltip = "Higher numbers increase the debanding strength dramatically but progressively diminish image details.";
> = 0.004;
uniform float Range < __UNIFORM_SLIDER_FLOAT1
	ui_min = 1; ui_max = 64;
	ui_label = "Initial Radius";
	ui_tooltip = "The radius increases linearly for each iteration. A higher radius will find more gradients, but a lower radius will smooth more aggressively.";
> = 16.0;
uniform int Iterations < __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 16;
	ui_tooltip = "The number of debanding steps to perform per sample. Each step reduces a bit more banding, but takes time to compute. Note that the strength of each step falls off very quickly, so high numbers (> 4) are practically useless.";
> = 1;
uniform float Grain < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0; ui_max = 0.5;
	ui_label = "Additional Noise";
	ui_tooltip = "Add some extra noise to the image. This significantly helps cover up remaining quantization artifacts. Higher numbers add more noise.";
> = 0.006;

#include "ReShade.fxh"

uniform int drandom < source = "random"; min = 0; max = 5000; >;

float rand(float x)
{
	return frac(x * 0.024390243);
}
float permute(float x)
{
	return ((34.0 * x + 1.0) * x) % 289.0;
}
float3 average(sampler2D tex, float2 pos, float range, inout float h)
{
	// Helper: Compute a stochastic approximation of the avg color around a pixel

	// Compute a random rangle and distance
	float dist = rand(h) * range;     h = permute(h);
	float dir  = rand(h) * 6.2831853; h = permute(h);

	float2 pt = dist * ReShade::PixelSize;
	float2 o = float2(cos(dir), sin(dir));

	// Sample at quarter-turn intervals around the source pixel
	float3 ref[4];
	ref[0] = tex2Dlod(tex, float4(pos + pt * float2( o.x,  o.y), 0, 0)).rgb;
	ref[1] = tex2Dlod(tex, float4(pos + pt * float2(-o.y,  o.x), 0, 0)).rgb;
	ref[2] = tex2Dlod(tex, float4(pos + pt * float2(-o.x, -o.y), 0, 0)).rgb;
	ref[3] = tex2Dlod(tex, float4(pos + pt * float2( o.y, -o.x), 0, 0)).rgb;

	// Return the (normalized) average
	return (ref[0] + ref[1] + ref[2] + ref[3]) * 0.25;
}

float3 PS_Deband(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	// Initialize the PRNG by hashing the position + a random uniform
	float3 m = float3(texcoord, drandom * 0.0002) + 1.0;
	float h = permute(permute(permute(m.x) + m.y) + m.z);

	// Sample the source pixel
	float3 avg, diff, col = tex2D(ReShade::BackBuffer, texcoord).rgb;
	
	for (int i = 1; i <= Iterations; i++)
	{
		// Sample the average pixel and use it instead of the original if the difference is below the given threshold
		avg = average(ReShade::BackBuffer, texcoord, i * Range, h);
		diff = abs(col - avg);
		col = lerp(avg, col, diff > Threshold * i);
	}

	if (Grain > 0.0)
	{
		// Add some random noise to smooth out residual differences
		float3 noise;
		noise.x = rand(h); h = permute(h);
		noise.y = rand(h); h = permute(h);
		noise.z = rand(h); h = permute(h);
		col += Grain * (noise - 0.5);
	}

	return col;
}

technique Deband
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Deband;
	}
}
