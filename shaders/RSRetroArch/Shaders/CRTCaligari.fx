#include "ReShade.fxh"

/*
    Phosphor shader - Copyright (C) 2011 caligari.

    Ported by Hyllian.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRTCaligari]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRTCaligari]";
> = 240.0;

uniform float SPOT_WIDTH <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 1.5;
	ui_step = 0.05;
	ui_label = "Spot Width [CRTCaligari]";
> = 0.9;

uniform float SPOT_HEIGHT <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 1.5;
	ui_step = 0.05;
	ui_label = "Spot Height [CRTCaligari]";
> = 0.65;

uniform float COLOR_BOOST <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 2.0;
	ui_step = 0.05;
	ui_label = "Color Boost [CRTCaligari]";
> = 1.45;

uniform float InputGamma <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Input Gamma [CRTCaligari]";
> = 2.4;

uniform float OutputGamma <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Output Gamma [CRTCaligari]";
> = 2.2;

#define texture_size float2(texture_sizeX, texture_sizeY)

/* Default Vertex shader */

#define GAMMA_IN(color) pow(color, float4(InputGamma, InputGamma, InputGamma, InputGamma))
#define GAMMA_OUT(color) pow(color, float4(1.0 / OutputGamma, 1.0 / OutputGamma, 1.0 / OutputGamma, 1.0 / OutputGamma))

#define TEX2D(coords) GAMMA_IN( tex2D(ReShade::BackBuffer, coords) )

// Macro for weights computing
float WEIGHT(float w){
	if(w>1.0){
		w=1.0;
	}
	w = 1.0 - w * w;
	w = w * w;
	
	return w;
} 

float4 PS_CRTCaligari( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
   float2 texCoord = texcoord + float2(0.0, 0.0);
   float2 onex = float2(1.0 / texture_size.x, 0.0);
   float2 oney = float2(0.0, 1.0 / texture_size.y);
   float2 coords = ( texCoord * texture_size );
   float2 pixel_center = floor( coords ) + float2(0.5, 0.5);
   float2 texture_coords = pixel_center / texture_size;

   float4 color = TEX2D( texture_coords );

   float dx = coords.x - pixel_center.x;

   float h_weight_00 = (dx / SPOT_WIDTH);
   WEIGHT(h_weight_00);

   color *= float4( h_weight_00, h_weight_00, h_weight_00, h_weight_00  );

   // get closest horizontal neighbour to blend
   float2 coords01;
   if (dx>0.0) {
      coords01 = onex;
      dx = 1.0 - dx;
   } else {
      coords01 = -onex;
      dx = 1.0 + dx;
   }
   
   float4 colorNB = TEX2D( texture_coords + coords01 );

   float h_weight_01 = dx / SPOT_WIDTH;
   WEIGHT( h_weight_01 );

   color = color + colorNB * float4( h_weight_01, h_weight_01, h_weight_01, h_weight_01 );

   //////////////////////////////////////////////////////
   // Vertical Blending
   float dy = coords.y - pixel_center.y;
   float v_weight_00 = dy / SPOT_HEIGHT;
   WEIGHT(v_weight_00);
   color *= float4( v_weight_00, v_weight_00, v_weight_00, v_weight_00 );

   // get closest vertical neighbour to blend
   float2 coords10;
   if (dy>0.0) {
      coords10 = oney;
      dy = 1.0 - dy;
   } else {
      coords10 = -oney;
      dy = 1.0 + dy;
   }
   colorNB = TEX2D( texture_coords + coords10 );

   float v_weight_10 = dy / SPOT_HEIGHT;
   WEIGHT( v_weight_10 );

   color = color + colorNB * float4( v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00 );

   colorNB = TEX2D(  texture_coords + coords01 + coords10 );

   color = color + colorNB * float4( v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01 );

   color *= float4( COLOR_BOOST, COLOR_BOOST, COLOR_BOOST, COLOR_BOOST );


   return clamp( GAMMA_OUT(color), 0.0, 1.0 );
}

technique CRTCaligari {
	pass CRTCaligari {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTCaligari;
	}
}