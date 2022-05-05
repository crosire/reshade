 ////-------------//
 ///**Limbo Mod**///
 //-------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Super Simple Limbo Like Shader implementation:
 //* For ReShade 3.0+
 //*  ---------------------------------
 //*                                                                           Limbo Mod
 //* Due Diligence
 //* https://reshade.me
 //* https://github.com/crosire/reshade-shaders/blob/a9ab2880eebf5b1b29cad0a16a5f0910fad52492/Shaders/DisplayDepth.fx
 //* 
 //*
 //* LICENSE
 //* ============
 //* Limbo Mod is licenses under: Attribution 2.0 Generic (CC BY 2.0)l
 //*
 //* You are free to:
 //* Share - copy and redistribute the material in any medium or format.
 //* Adapt - remix, transform, and build upon the material for any purpose, even commercially.
 //*
 //* The licensor cannot revoke these freedoms as long as you follow the license terms.
 //*
 //* Under the following terms:
 //* Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
 //* You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
 //*
 //* No additional restrictions - You may not apply legal terms or technological measures that legally restrict others
 //* from doing anything the license permits.
 //*
 //* https://creativecommons.org/licenses/by/2.0/
 //*
 //* Have fun,
 //* Jose Negrete AKA BlueSkyDefender
 //*
 //* https://github.com/BlueSkyDefender/Depth3D
 //* April 1 2021
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

uniform float G_Radius<
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Subtle Blur Radius";
	ui_tooltip = "Increase persistence of the frames this is really the Temporal Part.\n"
				 "Default is 0.625. But, a value around 0.625 is recommended.";
	ui_category = "Limbo Lighting";
> = 0.5;

uniform float Target_Lighting <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Target Lighting";
	ui_tooltip = "Use this to target the brighter areas of the game.\n"
				 "Default is 0.5. But, any value around 0 - 2 can be used.";
	ui_category = "Limbo Lighting";
> = 0.5;

uniform float2 Depth_Map_Adjust <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Depth Near and Far";
	ui_tooltip = "Adjust the depth map precision near and far.";
	ui_category = "Depth Buffer";
> = float2(0.075,0.750);

uniform float Dither_Bit <
 ui_type = "drag";
 ui_min = 1; ui_max = 15;
 ui_label = "Dither Bit";
 ui_tooltip = "Dither is an intentionally applied form of noise used to randomize quantization error, preventing banding in images.";
 ui_category = "Depth Buffer";
> = 8;


/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

texture DepthBufferTex : DEPTH;

sampler DepthBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

texture Mips_Buffer_A  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;MipLevels = 4;};

sampler MipMaps_A
	{
		Texture = Mips_Buffer_A;
	};

texture Mips_Buffer_B  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;MipLevels = 4;};

sampler MipMaps_B
	{
		Texture = Mips_Buffer_B;
	};
uniform float frametime < source = "frametime"; >;
float DepthM(float2 texcoord)
{
	float zBuffer = ReShade::GetLinearizedDepth(texcoord).x;

zBuffer = (zBuffer - Depth_Map_Adjust.x)/(Depth_Map_Adjust.y - Depth_Map_Adjust.x);
 // Dither for DepthBuffer adapted from gedosato ramdom dither https://github.com/PeterTh/gedosato/blob/master/pack/assets/dx9/deband.fx
 // I noticed in some games the depth buffer started to have banding so this is used to remove that.

 float DB  = Dither_Bit;
 float noise = frac(sin(dot(texcoord * frametime, float2(12.9898, 78.233))) * 43758.5453);
 float dither_shift = (1.0 / (pow(2,DB) - 1.0));
 float dither_shift_half = (dither_shift * 0.5);
 dither_shift = dither_shift * noise - dither_shift_half;

   zBuffer += -dither_shift;
   zBuffer += dither_shift;
   zBuffer += -dither_shift;
 
 // Dither End

	return zBuffer;
}

float4 BB_M(float2 TC)
{
	return tex2D(BackBuffer, TC );
}


float4 DirectLighting(float2 texcoords )
{
	float4 BC = BB_M(texcoords);
	
	float  GS = dot(BC.rgb,0.333), Boost = 1;
	
	BC.rgb /= GS;
	BC.rgb *= saturate(GS - lerp(0.0,0.5,saturate(Target_Lighting)));
	Boost = lerp(1,2.5,saturate(Target_Lighting));
	
	return float4(BC.rgb * Boost,BC.a);
}

float4 GussBlur(sampler image, float2 TC, int dir, float Mips) 
{
	//direction 
	float W0 = G_Radius > 0 ? 0.1964825501511404 : 1, W1 = 0.2969069646728344, W2 = 0.09447039785044732, W3 = 0.010381362401148057; 
	float2 off0 = pix * lerp(0,5,G_Radius);
	float2 off1 = dir ? float2( 0, 1.411764705882353) * off0 : float2(1.411764705882353, 0) * off0;
	float2 off2 = dir ? float2( 0, 3.294117647058823) * off0 : float2(3.294117647058823, 0) * off0;
	float2 off3 = dir ? float2( 0, 5.176470588235294) * off0 : float2(5.176470588235294, 0) * off0;
	float4 color = tex2Dlod(image, float4(TC,0,Mips)) * W0;
	if(G_Radius > 0)
	{ 
		color += tex2Dlod(image, float4(TC + off1,0,Mips) ) * W1;
		color += tex2Dlod(image, float4(TC - off1,0,Mips) ) * W1;
		color += tex2Dlod(image, float4(TC + off2,0,Mips) ) * W2;
		color += tex2Dlod(image, float4(TC - off2,0,Mips) ) * W2;
		color += tex2Dlod(image, float4(TC + off2,0,Mips) ) * W3;
		color += tex2Dlod(image, float4(TC - off2,0,Mips) ) * W3;
	}
	
	return color;
}

void Buffers_Mip_A(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 MipMapper_A : SV_Target0)
{

	MipMapper_A = DirectLighting( texcoord );
}

void Buffers_Mip_B(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 MipMapper_B : SV_Target0)
{

	MipMapper_B = GussBlur(MipMaps_A, texcoord, 0, lerp(0,4,G_Radius));
}


float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float4 BWorColor = dot(GussBlur(MipMaps_B, texcoord, 1, lerp(0,4,G_Radius)),0.333);
	return DepthM(texcoord) + (dot(tex2Dlod(MipMaps_A,float4(texcoord,0,0)),0.333) * BWorColor );
}

technique Limbo_Mod
	{
			pass Mips_A
		{
			VertexShader = PostProcessVS;
			PixelShader = Buffers_Mip_A;
			RenderTarget0 = Mips_Buffer_A;
		}
			pass Mips_B
		{
			VertexShader = PostProcessVS;
			PixelShader = Buffers_Mip_B;
			RenderTarget0 = Mips_Buffer_B;
		}
			pass Out
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
		}
	}
