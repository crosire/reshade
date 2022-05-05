 ////------------//
 ///**3DToElse**///
 //------------////
 
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Stereo Input Converter 1.0                             																														*//
 //* For Reshade 3.0																																								*//
 //* --------------------------																																						*//
 //* This work is licensed under a Creative Commons Attribution 3.0 Unported License.																								*//
 //* So you are free to share, modify and adapt it for your needs, and even use it for commercial use.																				*//
 //* I would also love to hear about a project you are using it with.																												*//
 //* https://creativecommons.org/licenses/by/3.0/us/																																*//
 //*																																												*//
 //* Have fun,																																										*//
 //* Jose Negrete AKA BlueSkyDefender																																				*//
 //*																																												*//
 //* http://reshade.me/forum/shader-presentation/2128-sidebyside-3d-depth-map-based-stereoscopic-shader																				*//	
 //* ---------------------------------																																				*//
 //*																																												*//
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uniform int Stereoscopic_Mode_Input <
	ui_type = "combo";
	ui_items = "Side by Side\0Top and Bottom\0Line Interlaced\0Checkerboard 3D\0Anaglyph GS *WIP*\0Anaglyph Color *WIP*\0Frame Sequential\0";
	ui_label = "Stereoscopic Mode Input";
	ui_tooltip = "Change to the proper stereoscopic input.";
	ui_category = "Stereoscopic Conversion";
> = 0;

uniform int Stereoscopic_Mode <
	ui_type = "combo";
	ui_items = "Side by Side\0Top and Bottom\0Line Interlaced\0Column Interlaced\0Checkerboard 3D\0Anaglyph\0";
	ui_label = "3D Display Mode";
	ui_tooltip = "Stereoscopic 3D display output selection.";
	ui_category = "Stereoscopic Conversion";
> = 0;

uniform int Perspective <
	ui_type = "drag";
	ui_min = -350; ui_max = 350;
	ui_label = "Perspective Slider";
	ui_tooltip = "Determines the perspective point.";
	ui_category = "Stereoscopic Options";
> = 0;

uniform int Scaling_Support <
	ui_type = "combo";
	ui_items = " 2160p\0 Native\0 1080p A\0 1080p B\0 1050p A\0 1050p B\0 720p A\0 720p B\0";
	ui_label = "Scaling Support";
	ui_tooltip = "Dynamic Super Resolution , Virtual Super Resolution, downscaling, or Upscaling support for Line Interlaced, Column Interlaced, & Checkerboard 3D displays.";
	ui_category = "Stereoscopic Options";
> = 1;

uniform float Interlace_Optimization <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.5;
	ui_label = " Interlace Optimization";
	ui_tooltip = "Interlace Optimization Is used to reduce alisesing in a Line or Column interlaced image.\n"
	             "This has the side effect of softening the image.\n"
	             "Default is 0.375";
	ui_category = "Stereoscopic Options";
> = 0.375;

uniform int Anaglyph_Colors <
	ui_type = "combo";
	ui_items = "Red/Cyan\0Dubois Red/Cyan\0Green/Magenta\0Dubois Green/Magenta\0";
	ui_label = "Anaglyph Color Mode";
	ui_tooltip = "Select colors for your 3D anaglyph glasses.";
	ui_category = "Stereoscopic Options";
> = 0;

uniform float Anaglyph_Desaturation <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Anaglyph Desaturation";
	ui_tooltip = "Adjust anaglyph desaturation, Zero is Black & White, One is full color.";
	ui_category = "Stereoscopic Options";
> = 1.0;

uniform bool Eye_Swap <
	ui_label = "Eye Swap";
	ui_tooltip = "Left right image change.";
	ui_category = "Stereoscopic Options";
> = false;

/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define TextureSize float2(BUFFER_WIDTH, BUFFER_HEIGHT)

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
	
texture texCL  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; 
texture texCR  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; 

sampler SamplerCL
	{
		Texture = texCL;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
	};

sampler SamplerCR
	{
		Texture = texCR;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
	};
	
texture texBB  { Width = 128; Height = 128; Format = RGBA8; MipLevels = 8;}; 

sampler SamplerBB
	{
		Texture = texBB;
	};
	
texture CurrentBackBuffer  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; 

sampler CBackBuffer
	{
		Texture = CurrentBackBuffer;
	};

texture PastSingleBackBuffer  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; 

sampler PSBackBuffer
	{
		Texture = PastSingleBackBuffer;
	};
  
////////////////////////////////////////////////Left/Right Eye////////////////////////////////////////////////////////
uniform uint framecount < source = "framecount"; >;
//Total amount of frames since the game started.

float fmod(float a, float b) 
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

void PS_InputBB(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float4 color : SV_Target)
{
	color = tex2D(BackBuffer, float2(texcoord.x,texcoord.y));
}

float2 GridXY(float2 texcoord)
{
	return floor(float2(texcoord.x*BUFFER_WIDTH,texcoord.y*BUFFER_HEIGHT));
}
//Bilateral & Unilateral Left
float4 B_U_L(float2 texcoord : TEXCOORD0)
{
	float4 BL = fmod(GridXY(texcoord).x+GridXY(texcoord).y,2.0) ? 0 : tex2Dlod(BackBuffer, float4(texcoord,0,0));
	float4 UL = fmod(GridXY(texcoord).y,2.0) ? 0 : tex2Dlod(BackBuffer, float4(texcoord,0,0) );
		
	if(Stereoscopic_Mode_Input == 2)
		BL = UL;

	return BL;
}
//Bilateral & Unilateral Right
float4 B_U_R(in float2 texcoord : TEXCOORD0)
{
	float4 BR = fmod(GridXY(texcoord).x+GridXY(texcoord).y,2.0) ? tex2Dlod(BackBuffer, float4(texcoord,0,0)) : 0 ;
	float4 UR = fmod(GridXY(texcoord).y,2.0) ? tex2Dlod(BackBuffer, float4(texcoord,0,0)) : 0 ;
		
	if(Stereoscopic_Mode_Input == 2)
		BR = UR;
	
	return BR;
}

#define Samples 2
float3 Content_Aware_Fill_L(float2 TC)
{  
	float Fill = 1.0; 
	if(Stereoscopic_Mode_Input == 2)
		Fill = 0.5;
		
    float4 Color = B_U_L(TC), total;
    [loop]
    for (int x = -Samples; x <= Samples; x++)
    {  
    	for (int y = -Samples; y <= Samples; y++)
   	 {  
		float2 Box = float2( x, y );
		float2 Calculate = Box * Fill;
		float Distance = sqrt(x > y);
   
        Calculate *= Distance;//get it calculate distance :D...
               
        Color = B_U_L(TC + Calculate * pix);
        total += Color ;//bluring can be done here but you will have to have and AA mask.
        
		if (total.a >= 1) //Mask Treshhold
			break; 
		}      
    }
       
	return total.rgb/total.a;
}

float3 Content_Aware_Fill_R(float2 TC)
{ 
	float Fill = 1.0; 
	if(Stereoscopic_Mode_Input == 2)
		Fill = 0.5;
		
    float4 Color = B_U_R(TC), total;
    [loop]
    for (int x = -Samples; x <= Samples; x++)
    {  
    	for (int y = -Samples; y <= Samples; y++)
   	 {  
		float2 Box = float2( x, y );
		float2 Calculate = Box * Fill;
		float Distance = sqrt(x>y);
   
        Calculate *= Distance;//get it calculate distance :D...
               
        Color = B_U_R(TC + Calculate * pix);
        total += Color ;//bluring can be done here but you will have to have and AA mask.
        
		if (total.a >= 1) //Mask Treshhold
			break; 
		}      
    }
       
	return total.rgb/total.a;
}

void PS_InputLR(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float4 colorA : SV_Target0 , out float4 colorB: SV_Target1)
{	
float4 Left,Right;
	if(Stereoscopic_Mode_Input == 0) //SbS
	{
		Left =  tex2D(BackBuffer,float2(texcoord.x*0.5,texcoord.y));
		Right = tex2D(BackBuffer,float2(texcoord.x*0.5+0.5,texcoord.y));
	}
	else if(Stereoscopic_Mode_Input == 1) //TnB
	{
		Left =  tex2D(BackBuffer,float2(texcoord.x,texcoord.y*0.5));
		Right = tex2D(BackBuffer,float2(texcoord.x,texcoord.y*0.5+0.5));
	}	
	else if(Stereoscopic_Mode_Input == 2 || Stereoscopic_Mode_Input == 3) //Bi & Unilateral Reconstruction needed.
	{
		Left = Content_Aware_Fill_L(texcoord);

		
		Right = Content_Aware_Fill_R(texcoord );

	}
	else if(Stereoscopic_Mode_Input == 4)
	{
		float3 Red = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).rrr * float3(1,0,0);
		float3 Green = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).ggg * float3(0,1,0);
		float3 Blue = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).bbb * float3(0,0,1);
			
		float GS_R = length(Red);
		float GS_GB = length(Blue+Green);
		
		float4 A = float4(GS_R,GS_R,GS_R,1);		
		float4 B = float4(GS_GB,GS_GB,GS_GB,1);

		Left =  A;
		Right = B;
	}
	else if(Stereoscopic_Mode_Input == 5) //  DeAnaglyph Need. Need to Do ReSearch.
	{
		float3 Red = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).rrr * float3(1,0,0);
		float3 Green = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).ggg * float3(0,1,0);
		float3 Blue = tex2D(BackBuffer,float2(texcoord.x,texcoord.y)).bbb * float3(0,0,1);
			
		float GS_R = length(Red);
		float GS_GB = length(Blue+Green);
		
		float4 A = float4(GS_R,GS_R,GS_R,1);				
		float4 B = float4(GS_GB,GS_GB,GS_GB,1);

		A = lerp(A , tex2Dlod(SamplerBB,float4(texcoord,0,5)) , 0.025);		 
		float3 GS_A = dot(A.rgb,float3(0.299, 0.587, 0.114));
		float3 ADone = lerp(GS_A,A.rgb,62.5);
	
		B = lerp(B , tex2Dlod(SamplerBB,float4(texcoord,0,5)) , 0.025);		 
		float3 GS_B = dot(B.rgb,float3(0.299, 0.587, 0.114));
		float3 BDone = lerp(GS_B,B.rgb,62.5);
		
	
		A = lerp(float4(ADone,1),float4(ADone,1)*tex2Dlod(SamplerBB,float4(texcoord,0,5)),0.25);
		B = lerp(float4(BDone,1),float4(BDone,1)*tex2Dlod(SamplerBB,float4(texcoord,0,5)),0.25);
		
		Left =  A;
		Right = B;
		
	}
	else //Frame Sequential Conversion.
	{
		float OddEven = framecount % 2 == 0;
		
		//Past Single Frame
		Left = tex2D(PSBackBuffer,float2(texcoord.x,texcoord.y));
		Right = tex2D(PSBackBuffer,float2(texcoord.x,texcoord.y));
		//Current Single Frame
		if (OddEven)
		{	
			Left =  tex2D(BackBuffer,float2(texcoord.x,texcoord.y));
		}
		else
		{
			Right = tex2D(BackBuffer,float2(texcoord.x,texcoord.y));
		}
	}
	colorA = Left;
	colorB = Right;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 toElse(float2 texcoord)
{
	float4 cL, cR , Out;
	float2 TCL, TCR;
	float P = Perspective * pix.x;
		
		if (Stereoscopic_Mode == 0)
		{
			TCR.x = (texcoord.x*2-1) - P;
			TCL.x = (texcoord.x*2) + P;
			TCR.y = texcoord.y;
			TCL.y = texcoord.y;
		}
		else if(Stereoscopic_Mode == 1)
		{
			TCR.x = texcoord.x - P;
			TCL.x = texcoord.x + P;
			TCR.y = (texcoord.y*2-1);
			TCL.y = (texcoord.y*2);
		}
		else
		{
			TCR.x = texcoord.x - P;
			TCL.x = texcoord.x + P;
			TCR.y = texcoord.y;
			TCL.y = texcoord.y;
		}
		
		//Optimization for line & column interlaced out.
		if (Stereoscopic_Mode == 2)
		{
			TCL.y = TCL.y + (Interlace_Optimization * pix.y);
			TCR.y = TCR.y - (Interlace_Optimization * pix.y);
		}
		else if (Stereoscopic_Mode == 3)
		{
			TCL.x = TCL.x + (Interlace_Optimization * pix.x);
			TCR.x = TCR.x - (Interlace_Optimization * pix.x);
		}
		
		if(Eye_Swap)
		{
		cL = tex2D(SamplerCL,float2(TCL.x,TCL.y));
		cR = tex2D(SamplerCR,float2(TCR.x,TCR.y));
		}
		else
		{
		cL = tex2D(SamplerCR,float2(TCL.x,TCL.y));
		cR = tex2D(SamplerCL,float2(TCR.x,TCR.y));
		}

	float2 gridxy;

	if(Scaling_Support == 0)
	{
		gridxy = floor(float2(texcoord.x*3840.0,texcoord.y*2160.0));
	}	
	else if(Scaling_Support == 1)
	{
		gridxy = floor(float2(texcoord.x*BUFFER_WIDTH,texcoord.y*BUFFER_HEIGHT));
	}
	else if(Scaling_Support == 2)
	{
		gridxy = floor(float2((texcoord.x*1920.0)*0.5,(texcoord.y*1080.0)*0.5));
	}
	else if(Scaling_Support == 3)
	{
		gridxy = floor(float2((texcoord.x*1921.0)*0.5,(texcoord.y*1081.0)*0.5));
	}
	else if(Scaling_Support == 4)
	{
		gridxy = floor(float2((texcoord.x*1680.0)*0.5,(texcoord.y*1050.0)*0.5));
	}
	else if(Scaling_Support == 5)
	{
		gridxy = floor(float2((texcoord.x*1681.0)*0.5,(texcoord.y*1051.0)*0.5));
	}
	else if(Scaling_Support == 6)
	{
		gridxy = floor(float2((texcoord.x*1280.0)*0.5,(texcoord.y*720.0)*0.5));
	}
	else if(Scaling_Support == 7)
	{
		gridxy = floor(float2((texcoord.x*1281.0)*0.5,(texcoord.y*721.0)*0.5));
	}
			
	if(Stereoscopic_Mode == 0)
	{	
		Out = texcoord.x < 0.5 ? cL : cR;
	}
	else if(Stereoscopic_Mode == 1)
	{	
		Out = texcoord.y < 0.5 ? cL : cR;
	}
	else if(Stereoscopic_Mode == 2)
	{
		Out = fmod(gridxy.y,2.0) ? cR : cL;	
	}
	else if(Stereoscopic_Mode == 3)
	{
		Out = fmod(gridxy.x,2.0) ? cR : cL;		
	}
	else if(Stereoscopic_Mode == 4)
	{	
		Out = fmod(gridxy.x+gridxy.y,2.0) ? cR : cL;
	}
	else if(Stereoscopic_Mode == 5)
	{													
			float3 HalfLA = dot(cL.rgb,float3(0.299, 0.587, 0.114));
			float3 HalfRA = dot(cR.rgb,float3(0.299, 0.587, 0.114));
			float3 LMA = lerp(HalfLA,cL.rgb,Anaglyph_Desaturation);  
			float3 RMA = lerp(HalfRA,cR.rgb,Anaglyph_Desaturation); 
			
			float4 cA = float4(LMA,1);
			float4 cB = float4(RMA,1);

		if (Anaglyph_Colors == 0)
		{
			float4 LeftEyecolor = float4(1.0,0.0,0.0,1.0);
			float4 RightEyecolor = float4(0.0,1.0,1.0,1.0);
			
			Out =  (cA*LeftEyecolor) + (cB*RightEyecolor);
		}
		else if (Anaglyph_Colors == 1)
		{
		float red = 0.437 * cA.r + 0.449 * cA.g + 0.164 * cA.b
				- 0.011 * cB.r - 0.032 * cB.g - 0.007 * cB.b;
		
		if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

		float green = -0.062 * cA.r -0.062 * cA.g -0.024 * cA.b 
					+ 0.377 * cB.r + 0.761 * cB.g + 0.009 * cB.b;
		
		if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

		float blue = -0.048 * cA.r - 0.050 * cA.g - 0.017 * cA.b 
					-0.026 * cB.r -0.093 * cB.g + 1.234  * cB.b;
		
		if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }

		Out = float4(red, green, blue, 0);
		}
		else if (Anaglyph_Colors == 2)
		{
			float4 LeftEyecolor = float4(0.0,1.0,0.0,1.0);
			float4 RightEyecolor = float4(1.0,0.0,1.0,1.0);
			
			Out =  (cA*LeftEyecolor) + (cB*RightEyecolor);			
		}
		else
		{
							
		float red = -0.062 * cA.r -0.158 * cA.g -0.039 * cA.b
				+ 0.529 * cB.r + 0.705 * cB.g + 0.024 * cB.b;
		
		if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

		float green = 0.284 * cA.r + 0.668 * cA.g + 0.143 * cA.b 
					- 0.016 * cB.r - 0.015 * cB.g + 0.065 * cB.b;
		
		if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

		float blue = -0.015 * cA.r -0.027 * cA.g + 0.021 * cA.b 
					+ 0.009 * cB.r + 0.075 * cB.g + 0.937  * cB.b;
		
		if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }
				
		Out = float4(red, green, blue, 0);
		}
	}
	return Out;
}

void Current_BackBuffer(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target)
{	 	
	color = tex2D(BackBuffer,texcoord);
}

void Past_BackBuffer(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 PastSingle : SV_Target)
{	
	PastSingle = tex2D(CBackBuffer,texcoord);
}
uniform float timer < source = "timer"; >; //Please do not remove.
////////////////////////////////////////////////////////Logo/////////////////////////////////////////////////////////////////////////
float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y;	
	float3 Color = toElse(texcoord).rgb,D,E,P,T,H,Three,DD,Dot,I,N,F,O;
	
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

technique To_Else
{		
			pass CBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Current_BackBuffer;
			RenderTarget = CurrentBackBuffer;
		}
			pass BB
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_InputBB;
			RenderTarget = texBB;
		}	
			pass StereoInput
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_InputLR;
			RenderTarget0 = texCL;
			RenderTarget1 = texCR;
		}
			pass StereoToElse
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;	
		}
			pass PBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Past_BackBuffer;
			RenderTarget = PastSingleBackBuffer;	
		}
		
}
