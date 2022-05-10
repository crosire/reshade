 ////-------------//
 ///**NFAA Fast**///
 //-------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Normal Filter Anti Aliasing.
 //* For ReShade 3.0+ & Freestyle
 //*  ---------------------------------
 //*                                                                          NFAA
 //* Due Diligence
 //* Based on port by b34r
 //* https://www.gamedev.net/forums/topic/580517-nfaa---a-post-process-anti-aliasing-filter-results-implementation-details/?page=2
 //* If I missed any please tell me.
 //*
 //* LICENSE
 //* ============
 //* Normal Filter Anti Aliasing is licenses under: Attribution-NoDerivatives 4.0 International
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
uniform int AA_Adjust <
	ui_type = "drag";
	ui_min = 1; ui_max = 128;
	ui_label = "AA Power";
	ui_tooltip = "Use this to adjust the AA power.\n"
				 "Default is 16";
	ui_category = "NFAA";
> = 16;

uniform float Mask_Adjust <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 4.0;
	ui_label = "Mask Adjustment";
	ui_tooltip = "Use this to adjust the Mask.\n"
				 "Default is 1.00";
	ui_category = "NFAA";
> = 1.00;

uniform int View_Mode <
	ui_type = "combo";
	ui_items = "NFAA\0Mask View\0Normals\0DLSS\0";
	ui_label = "View Mode";
	ui_tooltip = "This is used to select the normal view output or debug view.\n"
				 "NFAA Masked gives you a sharper image with applyed Normals AA.\n"
				 "Masked View gives you a view of the edge detection.\n"
				 "Normals gives you an view of the normals created.\n"
				 "DLSS is NV_AI_DLSS Parody experiance..........\n"
				 "Default is NFAA.";
	ui_category = "NFAA";
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
uniform float timer < source = "timer"; >;
////////////////////////////////////////////////////////////NFAA////////////////////////////////////////////////////////////////////
#define Alternate framecount % 2 == 0
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

void A0(float2 texcoord,float PosX,float PosY,inout float D, inout float E, inout float P, inout float T, inout float H, inout float III, inout float DD )
{
	float PosXD = -0.035+PosX, offsetD = 0.001;D = all( abs(float2( texcoord.x -PosXD, texcoord.y-PosY)) < float2(0.0025,0.009));D -= all( abs(float2( texcoord.x -PosXD-offsetD, texcoord.y-PosY)) < float2(0.0025,0.007));
	float PosXE = -0.028+PosX, offsetE = 0.0005;E = all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.009));E -= all( abs(float2( texcoord.x -PosXE-offsetE, texcoord.y-PosY)) < float2(0.0025,0.007));E += all( abs(float2( texcoord.x -PosXE, texcoord.y-PosY)) < float2(0.003,0.001));
	float PosXP = -0.0215+PosX, PosYP = -0.0025+PosY, offsetP = 0.001, offsetP1 = 0.002;P = all( abs(float2( texcoord.x -PosXP, texcoord.y-PosYP)) < float2(0.0025,0.009*0.775));P -= all( abs(float2( texcoord.x -PosXP-offsetP, texcoord.y-PosYP)) < float2(0.0025,0.007*0.680));float C[1];P += all( abs(float2( texcoord.x -PosXP+offsetP1, texcoord.y-PosY)) < float2(0.0005,0.009));	
	float PosXT = -0.014+PosX, PosYT = -0.008+PosY;T = all( abs(float2( texcoord.x -PosXT, texcoord.y-PosYT)) < float2(0.003,0.001));T += all( abs(float2( texcoord.x -PosXT, texcoord.y-PosY)) < float2(0.000625,0.009));
	float PosXH = -0.0072+PosX;H = all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.001));H -= all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.002,0.009));H += all( abs(float2( texcoord.x -PosXH, texcoord.y-PosY)) < float2(0.00325,0.009));
	float offsetFive = 0.001, PosX3 = -0.001+PosX;III = all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.009));III -= all( abs(float2( texcoord.x -PosX3 - offsetFive, texcoord.y-PosY)) < float2(0.003,0.007));III += all( abs(float2( texcoord.x -PosX3, texcoord.y-PosY)) < float2(0.002,0.001));
	float PosXDD = 0.006+PosX, offsetDD = 0.001;DD = all( abs(float2( texcoord.x -PosXDD, texcoord.y-PosY)) < float2(0.0025,0.009));DD -= all( abs(float2( texcoord.x -PosXDD-offsetDD, texcoord.y-PosY)) < float2(0.0025,0.007));
}
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

float4 NFAA(float2 texcoord)
{	float t, l, r, s, MA = Mask_Adjust;
  if(View_Mode == 3 )
    MA = 5.0;
	float2 UV = texcoord.xy, SW = pix * MA, n; // But, I don't think it's really needed.
	float4 NFAA; // The Edge Seeking code can be adjusted to look for longer edges.
	// Find Edges
	t = LI(GetBB( float2( UV.x , UV.y - SW.y ) ).rgb);
	s = LI(GetBB( float2( UV.x , UV.y + SW.y ) ).rgb);
	l = LI(GetBB( float2( UV.x - SW.x , UV.y ) ).rgb);
	r = LI(GetBB( float2( UV.x + SW.x , UV.y ) ).rgb);
    n = float2(t - s,-(r - l));
	// I should have made rep adjustable. But, I didn't see the need.
	// Since my goal was to make this AA fast cheap and simple.
  float nl = length(n), Rep = rcp(AA_Adjust);
	if(View_Mode == 3 || View_Mode == 4)
		Rep = rcp(128);
	// Seek aliasing and apply AA. Think of this as basically blur control.
    if (nl < Rep)
    {
		  NFAA = GetBB(UV);
    }
    else
    {
		  n *= pix / (nl * (View_Mode == 3 ? 0.5 : 1.0));

	float4   o = GetBB( UV ),
			t0 = GetBB( UV + float2(n.x, -n.y)  * 0.5) * 0.9,
			t1 = GetBB( UV - float2(n.x, -n.y)  * 0.5) * 0.9,
			t2 = GetBB( UV + n * 0.9) * 0.75,
			t3 = GetBB( UV - n * 0.9) * 0.75;

		NFAA = (o + t0 + t1 + t2 + t3) / 4.3;
	}
	// Lets make that mask for a sharper image.
	float Mask = nl * 5.0;
	if (Mask > 0.025)
		Mask = 1-Mask;
	else
		Mask = 1;
	// Super Evil Magic Number.
	Mask = saturate(lerp(Mask,1,-1));

	// Final color
	if(View_Mode == 0)
	{
		NFAA = lerp(NFAA,GetBB( texcoord.xy), Mask );
	}
	else if(View_Mode == 1)
	{
		NFAA = Mask;
	}
	else if (View_Mode == 2)
	{
		NFAA = float3(-float2(-(r - l),-(t - s)) * 0.5 + 0.5,1);
	}

return NFAA;
}

void A1(float2 texcoord,float PosX,float PosY,inout float I, inout float N, inout float F, inout float O)
{
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;I = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;N = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));N -= all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;F = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));F -= all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));F += all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;O = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));O -= all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
}

float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{   float3 Color = NFAA(texcoord).rgb;
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y,A,B,C,D,E,F,G,H,I,J,K,L,PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;L = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));A0(texcoord,PosX,PosY,A,B,C,D,E,F,G );A1(texcoord,PosX,PosY,H,I,J,K);
	return timer <= 12500 ? A+B+C+D+E+F+G+H+I+J+K+L ? 1-texcoord.y*50.0+48.35f : float4(Color,1.) : float4(Color,1.);
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
technique Normal_Filter_Anti_Aliasing
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
