 ////--------------//
 ///**DLAA Light**///
 //--------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Directionally Localized Antialiasing Light.
 //* For ReShade 3.0+ & Freestyle
 //*  ---------------------------------
 //*                                                                          DLAA Light
 //* Due Diligence
 //* Based on port by b34r
 //* https://www.gamedev.net/forums/topic/580517-nfaa---a-post-process-anti-aliasing-filter-results-implementation-details/?page=2
 //* If I missed any please tell me.
 //*
 //* LICENSE
 //* ============
 //* Directionally Localized Antialiasing Light is licenses under: Attribution-NoDerivatives 4.0 International
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
 //*
 //* Have fun,
 //* Jose Negrete AKA BlueSkyDefender
 //*
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

 uniform float Short_Edge_Mask <
  	ui_type = "drag";
  	ui_min = 0.0; ui_max = 1.0;
  	ui_label = "Short Edge AA";
  	ui_tooltip = "Use this to adjust the Long Edge AA.\n"
  				 "Default is 0.25";
  	ui_category = "DLAA";
  > = 0.25;

uniform float Mask_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Mask Adjustment";
	ui_tooltip = "Use this to adjust the Mask.\n"
				 "Default is 1.00";
	ui_category = "DLAA";
> = 1.00;

uniform int View_Mode <
	ui_type = "combo";
	ui_items = "DLAA\0Mask View\0";
	ui_label = "View Mode";
	ui_tooltip = "This is used to select the normal view output or debug view.\n"
				 "DLAA Masked gives you a sharper image with applyed Normals AA.\n"
				 "Masked View gives you a view of the edge detection.\n"
				 "Default is DLAA.";
	ui_category = "DLAA";
> = 0;

uniform bool HFR_AA <
	ui_label = "HFR AA";
	ui_tooltip = "This allows most monitors to assist in AA if your FPS is 60 or above and Locked to your monitors refresh-rate.";
	ui_category = "HFRAA";
> = false;

uniform float HFR_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "HFR AA Adjustment";
	ui_tooltip = "Use this to adjust HFR AA.\n"
				 "Default is 1.00";
	ui_category = "HFRAA";
> = 0.5;

//Total amount of frames since the game started.
uniform uint framecount < source = "framecount"; >;
////////////////////////////////////////////////////////////NFAA////////////////////////////////////////////////////////////////////
#define Alternate framecount % 2 == 0
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define lambda lerp(0,10,Short_Edge_Mask)
#define epsilon 0.0

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SD video
float LI(in float3 value)
{
	return dot(value.rgb,float3(0.299, 0.587, 0.114));
}

float4 GetBB(float2 texcoord : TEXCOORD)
{
	return tex2D(BackBuffer, texcoord);
}

float4 SLP(float2 tc,float dx, float dy) //Load Pixel
{ float4 BB = tex2D(BackBuffer,  tc + float2(dx, dy) * pix.xy);
	return BB;
}

float4 DLAA(float2 texcoord)
{	float t, l, r, d, n, MA = Mask_Adjust;
  if(View_Mode == 3)
    MA = 5;
	float2 UV = texcoord.xy, SW = pix * MA; // But, I don't think it's really needed.
	float4 NFAA; // The Edge Seeking code can be adjusted to look for longer edges.
	// Find Edges
	t = LI(GetBB( float2( UV.x , UV.y - SW.y ) ).rgb);
	d = LI(GetBB( float2( UV.x , UV.y + SW.y ) ).rgb);
	l = LI(GetBB( float2( UV.x - SW.x , UV.y ) ).rgb);
	r = LI(GetBB( float2( UV.x + SW.x , UV.y ) ).rgb);
	n = length(float2(t - d,-(r - l)));

  	//Short Edge Filter http://and.intercon.ru/releases/talks/dlaagdc2011/slides/#slide43
  	float4 DLAA; //DLAA is the completed AA Result.

  	//5 bi-linear samples cross
  	float4 Center = SLP(texcoord, 0 , 0);
  	float4 Left   = SLP(texcoord,-1.25 , 0.0);
  	float4 Right  = SLP(texcoord, 1.25 , 0.0);
  	float4 Up     = SLP(texcoord, 0.0 ,-1.25);
  	float4 Down   = SLP(texcoord, 0.0 , 1.25);

  	//Combine horizontal and vertical blurs together
  	float4 combH	   = 2.0 * ( Up + Down );
  	float4 combV	   = 2.0 * ( Left + Right );

  	//Bi-directional anti-aliasing using HORIZONTAL & VERTICAL blur and horizontal edge detection
  	//Slide information triped me up here. Read slide 43.
  	//Edge detection
  	float4 CenterDiffH = abs( combH - 4.0 * Center ) / 4.0;
  	float4 CenterDiffV = abs( combV - 4.0 * Center ) / 4.0;

  	//Blur
  	float4 blurredH    = (combH + 2.0 * Center) / 6.0;
  	float4 blurredV    = (combV + 2.0 * Center) / 6.0;

  	//Edge detection
  	float LumH         = LI( CenterDiffH.rgb );
  	float LumV         = LI( CenterDiffV.rgb );

  	float LumHB        = LI(blurredH.xyz);
      float LumVB        = LI(blurredV.xyz);

  	//t
  	float satAmountH   = saturate( ( lambda * LumH - epsilon ) / LumVB );
  	float satAmountV   = saturate( ( lambda * LumV - epsilon ) / LumHB );

  	//color = lerp(color,blur,sat(Edge/blur)
  	//Re-blend Short Edge Done
  	DLAA = lerp( Center, blurredH, saturate(lerp(satAmountV ,1,-1)) );//* 1.1
  	DLAA = lerp( DLAA,   blurredV, saturate(lerp(satAmountH ,1,-1)) );// * 0.5

	// Lets make that mask for a sharper image.
	float Mask = n * 5.0;
	if (Mask > 0)
		Mask = 1-Mask;
	else
		Mask = 1;
	// Super Evil Magic Number.
	Mask = saturate(lerp(Mask,1,-1));

	Mask += 1-saturate(lerp(satAmountV + satAmountH,1,-1));

	// Final color
	if(View_Mode == 0)
	{
		DLAA = lerp(DLAA,GetBB( texcoord.xy), Mask * 0.5 );
	}
	else if(View_Mode == 1)
	{
		DLAA = Mask  * 0.5 ;
	}

return DLAA;
}

uniform float timer < source = "timer"; >; //Please do not remove.
////////////////////////////////////////////////////////Logo/////////////////////////////////////////////////////////////////////////
float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y;
	float3 Color = DLAA(texcoord).rgb,D,E,P,T,H,Three,DD,Dot,I,N,F,O;

	[branch] if(timer <= 12500)
	{
		//DEPTH
		//D
		float PosXD = -0.035+PosX, offsetD = 0.001;
		float3 OneD = all( abs(float2( texcoord.x -PosXD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float3 TwoD = all( abs(float2( texcoord.x -PosXD-offsetD, texcoord.y-PosY)) < float2(0.0025,0.007));
		D = OneD-TwoD;

		//E
		float PosXE = -0.028+PosX, offsetE = 0.0005;
		float3 OneE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.009));
		float3 TwoE = all( abs(float2( texcoord.x -PosXE-offsetE, texcoord.y-PosY)) < float2(0.0025,0.007));
		float3 ThreeE = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.001));
		E = (OneE-TwoE)+ThreeE;

		//P
		float PosXP = -0.0215+PosX, PosYP = -0.0025+PosY, offsetP = 0.001, offsetP1 = 0.002;
		float3 OneP = all( abs(float2( texcoord.x -PosXP, texcoord.y-PosYP)) < float2(0.0025,0.009*0.775));
		float3 TwoP = all( abs(float2( texcoord.x -PosXP-offsetP, texcoord.y-PosYP)) < float2(0.0025,0.007*0.680));
		float3 ThreeP = all( abs(float2( texcoord.x -PosXP+offsetP1, texcoord.y-PosY)) < float2(0.0005,0.009));
		P = (OneP-TwoP) + ThreeP;

		//T
		float PosXT = -0.014+PosX, PosYT = -0.008+PosY;
		float3 OneT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosYT)) < float2(0.003,0.001));
		float3 TwoT = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosY)) < float2(0.000625,0.009));
		T = OneT+TwoT;

		//H
		float PosXH = -0.0072+PosX;
		float3 OneH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.001));
		float3 TwoH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.009));
		float3 ThreeH = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.00325,0.009));
		H = (OneH-TwoH)+ThreeH;

		//Three
		float offsetFive = 0.001, PosX3 = -0.001+PosX;
		float3 OneThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.009));
		float3 TwoThree = all( abs(float2( texcoord.x -PosX3 - offsetFive, texcoord.y-PosY)) < float2(0.003,0.007));
		float3 ThreeThree = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.001));
		Three = (OneThree-TwoThree)+ThreeThree;

		//DD
		float PosXDD = 0.006+PosX, offsetDD = 0.001;
		float3 OneDD = all( abs(float2( texcoord.x -PosXDD, texcoord.y-PosY)) < float2(0.0025,0.009));
		float3 TwoDD = all( abs(float2( texcoord.x -PosXDD-offsetDD, texcoord.y-PosY)) < float2(0.0025,0.007));
		DD = OneDD-TwoDD;

		//Dot
		float PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;
		float3 OneDot = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));
		Dot = OneDot;

		//INFO
		//I
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;
		float3 OneI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));
		float3 TwoI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));
		float3 ThreeI = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		I = OneI+TwoI+ThreeI;

		//N
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;
		float3 OneN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));
		float3 TwoN = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		N = OneN-TwoN;

		//F
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;
		float3 OneF = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));
		float3 TwoF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));
		float3 ThreeF = all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		F = (OneF-TwoF)+ThreeF;

		//O
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;
		float3 OneO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));
		float3 TwoO = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
		O = OneO-TwoO;
		//Website
		return float4(D+E+P+T+H+Three+DD+Dot+I+N+F+O,1.) ? 1-texcoord.y*50.0+48.35f : float4(Color,1.);
	}
	else
		return float4(Color,1.);
}

float4 PostFilter(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{ float Shift;
  if(Alternate && HFR_AA)
    Shift = pix.x;

    return tex2D(BackBuffer, texcoord +  float2(Shift * saturate(HFR_Adjust),0.0));
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
technique DLAA_Light
{
			pass NFAA_Fast
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
		}
			pass HFR_AA
		{
			VertexShader = PostProcessVS;
			PixelShader = PostFilter;
		}

}
