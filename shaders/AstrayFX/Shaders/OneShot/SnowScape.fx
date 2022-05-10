 ////--------------//
 ///**Snow Scape**///
 //--------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Super Simple Snow Shader "Snow Scape" implementation:
 //* For ReShade 3.0+
 //*  ---------------------------------
 //*                                                                           Snow Scape
 //* Due Diligence
 //* Based on POM
 //* http://graphics.cs.brown.edu/games/SteepParallax/index.html
 //* Other Needed Links for the code here.
 //* http://www.shadertoy.com/view/XtKyzD
 //* https://reshade.me
 //* https://github.com/crosire/reshade-shaders/blob/a9ab2880eebf5b1b29cad0a16a5f0910fad52492/Shaders/DisplayDepth.fx
 //* I am not sure I got it all.
 //*
 //* LICENSE
 //* ============
 //* Snow Scape is licenses under: Attribution 2.0 Generic (CC BY 2.0)l
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
 //*
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

#define MaxDepth_Cutoff 0.999 //[0.1 - 1.0]
#define SN_offset float2(2,2)                          // Smooth Normals Set the first value from 1-6

uniform float BG <
	ui_type = "slider";
	ui_min = 0.5; ui_max = 1.0;
	ui_label = "Best Guess Sensitivity";
	ui_tooltip = "Increases the sensitiviy of the Algo used for gussing where the user is looking at.\n"
				 "Default is 0.9625. But, any value around 0.5 - 1.0 can be used. Decreasing this makes it stronger.";
	ui_category = "Snow";
> = 0.9625;

uniform float SSC <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Snow Scape Adjust";
	ui_tooltip = "Increase persistence of the frames this is really the Temporal Part.\n"
				 "Default is 0.625. But, a value around 0.625 is recommended.";
	ui_category = "Snow";
> = 0.625;

uniform float HMP <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.00;
	ui_label = "Height Map Scaling";
	ui_tooltip = "Increases the Detail that it takes from the color information from the world.\n"
				 "Default is 0.5. But, any value around 0 - 2 can be used.";
	ui_category = "Snow";
> = 0.5;

uniform float Elevation <
	ui_type = "slider";
	ui_min = 0; ui_max = 50;
	ui_label = "Elevation";
	ui_tooltip = "Increase Parallax Occlusion Mapping Elevation.\n"
				 "Default is 10. But, a value around 1-25 is recommended.";
	ui_category = "Snow";
> = 10;

uniform bool I_Ice <
	ui_label = "Invert Snow";
	ui_tooltip = "Use this so you can invert the Height Map.";
	ui_category = "Snow";
> = !true;

uniform float Depth_Map_Adjust <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "Adjust the depth map and sharpness distance.";
	ui_category = "Depth Buffer";
> = 1.0;

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

texture SnowMask_A { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; MipLevels = 11;};

sampler CSnowMask
	{
		Texture = SnowMask_A;
	};

texture Normals_A  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16; MipLevels = 11;};

sampler N_Sampler_A
	{
		Texture = Normals_A;
	};

texture Normals_B  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16; MipLevels = 11;};

sampler N_Sampler_B
	{
		Texture = Normals_B;
	};

texture BB_Depth_A  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;MipLevels = 11;};

sampler CDBuffer
	{
		Texture = BB_Depth_A;
	};

texture Bump_C  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;MipLevels = 11;};

sampler BumpMap
	{
		Texture = Bump_C;
	};


//Total amount of frames since the game started.
uniform uint framecount < source = "framecount"; >;
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define PI 3.14159265358979323846264                   // PI

float fmod(float a, float b)
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float2 DepthM(float2 texcoord)
{
	float zBuffer = ReShade::GetLinearizedDepth(texcoord).x; //Depth Buffer
	zBuffer = smoothstep(0,Depth_Map_Adjust,zBuffer);
	return float2(zBuffer,(1-zBuffer - 0.5)/(1.0 - 0.5) );
}

float4 BB_H(float2 TC)
{
	return tex2D(BackBuffer, TC );
}

float3 DepthNormals(float2 texcoord)
{
	float3 offset = float3(pix.xy, 0.0);
	float2 posCenter = texcoord.xy;
	float2 posNorth  = posCenter - offset.zy;
	float2 posEast   = posCenter + offset.xz;

	float3 vertCenter = float3(posCenter - 0.5, 1) * DepthM(posCenter).x;
	float3 vertNorth  = float3(posNorth - 0.5,  1) * DepthM(posNorth).x;
	float3 vertEast   = float3(posEast - 0.5,   1) * DepthM(posEast).x;

	return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) ;
}

void MCNoise(inout float Noise,float2 TC, float FC,float seed)
{ //This is the noise I used for rendering
	float motion = FC, a = 12.9898, b = 78.233, c = 43758.5453, dt= dot( TC.xy * 2.0 , float2(a,b)), sn= fmod(dt,PI);
	Noise = frac(frac(tan(distance(sn*(seed+dt),  float2(a,b)  )) * c) + 0.61803398875f * motion);
}

float LumaMask(sampler Texture,float2 texcoord)
{
float2 samples[12] = {
						float2(-0.326212,-0.405805),
						float2(-0.840144,-0.073580),
						float2(-0.695914, 0.457137),
						float2(-0.203345, 0.620716),
						float2( 0.962340,-0.194983),
						float2( 0.473434,-0.480026),
						float2( 0.519456, 0.767022),
						float2( 0.185461,-0.893124),
						float2( 0.507431, 0.064425),
						float2( 0.896420, 0.412458),
						float2(-0.321940,-0.932615),
						float2(-0.791559,-0.597705)
					  };
  int MipM = 5;
	float4 sum = tex2Dlod(Texture,float4(texcoord,0,MipM));
	for (int i = 0; i < 12; i++)
	{
		float Spread = 0.0275;
		sum += tex2Dlod(Texture,float4(texcoord + Spread * samples[i],0,MipM));
	}

	sum /= 13;
	return dot(sum,0.333);

}
float4 Mask(float2 texcoord)
{
//Code for Guessing where the user was looking at was removed in favor of martys improved world normals.
//Nope added Guessing back Marty Improved World Normals not ready yet. But, I hope for the future.
// I am silly I forgot what I did befor..... so I had to rewrite this from memory welp....Forgot a lot lol Fuck......
//Not the same code in the video. since I forgot what I did and my cloud stroage seems to forget what I did if I rename the file.
//New thing was made........ Happy guessing.
float3 Normals = tex2Dlod(N_Sampler_B,float4(texcoord,0,0)).xyz;
//Changed my mind since my memory sucks New system Guessing where the user is looking at with 8 Detectors kind of like SuperDepth3D.
float2 Location[8] = {
						float2(0.25,0.5 ),//Left of Center
						float2(0.5 ,0.25),//Above Center
						float2(0.5 ,0.5 ),//Center
						float2(0.5 ,0.75),//Below Center
						float2(0.75,0.5 ),//Right of Center
						float2(0.25,0.75),//Down Left
						float2(0.75,0.75),//Down Right
						float2(0.5 ,0.125)//Way Above Center
					 };

   float Best_Guess = 0.5,Center,Above,Below,Left,Right,DL,DR,WAC,WTF;

	if(DepthM(Location[2]).x > BG)//Center
		Center = 1;
	if(DepthM(Location[1]).x > BG)//Above
		Above = 1;
	if(DepthM(Location[3]).x > BG)//Below
		Below = 1;
	if(DepthM(Location[0]).x > BG)//Left
		Left = 1;
	if(DepthM(Location[4]).x > BG)//Right
		Right = 1;
	if(DepthM(Location[5]).x > BG)//Down Left
		DL = 1;
	if(DepthM(Location[6]).x > BG)//Down Right
		DR = 1;
	if(DepthM(Location[7]).x > BG)//Down Right
		WAC = 1;

//Now we guess Where/What the fuck the User is looking at.
if(!Center && !Above && !Below && !Left && !Right && !DL && !DR && WAC)
	Best_Guess = 0.250;
if(!Center && Above && !Below && !Left && !Right && !DL && !DR && WAC)
	Best_Guess = 0.125;
if( Center && Above && !Below && !Left && !Right && !DL && !DR && WAC)
	Best_Guess = 0.0;
if( !Center && !Above && !Below && !Left && !Right && !DL && !DR && !WAC)
	Best_Guess = 1.0;
if( Center && Above && Below && Left && Right && !DL && DR && WAC)
	Best_Guess = 0.0625;
if( Center && Above && Below && Left && Right && DL && !DR && WAC)
	Best_Guess = 0.0625;

	//to be used later to guess if the user if close to a wall...... But, got lazy.
	//if( !Center && !Above && !Below && !Left && !Right && !DL && !DR && !WAC)
	//WTF = 1;
	// float DD = saturate(tex2Dlod(N_Sampler_B,float4(texcoord,0,10)).w),DS = smoothstep(0,0.1,DD),Depth = smoothstep(0,1,DepthM(0.5).x);

   float Top = dot(float3(0,1,0), Normals), Wall = step(Best_Guess,dot(float3(0,0,1), Normals));;
   float Snow = smoothstep(1-SSC,1,WTF ? Top : Top-Wall);

   float Noise;
	MCNoise( Noise, texcoord, 1 , 1 );
	//Hold over for wave noise
	float2 Di[4] = {float2(pix.x,0),float2(-pix.x,0),float2(0,pix.y),float2(0,-pix.y)};
	float3 SN = 1-BB_H(texcoord).rgb;
	[fastopt]
	for (int i = 0; i < 4; i ++)
	{
		SN += (1-BB_H(texcoord + Di[i] * Noise * (0.1*100)).rgb);
	}

   float HeightMap = Top  * dot(saturate(SN/5) , HMP);

    HeightMap = I_Ice ? 1-HeightMap : HeightMap;

   return float4(Snow, lerp(0,HeightMap,Snow) ,Snow,1);
}

float Bump(float2 TC)
{
	float2 t, l, r, d, MA = 5;
	float2 UV = TC.xy, SW = pix * MA;
	float3 NormalsFromHeightMap, NHM_A, NHM_B;

	float Noise;
	MCNoise( Noise, TC, 1 , 1 );

	t.x = Mask( float2( UV.x , UV.y - SW.y ) ).y;
	d.x = Mask( float2( UV.x , UV.y + SW.y ) ).y;
	l.x = Mask( float2( UV.x - SW.x , UV.y ) ).y;
	r.x = Mask( float2( UV.x + SW.x , UV.y ) ).y;
	SW *= 0.5;
	t.y = Mask( float2( UV.x , UV.y - SW.y ) ).y;
	d.y = Mask( float2( UV.x , UV.y + SW.y ) ).y;
	l.y = Mask( float2( UV.x - SW.x , UV.y ) ).y;
	r.y = Mask( float2( UV.x + SW.x , UV.y ) ).y;

	NHM_A = float3(-float2(-(r.x - l.x),-(t.x - d.x)) * 0.5 + 0.4,1);
	NHM_B = float3(-float2(-(r.y - l.y),-(t.y - d.y)) * 0.5 + 0.4,1);

	NormalsFromHeightMap = lerp(NHM_A , NHM_B, 0.5);

return saturate(dot(0.5,NormalsFromHeightMap));
}

float HeightMap(float2 TC)
{
	float HM1 = tex2Dlod(CSnowMask,float4(TC,0,1)).y;
	float HM2 = tex2Dlod(CSnowMask,float4(TC,0,3)).y;
	float HM = (HM1 + HM2)/2;
	return HM * DepthM(TC).y;
}

float2 Parallax( float2 Diverge, float2 Coordinates)
{   float2 TC = Coordinates;
	float D = abs(length(Diverge));
	float Cal_Steps = D + (D * 0.05);

	//ParallaxSteps
	float Steps = clamp(Cal_Steps * 2,1,255);

	// Offset per step progress & Limit
	float LayerDepth = rcp(Steps);

	//Offsets listed here Max Seperation is 3% - 8% of screen space with Depth Offsets & Netto layer offset change based on MS.
	float2 MS = Diverge * pix;
	float2  deltaCoordinates = MS * LayerDepth, ParallaxCoord = Coordinates;
	float CurrentDepthMapValue = Mask(ParallaxCoord).x, CurrentLayerDepth = 0;

	[loop] //Steep parallax mapping
    for ( int i = 0; i < Steps; i++ )
    {	// Doing it this way should stop crashes in older version of reshade, I hope.
        if (CurrentDepthMapValue <= CurrentLayerDepth)
			break; // Once we hit the limit Stop Exit Loop.
        // Shift coordinates horizontally in linear fasion
        ParallaxCoord -= deltaCoordinates;
        // Get depth value at current coordinates
		CurrentDepthMapValue = HeightMap( ParallaxCoord );
        // Get depth of next layer
        CurrentLayerDepth += LayerDepth;
    }

	// Parallax Occlusion Mapping
	float2 PrevParallaxCoord = ParallaxCoord + deltaCoordinates, DepthDifference;
	float afterDepthValue = CurrentDepthMapValue - CurrentLayerDepth;
	float beforeDepthValue = HeightMap( ParallaxCoord ) - CurrentLayerDepth + LayerDepth;
	// Interpolate coordinates
	float weight = afterDepthValue / (afterDepthValue - beforeDepthValue);
	ParallaxCoord = PrevParallaxCoord * max(0,weight) + ParallaxCoord * min(1,1.0f - weight);

	return ParallaxCoord;
}

//Super Simple Sparkly Shader https://www.shadertoy.com/view/XtKyzD
float SSSS(float2 TC)
{
	float2 Noise;
	Noise.x = tex2D(BumpMap,TC).y;
	Noise.y = tex2D(BumpMap,TC).z;
    float result = Noise.x;
    result *= Noise.y;
    result = pow(abs(result), 12.0);
    return 5.0*result;
}

void Out(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 color : SV_Target)
{
	float2 TC = Parallax(float2(0,-Elevation * DepthM(texcoord).y) , texcoord ).xy;
	float Snow = Mask(TC.xy).x, SS = tex2Dlod(BumpMap,float4(TC,0,2)).w;
	float3 Snow_Color = float3(0.75,0.8,0.85), Snow_Scape = (Mask(TC.xy).x - 0.01) * Snow_Color + SSSS(TC.xy * 0.75);

	float3 T_A_A = lerp(Snow_Scape + LumaMask(CDBuffer,TC)*0.2, BB_H( TC.xy ).rgb,1-Snow);
//	if(TC.y > 1)
//		color = Snow_Color;
//	else
		color = T_A_A * lerp(SS,1,1-Snow);
}

float3 SmoothNormals(float2 TC, int NS,int Dir, float SNOffset)
{   //Smooth Normals done in two passes now Faster. But, still slow.
	float4 StoredNormals_Depth = float4(Dir ? DepthNormals(TC) : tex2Dlod(N_Sampler_A,float4(TC,0,0)).xyz,DepthM(TC).x), SmoothedNormals = float4(StoredNormals_Depth.xyz,1);

	[loop] // -1 0 +1 on x and y
	for(float xy = -NS; xy <= NS; xy++)
	{
		if(smoothstep(0,1,StoredNormals_Depth.w) > MaxDepth_Cutoff)
			break;
		float2 XY = Dir ? float2(xy,0) : float2(0,xy);
		float2 offsetcoords = TC + XY * pix * SNOffset;
		float4 Normals_Depth     = float4(Dir ? DepthNormals(offsetcoords) : tex2Dlod(N_Sampler_A,float4(offsetcoords,0,0)).xyz,DepthM(offsetcoords).x);
		if (abs(StoredNormals_Depth.w - Normals_Depth.w) < 0.001 && dot(Normals_Depth.xyz, StoredNormals_Depth.xyz) > 0.5f)
		{
			SmoothedNormals.xyz += Normals_Depth.xyz;
			++SmoothedNormals.w;
		}
	}

	return SmoothedNormals.xyz / SmoothedNormals.w;
}

void Buffers_A(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 Normals : SV_Target0, out float4 BackBuffer_Depth : SV_Target1, out float4 Snow_Info : SV_Target2)
{
	//AO was removed seems going over board for this simple effect.
	Normals = SmoothNormals(texcoord, SN_offset.x, 1, SN_offset.y);
	BackBuffer_Depth =  float4(tex2D(BackBuffer,texcoord).rgb,DepthM(texcoord).x);
	Snow_Info = float4(Mask(texcoord).xyz,0);
}

void Buffers_B(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 Normals_LM : SV_Target0)
{   //AO was removed seems going over board for this simple effect.
	Normals_LM = float4(SmoothNormals(texcoord, SN_offset.x, 0, SN_offset.y).xyz,DepthM(texcoord).x);
}

void Buffers_C(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 BumpMap_Else : SV_Target0)
{
	float2 Noise;
	//should add fake glitter movement comp..... here
	MCNoise( Noise.x, texcoord , framecount * -0.005, 1 );
	MCNoise( Noise.y, texcoord , framecount * +0.005, 2 );

	BumpMap_Else = float4(0,Noise.x,Noise.y,Bump(texcoord));
}

technique OS_SnowScape
	{
			pass Alpha
		{
			VertexShader = PostProcessVS;
			PixelShader = Buffers_A;
			RenderTarget0 = Normals_A;
			RenderTarget1 = BB_Depth_A;
			RenderTarget2 = SnowMask_A;
		}
			pass Beta
		{
			VertexShader = PostProcessVS;
			PixelShader = Buffers_B;
			RenderTarget0 = Normals_B;
		}
			pass Gamma
		{
			VertexShader = PostProcessVS;
			PixelShader = Buffers_C;
			RenderTarget0 = Bump_C;
		}
			pass Out
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
		}
	}
