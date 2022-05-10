 ////---------------//
 ///**Smart Sharp**///
 //---------------////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Depth Based Unsharp Mask Bilateral Contrast Adaptive Sharpening v2.4
// For Reshade 3.0+
//  ---------------------------------
//								https://web.stanford.edu/class/cs448f/lectures/2.1/Sharpening.pdf
//
// 								Bilateral Filter Made by mrharicot ported over to Reshade by BSD
//								GitHub Link for sorce info github.com/SableRaf/Filters4Processin
// 								Shadertoy Link https://www.shadertoy.com/view/4dfGDH  Thank You.
//
//                                     Everyone wants to best the bilateral filter.....
//
//
// LICENSE
// ============
// Overwatch & Code out side the work of people mention above is licenses under: Attribution-NoDerivatives 4.0 International
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
// http://reshade.me/forum/shader-presentation/2128-sidebyside-3d-depth-map-based-stereoscopic-shader
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if exists "Overwatch.fxh"                                           //Overwatch Intercepter//
	#include "Overwatch.fxh"
#else //DA_W Depth_Linearization | DB_X Depth_Flip
	static const float DA_W = 0.0, DB_X = 0;
	#define NC 0
	#define NP 0
#endif

// This is the practical limit for the algorithm's scaling ability. Example resolutions;
//  1280x720  -> 1080p = 2.25x area
//  1536x864  -> 1080p = 1.56x area
//  1792x1008 -> 1440p = 2.04x area
//  1920x1080 -> 1440p = 1.78x area
//  1920x1080 ->    4K =  4.0x area
//  2048x1152 -> 1440p = 1.56x area
//  2560x1440 ->    4K = 2.25x area
//  3072x1728 ->    4K = 1.56x area

// Determines the power of the Bilateral Filter and sharpening quality. Lower the setting the more performance you would get along with lower quality.
// 0 = Off
// 1 = Low
// 2 = Default
// 3 = Medium
// 4 = High
// Default is off.
#define M_Quality 0 //Manualy set shader Quality. Defaults to 2 when set to off.

//Zero is Fast, a ''Optimized'' Bilateral Filtering approach wink wink and One is a Acuurate. Acuurate is the correct way of doing Bilateral filtering.
#define B_Accuracy 0 //Bilateral Accuracy

//Use this to enable motion Sharpen option also reduces perf a bit more
#define Motion_Sharpen 0

#define SRGB 0

// It is best to run Smart Sharp after tonemapping.
#if !defined(__RESHADE__) || __RESHADE__ < 40000
	#define Compatibility 1
#else
	#define Compatibility 0
#endif
/*
uniform float TEST <
	ui_type = "drag";
    ui_min = 0.0; ui_max = 1.0;
> = 0.0;
*/

uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "Normal\0Reverse\0";
	ui_label = "Custom Depth Map";
	ui_tooltip = "Pick your Depth Map.";
	ui_category = "Depth Buffer";
> = DA_W;

uniform float Depth_Map_Adjust <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
	ui_min = 1.0; ui_max = 1000.0; ui_step = 0.125;
	ui_label = "Depth Map Adjustment";
	ui_tooltip = "Adjust the depth map and sharpness distance.";
	ui_category = "Depth Buffer";
> = 250.0;

uniform bool Depth_Map_Flip <
	ui_label = "Depth Map Flip";
	ui_tooltip = "Flip the depth map if it is upside down.";
	ui_category = "Depth Buffer";
> = DB_X;

uniform bool No_Depth_Map <
	ui_label = "No Depth Map";
	ui_tooltip = "If you have No Depth Buffer turn this On.";
	ui_category = "Depth Buffer";
> = false;

uniform float Sharpness <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_label = "Sharpening Strength";
    ui_min = 0.0; ui_max = 2.0;
    ui_tooltip = "Scaled by adjusting this slider from Zero to One to increase sharpness of the image.\n"
				 "Zero = No Sharpening, to One = Full Sharpening, and Past One = Extra Crispy.\n"
				 "Number 0.625 is Equal to AMD FidelityFX Contrast Adaptive Sharpening at max settings.\n"
				 "Number 0.625 is default.";
	ui_category = "Bilateral CAS";
> = 0.625;

uniform float B_Grounding <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_label = "Coarseness";
    ui_min = 0.0; ui_max = 1.0;
	ui_tooltip = "Like Coffee pick how rough do you want this shader to be, really just adjusts the radius of the filter.\n"
				 "Fine -> Medium -> Coarse too what ever you think looks good.\n"
				 "Let me have fun with names and tooltips........\n"
				 "Number 0.0 is default.";
	ui_category = "Bilateral CAS";
> = 0.0;

uniform bool CAM_IOB <
	ui_label = "CAM Ignore Overbright";
	ui_tooltip = "Instead of of allowing Overbright in the mask this allows sharpening of this area.\n"
				 "I think it's more accurate to leave this on.";
	ui_category = "Bilateral CAS";
> = false;

uniform bool CA_Mask_Boost <
	ui_label = "CAM Boost";
	ui_tooltip = "This boosts the power of Contrast Adaptive Masking part of the shader.";
	ui_category = "Bilateral CAS";
> = false;

uniform bool CA_Removal <
	ui_label = "CAM Removal";
	ui_tooltip = "This removes the extra Contrast Adaptive Masking part of the shader.\n"
				 "Keep in mind This filter already has a level of Contrast Masking.\n"
				 "This is for people who like the Raw look of Bilateral Sharpen.\n"
				 "Noise reduction from the Bilateral Filter applies automatically.";
	ui_category = "Bilateral CAS";
> = false;

#if Motion_Sharpen
uniform int Local_Motion <
	ui_type = "combo";
	ui_items = "General Motion\0Local Motion\0";
	ui_label = "View Mode";
	ui_tooltip = "This is used to select between General Motion & Local Motion.\n"
				 "Default is General Motion.";
	ui_category = "Motion Bilateral CAS";
> = 0;

uniform float GMD <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_label = "General Motion Detection";
    ui_min = 0.0; ui_max = 1.0;
    ui_tooltip = "Increase the General Motion Detection power.\n"
				 "This is used to boost Sharpening strength by the user selected ammount.\n"
				 "Number Zero is default, Off.";
	ui_category = "Motion Bilateral CAS";
> = 0.0;

uniform float MDSM <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_label = "Sharpen Multiplier";
    ui_min = 1.0; ui_max = 10.0;
    ui_tooltip = "Motion Detection Sharpen Multiplier.\n"
				 "This is the user set mutliplyer for how much you want to increase the base sharpen.\n"
				 "A Multiplier of 5 should be fine at base sharpness, Try messing around with this.\n"
				 "A Sharpen Multiplier of 2 is two times the user set Sharpening Strength.\n"
				 "Number 1 is default.";
	ui_category = "Motion Bilateral CAS";
> = 1.0;
#else
static const int Local_Motion = 0;
static const float GMD = 0.0;
static const float MDSM = 0.0;
#endif
uniform int Debug_View <
	ui_type = "combo";
	ui_items = "Normal\0Sharpen View\0Depth Masking\0CAS Mask\0Noise Mask\0";
	ui_label = "Debug View";
	ui_tooltip = "This is to DeBug Smart Sharp and lets you fine tune the shader.\n"
				 "Let me have fun with names and tooltips.......";
	ui_category = "Debug";
> = 0;

uniform int F_DeNoise <
	ui_type = "combo";
	ui_items = "Off\0Level One DeNoiser\0Level Two DeNoiser\0";
	ui_label = "Smart Denoiser Options";
	ui_tooltip = "This Denoiser can help with Noise in the image.\n"
				 "LvL One DeNoiser Works with Smart Sharp Only.\n"
				 "LvL Two DeNoiser Works with other shaders. But, it's a little stronger.\n"
				 "LvL Three DeNoiser N/A Future add on.";
	ui_category = "Extra Menu";
> = false;

uniform float Denoise_Power <
	#if Compatibility
	ui_type = "drag";
	#else
	ui_type = "slider";
	#endif
    ui_label = "Denoise Power";
    ui_min = 0.0; ui_max = 1.0;
    ui_tooltip = "Increase the Sharpening strength of the DeNoiser.\n"
				 "Number Zero is default, Off.";
	ui_category = "Extra Menu";
> = 0.5;

uniform bool ClampSharp <
	ui_label = "Clamp Sharpen";
	ui_tooltip = "This Clamps the output of the Sharpen Shader.";
	ui_category = "Extra Menu";
> = true;

#define Quality 2

#if M_Quality > 0
	#undef Quality
    #define Quality M_Quality
#endif
/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define TexSize float2(BUFFER_WIDTH, BUFFER_HEIGHT)
uniform float timer < source = "timer"; >;
//Since I don't use this for the incorrect Blur I keep it here for the correct one.
#define SIGMA 15
#define BSIGMA 0.25 //Now I need to keep this one below 0.25 or Halo will happen MC and everything.

#if Quality == 1
	#define MSIZE 3
#endif
#if Quality == 2
	#define MSIZE 5
#endif
#if Quality == 3
	#define MSIZE 7
#endif
#if Quality == 4
	#define MSIZE 9
#endif

texture DepthBufferTex : DEPTH;

sampler DepthBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
		#if SRGB
		SRGBTexture = true;
		#endif
	};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if Motion_Sharpen
texture CurrentBBSSTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler CBBSS
	{
		Texture = CurrentBBSSTex;
	};

texture PastBBSSTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler PBBSS
	{
		Texture = PastBBSSTex;
	};

texture DownSTex {Width = 256; Height = 256; Format = R8;  MipLevels = 9;};

sampler DSM
	{
		Texture = DownSTex;
	};
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float Depth(in float2 texcoord : TEXCOORD0)
{
	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	float zBuffer = tex2Dlod(DepthBuffer, float4(texcoord,0,0)).x; //Depth Buffer

	//Conversions to linear space.....
	//Near & Far Adjustment
	float Far = 1.0, Near = 0.125/Depth_Map_Adjust; //Division Depth Map Adjust - Near

	float2 Z = float2( zBuffer, 1-zBuffer );

	if (Depth_Map == 0)//DM0. Normal
		zBuffer = Far * Near / (Far + Z.x * (Near - Far));
	else if (Depth_Map == 1)//DM1. Reverse
		zBuffer = Far * Near / (Far + Z.y * (Near - Far));

	return saturate(zBuffer);
}

float3 RGBtoYCbCr(float3 rgb)
{   float C[1];//The Chronicles of Riddick: Assault on Dark Athena FIX I don't know why it works.......
	float Y  =  .299 * rgb.x + .587 * rgb.y + .114 * rgb.z; // Luminance
	float Cb = -.169 * rgb.x - .331 * rgb.y + .500 * rgb.z; // Chrominance Blue
	float Cr =  .500 * rgb.x - .419 * rgb.y - .081 * rgb.z; // Chrominance Red
	return float3(Y,Cb + 128./255.,Cr + 128./255.);
}

float3 YCbCrtoRGB(float3 ycc)
{
	float3 c = ycc - float3(0., 128./255., 128./255.);

	float R = c.x + 1.400 * c.z;
	float G = c.x - 0.343 * c.y - 0.711 * c.z;
	float B = c.x + 1.765 * c.y;
	return float3(R,G,B);
}

float Min3(float x, float y, float z)
{
    return min(x, min(y, z));
}

float Max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

float normpdf3(in float3 v, in float sigma)
{
	return 0.39894*exp(-0.5*dot(v,v)/(sigma*sigma))/sigma;
}

float3 BB(in float2 texcoord, float2 AD)
{
	return tex2Dlod(BackBuffer, float4(texcoord + AD,0,0)).rgb;
}

float LI(float3 RGB)
{
	return dot(RGB,float3(0.2126, 0.7152, 0.0722));
}

float GT() { return lerp(1.0,0.5,B_Grounding);}

float MotionSharpen(float2 texcoord)
{
#if Motion_Sharpen
	float2 PS = pix * 5.0;
	float BlurMotion = tex2D(DSM,texcoord).x;
	if(Local_Motion)
	{
		BlurMotion += tex2D(DSM,texcoord + float2(0,PS.y)).x;
		BlurMotion += tex2D(DSM,texcoord + float2(PS.x,0)).x;
		BlurMotion += tex2D(DSM,texcoord + float2(-PS.x,0)).x;
		BlurMotion += tex2D(DSM,texcoord + float2(0,-PS.y)).x;
		return (BlurMotion * 0.2) * 12.5;
	}
	else
		return tex2Dlod(DSM,float4(texcoord,0,11)).x * lerp(0.0,25.0,GMD);
#else
	return 0;
#endif
}

float4 GetBBfetch(float2 texcoord, int2 Offset )
{
	return  tex2Dfetch(BackBuffer, texcoord * TexSize + Offset );
}

void Smart_Sharp( float2 texcoord,inout float Edge, inout float3 LVB, inout float CAM)
{
	//Bilateral Filter//
	const int kSize = MSIZE * 0.5; // Default M-size is Quality 2 so [MSIZE 3] [MSIZE 5] [MSIZE 7] [MSIZE 9] / 2.
	float mnRGB, mxRGB, t, l, r, s;
	
	// Find Edges
	t = LI(GetBBfetch(texcoord , int2( 0,-2) ).rgb);
	s = LI(GetBBfetch(texcoord , int2( 0, 2) ).rgb);
	l = LI(GetBBfetch(texcoord , int2(-2, 0) ).rgb);
	r = LI(GetBBfetch(texcoord , int2( 2, 0) ).rgb);
    float2 n = float2(t - s,-(r - l));
    float nl = length(n);

	//Later I will add a 3x3 Medium Filter for LVL Three WIP

	float3 final_color, c = BB(texcoord.xy,0), cc;
	float2 RPC_WS = pix;
	float bZ = rcp(normpdf(0.0, BSIGMA)), Z, factor;
	#if B_Accuracy
	float kernal[MSIZE];
	[unroll]
	for (int o = 0; o <= kSize; ++o)
	{
		kernal[kSize+o] = kernal[kSize-o] = normpdf(o, SIGMA);
	}
	#endif
	[loop]
	for (int i=-kSize; i <= kSize; ++i)
	{
		for (int j=-kSize; j <= kSize; ++j)
		{
			cc = BB(texcoord.xy, float2(i,j) * RPC_WS * rcp(kSize * GT()) );
			#if B_Accuracy
				factor = normpdf3(cc-c, BSIGMA) * bZ * kernal[kSize+j] * kernal[kSize+i];
			#else
				factor = normpdf3(cc-c, BSIGMA);
			#endif
			Z += factor;
			final_color += factor * cc;
		}
	}

	final_color = saturate(final_color/Z);

    mnRGB = min( min( LI(c), LI(final_color)), LI(cc));
	mxRGB = max( max( LI(c), LI(final_color)), LI(cc));

   // Smooth minimum distance to signal limit divided by smooth max.
    float rcpMRGB = rcp(mxRGB), RGB_D = saturate(min(mnRGB, 1.0 - mxRGB) * rcpMRGB);

	if( CAM_IOB )
		RGB_D = saturate(min(mnRGB, 2.0 - mxRGB) * rcpMRGB);

	// Shaping amount of sharpening masked
	float CAS_Mask = RGB_D, Sharp = Sharpness, MD = MotionSharpen(texcoord);

	if(GMD > 0 || Local_Motion)
		Sharp = Sharpness * lerp( 1,MDSM,saturate(MD));

	if(CA_Mask_Boost)
		CAS_Mask = lerp(CAS_Mask,CAS_Mask * CAS_Mask,saturate(Sharp * 0.5));

	if(CA_Removal)
		CAS_Mask = 1;

	//Edge Detection
	Edge = nl;
	//Low Variance Blur
	LVB = final_color;
	//Contrast Adaptive Masking
	CAM = CAS_Mask;

}


float4 Sharpen_Out(float2 texcoord)
{
	float Noise, Edge, CAM, Sharpen_Power = Sharpness, MD = MotionSharpen(texcoord);
	float3 LVB;
	float4 color = tex2D(BackBuffer, texcoord),Done;

	Smart_Sharp( texcoord, Edge, LVB, CAM);

	if(GMD > 0 || Local_Motion)
		Sharpen_Power = Sharpness * lerp( 1,MDSM,saturate(MD));

    if( F_DeNoise )
    {   //Noise reduction for pure Bilateral Sharp
		Done = color;
    	Done /= LVB;
    	Done = min( Min3(Done.r,Done.g,Done.b) ,2-Max3(Done.r,Done.g,Done.b));
		Noise = saturate(length(Done > 0.950f));
    }

    Sharpen_Power *= 3.1;

	float3 Sharpen;// = (tex2D(BackBuffer,texcoord).rgb - LVB) * Sharpen_Power ;

//RGBtoYCbCr(
//YCbCrtoRGB(
Sharpen = RGBtoYCbCr(tex2D(BackBuffer,texcoord).rgb - LVB);

Sharpen.x *= Sharpen_Power; 

Sharpen = YCbCrtoRGB(Sharpen);

	if (ClampSharp)
		Sharpen = saturate(Sharpen);
/*
	float Grayscale_Sharpen = LI(Sharpen);

	if (Output_Selection == 0)
		Sharpen = lerp(Grayscale_Sharpen,Sharpen,0.5);
	else if (Output_Selection == 1)
		Sharpen = Sharpen;
	else
		Sharpen = Grayscale_Sharpen;
*/		
	float SNoise = Noise;
	Noise = saturate(lerp( 1, Noise  , Denoise_Power));
	Edge = saturate(Edge > 0.125);

	Sharpen = lerp( max(0,lerp( 0 , Sharpen , Noise )) , Sharpen, Edge) ;

	if( Debug_View == 1)
	{
			Sharpen = Sharpen;
	}
	else
	{
		if( F_DeNoise )
		{
		float3 B = LVB, C = F_DeNoise == 1 ? color.rgb : B, S = color.rgb + Sharpen;

			   C = RGBtoYCbCr(C); S = RGBtoYCbCr(S); B = RGBtoYCbCr(B);
				
			   C.x = lerp( C.x , S.x ,  Noise  );
			   C.yz = lerp( Debug_View == 4 ? 1 : B.yz , S.yz , SNoise );

			Sharpen.rgb = YCbCrtoRGB(lerp( C , S, Edge )) ;
		}
		else
			Sharpen.rgb = color.rgb + Sharpen;
	}

	return float4(lerp(Debug_View == 1 ? 0 : color.rgb,Sharpen, CAM * saturate(Sharpen_Power)),CAM); //Sharpen Out
}


float3 ShaderOut(float2 texcoord)
{
	float3 Out, Luma, Sharpen = Sharpen_Out(texcoord).rgb,BB = tex2D(BackBuffer,texcoord).rgb;
	float DB = Depth(texcoord).r;

	if(No_Depth_Map)
		DB = 0.0;

	if (Debug_View == 0)
		Out.rgb = lerp(Sharpen, BB, DB);
	else if (Debug_View == 1)
		Out.rgb = Sharpen_Out(texcoord).rgb;
	else if (Debug_View == 2)
		Out.rgb = lerp(float3(1., 0., 1.),tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).rgb,DB);
	else if (Debug_View == 3)
		Out.rgb = lerp(1.0,Sharpen_Out(texcoord).www,1-DB);
	else if (Debug_View == 4)
		Out.rgb = lerp(Sharpen, BB, DB);


	#if Motion_Sharpen
	if (Debug_View >= 1)
	{
		if(texcoord.y < 0.666 && texcoord.y > 0.333 && texcoord.x < 0.666 && texcoord.x > 0.333)
			Out = lerp( 0,MDSM,MotionSharpen(texcoord * 3 - 1.0));
	}
	#endif

	return Out;
}
#if Motion_Sharpen
float CBackBuffer_SS(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	return dot(tex2D(BackBuffer,texcoord),0.333);
}

float PBackBuffer_SS(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	return tex2D(CBBSS,texcoord).x;
}

float2 DownSampleMotion(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{	float Motion = abs(tex2D(CBBSS,texcoord).x - tex2D(PBBSS,texcoord).x);
	return Motion;
}
#endif
////////////////////////////////////////////////////////Logo/////////////////////////////////////////////////////////////////////////
float3 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{   //Overwatch integration
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y, Text_Timer = 12500, BT = smoothstep(0,1,sin(timer*(3.75/1000)));
	float D,E,P,T,H,Three,DD,Dot,I,N,F,O,R,EE,A,DDD,HH,EEE,L,PP,NN,PPP,C,Not,No;
	float3 Color = ShaderOut(texcoord).rgb;
	//Color = tex2Dlod(DSM,float4(texcoord,0,11)).x * lerp(0.0,25.0,GMD);
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

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

//*Rendering passes*//
technique Smart_Sharp
< ui_tooltip = "Suggestion : You Can Enable 'Performance Mode Checkbox,' in the lower bottom right of the ReShade's Main UI.\n"
			   "             Do this once you set your Smart Sharp settings of course."; >
{
		#if Motion_Sharpen // Motion Sharpen makes this shader slower.
			pass PBB //Done this way to keep Freestyle comp.
		{
			VertexShader = PostProcessVS;
			PixelShader = PBackBuffer_SS;
			RenderTarget = PastBBSSTex;
		}
			pass CBB
		{
			VertexShader = PostProcessVS;
			PixelShader = CBackBuffer_SS;
			RenderTarget = CurrentBBSSTex;
		}
			pass Down_Sample_Motion
		{
			VertexShader = PostProcessVS;
			PixelShader = DownSampleMotion;
			RenderTarget = DownSTex;
		}
		#endif
			pass UnsharpMask
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
			#if SRGB
			SRGBWriteEnable = true;
			#endif
		}
}
