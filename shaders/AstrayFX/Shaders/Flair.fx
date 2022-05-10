 ////---------//
 ///**Flair**///
 //---------////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Lens Flares are based on Blooming HDR Original Code
//* For Reshade 3.0+
//*  ---------------------------------
//*                                                                    Flair
//*                                                    Let's add some  ------  to your images.
//*                                                                    Flares
//* Due Diligence
//* Eye Adaptation
//* https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/ Search Ref. Temporal Adaptation
//* If I miss any please tell me.
//*
//* LICENSE
//* ============
//* Flare is licenses under: Attribution-NoDerivatives 4.0 International
//*
//* You are free to:
//* Share - copy and redistribute the material in any medium or format
//* for any purpose, even commercially.
//* The licensor cannot revoke these freedoms as long as you follow the license terms.
//* Under the following terms:
//* Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
//* You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
//*
//* NoDerivatives - If you remix, transform, or build upon the material, you may not distribute the modified material.
//*
//* No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//*
//* https://creativecommons.org/licenses/by-nd/4.0/
//*
//* Have fun,
//* Jose Negrete AKA BlueSkyDefender
//*
//* https://github.com/BlueSkyDefender/Depth3D
//* Special thanks to NVIDIA on compatibility with GeForce GPUs and feedback on shader development
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Shared Texture for Blooming HDR or any other shader that want's to use it.
texture TexFlareShared { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };

//You can also make a text file named "DisableFlare.txt" to dissable this shader output and
//only use the Share Texture above in your your shader.

uniform int Flare_Type <
    ui_type = "combo";
    ui_items = "Star Full\0Star Half\0Cross +\0Cross x\0Flare +Skew\0Flare -Skew\0Flare Horizontal\0Flare Vertical\0";
    ui_label = "Flare Type";
    ui_category = "Flare Type";
> = 0;

uniform bool Auto_Flare_Intensity <
	ui_label = "Auto Flare Intensity";
	ui_tooltip = "This will enable the shader to adjust Flare Intensity automatically.\n"
				 "Auto Bloom Intensity will set Bloom Intensity below.";
    ui_category = "Flare Adjustments";
> = false;

uniform float Flare_Clamp_A <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Flare Brightness Threshold";
	ui_tooltip = "Use this to set the color based brightness threshold for what is and what isn't allowed.\n"
				 "Number 1.0 is default.";
	ui_category = "Flare Adjustments";
> = 1.0;

uniform float Flare_Clamp_B <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Flare Max Brightness Cutoff";
	ui_tooltip = "Use this to set the max cutoff point of the brightness things in the image.\n"
				 "Number 1.0 is default.";
	ui_category = "Flare Adjustments";
> = 1.0;

uniform float Flare_Spread <
	ui_type = "slider";
	ui_min = -1.0; ui_max = 1.0; ui_step = 0.01;
	ui_label = "Flare Spread";
	ui_tooltip = "Use this to adjust Flare size & you can override this.\n"
				 "Number 0.5 is default.";
	ui_category = "Flare Adjustments";
> = 0.5;

uniform float Flare_Power <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 12.5; ui_step = 0.125;
	ui_label = "Flare Intensity";
	ui_tooltip = "Use this to set Flare Intensity for your content.\n"
								"Number 1.0 is default.";
	ui_category = "Flare Adjustments";
> = 1.0;

uniform float Flare_Saturation <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 10.0; ui_step = 0.1;
	ui_label = "Flare Saturation";
	ui_tooltip = "Adjustment The amount to adjust the saturation of the flare color.\n"
				 "Number 1.0 is default.";
	ui_category = "Flare Adjustments";
> = 5.0;
//User_Flare_Color needs Flare_Color_Map to be set to -1 and the rest uncommented.
uniform int Flare_Color_Map <
	ui_type = "slider";
	ui_min = -1; ui_max = 8;
	ui_label = "Flare Color Map";
	ui_tooltip = "Use this to set MIP levels for the color map used to se Flare Color.\n"
				 "This samples color in the general area to achieve this effect.\n"
								"Number 4 is default & Zero is Off.";
	ui_category = "Flare Adjustments";
> = 4;

uniform float3 User_Flare_Color <
	ui_type = "color";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Flare Color Adjust";
	ui_tooltip = "Use this to set your own color settings.";
	ui_category = "Flare Adjustments";
> = float3(1.0,1.0,1.0);

uniform float Flare_Localization <
	ui_type = "slider";
	ui_min = -1.0; ui_max = 1.0; ui_step = 0.1;
	ui_label = "Flare Localization";
	ui_tooltip = "This localizes flare to a user selected area.\n"
								"Number Zero is default.";
	ui_category = "Flare Adjustments";
> = 0.0;

uniform int CA_Type <
    ui_type = "combo";
    ui_items = "Off\0Type BD\0Type Box\0Type Hozi\0Type Verti\0";
    ui_label = "Flare CA Type";
	ui_category = "Flare Chromatic Aberration";
> = 1;

uniform float3 Flare_PBD <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Flare CA Adjust";
	ui_tooltip = "Use this to set chromatic aberration for your content.";
	ui_category = "Flare Chromatic Aberration";
> = float3(1.0,0.5,0.0);

//Depth Map//
uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "Normal\0Reversed\0";
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "Linearization for the zBuffer also known as Depth Map.\n"
			     "DM0 is Z-Normal and DM1 is Z-Reversed.\n";
	ui_category = "Depth Map Masking";
> = 0;

uniform float Depth_Map_Adjust <
	ui_type = "drag";
	ui_min = 0.25; ui_max = 250.0;  ui_step = 0.25;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "This allows for you to adjust the DM precision.\n"
				 "Adjust this to keep it as low as possible.\n"
				 "Default is 1.5";
	ui_category = "Depth Map Masking";
> = 1.5;

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Map Masking";
> = false;

uniform bool Mask_Sky <
	ui_label = "Sky Mask";
	ui_tooltip = "Mask Sky using depth";
	ui_category = "Depth Map Masking";
> = false;

uniform bool Flair_Falloff <
	ui_label = "Falloff";
	ui_tooltip = "A depth mask is use to add a fall off distance";
	ui_category = "Depth Map Masking";
> = false;

#if __RENDERER__ >= 0x20000 //Vulkan
	#define Rend 1
#else
	#define Rend 0
#endif
/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
uniform float timer < source = "timer"; >;
//Total amount of frames since the game started.
uniform uint framecount < source = "framecount"; >;
uniform float frametime < source = "frametime";>;
//Made Up Values that look nice not correct.
static const float OffsetA[25] = { 0.0, 1.0, 3.0, 5.0, 7.0, 9.0, 11.0, 13.0, 15.0, 17.0, 19.0, 21.0, 23.0, 25.0, 27.0, 29.0, 31.0, 33.0, 35.0, 37.0, 39.0, 41.0, 43.0, 45.0, 47.0 };
static const float OffsetB[25] = { 0.0, 2.0, 4.0, 6.0, 8.0,10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 22.0, 24.0, 26.0, 28.0, 30.0, 32.0, 34.0, 36.0, 38.0, 40.0, 42.0, 44.0, 46.0, 48.0 };
static const float OffsetC[25] = { 0.5, 1.5, 3.5, 5.5, 7.5, 9.5, 11.5, 13.5, 15.5, 17.5, 19.5, 21.5, 23.5, 25.5, 27.5, 29.5, 31.5, 33.5, 35.5, 37.5, 39.5, 41.5, 43.5, 45.5, 47.5 };
static const float OffsetD[25] = { 0.5, 2.5, 4.5, 6.5, 8.5,10.5, 12.5, 14.5, 16.5, 18.5, 20.5, 22.5, 24.5, 26.5, 28.5, 30.5, 32.5, 34.5, 36.5, 38.5, 40.5, 42.5, 44.5, 46.5, 48.5 };
static const float Weight[25] = { 0.10761,0.10306,0.09750, 0.09110,0.08409,0.07666,0.06903,0.06139,0.05394,0.04680,0.0401,0.0339,0.0284,0.0234,0.0191,0.0154,0.0122,0.0096,0.0074,0.0057,0.0043,0.0032,0.0024,0.0017,0.0012 };
#define FS Flare_Spread < 0 ? Flare_Spread * 4.0 : Flare_Spread * 2.2
#define Flare_Softness 0 //saturate(abs(Flare_Spread)) //Removed because of Opti
#define Alternate framecount % 2 == 0
//Eye Adaptation Set from 0 to 1
static const float Adapt_Seek = 0.5;//Use this to Adjust Eye Adaptation Speed
static const float Adapt_Adjust = 0.5;//Use this to Adjust Eye Seeking Radius for Average Brightness.

float lum(float3 RGB){ return dot(RGB, float3(0.2126, 0.7152, 0.0722) );}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

texture TexDepthBuffer : DEPTH;

texture TexBackBuffer : COLOR;

sampler DepthBufferF
    {
        Texture = TexDepthBuffer;
    };

sampler BackBufferF
    {
        Texture = TexBackBuffer;
    };

texture texFlare {  Width = BUFFER_WIDTH * 0.25; Height = BUFFER_HEIGHT * 0.25; Format = RGBA16f; };

sampler SamplerGlammor
	{
		Texture = texFlare;
	};

texture TexBC { Width = BUFFER_WIDTH  * 0.5; Height = BUFFER_HEIGHT * 0.5; Format = RGBA8; };

sampler SamplerBC
	{
		Texture = TexBC;
	};

texture TexMBB { Width = BUFFER_WIDTH ; Height = BUFFER_HEIGHT ; Format = RGBA8; MipLevels = 8; };

sampler SamplerMBB
	{
		Texture = TexMBB;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

texture TexFlarePast { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler SamplerPastFlare
	{
		Texture = TexFlarePast;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

void A0(float2 texcoord,float PosX,float PosY,inout float D, inout float E, inout float P, inout float T, inout float H, inout float III, inout float DD )
{
	float PosXD = -0.035+PosX, offsetD = 0.001;D = all( abs(float2( texcoord.x -PosXD, texcoord.y-PosY)) < float2(0.0025,0.009));D -= all( abs(float2( texcoord.x -PosXD-offsetD, texcoord.y-PosY)) < float2(0.0025,0.007));
	float PosXE = -0.028+PosX, offsetE = 0.0005;E = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.009));E -= all( abs(float2( texcoord.x -PosXE-offsetE, texcoord.y-PosY)) < float2(0.0025,0.007));E += all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.001));
	float PosXP = -0.0215+PosX, PosYP = -0.0025+PosY, offsetP = 0.001, offsetP1 = 0.002;P = all( abs(float2( texcoord.x -PosXP, texcoord.y-PosYP)) < float2(0.0025,0.009*0.775));P -= all( abs(float2( texcoord.x -PosXP-offsetP, texcoord.y-PosYP)) < float2(0.0025,0.007*0.680));P += all( abs(float2( texcoord.x -PosXP+offsetP1, texcoord.y-PosY)) < float2(0.0005,0.009));
	float PosXT = -0.014+PosX, PosYT = -0.008+PosY;T = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosYT)) < float2(0.003,0.001));T += all( abs(float2( texcoord.x -PosXT, texcoord.y-PosY)) < float2(0.000625,0.009));
	float PosXH = -0.0072+PosX;H = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.001));H -= all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.009));H += all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.00325,0.009));
	float offsetFive = 0.001, PosX3 = -0.001+PosX;III = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.009));III -= all( abs(float2( texcoord.x -PosX3 - offsetFive, texcoord.y-PosY)) < float2(0.003,0.007));III += all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.001));
	float PosXDD = 0.006+PosX, offsetDD = 0.001;DD = all( abs(float2( texcoord.x -PosXDD, texcoord.y-PosY)) < float2(0.0025,0.009));DD -= all( abs(float2( texcoord.x -PosXDD-offsetDD, texcoord.y-PosY)) < float2(0.0025,0.007));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
texture texLumF {Width = 256; Height = 256; Format = R16F; MipLevels = 9;}; //Sample at 256x256 map only has nine mip levels; 0-1-2-3-4-5-6-7-8 : 256,128,64,32,16,8,4,2, and 1 (1x1).

sampler SamplerLumG
	{
		Texture = texLumF;
	};

float LuminanceG(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 texXY = texcoord;
	float2 midHV = (Adapt_Seek-1) * float2(BUFFER_WIDTH * 0.5,BUFFER_HEIGHT * 0.5) * pix;
	texcoord = float2((texXY.x*Adapt_Seek)-midHV.x,(texXY.y*Adapt_Seek)-midHV.y);
	return lum(tex2D(BackBufferF,texcoord).rgb);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
texture texAvgLumF { Width = 256; Height = 256; Format = R16F; };

sampler SamplerAvgLumG
	{
		Texture = texAvgLumF;
	};

texture TexAvgLumaLastF { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R16F; };

sampler SamplerAvgLumaLastG
	{
		Texture = TexAvgLumaLastF;
	};

float Average_LuminanceF(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float AA = (1-Adapt_Adjust) * 1000, L =  tex2Dlod(SamplerLumG,float4(texcoord,0,11)).x, PL = tex2D(SamplerAvgLumaLastG, texcoord).x;
	//Temporal Adaptation
	return PL + (L - PL) * (1.0 - exp(-frametime/AA));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float2 DepthMap(float2 texcoord : TEXCOORD0)
{
	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	float zBuffer = tex2Dlod(DepthBufferF, float4(texcoord,0,0)).x,Raw; //Depth Buffer

	//Conversions to linear space.....
	//Near & Far Adjustment
	float Far = 1.0, Near = 0.125/Depth_Map_Adjust,DA = Depth_Map_Adjust*2; //Division Depth Map Adjust - Near

	float2 Z = float2( zBuffer, 1-zBuffer );

	if (Depth_Map == 0)//DM0. Normal
	{
		zBuffer = Far * Near / (Far + Z.x * (Near - Far));
		Raw = pow(abs(Z.x),DA);
	}
	else if (Depth_Map == 1)//DM1. Reverse
	{
		zBuffer = Far * Near / (Far + Z.y * (Near - Far));
		Raw = pow(abs(Z.y),DA);
	}

	return saturate(float2(zBuffer,Raw));
}

float4 BB( float2 texcoord )
{
	return tex2Dlod(BackBufferF, float4(texcoord,0,0)).rgba;
}

void ColorBB(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 MBB : SV_Target)
{
    MBB = BB( texcoord ).rgb;
}

float4 GBightColors( float2 texcoord )
{ float DM = smoothstep(0,1,DepthMap(texcoord).x);
	//Forced Depth Masking
	if(DM == 1)
		DM = 1;
	else
		DM = 0;

	if(!Mask_Sky)
		DM = 0;

	float4 CM = lerp(tex2Dlod(SamplerMBB,float4(texcoord,0,clamp(Flare_Color_Map,0,8))).rgba,0,DM);
	float4 BC = lerp(BB(texcoord),0,DM);
    // Check whether fragment output is higher than threshold,if so output as brightness color.
	float I_A = lum(BC.rgb), I_B = 1-I_A;   
	I_A *= I_B >= 1-Flare_Clamp_B;//Trim from Top
    BC.a = I_A > lerp(0.9,0.999,Flare_Clamp_A);// ? 1 : 0;
    BC.rgb *= BC.a ;

	if(Flare_Color_Map > 0)
		BC.rgb = CM.rgb * BC.a;
	else if(Flare_Color_Map == -1)
    	BC.rgb *= User_Flare_Color;

	if(Flair_Falloff)
    	BC.rgb = lerp(BC.rgb,0.0,DepthMap(texcoord).y);

   //Bloom Saturation
   BC.rgb  = lerp(lum(BC.rgb),BC.rgb,Flare_Saturation);

	float2 Stored_TC = texcoord, FL;
	if (Flare_Localization != 0)
	{
		Stored_TC *= 1.0 - Stored_TC;

		float vignette = Stored_TC.x*Stored_TC.y*12.5;

		if (Flare_Localization < 0)
			FL = float2(0,abs(Flare_Localization));
		else
			FL = float2(abs(Flare_Localization),1);

	    vignette = 1-smoothstep(FL.x,FL.y,vignette);

		if (Flare_Localization < 0)
			vignette = 1-vignette;

		BC.rgb = lerp(BC.rgb,0.0,vignette);
	}
   return float4(saturate(BC.rgb),BC.a);
}

void StoredBB(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 BC : SV_Target)
{
    BC = GBightColors( texcoord ).rgb;
}
#if !Rend
//Done this way to reduce TempRegisters for DX9 compatablity.
float3 Glammor(sampler2D Sample, float2 texcoord, float N, int Switch, float BBS)
{
	float3 GBloom = Switch ? tex2D(Sample,texcoord).rgb : GBightColors(texcoord).rgb;
		   GBloom *= Weight[0];
	[loop]
	for(int i = 1; i < N; ++i)
	{   float4 offset = float4(OffsetA[i] * pix.xy,OffsetB[i] * pix.xy);
		if (Alternate)
			offset = float4(OffsetC[i] * pix.xy,OffsetD[i] * pix.xy);
			//+ & X
		if(Flare_Type == 4 || Flare_Type == 5 || Flare_Type == 6 || Flare_Type == 7)
		{
			float4 XXZZ = float4( offset.x,-offset.x, offset.z,-offset.z);
			float4 YYWW = float4( offset.y,-offset.y, offset.w,-offset.w);
			if(Flare_Type == 5 || Flare_Type == 7)
				XXZZ = Switch ? 0 : float4(-offset.x, offset.x,-offset.z, offset.z);
			if(Flare_Type == 6)
				YYWW = Switch ? 0 : float4( offset.y,-offset.y, offset.w,-offset.w);

			GBloom += tex2Dlod(Sample, float4(texcoord + float2( XXZZ.x, YYWW.x) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( XXZZ.y, YYWW.y) * BBS,0,0) ).rgb * Weight[i];

			GBloom += tex2Dlod(Sample, float4(texcoord + float2( XXZZ.z, YYWW.z) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( XXZZ.w, YYWW.w) * BBS,0,0) ).rgb * Weight[i];
		}
		else
		{
			float4 YYXX = Switch ? 0 : float4( offset.y,-offset.y,-offset.x, offset.x);

			GBloom += tex2Dlod(Sample, float4(texcoord + float2( offset.x, YYXX.x) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2(-offset.x, YYXX.y) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( YYXX.z, offset.y) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( YYXX.w,-offset.y) * BBS,0,0) ).rgb * Weight[i];

			float4 WWZZ = Switch ? 0 : float4( offset.w,-offset.w,-offset.z, offset.z);

			GBloom += tex2Dlod(Sample, float4(texcoord + float2( offset.z, WWZZ.x) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2(-offset.z, WWZZ.y) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( WWZZ.z, offset.w) * BBS,0,0) ).rgb * Weight[i];
			GBloom += tex2Dlod(Sample, float4(texcoord + float2( WWZZ.w,-offset.w) * BBS,0,0) ).rgb * Weight[i];
		}

	}
	GBloom *= 0.5;

   return GBloom;
}

float3 GlammorA(float2 texcoord )
{
	float N, BBS = FS;
	if(Flare_Type == 0 || Flare_Type == 1 || Flare_Type == 2 || Flare_Type == 6 || Flare_Type == 7)
		N = int(lerp(5,25,abs(Flare_Spread)));
	else if(Flare_Type == 3 || Flare_Type == 4 || Flare_Type == 5 )
		N = 1;

   return Glammor(SamplerBC, texcoord, N, 1, BBS);
}

float3 GlammorB(float2 texcoord )
{
	float N, BBS = FS;
	if(Flare_Type == 0 || Flare_Type == 1 || Flare_Type == 3 || Flare_Type == 4 || Flare_Type == 5 )
		N = int(lerp(5,25,abs(Flare_Spread)));
	else if(Flare_Type == 2 || Flare_Type == 6 || Flare_Type == 7 )
		N = 1;

	if(Flare_Type == 1)
		BBS *= 0.5;

   return Glammor(SamplerBC, texcoord, N, 0, BBS);
}
#else
float3 GlammorA(float2 texcoord )
{   float3 GBloom = tex2D(SamplerBC,texcoord).rgb;
	float N, BBS = FS;
	GBloom *= Weight[0];
	if(Flare_Type == 0 || Flare_Type == 1 || Flare_Type == 2 || Flare_Type == 6 || Flare_Type == 7)
		N = 25;
	else if(Flare_Type == 3 || Flare_Type == 4 || Flare_Type == 5 )
		N = 1;

	[unroll]
	for(int i = 1; i < N; ++i)
	{   float4 offset = float4(OffsetA[i] * pix.xy,OffsetB[i] * pix.xy);
		if (Alternate)
			offset = float4(OffsetC[i] * pix.xy,OffsetD[i] * pix.xy);
		//+
		if(Flare_Type == 6)
		{
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x, 0) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z, 0) * BBS ).rgb * Weight[i];
		}
		else if(Flare_Type == 7)
		{
			GBloom += tex2D(SamplerBC, texcoord + float2( 0, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0,-offset.y) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2( 0, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0,-offset.w) * BBS ).rgb * Weight[i];
		}
		else
		{
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0,-offset.y) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z, 0) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( 0,-offset.w) * BBS ).rgb * Weight[i];
		}
	}
		GBloom *= 0.5;

   return GBloom;
}

float3 GlammorB(float2 texcoord )
{   float3 GBloom = GBightColors(texcoord).rgb;
	float N, BBS = FS;
	GBloom *= Weight[0];
	if(Flare_Type == 0 || Flare_Type == 1 || Flare_Type == 3 || Flare_Type == 4 || Flare_Type == 5 )
		N = 25;
	else if(Flare_Type == 2 || Flare_Type == 6 || Flare_Type == 7 )
		N = 1;

	if(Flare_Type == 1)
		BBS *= 0.5;

	[unroll]
	for(int i = 1; i < N; ++i)
	{   float4 offset = float4(OffsetA[i] * pix.xy,OffsetB[i] * pix.xy);
		if (Alternate)
			offset = float4(OffsetC[i] * pix.xy,OffsetD[i] * pix.xy);
		//X
		if(Flare_Type == 4)
		{
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x,-offset.y) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z,-offset.w) * BBS ).rgb * Weight[i];
		}
		else if(Flare_Type == 5)
		{
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x,-offset.y) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z,-offset.w) * BBS ).rgb * Weight[i];
		}
		else
		{
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x,-offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.x, offset.y) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.x,-offset.y) * BBS ).rgb * Weight[i];

			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z,-offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2(-offset.z, offset.w) * BBS ).rgb * Weight[i];
			GBloom += tex2D(SamplerBC, texcoord + float2( offset.z,-offset.w) * BBS ).rgb * Weight[i];
		}
	}
		GBloom *= 0.5;

   return GBloom;
}
#endif

// Spread the blur a bit more.
void Glammor(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 GBloom : SV_Target)
{	GBloom = 0;
	float GI = Flare_Power, AL = smoothstep(0,1,1. - tex2D(SamplerAvgLumG,0.0).x);
	if(Auto_Flare_Intensity)
			GI *= AL;
		GBloom = ( GlammorA(texcoord) + GlammorB(texcoord) ) * GI;
}

float2 BD(float2 p, float k1) //Polynomial Lens
{
	if(!CA_Type == 1)
		discard;
	p *= 0.5 + 0.5;//Center
	// Normalize the u,v coordinates in the range [-1;+1]
	p = (2.0f * p - 1.0f) / 1.0f;
	// Calculate l2 norm
	float r2 = p.x*p.x + p.y*p.y;
	float r4 = pow(r2,2);
	// Forward transform
	float x2 = p.x * (1.0 + k1 * r2);
	float y2 = p.y * (1.0 + k1 * r2);
	// De-normalize to the original range
	p.x = (x2 + 1.0) * 1.0 / 2.0;
	p.y = (y2 + 1.0) * 1.0 / 2.0;

return p;
}

float2 HVP(float2 texcoord, float k1)
{
	if(!CA_Type == 2)
		discard;
    float X = k1;
    float Y = k1;
    float midW = (X - 1)*(BUFFER_WIDTH*0.5)*pix.x;
    float midH = (Y - 1)*(BUFFER_HEIGHT*0.5)*pix.y;

    return  float2((texcoord.x*X)-midW,(texcoord.y*Y)-midH);
}

float2 HP(float2 texcoord, float k1)
{
	if(!CA_Type == 3)
		discard;
    float X = k1;
    float Y = 1;
    float midW = (X - 1)*(BUFFER_WIDTH*0.5)*pix.x;
    float midH = (Y - 1)*(BUFFER_HEIGHT*0.5)*pix.y;

    return float2((texcoord.x*X)-midW,(texcoord.y*Y)-midH);
}

float2 VP(float2 texcoord, float k1)
{
	if(!CA_Type == 4)
		discard;
    float X = 1;
    float Y = k1;
    float midW = (X - 1)*(BUFFER_WIDTH*0.5)*pix.x;
    float midH = (Y - 1)*(BUFFER_HEIGHT*0.5)*pix.y;

    return float2((texcoord.x*X)-midW,(texcoord.y*Y)-midH);
}

float3 CAFlare(float2 texcoord)
{
  float2 uv_red, uv_green, uv_blue;
	float DM, color_red, color_green, color_blue, K1_Red = Flare_PBD.x , K1_Green = Flare_PBD.y , K1_Blue = Flare_PBD.z ;
	if(CA_Type == 1)
	{
		uv_red = BD(texcoord.xy,K1_Red * 0.03);
		uv_green = BD(texcoord.xy,K1_Green * 0.03);
		uv_blue = BD(texcoord.xy,K1_Blue * 0.03);
	}
	else if(CA_Type == 2)
	{
		uv_red = HVP(texcoord.xy,1-K1_Red * 0.025);
		uv_green = HVP(texcoord.xy,1-K1_Green * 0.025);
		uv_blue = HVP(texcoord.xy,1-K1_Blue * 0.025);
	}
	else if(CA_Type == 3)
	{
		uv_red = HP(texcoord.xy,1-K1_Red * 0.025);
		uv_green = HP(texcoord.xy,1-K1_Green * 0.025);
		uv_blue = HP(texcoord.xy,1-K1_Blue * 0.025);
	}
	else if(CA_Type == 4)
	{
		uv_red = VP(texcoord.xy,1-K1_Red * 0.025);
		uv_green = VP(texcoord.xy,1-K1_Green * 0.025);
		uv_blue = VP(texcoord.xy,1-K1_Blue * 0.025);
	}

	if(CA_Type > 0)
	{
		color_red = tex2Dlod(SamplerGlammor,float4(uv_red,0,Flare_Softness)).r;
		color_green = tex2Dlod(SamplerGlammor,float4(uv_green,0,Flare_Softness)).g;
		color_blue = tex2Dlod(SamplerGlammor,float4(uv_blue,0,Flare_Softness)).b;
	}
	else
	{
		color_red = tex2Dlod(SamplerGlammor,float4(texcoord,0,Flare_Softness)).r;
		color_green = tex2Dlod(SamplerGlammor,float4(texcoord,0,Flare_Softness)).g;
		color_blue = tex2Dlod(SamplerGlammor,float4(texcoord,0,Flare_Softness)).b;
	}

	return float3(color_red,color_green,color_blue);
}

void PS_StoreAvgLumaF(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float Stored : SV_Target0, out float3 Shared : SV_Target1)
{
    Stored = tex2D(SamplerAvgLumG,texcoord).x;
    Shared = (CAFlare(texcoord) + tex2D(SamplerPastFlare,texcoord).rgb) ;
}

float3 ShaderGlammor(float2 texcoord)
{ float DM;
	float3 Color = BB(texcoord).rgb, FBloom = (CAFlare(texcoord) + tex2D(SamplerPastFlare,texcoord).rgb) ;
	//Gamma Correction
	Color = pow(abs(Color),2.2);

	//Bloom
	Color = Color + FBloom;

	Color = pow(abs(Color),rcp(2.2));

	return Color;
}

void Past_Flare(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float3 Stored : SV_Target)
{
    Stored = CAFlare(texcoord);
}

void A1(float2 texcoord,float PosX,float PosY,inout float I, inout float N, inout float F, inout float O)
{
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;I = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;N = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));N -= all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;F = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));F -= all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));F += all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;O = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));O -= all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
}

////////////////////////////////////////////////////////Watermark/////////////////////////////////////////////////////////////////////////
float4 OutF(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 Color = ShaderGlammor(texcoord).rgb;
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y,A,B,C,D,E,F,G,H,I,J,K,L,PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;L = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));A0(texcoord,PosX,PosY,A,B,C,D,E,F,G );A1(texcoord,PosX,PosY,H,I,J,K);
	return timer <= 12500 ? A+B+C+D+E+F+G+H+I+J+K+L ? 0.02 : float4(Color,1.) : float4(Color,1.);
}
///////////////////////////////////////////////////////////////////ReShade.fxh//////////////////////////////////////////////////////////////////////
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{// Vertex shader generating a triangle covering the entire screen
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}//Using this to make it portable
//*Rendering passes*//
technique Flair
{
		pass ColorMap
	{
		VertexShader = PostProcessVS;
		PixelShader = ColorBB;
		RenderTarget = TexMBB;
	}
		pass Past
	{
		VertexShader = PostProcessVS;
		PixelShader = Past_Flare;
		RenderTarget = TexFlarePast;
	}
		pass BrigthColors
	{
		VertexShader = PostProcessVS;
		PixelShader = StoredBB;
		RenderTarget = TexBC;
	}
		pass Star
	{
		VertexShader = PostProcessVS;
		PixelShader = Glammor;
		RenderTarget = texFlare;
	}
			pass Lum
	    {
	        VertexShader = PostProcessVS;
	        PixelShader = LuminanceG;
	        RenderTarget = texLumF;
	    }
	    	pass Avg_Lum
	    {
	        VertexShader = PostProcessVS;
	        PixelShader = Average_LuminanceF;
	        RenderTarget = texAvgLumF;
	    }
			pass StoreAvgLuma
	    {
	        VertexShader = PostProcessVS;
	        PixelShader = PS_StoreAvgLumaF;
	        RenderTarget0 = TexAvgLumaLastF;
	        RenderTarget1 = TexFlareShared;
	    }
	#if !exists "DisableFlare.txt"
	pass FlareOut
	{
		VertexShader = PostProcessVS;
		PixelShader = OutF;
	}
	#endif
}
