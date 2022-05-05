// Made by Marot Satil for the GShade ReShade package!
// You can follow me via @MarotSatil on Twitter, but I don't use it all that much.
// Follow @GPOSERS_FFXIV on Twitter and join us on Discord (https://discord.gg/39WpvU2)
// for the latest GShade package updates!
//
// This shader was designed in the same vein as GreenScreenDepth.fx, but instead of applying a
// green screen with adjustable distance, it applies a PNG texture with adjustable opacity.
//
// PNG transparency is fully supported, so you could for example add another moon to the sky
// just as readily as create a "green screen" stage like in real life.
//
// Copyright (c) 2019, Marot Satil
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, the header above it, this list of conditions, and the following disclaimer
//    in this position and unchanged.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, the header above it, this list of conditions, and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "Reshade.fxh"

#define TEXFORMAT RGBA8

#include "ReShadeUI.fxh"

uniform float Stage_Opacity < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Opacity";
	ui_tooltip = "Set the transparency of the image.";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.002;
> = 1.0;

uniform float Stage_depth < __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.0;
	ui_max = .987;
	ui_label = "Depth Slider";
> = 0.97;

texture Stage_texture <source="Stage.png";> { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=TEXFORMAT; };

sampler Stage_sampler { Texture = Stage_texture; };

void PS_StageDepth(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float3 color : SV_Target)
{
	float4 stage = tex2D(Stage_sampler, texcoord).rgba;
	color = tex2D(ReShade::BackBuffer, texcoord).rgb;

	float depth = 1 - ReShade::GetLinearizedDepth(texcoord).r;

	if( depth < Stage_depth )
	{
		color = lerp(color, stage.rgb, stage.a * Stage_Opacity);
	}
}

technique StageDepth
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_StageDepth;
	}
}
