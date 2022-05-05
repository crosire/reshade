////----------//
///**Trails**///
//----------////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Trails
//* For Reshade 3.0
//* --------------------------
//* This work is licensed under a Creative Commons Attribution 3.0 Unported License.
//* So you are free to share, modify and adapt it for your needs, and even use it for commercial use.
//* I would also love to hear about a project you are using it with.
//* https://creativecommons.org/licenses/by/3.0/us/
//*
//* Have fun,
//* Jose Negrete AKA BlueSkyDefender
//*
//* https://github.com/BlueSkyDefender/Depth3D
//* ---------------------------------
//*
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define Per_Color_Channel 0 // Lets you adjust per Color Channel.Default 0 off
#define Add_Depth_Effects 0 // Lets this effect be affected by Depth..Default 0 off

#if !Per_Color_Channel
uniform float Persistence <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Persistence";
	ui_tooltip = "Increase persistence longer the trail or afterimage.\n"
				"If pushed out the effect is alot like long exposure.\n"
				"This can be used for light painting in games.\n"
				"1000/1 is 1.0, so 1/2 is 0.5 and so forth.\n"
				"Default is 0.25, 0 is infinity.";
> = 0.25;
#else
uniform float3 Persistence <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Persistence";
	ui_tooltip = "Increase persistence longer the trail or afterimage RGB.\n"
				"If pushed out the effect is alot like long exposure.\n"
				"This can be used for light painting in games.\n"
				"1000/1 is 1.0, so 1/2 is 0.5 and so forth.\n"
				"Default is 0.25, 0 is infinity.";
> = float3(0.25,0.25,0.25);
#endif

uniform float TQ <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Trail Blur Quality";
	ui_tooltip = "Adjust Trail Blur Quality.\n"
				"Default is Zero.";
> = 0.0;

//uniform bool TrailsX2 <
//	ui_label = "Trails X2";
//	ui_tooltip = "Two times the samples.\n"
//				 "This disables Trail Quality.";
//> = false;

uniform bool PS2 <
	ui_label = "PS2 Style Echo";
	ui_tooltip = "This enables PS2 Style Echo in your game.\n"
				 "This disables Trail Quality.";
> = false;
#if Add_Depth_Effects
uniform bool Allow_Depth <
	ui_label = "Depth Map Toggle";
	ui_tooltip = "This Alows Depth to be used in Trails.";
	ui_category = "Depth Buffer";
> = 0;

uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "Normal\0Reverse\0";
	ui_label = "Custom Depth Map";
	ui_tooltip = "Pick your Depth Map.";
	ui_category = "Depth Buffer";
> = 0;

uniform float Depth_Map_Adjust <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "Adjust the depth map and sharpness distance.";
	ui_category = "Depth Buffer";
> = 0.0;

uniform bool Hard_CutOff <
	ui_label = "Hard CutOff";
	ui_tooltip = "Depth Cutoff toggle this give a hard cutoff for depth isolation.";
	ui_category = "Depth Buffer";
> = 0;

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Buffer";
> = 0;

uniform bool Invert_Depth <
	ui_label = "Depth Map Inverte";
	ui_tooltip = "Inverts Depth so you can target only your weapon or what's near you.";
	ui_category = "Depth Buffer";
> = 0;

uniform bool Depth_View <
	ui_label = "Depth Map View";
	ui_tooltip = "Lets you see Depth so you can Debug.";
	ui_category = "Depth Buffer";
> = 0;
#else
static const int Allow_Depth = 0;
static const int Depth_Map = 0;
static const float Depth_Map_Adjust = 250.0;
static const int Depth_Map_Flip = 0;
static const int Invert_Depth = 0;
static const int Depth_View = 0;
static const int Hard_CutOff = 0;
#endif
/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
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

texture CurrentBackBufferT  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler CBackBuffer
	{
		Texture = CurrentBackBufferT;
	};


texture PBB  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; MipLevels = 2;};

sampler PBackBuffer
	{
		Texture = PBB;
	};
	
texture PSBB  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler PSBackBuffer
	{
		Texture = PSBB;
	};

///////////////////////////////////////////////////////////TAA/////////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float Depth(in float2 texcoord : TEXCOORD0)
{
	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	float zBuffer = tex2D(DepthBuffer, texcoord).x; //Depth Buffer

	//Conversions to linear space.....
	//Near & Far Adjustment
	float Far = 1.0, Near = 0.125/250.0; //Division Depth Map Adjust - Near

	float2 Z = float2( zBuffer, 1-zBuffer );

	if (Depth_Map == 0)//DM0. Normal
		zBuffer = Far * Near / (Far + Z.x * (Near - Far));
	else if (Depth_Map == 1)//DM1. Reverse
		zBuffer = Far * Near / (Far + Z.y * (Near - Far));

	return saturate(zBuffer);
}

float3 T_Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float TQA = TQ, D = Depth(texcoord);
	if(PS2)
		TQA = 0;
		
    float3 C = tex2D(BackBuffer, texcoord).rgb;
	//float3 PS = tex2D(PSBackBuffer, texcoord).rgb;

    float3 P = tex2Dlod(PBackBuffer, float4(texcoord,0,TQA)).rgb;
	
    #if !PerColor
      float Per = 1-Persistence;
    #else
      float3 Per = 1-Persistence;
    #endif

	D = smoothstep(0,Depth_Map_Adjust,D);
	
	if(Hard_CutOff)
		D = step(0.5,D);

	if(Invert_Depth)
	D = 1-D;

    if(!PS2)
    {
		P *= Per;
		C = max( tex2D(BackBuffer, texcoord).rgb, P);
		//PS = max( tex2D(BackBuffer, texcoord).rgb, P);
    }
    else
    {
		C = (1-Per) * C + Per * P;
		//PS = (1-Per) * PS + Per * P;
	}
	
	//if(TrailsX2)
	//{
	//	C = lerp(PS,C,0.5);
	//}
	
	if(Allow_Depth)
		C = lerp(C,tex2D(BackBuffer, texcoord).rgb,saturate(D));

	if(Depth_View)
		C = D;

  return C;
}

void Current_BackBuffer_T(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target0)
{
	color = tex2D(BackBuffer,texcoord);
}

void Past_BB(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 Past : SV_Target0, out float4 PastSingle : SV_Target1)
{   float2 samples[12] = {
	float2(-0.326212, -0.405805),
	float2(-0.840144, -0.073580),
	float2(-0.695914, 0.457137),
	float2(-0.203345, 0.620716),
	float2(0.962340, -0.194983),
	float2(0.473434, -0.480026),
	float2(0.519456, 0.767022),
	float2(0.185461, -0.893124),
	float2(0.507431, 0.064425),
	float2(0.896420, 0.412458),
	float2(-0.321940, -0.932615),
	float2(-0.791559, -0.597705)
	};

	float4 sum_A = tex2D(BackBuffer,texcoord), sum_B = 0;//tex2D(CBackBuffer,texcoord);

	if(!PS2)
	{
			float Adjust = TQ*pix.x;
			[loop]
			for (int i = 0; i < 12; i++)
			{
				sum_A += tex2Dlod(BackBuffer, float4(texcoord + Adjust * samples[i],0,0));
				//sum_B += tex2Dlod(CBackBuffer, float4(texcoord + Adjust * samples[i],0,0));
			}
		Past = sum_A * 0.07692307;
		PastSingle = 0;//sum_B * 0.07692307;
	}
	else
	{
		Past = sum_A;
		PastSingle = 0;//sum_B * 0.07692307;
	}
}

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////
// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
technique Trails
	{
			pass CBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Current_BackBuffer_T;
			RenderTarget = CurrentBackBufferT;
		}
			pass Trails
		{
			VertexShader = PostProcessVS;
			PixelShader = T_Out;
		}
			pass PBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Past_BB;
			RenderTarget0 = PBB;
			RenderTarget1 = PSBB;

		}
	}
