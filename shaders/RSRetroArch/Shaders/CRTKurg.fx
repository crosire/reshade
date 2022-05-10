 /*
 CRT shader

Copyright (C) 2014 kurg, SKR!

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.
*/

#include "ReShade.fxh"

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_step = 1.0;
	ui_label = "Screen Width [CRT-KurgV2]";
> = 320.0;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1.0;
	ui_label = "Screen Height [CRT-KurgV2]";
> = 240.0;

#define ILLUMASPECT  2.4
#define DIST(dx, dy)  clamp((2.0 - sqrt(dx*dx+(dy*ILLUMASPECT)*(dy*ILLUMASPECT))) / 2.0, 0.0, 1.0)
#define ATT(dx, dy)   DIST(dx, pow(dy, 1.3) * 0.6)

float4 PS_CRTKurg(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
		float4 col = tex2D(ReShade::BackBuffer, texcoord.xy);
		float2 rubyTextureSize = float2(display_sizeX / 1.0, display_sizeY / 2.0); 
        float vert_column = texcoord.s * rubyTextureSize.x;
        float horz_line = texcoord.t * rubyTextureSize.y;
        float dx1 = frac(vert_column);
        float dy1 = frac(horz_line);
        
        float2 texel;
        texel.x = texcoord.s - dx1/rubyTextureSize.x + 1.0/rubyTextureSize.x/2.0;
        texel.y = texcoord.t - dy1/rubyTextureSize.y + 1.0/rubyTextureSize.y/2.0;
        float4 rgb1 = tex2D(ReShade::BackBuffer, texel.xy);
        float4 rgb2 = tex2D(ReShade::BackBuffer, float2(texel.s + 1.0/rubyTextureSize.x, texel.t));
        float4 rgb3 = tex2D(ReShade::BackBuffer, float2(texel.s, texel.t + 1.0/rubyTextureSize.y));
        float4 rgb4 = tex2D(ReShade::BackBuffer, float2(texel.s + 1.0/rubyTextureSize.x, texel.t + 1.0/rubyTextureSize.y));

        float4 rgb5 = tex2D(ReShade::BackBuffer, float2(texel.s - 1.0/rubyTextureSize.x, texel.t - 1.0/rubyTextureSize.y));
        float4 rgb6 = tex2D(ReShade::BackBuffer, float2(texel.s + 0.0/rubyTextureSize.x, texel.t - 1.0/rubyTextureSize.y));
        float4 rgb7 = tex2D(ReShade::BackBuffer, float2(texel.s + 1.0/rubyTextureSize.x, texel.t - 1.0/rubyTextureSize.y));
        float4 rgb8 = tex2D(ReShade::BackBuffer, float2(texel.s + 2.0/rubyTextureSize.x, texel.t - 1.0/rubyTextureSize.y));

        float4 rgb9 = tex2D(ReShade::BackBuffer, float2(texel.s - 1.0/rubyTextureSize.x, texel.t + 0.0/rubyTextureSize.y));
        float4 rgb10 = tex2D(ReShade::BackBuffer, float2(texel.s + 2.0/rubyTextureSize.x, texel.t + 0.0/rubyTextureSize.y));
        float4 rgb11 = tex2D(ReShade::BackBuffer, float2(texel.s - 1.0/rubyTextureSize.x, texel.t + 1.0/rubyTextureSize.y));
        float4 rgb12 = tex2D(ReShade::BackBuffer, float2(texel.s + 2.0/rubyTextureSize.x, texel.t + 1.0/rubyTextureSize.y));

        float4 rgb13 = tex2D(ReShade::BackBuffer, float2(texel.s - 1.0/rubyTextureSize.x, texel.t + 2.0/rubyTextureSize.y));
        float4 rgb14 = tex2D(ReShade::BackBuffer, float2(texel.s + 0.0/rubyTextureSize.x, texel.t + 2.0/rubyTextureSize.y));
        float4 rgb15 = tex2D(ReShade::BackBuffer, float2(texel.s + 1.0/rubyTextureSize.x, texel.t + 2.0/rubyTextureSize.y));
        float4 rgb16 = tex2D(ReShade::BackBuffer, float2(texel.s + 2.0/rubyTextureSize.x, texel.t + 2.0/rubyTextureSize.y));

        float4x4 dist;
        dist[0][0] = DIST(dx1, dy1);
        dist[0][1] = DIST((1.0 - dx1), dy1);
        dist[0][2] = DIST(dx1, (1.0 - dy1));
        dist[0][3] = DIST((1.0 - dx1), (1.0 - dy1));
        
        dist[1][0] = DIST((1.0 + dx1), (1.0 + dy1));
        dist[1][1] = DIST((0.0 + dx1), (1.0 + dy1));
        dist[1][2] = DIST((1.0 - dx1), (1.0 + dy1));
        dist[1][3] = DIST((2.0 - dx1), (1.0 + dy1));

        dist[2][0] = DIST((1.0 + dx1), dy1);
        dist[2][1] = DIST((2.0 - dx1), dy1);
        dist[2][2] = DIST((1.0 + dx1), (1.0 - dy1));
        dist[2][3] = DIST((2.0 - dx1), (1.0 - dy1));

        dist[3][0] = DIST((1.0 + dx1), (2.0 - dy1));
        dist[3][1] = DIST((0.0 + dx1), (2.0 - dy1));
        dist[3][2] = DIST((1.0 - dx1), (2.0 - dy1));
        dist[3][3] = DIST((2.0 - dx1), (2.0 - dy1));

        float4 tex1 = (rgb1*dist[0][0] + rgb2*dist[0][1] + rgb3*dist[0][2] + rgb4*dist[0][3]);
        float4 tex2 = (rgb5*dist[1][0] + rgb6*dist[1][1] + rgb7*dist[1][2] + rgb8*dist[1][3]);
        float4 tex3 = (rgb9*dist[2][0] + rgb10*dist[2][1] + rgb11*dist[2][2] + rgb12*dist[2][3]);
        float4 tex4 = (rgb13*dist[3][0] + rgb14*dist[3][1] + rgb15*dist[3][2] + rgb16*dist[3][3]);

        float4 tex = (tex1 + tex2 + tex3 + tex4 + ((rgb9+rgb11)/2.0)*0.3) / 2.0;
        col = float4(tex.rgb, 1.0);
		return col;
}

technique CRTKurgV2 {
    pass CRTKurgV2 {
        VertexShader=PostProcessVS;
        PixelShader=PS_CRTKurg;
    }
}