////-----------//
///**Depth3D**///
//-----------////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Depth Map Based 3D post-process shader Depth3D v1.4.0
//* For Reshade 3.0 & 4.0
//* ---------------------------------------------------------------------------------------------------
//*
//* This Shader is an simplified version of SuperDepth3D.fx a shader I made for ReShade's collection standard effects. For the use with stereo 3D screens.
//* Also had to rework Philippe David http://graphics.cs.brown.edu/games/SteepParallax/index.html code to work with reshade. This is used for the parallax effect.
//* This idea was taken from this shader here located at https://github.com/Fubaxiusz/fubax-shaders/blob/596d06958e156d59ab6cd8717db5f442e95b2e6b/Shaders/VR.fx#L395
//* It's also based on Philippe David Steep Parallax mapping code. If I missed any information please contact me so I can make corrections.
//* 													Multi-licensing
//* LICENSE
//* ============
//* Overwatch & Code out side the work of people mention above is licenses under: Attribution-NoDerivatives 4.0 International
//*
//* You are free to:
//* Share - copy and redistribute the material in any medium or format
//* for any purpose, even commercially.
//* The licensor cannot revoke these freedoms as long as you follow the license terms.
//* Under the following terms:
//* Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made.
// *You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
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
//* http://reshade.me/forum/shader-presentation/2128-sidebyside-3d-depth-map-based-stereoscopic-shader
//*
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//USER EDITABLE PREPROCESSOR FUNCTIONS START//
#define Image_Size 0.0 //Set Image Size
//USER EDITABLE PREPROCESSOR FUNCTIONS END//
/* //Not used
uniform float Elevation_Sigma<
	ui_type = "drag";
	ui_min = -5; ui_max = 5;
> = 0;
*/
uniform float Divergence <
	ui_type = "drag";
	ui_min = 1; ui_max = 100; ui_step = 0.5;
	ui_label = "·Divergence Slider·";
	ui_tooltip = "Divergence increases differences between the left and right retinal images and allows you to experience depth.\n"
				 "The process of deriving binocular depth information is called stereopsis.\n"
				 "You can override this value.";
	ui_category = "Divergence & Convergence";
> = 50.0;

uniform float Pivot_Point <
	ui_type = "drag";
	ui_min = 0; ui_max = 1;
	ui_label = "·Pivot Point Slider·";
	ui_tooltip = "Pivot Point works like convergence in an 3D image.";
	ui_category = "Divergence & Convergence";
> = 0.5;

uniform bool invertX <
	ui_label = " Invert X";
	ui_tooltip = "Invert X.";
> = false;
/*
uniform bool invertY <
	ui_label = " Invert Y";
	ui_tooltip = "Invert Y.";
> = false;
*/
//Depth Buffer Adjust//
uniform int Depth_Map <
	ui_type = "combo";
	ui_items = "Z-Buffer Normal\0Z-Buffer Reversed\0";
	ui_label = "·Z-Buffer Selection·";
	ui_tooltip = "Select Depth Buffer Linearization.";
	ui_category = "Depth Buffer Adjust";
> = 0;

uniform float Depth_Map_Adjust <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 250.0; ui_step = 0.125;
	ui_label = " Z-Buffer Adjustment";
	ui_tooltip = "This allows for you to adjust Depth Buffer Precision.\n"
	             "Try to adjust this to keep it as low as possible.\n"
	             "Don't go too high with this adjustment.\n"
	             "Default is 5.0";
	ui_category = "Depth Buffer Adjust";
> = 5.0;

uniform float Offset <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = " Depth Map Offset";
	ui_tooltip = "Depth Map Offset is for non conforming ZBuffer.\n"
				 "It's rare if you need to use this in any game.\n"
				 "Use this to make adjustments to DM 0 or DM 1.\n"
				 "Default and starts at Zero and it's Off.";
	ui_category = "Depth Buffer Adjust";
> = 0;

uniform bool Depth_Map_View <
	ui_label = " Display Depth";
	ui_tooltip = "Display the Depth Buffer.";
	ui_category = "Depth Buffer Adjust";
> = false;

uniform bool Depth_Map_Flip <
	ui_label = " Flip Depth";
	ui_tooltip = "Flip the Depth Buffer if it is upside down.";
	ui_category = "Depth Buffer Adjust";
> = false;

uniform bool Debug_Mode <
	ui_label = " Debug";
	ui_tooltip = "Use this to view head Postion in relation to the screen.\n"
 				"It's how eye see you........................";
	ui_category = "Depth Buffer Adjust";
> = false;

/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
#define AR 1.77 //BUFFER_WIDTH / BUFFER_HEIGHT Don't work ???
texture DepthBufferTex : DEPTH;

sampler DepthBuffer
	{
		Texture = DepthBufferTex;
	};

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
	};

texture texDiplus {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;}; //Sample at 256x256/2 and a mip bias of 8 should be 1x1

sampler SamplerDiplus
	{
		Texture = texDiplus;
		AddressU = BORDER;
		AddressV = BORDER;
		AddressW = BORDER;
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

/////////////////////////////////////////////////////////////////////////////////Depth Map Information/////////////////////////////////////////////////////////////////////////////////
uniform float3 motion[2] < source = "freepie"; index = 0; >;
//. motion[0] is yaw, pitch, roll and motion[1] is x,y,z.
float3 FP_IO_Rot(){return motion[0];}
float3 FP_IO_Pos(){return motion[1];}

float2 Depth(in float2 texcoord : TEXCOORD)
{
	if (Depth_Map_Flip)
		texcoord.y =  1 - texcoord.y;

	float zBuffer = tex2Dlod(DepthBuffer,float4(texcoord,0,0)).x; //Depth Buffer

	//Conversions to linear space.....
	//Near & Far Adjustment
	float Far = 1., Near = 0.125/Depth_Map_Adjust, DA = Depth_Map_Adjust * 2.0f; //Depth Map Adjust - Near

	float2 Offsets = float2(1 + Offset,1 - Offset), Z = float2( zBuffer, 1-zBuffer );

	if (Offset > 0)
	Z = min( 1, float2( Z.x * Offsets.x , Z.y / Offsets.y  ));


	[branch] if (Depth_Map == 0)//DM0. Normal
		zBuffer = pow(abs(Z.x),DA);
	else if (Depth_Map == 1)//DM1. Reverse
		zBuffer = pow(abs(Z.y),DA);
	float StoreZ = zBuffer;
	[branch] if (Depth_Map == 0)//DM0. Normal
		zBuffer = Far * Near / (Far + Z.x * (Near - Far));
	else if (Depth_Map == 1)//DM1. Reverse
		zBuffer = Far * Near / (Far + Z.y * (Near - Far));


	return saturate(float2(zBuffer,StoreZ));
}

float Scale(float val,float max,float min) //Scale to 0 - 1
{
	return (val - min) / (max - min);
}

float zBuffer(in float2 texcoord : TEXCOORD0)
{
	float2 DM = Depth(texcoord);

	float Conv = 1-0.025/DM.x;
	return lerp(Scale(Conv,1,0) ,DM.y,0.125);
}

float2 Parallax( float2 Diverge, float2 Coordinates)
{
	float D = abs(length(Diverge)) ;
	float Cal_Steps = D + (D * 0.05);
	//ParallaxSteps
	float Steps = clamp(Cal_Steps,1,255);
	// Offset per step progress & Limit
	float LayerDepth = rcp(Steps);
	//Offsets listed here Max Seperation is 3% - 8% of screen space with Depth Offsets & Netto layer offset change based on MS.
	float2 MS = Diverge * pix;
	float2  deltaCoordinates = MS * LayerDepth, ParallaxCoord = Coordinates , DB_Offset = (Diverge * 0.025) * pix;
	float CurrentDepthMapValue = zBuffer(ParallaxCoord), CurrentLayerDepth = 0;

	[loop] //Steep parallax mapping
    for ( int i = 0; i < Steps; i++ )
    {	// Doing it this way should stop crashes in older version of reshade, I hope.
        if (CurrentDepthMapValue <= CurrentLayerDepth)
			break; // Once we hit the limit Stop Exit Loop.
        // Shift coordinates horizontally in linear fasion
        ParallaxCoord -= deltaCoordinates;
        // Get depth value at current coordinates
		CurrentDepthMapValue = zBuffer( ParallaxCoord - DB_Offset);
        // Get depth of next layer
        CurrentLayerDepth += LayerDepth;
    }
	// Parallax Occlusion Mapping
	float2 PrevParallaxCoord = ParallaxCoord + deltaCoordinates, DepthDifference;
	float afterDepthValue = CurrentDepthMapValue - CurrentLayerDepth;
	float beforeDepthValue = zBuffer( ParallaxCoord ) + CurrentLayerDepth + LayerDepth;

	// Interpolate coordinates
	float weight = afterDepthValue / (afterDepthValue - beforeDepthValue);
	ParallaxCoord = PrevParallaxCoord * max(0,weight) + ParallaxCoord * min(1,1.0f - weight);

	ParallaxCoord += DB_Offset;

	return ParallaxCoord ;
}

float4 PS_calcLRUD(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 IS = float2(1+Image_Size,Image_Size * 0.5);
	texcoord *= IS.x; 
	texcoord -= IS.y;

	float2 TexCoords = texcoord,Center;
	float4 color;
	float2 coordsXY;

	coordsXY.xy = float2(FP_IO_Pos().x,FP_IO_Pos().y);

	Center.x = coordsXY.x;
	//Center.y = MousecoordsXY.y + Elevation_Sigma ;
	if( invertX )
		Center.x = -Center.x;
	//if( invertY )
		//Center.y = -Center.y;
	float PP = Divergence * Pivot_Point;

	float2 Per = (Center * pix) * PP ;

	texcoord = Parallax(float2(Center.x * Divergence,Center.y * Divergence) , texcoord + Per);

	if(!Depth_Map_View)
			color = tex2Dlod(BackBuffer, float4(texcoord,0,0));
		else
			color = zBuffer(texcoord).xx;

	float2 TC_A = invertX  ? TexCoords - Per : TexCoords + Per;
	float2 TC_B = invertX  ? TexCoords - (Per * 0.9375) : TexCoords + (Per * 0.9375);
	/* //Used to show the Parallax
		float2 TC_A = TexCoords - Per;
		float2 TC_B = TexCoords - (Per * 0.5);
	if(TC_A.x > 0.51 || TC_A.x < 0.490 || TC_A.y > 0.515 || TC_A.y < 0.485 )
	{
		if(TC_B.x > 0.52 || TC_B.x < 0.48 || TC_B.y > 0.530 || TC_B.y < 0.47)
		{
			if (TexCoords.x > 0.54 || TexCoords.x < 0.46 || TexCoords.y > 0.545 || TexCoords.y < 0.455)
				return float4(color.rgb,1.0);
			else
				return 1;
		}
		else
			return float4(0.25,0.5,0.9,1.0);
	}
	else
		return 0;
	*/
	if ((TexCoords.x > 0.54 || TexCoords.x < 0.46 || TexCoords.y > 0.545 || TexCoords.y < 0.455) || !Debug_Mode)
		return float4(color.rgb,1.0);
	else
	{
		if(TC_A.x > 0.51 || TC_A.x < 0.490 || TC_A.y > 0.515 || TC_A.y < 0.485 )
		{if (TC_B.x > 0.52 || TC_B.x < 0.48 || TC_B.y > 0.530 || TC_B.y < 0.47)
			return 1;
		else
			return float4(0.25,0.5,0.9,1.0);
		}
		else
			return 0;
	}

}


void A1(float2 texcoord,float PosX,float PosY,inout float I, inout float N, inout float F, inout float O)
{
		float PosXI = 0.0155+PosX, PosYI = 0.004+PosY, PosYII = 0.008+PosY;I = all( abs(float2( texcoord.x - PosXI, texcoord.y - PosY)) < float2(0.003,0.001));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYI)) < float2(0.000625,0.005));I += all( abs(float2( texcoord.x - PosXI, texcoord.y - PosYII)) < float2(0.003,0.001));
		float PosXN = 0.0225+PosX, PosYN = 0.005+PosY,offsetN = -0.001;N = all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN)) < float2(0.002,0.004));N -= all( abs(float2( texcoord.x - PosXN, texcoord.y - PosYN - offsetN)) < float2(0.003,0.005));
		float PosXF = 0.029+PosX, PosYF = 0.004+PosY, offsetF = 0.0005, offsetF1 = 0.001;F = all( abs(float2( texcoord.x -PosXF-offsetF, texcoord.y-PosYF-offsetF1)) < float2(0.002,0.004));F -= all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0025,0.005));F += all( abs(float2( texcoord.x -PosXF, texcoord.y-PosYF)) < float2(0.0015,0.00075));
		float PosXO = 0.035+PosX, PosYO = 0.004+PosY;O = all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.003,0.005));O -= all( abs(float2( texcoord.x -PosXO, texcoord.y-PosYO)) < float2(0.002,0.003));
}
////////////////////////////////////////////////////////Logo/////////////////////////////////////////////////////////////////////////
uniform float timer < source = "timer"; >;
float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float2 PivotPoint = float2(0.5,0.5);
	//float Rot;
	//Nope this don't work well with Tobii Need other method.....
	//Rot = 0;//radians(-FP_IO_Rot().z);

    float2 Rotationtexcoord = texcoord ;
    //float sin_factor = sin(Rot);
    //float cos_factor = cos(Rot);
    //Rotationtexcoord = mul(Rotationtexcoord - PivotPoint,float2x2(float2( cos_factor, -sin_factor) * float2(1.0,AR),float2( sin_factor,  cos_factor) * float2(rcp(AR),1.0)));
	//Rotationtexcoord += PivotPoint;

	float3 Color = tex2D(SamplerDiplus,Rotationtexcoord).rgb;
	float PosX = 0.9525f*BUFFER_WIDTH*pix.x,PosY = 0.975f*BUFFER_HEIGHT*pix.y,A,B,C,D,E,F,G,H,I,J,K,L,PosXDot = 0.011+PosX, PosYDot = 0.008+PosY;L = all( abs(float2( texcoord.x -PosXDot, texcoord.y-PosYDot)) < float2(0.00075,0.0015));A0(texcoord,PosX,PosY,A,B,C,D,E,F,G );A1(texcoord,PosX,PosY,H,I,J,K);
	return timer <= 12500 ? A+B+C+D+E+F+G+H+I+J+K+L ? 1-texcoord.y*50.0+48.35f : float4(Color,1.) : float4(Color,1.);
}
///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{// Vertex shader generating a triangle covering the entire screen
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
//*Rendering passes*//
technique Dimension_Plus
< ui_tooltip = "This Shader should be the VERY LAST Shader in your master shader list.\n"
	           "You can always Drag shaders around by clicking them and moving them.\n"
	           "For more help you can always contact me at DEPTH3D.info."; >//Website WIP
{
		pass Perspective
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_calcLRUD;
		RenderTarget = texDiplus;
	}
		pass StereoOut
	{
		VertexShader = PostProcessVS;
		PixelShader = Out;
	}
}
