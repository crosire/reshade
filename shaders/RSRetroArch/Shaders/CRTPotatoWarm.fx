#include "ReShade.fxh"

///////////////////////////////////////////////////////////////////////////
//                                                                       //
// Copyright (C) 2017 - Brad Parker                                      //
//                                                                       //
// This program is free software: you can redistribute it and/or modify  //
// it under the terms of the GNU General Public License as published by  //
// the Free Software Foundation, either version 3 of the License, or     //
// (at your option) any later version.                                   //
//                                                                       //
// This program is distributed in the hope that it will be useful,       //
// but WITHOUT ANY WARRANTY; without even the implied warranty of        //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
// GNU General Public License for more details.                          //
//                                                                       //
// You should have received a copy of the GNU General Public License     //
// along with this program.  If not, see <http://www.gnu.org/licenses/>. //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Potato]";
	ui_step = 1.0;
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Potato]";
	ui_step = 1.0;
> = 240.0;

//Stuff
#define texture_size float2(texture_sizeX, texture_sizeY)

texture tMaskThick <source="crt_potato/crt-potato-thick.png";> { Width=2; Height=5;};
sampler sMaskThick {
  Texture=tMaskThick;
  MinFilter=POINT;
  MagFilter=POINT;
  AddressU = REPEAT;
  AddressV = REPEAT;
  AddressW = REPEAT;
};

float4 PS_CRTPotatoWarm(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float2 tiles_per_screen = texture_size / tex2Dsize(sMaskThick);
	float2 mask_coord = frac(uv * tiles_per_screen);

	float3 base_col = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 mask_col = tex2D(sMaskThick, mask_coord).rgb;
	return float4(mask_col * base_col, 1.0);
}

technique CRTPotatoWarm {
    pass CRTPotatoWarm {
        VertexShader=PostProcessVS;
        PixelShader=PS_CRTPotatoWarm;
    }
}