 ////----------//
 ///**Medain**///
 //----------////
/*
3x3 Median
Morgan McGuire and Kyle Whitson
http://graphics.cs.williams.edu


Copyright (c) Morgan McGuire and Williams College, 2006
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Ported

uniform float Power <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_label = "Median Power Slider";
	ui_tooltip = "Determines the Median Power.";
> = 1.0;

uniform int MedianFilter <
	ui_type = "combo";
	ui_items = "Off\0Median\0Median Control\0";
	ui_label = "Median Selection";
> = 0;

/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////

#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

texture BackBufferTex : COLOR;

sampler BackBuffer 
	{ 
		Texture = BackBufferTex;
	};

texture texMed { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F;};

sampler SamplerMed
	{
		Texture = texMed;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
		MipFilter = Linear; 
		MinFilter = Linear; 
		MagFilter = Linear;
	};
	
#define s2(a, b)				temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges	

float4 Median(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float2 ScreenCal;
    if(MedianFilter == 2)
    {
    ScreenCal = float2(Power*pix.x,Power*pix.y);
    }
    else
    {
    ScreenCal = float2(pix.x,pix.y);
	}
	
	float2 FinCal = ScreenCal*0.6;

	float4 v[9];
	
	[unroll]
	for(int i = -1; i <= 1; ++i) 
	{
		for(int j = -1; j <= 1; ++j)
		{		
		  float2 offset = float2(i, j);

		  v[(i + 1) * 3 + (j + 1)] = tex2D(BackBuffer, texcoord + offset * FinCal);
		}
	}

	float4 temp;

	mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);
	mnmx5(v[1], v[2], v[3], v[4], v[6]);
	mnmx4(v[2], v[3], v[4], v[7]);
	mnmx3(v[3], v[4], v[8]);
	
	return v[4];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PS0(float4 position : SV_Position, float2 texcoord : TEXCOORD0, out float4 color : SV_Target)
{

	if(MedianFilter == 1 || MedianFilter == 2)
		color = tex2D(SamplerMed,float2(texcoord.x,texcoord.y));	
	else
		color = tex2D(BackBuffer,float2(texcoord.x,texcoord.y));
}


///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

///////////////////////////////////////////////Depth Map View//////////////////////////////////////////////////////////////////////

//*Rendering passes*//

technique Median_Filter
{
			pass MedianPass
		{
			VertexShader = PostProcessVS;
			PixelShader = Median;
			RenderTarget = texMed;
		}
			pass OutputPass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS0;	
		}
}
