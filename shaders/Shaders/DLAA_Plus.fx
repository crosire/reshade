 ////-------------//
 ///**DLAA Plus**///
 //-------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Directionally Localized Antialiasing Plus.
 //* For ReShade 3.0+
 //*  ---------------------------------
 //*                                                                           DLAA+
 //* Due Diligence
 //* Directionally Localized Anti-Aliasing (DLAA)
 //* Original method by Dmitry Andreev
 //* http://and.intercon.ru/releases/talks/dlaagdc2011/
 //*
 //* LICENSE
 //* ============
 //* Directionally Localized Anti-Aliasing Plus is licenses under: Attribution-NoDerivatives 4.0 International
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
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uniform float Short_Edge_Mask <
 	ui_type = "drag";
 	ui_min = 0.0; ui_max = 1.0;
 	ui_label = "Short Edge AA";
 	ui_tooltip = "Use this to adjust the Short Edge AA.\n"
 				 "Default is 0.25";
 	ui_category = "DLAA";
 > = 0.25;
/*
 uniform float Long_Edge_Mask <
 	ui_type = "drag";
 	ui_min = 0.0; ui_max = 1.0;
 	ui_label = "Long Edge AA";
 	ui_tooltip = "Use this to adjust the Long Edge AA.\n"
 				 "Default is 0.5";
 	ui_category = "DLAA";
 > = 0.5;
*/
uniform float Long_Edge_Mask_H <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Long Edge H+";
	ui_tooltip = "Use this to adjust the Super Long Edge AA.\n"
				 "Default is 1.0";
	ui_category = "DLAA Expanded";
> = 0.0;

uniform float Long_Edge_Mask_V <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Long Edge V+";
	ui_tooltip = "Use this to adjust the Super Long Edge AA.\n"
				 "Default is 1.0";
	ui_category = "DLAA Expanded";
> = 0.0;

uniform float De_Artifact <
 	ui_type = "drag";
 	ui_min = 0.0; ui_max = 1.0;
 	ui_label = "De-Artifacting";
 	ui_tooltip = "Use this to adjust de-artifacting power.\n"
 				 "Default is 0.5";
	ui_category = "DLAA Debuging";
 > = 0.5;

uniform float Text_Preservation <
 	ui_type = "drag";
 	ui_min = 0.0; ui_max = 1.0;
 	ui_label = "Text Preservation";
 	ui_tooltip = "Use this to adjust Text Preservation Power.\n"
 				 "Default is 0.5";
	ui_category = "DLAA Debuging";
 > = 0.5;

uniform int View_Mode <
	ui_type = "combo";
	ui_items = "DLAA\0DLAA Text Preservation\0Blue Short H/V AA Mask\0Red H & Green V Long Edge Mask\0Text Preservation Mask\0";
	ui_label = "View Mode";
	ui_tooltip = "This is used to select the normal view output or debug view.";
	ui_category = "DLAA Debuging";
> = 0;

//Total amount of frames since the game started.
uniform uint framecount < source = "framecount"; >;
////////////////////////////////////////////////////////////DLAA////////////////////////////////////////////////////////////////////
#define Alternate framecount % 2 == 0
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define lambda lerp(0,10,Short_Edge_Mask)
#define epsilon 0.0 //lerp(0,0.5,Error_Clamping)
#define DA lerp(0,16,De_Artifact)
#define Hoiz lerp(1,10,Long_Edge_Mask_H)
#define Vert lerp(1,10,Long_Edge_Mask_V)
texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

texture DLAAtex_S {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler SamplerDLAA_S
	{
		Texture = DLAAtex_S;
	};

texture SLPtex_H {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler SamplerLoadedPixel_H
	{
		Texture = SLPtex_H;
	};

  texture TexDLAA_H {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

  sampler SamplerDLAA_H
  	{
  		Texture = TexDLAA_H;
  	};

  texture SLPtex_V {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

  sampler SamplerLoadedPixel_V
  	{
  		Texture = SLPtex_V;
  	};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Luminosity Intensity
float LI(in float3 value)
{
	//Luminosity Controll from 0.1 to 1.0
	//If GGG value of 0.333, 0.333, 0.333 is about right for Green channel.
	//Slide 51 talk more about this.
	return dot(value.rgb,float3(0.299, 0.587, 0.114));
}
//WIP
float Text_Detection(float2 texcoord)
{
	float4 BC = tex2D(BackBuffer, texcoord);
	//BC += tex2D(BackBuffer, texcoord + float2( TEST, TEST) * pix.xy);
	//BC += tex2D(BackBuffer, texcoord + float2( TEST,-TEST) * pix.xy);
	//BC += tex2D(BackBuffer, texcoord + float2(-TEST, TEST) * pix.xy);
	//BC += tex2D(BackBuffer, texcoord + float2(-TEST, -TEST) * pix.xy);
	//BC /= 4;
	// Luma Threshold Thank you Adyss
	BC.a    = LI(BC.rgb);//Luma
	BC.rgb /= max(BC.a, 0.001);
	BC.a    = max(0.0, BC.a - Text_Preservation);
	BC.rgb *= BC.a;

    return 1-dot(BC.rgb,BC.rgb);
}

float4 SLP(float2 tc,float dx, float dy) //Load Pixel
{
	float4 BB = tex2D(BackBuffer, tc + float2(dx, dy) * pix.xy);
	return BB;
}

float4 DLAA_Short( float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float t, l, r, d, n, MA = 1;

	float2 UV = texcoord.xy, SW =  MA; // But, I don't think it's really needed.
	float4 NFAA; // The Edge Seeking code can be adjusted to look for longer edges.
	// Find Edges
	t = LI(SLP(texcoord, 0 ,-1).rgb);
	d = LI(SLP(texcoord, 0 , 1).rgb);
	l = LI(SLP(texcoord,-1 , 0).rgb);
	r = LI(SLP(texcoord, 1 , 0).rgb);
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

	return float4(DLAA.rgb,(satAmountV + satAmountH) * 0.5);
}

float4 LP_H(float2 tc,float dx, float dy) //Load Pixel
{
	float4 BB = tex2D(BackBuffer, tc + float2(dx, dy) * pix.xy);
	return BB;
}

float4 PreFilter_H(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target //Loaded Pixel
{

    float4 center = LP_H(texcoord, 0,   0);
    float4 left   = LP_H(texcoord,-1.0, 0);
    float4 right  = LP_H(texcoord, 1.0, 0);
    float4 top    = LP_H(texcoord, 0,-1.0);
    float4 bottom = LP_H(texcoord, 0, 1.0);

    float4 edges = 4.0 * abs((left + right + top + bottom) - 4.0 * center);
    float  edgesLum = LI(edges.rgb);

    return float4(center.rgb, edgesLum);
}

float4 SLP_H(float2 tc,float dx, float dy) //Load Pixel
{
	float4 BB = tex2D(SamplerLoadedPixel_H, tc + float2(dx, dy) * pix.xy);
	return BB;
}

//Information on Slide 44 says to run the edge processing jointly short and Large.
float4 DLAA_H(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target //Loaded Pixel
{
	//Short Edge Filter http://and.intercon.ru/releases/talks/dlaagdc2011/slides/#slide43
	float4 DLAA, H, V, UDLR; //DLAA is the completed AA Result.
	float SLE_H = 2 * Hoiz;
	//Center sample
	float4 Center = SLP_H(texcoord, 0 , 0);

	DLAA = tex2D(SamplerDLAA_S,texcoord);
	float4  HNeg, HNegA, HNegB, HNegC, HNegD, HNegE,
			HPos, HPosA, HPosB, HPosC, HPosD, HPosE,
			VNeg, VNegA, VNegB, VNegC,
			VPos, VPosA, VPosB, VPosC;

	// Long Edges
	//16 bi-linear samples cross, added extra bi-linear samples in each direction.
    HNeg    = SLP_H( texcoord, -1.5 , 0.0 );
    HNegA   = SLP_H( texcoord, -3.5 , 0.0 );
    HNegB   = SLP_H( texcoord, -5.5 , 0.0 );
    HNegC   = SLP_H( texcoord, -7.0 , 0.0 );

    HPos    = SLP_H( texcoord,  1.5 , 0.0 );
    HPosA   = SLP_H( texcoord,  3.5 , 0.0 );
    HPosB   = SLP_H( texcoord,  5.5 , 0.0 );
    HPosC   = SLP_H( texcoord,  7.0 , 0.0 );

    VNeg    = SLP_H( texcoord,  0.0,-1.5  );
    VNegA   = SLP_H( texcoord,  0.0,-3.5  );
    VNegB   = SLP_H( texcoord,  0.0,-5.5  );
    VNegC   = SLP_H( texcoord,  0.0,-7.0  );

    VPos    = SLP_H( texcoord,  0.0, 1.5  );
    VPosA   = SLP_H( texcoord,  0.0, 3.5  );
    VPosB   = SLP_H( texcoord,  0.0, 5.5  );
    VPosC   = SLP_H( texcoord,  0.0, 7.0  );

	//Long Edge detection H & V
	float4 AvgBlurH = ( HNeg + HNegA + HNegB + HNegC + HPos + HPosA + HPosB + HPosC ) / 8;
	float4 AvgBlurV = ( VNeg + VNegA + VNegB + VNegC + VPos + VPosA + VPosB + VPosC ) / 8;
	float EAH = saturate( AvgBlurH.a * SLE_H - 1.0 );
	float EAV = saturate( AvgBlurV.a * 2.0 - 1.0 );

	float longEdge_H = abs( EAH - EAV );

	float Mask_H = longEdge_H > 0;

    if ( Mask_H )
    {
		float4 up    = LP_H(texcoord, 0 ,-1);
		float4 down  = LP_H(texcoord, 0 , 1);
		//Merge for BlurSamples.
		//Long Blur H
		float LongBlurLumH = LI( AvgBlurH.rgb);//8 samples

		float centerLI = LI( Center.rgb);
		float upLI     = LI( up.rgb    );
		float downLI   = LI( down.rgb  );

		float blurUp = saturate( 0.0 + ( LongBlurLumH - upLI      ) / (centerLI - upLI  ) );
		float blurDown = saturate( 1.0 + ( LongBlurLumH - centerLI) / (centerLI - downLI) );

		UDLR = float4( 1, 1, blurUp, blurDown );

		UDLR = UDLR == float4(0.0, 0.0, 0.0, 0.0) ? float4(1.0, 1.0, 1.0, 1.0) : UDLR;

		H = lerp( up   , Center, UDLR.z );
		H = lerp( down , H	 , UDLR.w );

		DLAA = lerp( DLAA , H , EAH);
    }

	return float4(DLAA.rgb,EAH);
}

float4 LP_V(float2 tc,float dx, float dy) //Load Pixel
{
	float4 BB = tex2D(SamplerDLAA_H, tc + float2(dx, dy) * pix.xy);
	return BB;
}

float4 PreFilter_V(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target //Loaded Pixel
{

    float4 center = LP_V(texcoord,  0,    0);
    float4 left   = LP_V(texcoord, -1.0,  0);
    float4 right  = LP_V(texcoord,  1.0,  0);
    float4 top    = LP_V(texcoord,  0, -1.0);
    float4 bottom = LP_V(texcoord,  0,  1.0);

    float4 edges = 4.0 * abs((left + right + top + bottom) - 4.0 * center);
    float  edgesLum = LI(edges.rgb);

    return float4(center.rgb, edgesLum);
}

float4 SLP_V(float2 tc,float dx, float dy) //Load Pixel
{
	float4 BB = tex2D(SamplerLoadedPixel_V, tc + float2(dx, dy) * pix.xy);
	return BB;
}

float4 DLAA_V(float2 texcoord)
{
	//Short Edge Filter http://and.intercon.ru/releases/talks/dlaagdc2011/slides/#slide43
	float4 DLAA, H, V, UDLR; //DLAA is the completed AA Result.
	float SLE_V = 2 * Vert;
	//Center sample
	float4 Center = SLP_V(texcoord, 0 , 0);

    //Reuse Long Horizontal AA
    DLAA = Center;
	float4  HNeg, HNegA, HNegB, HNegC, HNegD, HNegE,
			HPos, HPosA, HPosB, HPosC, HPosD, HPosE,
			VNeg, VNegA, VNegB, VNegC,
			VPos, VPosA, VPosB, VPosC;

	// Long Edges
    //16 bi-linear samples cross, added extra bi-linear samples in each direction.
    HNeg    = SLP_V( texcoord, -1.5 , 0.0 );
    HNegA   = SLP_V( texcoord, -3.5 , 0.0 );
    HNegB   = SLP_V( texcoord, -5.5 , 0.0 );
    HNegC   = SLP_V( texcoord, -7.0 , 0.0 );

    HPos    = SLP_V( texcoord,  1.5 , 0.0 );
    HPosA   = SLP_V( texcoord,  3.5 , 0.0 );
    HPosB   = SLP_V( texcoord,  5.5 , 0.0 );
    HPosC   = SLP_V( texcoord,  7.0 , 0.0 );

    VNeg    = SLP_V( texcoord,  0.0,-1.5  );
    VNegA   = SLP_V( texcoord,  0.0,-3.5  );
    VNegB   = SLP_V( texcoord,  0.0,-5.5  );
    VNegC   = SLP_V( texcoord,  0.0,-7.0  );

    VPos    = SLP_V( texcoord,  0.0, 1.5  );
    VPosA   = SLP_V( texcoord,  0.0, 3.5  );
    VPosB   = SLP_V( texcoord,  0.0, 5.5  );
    VPosC   = SLP_V( texcoord,  0.0, 7.0  );

    //Long Edge detection H & V
    float4 AvgBlurH = ( HNeg + HNegA + HNegB + HNegC + HPos + HPosA + HPosB + HPosC ) / 8;
    float4 AvgBlurV = ( VNeg + VNegA + VNegB + VNegC + VPos + VPosA + VPosB + VPosC ) / 8;
    float EAH = saturate( AvgBlurH.a * 2.0 - 1.0 );
    float EAV = saturate( AvgBlurV.a * SLE_V - 1.0 );

    float longEdge_V = abs( EAV - EAH );

    float Mask_V = longEdge_V > 0;

    if ( Mask_V )
    {
      float4 left  = LP_V(texcoord,-1 , 0);
      float4 right = LP_V(texcoord, 1 , 0);
      //Merge for BlurSamples.

      //Long Blur V
      float LongBlurLumV = LI( AvgBlurV.rgb );//8 samples

      float centerLI = LI( Center.rgb);
      float leftLI   = LI( left.rgb  );
      float rightLI  = LI( right.rgb );

      float blurLeft  = saturate( 0.0 + ( LongBlurLumV - leftLI  ) / (centerLI - leftLI ) );
      float blurRight = saturate( 1.0 + ( LongBlurLumV - centerLI) / (centerLI - rightLI) );

      UDLR = float4( blurLeft, blurRight, 1, 1 );

      UDLR = UDLR == float4(0.0, 0.0, 0.0, 0.0) ? float4(1.0, 1.0, 1.0, 1.0) : UDLR;

      V = lerp( left , Center, UDLR.x );
      V = lerp( right, V	 , UDLR.y );

      //Reuse short samples and DLAA Long Edge Out.
      DLAA = lerp( DLAA , V , EAV);
    }

    return float4(DLAA.rgb,EAV);
}


float4 DLAA(float2 texcoord)
{
	float t, l, r, d;

	float2 UV = texcoord.xy, SW = pix, n; // But, I don't think it's really needed.
	float4 D_A;
	// Find Edges
	t = LI(DLAA_V( float2( UV.x , UV.y - SW.y ) ).rgb);
	d = LI(DLAA_V( float2( UV.x , UV.y + SW.y ) ).rgb);
	l = LI(DLAA_V( float2( UV.x - SW.x , UV.y ) ).rgb);
	r = LI(DLAA_V( float2( UV.x + SW.x , UV.y ) ).rgb);
	n = float2(t - d,-(r - l));

	float nl = length(n), Rep = rcp(DA);

	if (nl < Rep)
		D_A = DLAA_V(UV);
	else
	{
		n *= pix / nl;

		float4   o = DLAA_V( UV ),
				t0 = DLAA_V( UV + float2(n.x, -n.y)  * 0.5) * 0.9,
				t1 = DLAA_V( UV - float2(n.x, -n.y)  * 0.5) * 0.9,
				t2 = DLAA_V( UV + n * 0.9) * 0.75,
				t3 = DLAA_V( UV - n * 0.9) * 0.75;

		D_A = (o + t0 + t1 + t2 + t3) / 4.3;
	}

		  //NFAA = tex2D(SamplerDLAA_S,UV);

	float4 DLAA = D_A;

	if(View_Mode == 1)
	{
		DLAA =  lerp(tex2D(BackBuffer, texcoord),DLAA,Text_Detection(texcoord));
	}
	else if (View_Mode == 2)
	{
		DLAA =  lerp(DLAA,1,float4(0,0,tex2D(SamplerDLAA_S,UV).w,1));
	}
	else if (View_Mode == 3)
	{
		DLAA =  lerp(DLAA,1,float4(tex2D(SamplerDLAA_H,texcoord).w,DLAA_V(texcoord).w,0,1));
	}
		else if (View_Mode == 4)
	{
		DLAA =  lerp(float4(1,1,0,1),DLAA,Text_Detection(texcoord));
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

///////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

//*Rendering passes*//
technique Directionally_Localized_Anti_Aliasing_Plus
{
	pass Short_Edge_AA
	{
		VertexShader = PostProcessVS;
		PixelShader = DLAA_Short;
		RenderTarget = DLAAtex_S;
	}
	pass Pre_Filter_Hoizontal
	{
		VertexShader = PostProcessVS;
		PixelShader = PreFilter_H;
		RenderTarget = SLPtex_H;
	}
	pass Pre_Filter_Vertical
	{
		VertexShader = PostProcessVS;
		PixelShader = DLAA_H;
		RenderTarget = TexDLAA_H;
	}
	pass Pre_Filter_Vertical
	{
		VertexShader = PostProcessVS;
		PixelShader = PreFilter_V;
		RenderTarget = SLPtex_V;
	}
	pass DLAA
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
	}
}
