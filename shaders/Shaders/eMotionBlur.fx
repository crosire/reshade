////----------------//
///**Optical Flow**///
//----------------////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// For Reshade 3.0
// --------------------------
// This work is licensed under a Creative Commons Attribution 3.0 Unported License.
// So you are free to share, modify and adapt it for your needs, and even use it for commercial use.
// I would also love to hear about a project you are using it with.
// https://creativecommons.org/licenses/by/3.0/us/
//
// Have fun,
// Jose Negrete AKA BlueSkyDefender
//                                                                   LICENSE A for Optical Flow
//                                        //---------------------------------------------------------------------------------//
//                                        // Optical Flow Filter Made by Thomas Diewald ported/modded over to Reshade by BSD //
//                                        //		      GitHub Link for sorce info https://github.com/diwi/                //
//                                        // 	      His Web Link http://thomasdiewald.com/blog/?p=2766 Thank You.         //
//                                        //_________________________________________________________________________________//
//
// Feel Free to to use The Optical Flow code in this shader if your going to reuse this code make sure it don't use any of the overwatch part of the code.
//
// LICENSE B for Overwatch.fx
// ============
// Overwatch is licenses under: Attribution-NoDerivatives 4.0 International
//
// You are free to:
// Share - copy and redistribute the material in any medium or format
// for any purpose, even commercially.
// The licensor cannot revoke these freedoms as long as you follow the license terms.
// Under the following terms:
// Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
// You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
//
// NoDerivatives - If you remix, transform, or build upon the material, you may not distribute the modified material.
//
// No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//
// https://creativecommons.org/licenses/by-nd/4.0/
//
// Have fun,
// Jose Negrete AKA BlueSkyDefender
//
// https://github.com/BlueSkyDefender/Depth3D
 ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if exists "Overwatch.fxh"                                           //Overwatch Intercepter//
	#include "Overwatch.fxh"
#else //DA_W Depth_Linearization | DB_X Depth_Flip
	static const float DA_W = 0.0, DB_X = 0, DD_X = 1,DD_Y = 1,DD_Z = 0,DD_W = 0;
	#define NC 0
	#define NP 0
#endif

uniform int Blur_Amount <
	ui_type = "drag";
	ui_min = 0; ui_max = 32;
	ui_label = "Blur Amount";
	ui_tooltip = "Blur Step Ammount";
	ui_category = "Motion Blur";
> = 16;

uniform float mblurstrength <
	ui_type = "drag";
	ui_min = 0; ui_max = 1.0;
	ui_label = "Motion Blur Strength";
	ui_tooltip = "Motion Blur Power, this adjust the application of the blur";
	ui_category = "Motion Blur";
> = 0.5;

uniform bool Depth_Scaling <
	ui_label = "Depth Scaling";
	ui_tooltip = "Use this to scale Direction to Depth.";
  ui_category = "Motion Blur";
> = true;

/*
uniform int Switch <
	ui_type = "combo";
	ui_items = "Velocity\0Direction\0";
	ui_label = "Custom Depth Map";
	ui_tooltip = "Pick your Depth Map.";
	ui_category = "Depth Buffer";
> = 1;
*/

uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "DM0 Normal\0DM1 Reversed\0";
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "Linearization for the zBuffer also known as Depth Map.\n"
			     "DM0 is Z-Normal and DM1 is Z-Reversed.\n";
	ui_category = "Depth Buffer";
> = DA_W;

uniform float Depth_Map_Adjust <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 250.0;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "This allows for you to adjust the DM precision.\n"
				 "Adjust this to keep it as low as possible.\n"
				 "Default is 7.5";
	ui_category = "Depth Buffer";
> = 7.5;

uniform float Offset <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Depth Map Offset";
	ui_tooltip = "Depth Map Offset is for non conforming ZBuffer.\n"
				 "It,s rare if you need to use this in any game.\n"
				 "Use this to make adjustments to DM 0 or DM 1.\n"
				 "Default and starts at Zero and it's Off.";
	ui_category = "Depth Buffer";
> = 0;

uniform float3 WeaponHandAdjust <
	ui_type = "drag";
	ui_min = 0; ui_max = 1;
	ui_label = "Weapon Hand";
	ui_tooltip = "Use this to adjust the weapon hand. Also make sure you don't adjust the Depth Buffer after this.";
	ui_category = "Depth Buffer";
> = 0.0;

uniform float2 Horizontal_and_Vertical <
	ui_type = "drag";
	ui_min = 0.125; ui_max = 2;
	ui_label = "Z Horizontal & Vertical Size";
	ui_tooltip = "Adjust Horizontal and Vertical Resize. Default is 1.0.";
	ui_category = "Depth Buffer";
> = float2(DD_X,DD_Y);

uniform int2 Image_Position_Adjust<
	ui_type = "drag";
	ui_min = -4096.0; ui_max = 4096.0;
	ui_label = "Z Position";
	ui_tooltip = "Adjust the Image Position if it's off by a bit. Default is Zero.";
	ui_category = "Depth Buffer";
> = int2(DD_Z,DD_W);

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Buffer";
> = DB_X;

uniform int Debug <
	ui_type = "combo";
	ui_items = "Off\0Depth\0Direction\0";
	ui_label = "Debug View";
	ui_tooltip = "View Debug Buffers.";
	ui_category = "Debug Buffer";
> = 0;

/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
uniform float timer < source = "timer"; >;
//#define SUM_RGB(v) ((v).r + (v).g + (v).b)

texture DepthBufferTex : DEPTH;

sampler ZBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

texture2D currTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R32f; MipLevels = 2;};
sampler2D curr_frame { Texture = currTex; };
//Needs to be 32f or bit loss will de-sync the images
texture2D prevTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R32f; };
sampler2D prev_frame { Texture = prevTex; };

texture2D MF { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; MipLevels = 2;};
sampler2D Motion_Info { Texture = MF;
 };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float DepthMap(float2 texcoord : TEXCOORD0)
{
	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	float2 texXY = texcoord + Image_Position_Adjust * pix;
	float2 midHV = (Horizontal_and_Vertical-1) * float2(BUFFER_WIDTH * 0.5,BUFFER_HEIGHT * 0.5) * pix;
	texcoord = float2((texXY.x*Horizontal_and_Vertical.x)-midHV.x,(texXY.y*Horizontal_and_Vertical.y)-midHV.y);

	float zBuffer = tex2D(ZBuffer, texcoord).x; //Depth Buffer

	//Conversions to linear space.....
	//Near & Far Adjustment
	float Far = 1.0, Near = 0.125/Depth_Map_Adjust; //Division Depth Map Adjust - Near
	float2 C = float2( Far / Near, 1. - Far / Near ), Offsets = float2(1 + Offset,1 - Offset), Z = float2( zBuffer, 1-zBuffer );

	if (Offset > 0)
		Z = min( 1., float2( Z.x * Offsets.x , Z.y / Offsets.y  ));
	//MAD - RCP
	if (Depth_Map == 0) //DM0 Normal
		zBuffer = rcp(Z.x * C.y + C.x);
	else if (Depth_Map == 1) //DM1 Reverse
		zBuffer = rcp(Z.y * C.y + C.x);

	float  WA_A = lerp(100,10000,WeaponHandAdjust.x), WA_B = WeaponHandAdjust.y * 0.05;
	return lerp(saturate(zBuffer),(zBuffer - WA_B / ( 1 - WA_B)) * WA_A,saturate(step(saturate(zBuffer),WeaponHandAdjust.z)));
}

float DS(float2 texcoord)
{
  return tex2Dlod(curr_frame, float4(texcoord,0,0.125)).x ;
}

float4 Flow(float Past, float Current,float Sensitivy)
{ // threshold
  float threshold = 1.0;
  //OG code used Sobel for this part. It's faster to do it this way.
  float currddx = ddx(Current);
  float currddy = ddy(Current);
  float prevddx = ddx(Past);
  float prevddy = ddy(Past);

  // dt, dx, dy
  float dt = clamp(-10. * ( Current - Past ),-1.,1.); //Direction + Time + Clamp to keep in range.
  float dx = currddx + prevddx;
  float dy = currddy + prevddy;

  // gradient length
  float dd = sqrt(dx*dx + dy*dy + 1);
  // optical flow
  float2 flow = dt * float2(dx, dy) / dd;


  float len_old = sqrt(flow.x*flow.x + flow.y*flow.y + Sensitivy );
  float len_new = max(len_old - threshold, 1);
  flow *= len_new / len_old;
  //if(Switch)
    return float4(flow.rg,0, 1.0);
  /*
  else
  {
    float mag = length(flow) * 1;
    mag = clamp(mag, 0.0, 1.0);

    float len = mag;
    float r = 0.5 * (1.0 + flow.x / (len + 0.0001));
    float g = 0.5 * (1.0 + flow.y / (len + 0.0001));
    float b = 0.5 * (2.0 - (r + g));
    return float4(r,g,b,len);
  }
  */
}

float4 MotionFlow(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
  float Sensitivy = 0.00000001;

  float Current = DS(texcoord).x;
  float Past = tex2D(prev_frame ,texcoord).x;

  return Flow(Past,Current,Sensitivy);
}

float3 MotionBlur(float2 texcoord)
{
    float D_S = Depth_Scaling ? smoothstep(0,1,saturate(1-DS(texcoord).x) * mblurstrength) : mblurstrength * 0.5;
    float weight = 1.0, blursamples = Blur_Amount;
    //Direction of blur and assumption that blur should be stronger near the cam.
    float2 uvoffsets = tex2Dlod(Motion_Info,float4(texcoord,0,0)).xy * D_S;
    //apply motion blur
    float3 sum, accumulation, weightsum;
    [loop]
    for (float i = -blursamples; i <= blursamples; i++)
    {
      float3 currsample = tex2Dlod(BackBuffer, float4(texcoord + (i * uvoffsets) * pix,0,0) ).rgb;
      accumulation += currsample * weight;
      weightsum += weight;
    }

    if(Debug == 0)
      return accumulation / weightsum;
    else if(Debug == 1)
      return DS(texcoord).xxx;
    else
      return float3(uvoffsets * 0.5 + 0.5,0);
}

float3 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{   //Overwatch integration
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y, Text_Timer = 12500, BT = smoothstep(0,1,sin(timer*(3.75/1000)));
	float D,E,P,T,H,Three,DD,Dot,I,N,F,O,R,EE,A,DDD,HH,EEE,L,PP,NN,PPP,C,Not,No;
	float3 Color = MotionBlur(texcoord).rgb;

	if(NC || NP)
		Text_Timer = 18750;

	[branch] if(timer <= Text_Timer)
	{
		//DEPTH
		//D
		float PosXD = -0.035+PosX, offsetD = 0.001;
		float OneD = all( abs(float2( texcoord.x -PosXD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float TwoD = all( abs(float2( texcoord.x -PosXD-offsetD, texcoord.y-PosY)) < float2(0.0025,0.007));
		D = OneD-TwoD;
		//E
		float PosXE = -0.028+PosX, offsetE = 0.0005;
		float OneE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.009));
		float TwoE = all( abs(float2( texcoord.x -PosXE-offsetE, texcoord.y-PosY)) < float2(0.0025,0.007));
		float ThreeE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.001));
		E = (OneE-TwoE)+ThreeE;
		//P
		float PosXP = -0.0215+PosX, PosYP = -0.0025+PosY, offsetP = 0.001, offsetP1 = 0.002;
		float OneP = all( abs(float2( texcoord.x -PosXP, texcoord.y-PosYP)) < float2(0.0025,0.009*0.775));
		float TwoP = all( abs(float2( texcoord.x -PosXP-offsetP, texcoord.y-PosYP)) < float2(0.0025,0.007*0.680));
		float ThreeP = all( abs(float2( texcoord.x -PosXP+offsetP1, texcoord.y-PosY)) < float2(0.0005,0.009));
		P = (OneP-TwoP) + ThreeP;
		//T
		float PosXT = -0.014+PosX, PosYT = -0.008+PosY;
		float OneT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosYT)) < float2(0.003,0.001));
		float TwoT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosY)) < float2(0.000625,0.009));
		T = OneT+TwoT;
		//H
		float PosXH = -0.0072+PosX;
		float OneH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.001));
		float TwoH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.009));
		float ThreeH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.00325,0.009));
		H = (OneH-TwoH)+ThreeH;
		//Three
		float offsetFive = 0.001, PosX3 = -0.001+PosX;
		float OneThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.009));
		float TwoThree = all( abs(float2( texcoord.x -PosX3 - offsetFive, texcoord.y-PosY)) < float2(0.003,0.007));
		float ThreeThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.001));
		Three = (OneThree-TwoThree)+ThreeThree;
		//DD
		float PosXDD = 0.006+PosX, offsetDD = 0.001;
		float OneDD = all( abs(float2( texcoord.x -PosXDD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float TwoDD = all( abs(float2( texcoord.x -PosXDD-offsetDD, texcoord.y-PosY)) < float2(0.0025,0.007));
		DD = OneDD-TwoDD;
		//Dot
		float PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;
		float OneDot = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));
		Dot = OneDot;
		//INFO
		//I
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;
		float OneI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));
		float TwoI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));
		float ThreeI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		I = OneI+TwoI+ThreeI;
		//N
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;
		float OneN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));
		float TwoN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		N = OneN-TwoN;
		//F
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;
		float OneF = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));
		float TwoF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));
		float ThreeF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		F = (OneF-TwoF)+ThreeF;
		//O
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;
		float OneO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));
		float TwoO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
		O = OneO-TwoO;
		//Text Warnings: No Profile / Not Compatible
		//PosY += 0.953;
		PosX -= 0.483;
		float PosXNN = -0.458+PosX, offsetNN = 0.0015;
		float OneNN = all( abs(float2( texcoord.x -PosXNN, texcoord.y-PosY)) < float2(0.00325,0.009));
		float TwoNN = all( abs(float2( texcoord.x -PosXNN, texcoord.y-PosY-offsetNN)) < float2(0.002,0.008));
		NN = OneNN-TwoNN;
		//PPP
		float PosXPPP = -0.451+PosX, PosYPPP = -0.0025+PosY, offsetPPP = 0.001, offsetPPP1 = 0.002;
		float OnePPP = all( abs(float2( texcoord.x -PosXPPP, texcoord.y-PosYPPP)) < float2(0.0025,0.009*0.775));
		float TwoPPP = all( abs(float2( texcoord.x -PosXPPP-offsetPPP, texcoord.y-PosYPPP)) < float2(0.0025,0.007*0.680));
		float ThreePPP = all( abs(float2( texcoord.x -PosXPPP+offsetPPP1, texcoord.y-PosY)) < float2(0.0005,0.009));
		PPP = (OnePPP-TwoPPP) + ThreePPP;
		//C
		float PosXC = -0.450+PosX, offsetC = 0.001;
		float OneC = all( abs(float2( texcoord.x -PosXC, texcoord.y-PosY)) < float2(0.0035,0.009));
		float TwoC = all( abs(float2( texcoord.x -PosXC-offsetC, texcoord.y-PosY)) < float2(0.0025,0.007));
		C = OneC-TwoC;
		if(NP)
			No = (NN + PPP) * BT; //Blinking Text
		if(NC)
			Not = (NN + C) * BT; //Blinking Text
		//Website
		return D+E+P+T+H+Three+DD+Dot+I+N+F+O+No+Not ? (1-texcoord.y*50.0+48.85)*texcoord.y-0.500: Color;
	}
	else
		return Color;
}

void CurrentFrame(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float color : SV_Target)
{
	color = DepthMap(texcoord);
}

void PreviousFrame(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float prev : SV_Target)
{
	prev = DS(texcoord).x;
}

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

//*Rendering passes*//

technique eMotionBlur
< ui_tooltip = "Info : Estimated Motion Blur is based on Depth + Motion Flow Direction information."; >
{
		pass CopyFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = CurrentFrame;
		RenderTarget = currTex;
	}
		pass Flow
	{
		VertexShader = PostProcessVS;
		PixelShader = MotionFlow;
		RenderTarget = MF;
	}
		pass PrevFrame
	{
		VertexShader = PostProcessVS;
		PixelShader = PreviousFrame;
		RenderTarget = prevTex;
	}
			pass MotionBlur
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
		}

}
