 ////------------//
 ///**3DToElse**///
 //------------////
 
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Stereo Input Converter 1.2                           																														*//
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

uniform bool SbS_Half_Full <
	ui_label = "Half / Full";
	ui_tooltip = "Switch Aspect Ratio From Half to Full for Side by Side Video.";
	ui_category = "Stereoscopic Conversion";
> = false;

uniform int Stereoscopic_Mode_Input <
	ui_type = "combo";
	ui_items = "Off\0Side by Side\0Top and Bottom\0Line Interlaced\0Checkerboard 3D\0Frame Sequential\0";
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

uniform int Anaglyph_Colors <
	ui_type = "combo";
	ui_items = "Anaglyph 3D Red/Cyan\0Anaglyph 3D Red/Cyan Dubois\0Anaglyph 3D Red/Cyan Anachrome\0Anaglyph 3D Green/Magenta\0Anaglyph 3D Green/Magenta Dubois\0Anaglyph 3D Green/Magenta Triochrome\0Anaglyph 3D Blue/Amber ColorCode\0";
	ui_label = "Anaglyph Color Mode";
	ui_tooltip = "Select colors for your 3D anaglyph glasses.";
	ui_category = "Stereoscopic Conversion";
> = 0;

uniform int Perspective <
	ui_type = "drag";
	ui_min = -100; ui_max = 100;
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

uniform float2 Interlace_Anaglyph <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " Interlace & Anaglyph";
	ui_tooltip = "Interlace Optimization is used to reduce aliasing in a Line or Column interlaced image. This has the side effect of softening the image.\n"
	             "Anaglyph Desaturation allows for removing color from an anaglyph 3D image. Zero is Black & White, One is full color.\n"
				 "Default for Interlace Optimization is 0.5 and for Anaglyph Desaturation is One.";
	ui_category = "Stereoscopic Options";
> = float2(0.5,1.0);

uniform bool Eye_Swap <
	ui_label = "Eye Swap";
	ui_tooltip = "Left right image change.";
	ui_category = "Stereoscopic Options";
> = true;

/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define TextureSize float2(BUFFER_WIDTH, BUFFER_HEIGHT)
uniform float frametime < source = "frametime";>;
uniform float timer < source = "timer"; >;	

texture BackBufferTex : COLOR;

sampler BackBuffer 
	{ 
		Texture = BackBufferTex;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
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

texture Current_BackBuffer_Tex  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; }; 

sampler CBackBuffer
	{
		Texture = Current_BackBuffer_Tex;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};

texture PastSingleBackBuffer  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; 
sampler PSBackBuffer
	{
		Texture = PastSingleBackBuffer;
		MagFilter = POINT;
		MinFilter = POINT;
		MipFilter = POINT;
	};
 
////////////////////////////////////////////////Left/Right Eye////////////////////////////////////////////////////////
uniform uint framecount < source = "framecount"; >;
//Total amount of frames since the game started.

float fmod(float a, float b) 
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

//Stereo Texture grabber
float4 BB_Texture(float2 TC)
{
	if(Stereoscopic_Mode_Input == 1)
		TC.y = SbS_Half_Full ? TC.y * 0.5 + 0.25 : TC.y;
	float4 Color = tex2Dlod(BackBuffer, float4(TC,0,0) ), Exp_Darks, Exp_Brights;
	        	 
	float3 AdaptColor = tex2Dlod(BackBuffer, float4(TC,0,0) ).rgb;
	
    return float4(Color.rgb,max(AdaptColor.r, max(AdaptColor.g, AdaptColor.b)));
}

//Unilinear Left / Right
float4 U_LR(in float2 texcoord,in int Switcher)
{
	float gridy = floor(texcoord.y*TextureSize.y); //Native
	if(Switcher == 1)
		return fmod(gridy,2) ? 0 : tex2Dlod(CBackBuffer, float4(texcoord,0,0) ) ;
	else	
		return fmod(gridy,2) ? tex2Dlod(CBackBuffer, float4(texcoord,0,0) ) : 0 ;
}

float4 Uni_LR(in float2 texcoord,in int Switcher )
{
   float4 tl = U_LR(texcoord, Switcher),
		  tr = U_LR(texcoord + float2(0.0,-pix.y), Switcher),
		  bl = U_LR(texcoord + float2(0.0, pix.y), Switcher);
   float2 f = frac( texcoord * TextureSize );
   float4 tA = lerp( tl, tr, f.x );
   float4 tB = lerp( tl, bl, f.x );
   float4 done = lerp( tA, tB, f.y ) * 2.0;//2.0 Gamma correction.
   return done;
}

//Bilinear Left
float4 B_LR(in float2 texcoord,in int Switcher)
{
	float gridy = floor(texcoord.y*(BUFFER_HEIGHT)); //Native
	float gridx = floor(texcoord.x*(BUFFER_WIDTH)); //Native
	if(Switcher == 1)
		return fmod(gridy+gridx,2) ? 0 : tex2Dlod(CBackBuffer, float4(texcoord,0,0) ) ;
	else
		return fmod(gridy+gridx,2) ? tex2Dlod(CBackBuffer, float4(texcoord,0,0) ) : 0 ;
}

float4 Bi_LR(in float2 texcoord,in int Switcher )
{
   float4 tl = B_LR(texcoord, Switcher);
   float4 tr = B_LR(texcoord + float2( pix.x,  0.0 ), Switcher);
   float4 bl = B_LR(texcoord + float2(  0.0 , pix.y), Switcher);
   float4 br = B_LR(texcoord + float2( pix.x, pix.y), Switcher);
   float2 f = frac( texcoord * TextureSize );
   float4 tA = lerp( tl, tr, f.x );
   float4 tB = lerp( bl, br, f.x );
   float4 done = lerp( tA, tB, f.y ) * 2.0;//2.0 Gamma correction.
   return done;
}

void PS_InputLR(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float4 colorA : SV_Target0 , out float4 colorB: SV_Target1)
{	
float4 Left, Right;
	float P = Perspective * pix.x;
			
	if(Stereoscopic_Mode_Input == 1) //SbS
	{
		Left =  BB_Texture(float2((texcoord.x*0.5) + P,texcoord.y));
		Right = BB_Texture(float2((texcoord.x*0.5+0.5)- P,texcoord.y));
	}
	else if(Stereoscopic_Mode_Input == 2) //TnB
	{
		Left =  BB_Texture(float2(texcoord.x + P,texcoord.y*0.5));
		Right = BB_Texture(float2(texcoord.x - P,texcoord.y*0.5+0.5));
	}
	else if(Stereoscopic_Mode_Input == 3) //Line_Interlaced Unilateral.
	{
		Left =  Uni_LR(float2(texcoord.x + P,texcoord.y), 0);
		Right = Uni_LR(float2(texcoord.x - P,texcoord.y), 1);
	}	
	else if(Stereoscopic_Mode_Input == 4) //CB_3D Bilateral Reconstruction.
	{
		Left =  Bi_LR(float2(texcoord.x + P,texcoord.y), 0);
		Right = Bi_LR(float2(texcoord.x - P,texcoord.y), 1);
	}
	if(Stereoscopic_Mode_Input == 5) //Frame Sequential Conversion.
	{
		float OddEven = framecount % 2 == 0;
		//Past Single Frame
		Left = tex2D(PSBackBuffer,float2(texcoord.x + P,texcoord.y));
		Right = tex2D(PSBackBuffer,float2(texcoord.x - P,texcoord.y));
		//Current Single Frame
		if (OddEven)
			Left =  BB_Texture(float2(texcoord.x+P,texcoord.y));
		else
			Right = BB_Texture(float2(texcoord.x-P,texcoord.y));
	}

	
	colorA = Left;
	colorB = Right;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 toElse(float2 texcoord)
{
	float4 image = 1, cL, cR, L, R, Out, accum;
	float2 TCL, TCR, StoreTC = texcoord;
	float P = 0;//Perspective * pix.x;

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
			TCL.y = TCL.y + (Interlace_Anaglyph.x * pix.y);
			TCR.y = TCR.y - (Interlace_Anaglyph.x * pix.y);
		}
		else if (Stereoscopic_Mode == 3)
		{
			TCL.x = TCL.x + (Interlace_Anaglyph.x * pix.x);
			TCR.x = TCR.x - (Interlace_Anaglyph.x * pix.x);
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
		gridxy = floor(float2(texcoord.x*3840.0,texcoord.y*2160.0));	
	else if(Scaling_Support == 1)
		gridxy = floor(float2(texcoord.x*BUFFER_WIDTH,texcoord.y*BUFFER_HEIGHT));
	else if(Scaling_Support == 2)
		gridxy = floor(float2((texcoord.x*1920.0)*0.5,(texcoord.y*1080.0)*0.5));
	else if(Scaling_Support == 3)
		gridxy = floor(float2((texcoord.x*1921.0)*0.5,(texcoord.y*1081.0)*0.5));
	else if(Scaling_Support == 4)
		gridxy = floor(float2((texcoord.x*1680.0)*0.5,(texcoord.y*1050.0)*0.5));
	else if(Scaling_Support == 5)
		gridxy = floor(float2((texcoord.x*1681.0)*0.5,(texcoord.y*1051.0)*0.5));
	else if(Scaling_Support == 6)
		gridxy = floor(float2((texcoord.x*1280.0)*0.5,(texcoord.y*720.0)*0.5));
	else if(Scaling_Support == 7)
		gridxy = floor(float2((texcoord.x*1281.0)*0.5,(texcoord.y*721.0)*0.5));

			
	if(Stereoscopic_Mode == 0)	
		Out = texcoord.x < 0.5 ? cL : cR;
	else if(Stereoscopic_Mode == 1)	
		Out = texcoord.y < 0.5 ? cL : cR;
	else if(Stereoscopic_Mode == 2)
		Out = fmod(gridxy.y,2.0) ? cR : cL;	
	else if(Stereoscopic_Mode == 3)
		Out = fmod(gridxy.x,2.0) ? cR : cL;		
	else if(Stereoscopic_Mode == 4)	
		Out = fmod(gridxy.x+gridxy.y,2.0) ? cR : cL;
	else if(Stereoscopic_Mode == 5)
	{													
		float Contrast = 1.0, DeGhost = 0.06, LOne, ROne;
			float3 HalfLA = dot(cL.rgb,float3(0.299, 0.587, 0.114));
			float3 HalfRA = dot(cR.rgb,float3(0.299, 0.587, 0.114));
			float3 LMA = lerp(HalfLA,cL.rgb,Interlace_Anaglyph.y);  
			float3 RMA = lerp(HalfRA,cR.rgb,Interlace_Anaglyph.y); 
		float contrast = (Contrast*0.5)+0.5;

		// Left/Right Image
		float4 cA = float4(LMA,1);
		float4 cB = float4(RMA,1);

		if (Anaglyph_Colors == 0) // Anaglyph 3D Colors Red/Cyan
			Out =  float4(cA.r,cB.g,cB.b,1.0);
		else if (Anaglyph_Colors == 1) // Anaglyph 3D Dubois Red/Cyan
		{
			float red = 0.437 * cA.r + 0.449 * cA.g + 0.164 * cA.b - 0.011 * cB.r - 0.032 * cB.g - 0.007 * cB.b;

			if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

			float green = -0.062 * cA.r -0.062 * cA.g -0.024 * cA.b + 0.377 * cB.r + 0.761 * cB.g + 0.009 * cB.b;

			if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

			float blue = -0.048 * cA.r - 0.050 * cA.g - 0.017 * cA.b -0.026 * cB.r -0.093 * cB.g + 1.234  * cB.b;

			if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }

			Out = float4(red, green, blue, 0);
		}
		else if (Anaglyph_Colors == 2) // Anaglyph 3D Deghosted Red/Cyan Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast;
			DeGhost *= 0.1;

			accum = saturate(cA*float4(LOne,(1.0-LOne)*0.5,(1.0-LOne)*0.5,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.00);
			image.a = accum.a;

			accum = saturate(cB*float4(1.0-ROne,ROne,0.0,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4(1.0-ROne,0.0,ROne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = (image.a+accum.a)/3.0;

			accum = image;
			image.r = (accum.r+(accum.r*DeGhost)+(accum.g*(DeGhost*-0.5))+(accum.b*(DeGhost*-0.5)));
			image.g = (accum.g+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*0.5))+(accum.b*(DeGhost*-0.25)));
			image.b = (accum.b+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*0.5)));
			Out = image;
		}
		else if (Anaglyph_Colors == 3) // Anaglyph 3D Green/Magenta
			Out = float4(cB.r,cA.g,cB.b,1.0);
		else if (Anaglyph_Colors == 4) // Anaglyph 3D Dubois Green/Magenta
		{

			float red = -0.062 * cA.r -0.158 * cA.g -0.039 * cA.b + 0.529 * cB.r + 0.705 * cB.g + 0.024 * cB.b;

			if (red > 1) { red = 1; }   if (red < 0) { red = 0; }

			float green = 0.284 * cA.r + 0.668 * cA.g + 0.143 * cA.b - 0.016 * cB.r - 0.015 * cB.g + 0.065 * cB.b;

			if (green > 1) { green = 1; }   if (green < 0) { green = 0; }

			float blue = -0.015 * cA.r -0.027 * cA.g + 0.021 * cA.b + 0.009 * cB.r + 0.075 * cB.g + 0.937  * cB.b;

			if (blue > 1) { blue = 1; }   if (blue < 0) { blue = 0; }

			Out = float4(red, green, blue, 0);
		}
		else if (Anaglyph_Colors == 5)// Anaglyph 3D Deghosted Green/Magenta Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast*0.8;
			DeGhost *= 0.275;

			accum = saturate(cB*float4(ROne,1.0-ROne,0.0,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = accum.a;

			accum = saturate(cA*float4((1.0-LOne)*0.5,LOne,(1.0-LOne)*0.5,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.05);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4(0.0,1.0-ROne,ROne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.15);
			image.a = (image.a+accum.a)*0.33333333;

			accum = image;
			image.r = accum.r+(accum.r*(DeGhost*0.5))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*-0.25));
			image.g = accum.g+(accum.r*(DeGhost*-0.5))+(accum.g*(DeGhost*0.25))+(accum.b*(DeGhost*-0.5));
			image.b = accum.b+(accum.r*(DeGhost*-0.25))+(accum.g*(DeGhost*-0.25))+(accum.b*(DeGhost*0.5));
			Out = image;
		}
		else if (Anaglyph_Colors == 6) // Anaglyph 3D Blue/Amber Code From http://iaian7.com/quartz/AnaglyphCompositing & vectorform.com by John Einselen
		{
			LOne = contrast*0.45;
			ROne = contrast;
			float D[1];//The Chronicles of Riddick: Assault on Dark Athena FIX I don't know why it works.......
			DeGhost *= 0.275;

			accum = saturate(cA*float4(ROne,0.0,1.0-ROne,1.0));
			image.r = pow(accum.r+accum.g+accum.b, 1.05);
			image.a = accum.a;

			accum = saturate(cA*float4(0.0,ROne,1.0-ROne,1.0));
			image.g = pow(accum.r+accum.g+accum.b, 1.10);
			image.a = image.a+accum.a;

			accum = saturate(cB*float4((1.0-LOne)*0.5,(1.0-LOne)*0.5,LOne,1.0));
			image.b = pow(accum.r+accum.g+accum.b, 1.0);
			image.b = lerp(pow(image.b,(DeGhost*0.15)+1.0),1.0-pow(abs(1.0-image.b),(DeGhost*0.15)+1.0),image.b);
			image.a = (image.a+accum.a)*0.33333333;

			accum = image;
			image.r = accum.r+(accum.r*(DeGhost*1.5))+(accum.g*(DeGhost*-0.75))+(accum.b*(DeGhost*-0.75));
			image.g = accum.g+(accum.r*(DeGhost*-0.75))+(accum.g*(DeGhost*1.5))+(accum.b*(DeGhost*-0.75));
			image.b = accum.b+(accum.r*(DeGhost*-1.5))+(accum.g*(DeGhost*-1.5))+(accum.b*(DeGhost*3.0));
			Out = saturate(image);
		}
	}

	return Out;
}

void Current_BackBuffer(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target)
{	 	
	color = BB_Texture(texcoord);
}

void Past_BackBuffer(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 PastSingle : SV_Target0)
{
		PastSingle = tex2D(CBackBuffer,texcoord);
}
////////////////////////////////////////////////////////Fin/////////////////////////////////////////////////////////////////////////
float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float3 Color = Stereoscopic_Mode_Input == 0 ? BB_Texture(texcoord).rgb : toElse(texcoord).rgb;
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

//*Rendering passes*//
technique To_Else
{		
			pass PBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Past_BackBuffer;
			RenderTarget = PastSingleBackBuffer;
		}
			pass CBB
		{
			VertexShader = PostProcessVS;
			PixelShader = Current_BackBuffer;
			RenderTarget = Current_BackBuffer_Tex;
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
}
