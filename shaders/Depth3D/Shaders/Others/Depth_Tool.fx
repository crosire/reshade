////--------------//
///**Depth Tool**///
//--------------////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//* Depth Map Shader                                           																													*//
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
//* ---------------------------------																																				*//																																											*//
//* 																																												*//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Packed Depth Code from this link.
//https://stackoverflow.com/questions/48288154/pack-depth-information-in-a-rgba-texture-using-mediump-precison
// Use this to unpack depth from XYZ to X
/* 
float UnpackDepth24( in float3 pack )
{
  float depth = dot( pack, 1.0 / float3(1.0, 256.0, 256.0*256.0) );
  return depth * (256.0*256.0*256.0) / (256.0*256.0*256.0 - 1.0);
}
*/

uniform bool Dither <
 ui_label = "Enable Dither";
 ui_tooltip = "Lets turn Dither on or off.";
> = false;

uniform float Dither_Bit <
 ui_type = "drag";
 ui_min = 1; ui_max = 15;
 ui_label = "Dither Bit";
 ui_tooltip = "Dither is an intentionally applied form of noise used to randomize quantization error, preventing banding in images.";
> = 6;

uniform int Depth_Map <
 ui_type = "combo";
 ui_items = "Normal\0Reversed\0";
 ui_label = "Custom Depth Map";
 ui_tooltip = "Pick your Depth Map.";
> = 0;

uniform float Depth_Map_Adjust <
 ui_type = "drag";
 ui_min = 0.25; ui_max = 250.0;
 ui_label = "Depth Map Adjustment";
 ui_tooltip = "Adjust the depth map and sharpness.";
> = 5.0;

uniform float Offset <
 ui_type = "drag";
 ui_min = 0; ui_max = 1.0;
 ui_label = "Offset";
 ui_tooltip = "Offset is for the Special Depth Map Only";
> = 0.0;

uniform bool PackDepth <
 ui_label = "PackDepth 24";
 ui_tooltip = "Use this toggle to pack depth in to RGB So it can be unpacked later.";
> = false;

uniform bool Depth_Map_Flip <
 ui_label = "Depth Map Flip";
 ui_tooltip = "Flip the depth map if it is upside down.";
> = false;

uniform bool LP <
 ui_label = "Linear or Point";
 ui_tooltip = "You try coming up with names on the fly.";
> = false;

uniform int Plus_Depth <
 ui_type = "combo";
 ui_items = " Color\0 2D + Depth Side by Side\0 2D + Depth Side by Side AR\0 2D + Depth Top n Bottom\0 Stored Color\0 Stored Depth\0";
 ui_label = "2D + Depth Selection";
 ui_tooltip = "Pick your Output.";
> = 0;

uniform bool SCD <
 ui_label = "Store Color & Depth";
 ui_tooltip = "Use this to store Color and Depth.";
> = false;

/////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////

texture BackBufferTex : COLOR;

sampler BackBufferL
 {
   Texture = BackBufferTex;
 };

sampler BackBufferP
 {
   Texture = BackBufferTex;
   MagFilter = LINEAR;
   MinFilter = LINEAR;
   MipFilter = LINEAR;
 };


texture DepthBufferTex : DEPTH;

sampler DepthBufferL
 {
   Texture = DepthBufferTex;
 };

sampler DepthBufferP
 {
   Texture = DepthBufferTex;
   MagFilter = LINEAR;
   MinFilter = LINEAR;
   MipFilter = LINEAR;
 };

texture SPC  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler SPColor
 {
   Texture = SPC;
 };
texture SPD  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F;};

sampler SPDepth
 {
   Texture = SPD;
 };

texture SC  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};

sampler SColor
 {
   Texture = SC;
 };

texture SD  { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F;};

sampler SDepth
 {
   Texture = SD;
 };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

float3 PackDepth24( in float depth )
{
    float depthVal = depth * (256.0*256.0*256.0 - 1.0) / (256.0*256.0*256.0);
    float4 encode = frac( depthVal * float4(1.0, 256.0, 256.0*256.0, 256.0*256.0*256.0) );
    return encode.xyz - encode.yzw / 256.0 + 1.0/512.0;
}

uniform float frametime < source = "frametime"; >;
float zBuffer(in float2 texcoord : TEXCOORD0)
{
   if (Depth_Map_Flip)
     texcoord.y =  1 - texcoord.y;

   float DM, zBuffer = tex2D(DepthBufferL, texcoord).x; //Depth Buffer

   if(LP)
   zBuffer = tex2D(DepthBufferP, texcoord).x;

   //Conversions to linear space.....
   //Near & Far Adjustment
   float Far = 1.0, Near = 0.125/Depth_Map_Adjust; //Division Depth Map Adjust - Near

   float2 Offsets = float2(1 + Offset,1 - Offset), Z = float2( zBuffer, 1-zBuffer );

   if (Offset > 0)
   Z = min( 1, float2( Z.x*Offsets.x , ( Z.y - 0.0 ) / ( Offsets.y - 0.0 ) ) );

   if (Depth_Map == 0)//DM0. Normal
   {
     DM = 2.0 * Near * Far / (Far + Near - (2.0 * Z.x - 1.0) * (Far - Near));
   }
   else if (Depth_Map == 1)//DM1. Reverse
   {
     DM = 2.0 * Near * Far / (Far + Near - (1.375 * Z.y - 0.375) * (Far - Near));
   }

 // Dither for DepthBuffer adapted from gedosato ramdom dither https://github.com/PeterTh/gedosato/blob/master/pack/assets/dx9/deband.fx
 // I noticed in some games the depth buffer started to have banding so this is used to remove that.

 float DB  = Dither_Bit;
 float noise = frac(sin(dot(texcoord * frametime, float2(12.9898, 78.233))) * 43758.5453);
 float dither_shift = (1.0 / (pow(2,DB) - 1.0));
 float dither_shift_half = (dither_shift * 0.5);
 dither_shift = dither_shift * noise - dither_shift_half;

 if(Dither)
 {
   DM += -dither_shift;
   DM += dither_shift;
   DM += -dither_shift;
 }
 // Dither End

 return saturate(DM);
}

void S_CD(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 C : SV_Target0, out float4 D : SV_Target1)
{
 C = tex2D(BackBufferL, texcoord);
 D = float4(PackDepth24( zBuffer(texcoord).x ),1.0);
 float4 PC = tex2Dlod(SPColor, float4(texcoord,0,0));
 float4 PD = tex2Dlod(SPDepth, float4(texcoord,0,0));
 float S = SCD;
 //Didn't even needed to do it this way. But, I was lazy. :D
 C = lerp(C , PC, S);
 D = lerp(D , PD, S);
}

float4 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{	float2 StoreTC = texcoord,TCL,TCR;
 float4 color;

 float2 Z_A = float2(1.0,0.5);
 if(Plus_Depth == 2)
 {
   Z_A = float2(1.0,1.0);
 }
 //Texture Zoom & Aspect Ratio//
 float X = Z_A.x;
 float Y = Z_A.y * Z_A.x * 2;
 float midW = (X - 1)*(BUFFER_WIDTH*0.5)*pix.x;
 float midH = (Y - 1)*(BUFFER_HEIGHT*0.5)*pix.y;

 texcoord = float2((texcoord.x*X)-midW,(texcoord.y*Y)-midH);


 if (Plus_Depth == 1 || Plus_Depth == 2)
 {
   TCL = float2(texcoord.x*2,texcoord.y);
   TCR = float2(texcoord.x*2-1,texcoord.y);
 }
 else if(Plus_Depth == 3)
 {
   TCL = float2(texcoord.x,texcoord.y*2);
   TCR = float2(texcoord.x,texcoord.y*2-1);
 }

 float4 Co = tex2D(BackBufferL,TCL);

 if(LP)
   Co = tex2D(BackBufferP,TCL);


 if(texcoord.y > 0 && texcoord.y < 1)
 {
 if(Plus_Depth == 1 || Plus_Depth == 2)
   color = texcoord.x < 0.5 ? Co : PackDepth ? float4(PackDepth24( zBuffer(TCR).x ),1.0) : zBuffer(TCR).x;
 else if(Plus_Depth == 3)
   color = texcoord.y < 0.5 ? Co : PackDepth ? float4(PackDepth24( zBuffer(TCR).x ),1.0) : zBuffer(TCR).x;
 }

 if(Plus_Depth == 4)
   color = tex2D(SColor,StoreTC);

 if(Plus_Depth == 5)
   color = tex2D(SDepth,StoreTC);

 if (Plus_Depth == 0)
 color = tex2D(BackBufferL, texcoord);

 return color;
}

void S_BB(float4 position : SV_Position, float2 texcoord : TEXCOORD, out float4 PastC : SV_Target0, out float4 PastD : SV_Target1)
{
 PastC = tex2D(SColor,texcoord);
 PastD = tex2D(SDepth,texcoord);
}

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
 texcoord.x = (id == 2) ? 2.0 : 0.0;
 texcoord.y = (id == 1) ? 2.0 : 0.0;
 position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

technique Depth_Tool
{
     pass StoreColorDepth
   {
     VertexShader = PostProcessVS;
     PixelShader = S_CD;
     RenderTarget0 = SC;
     RenderTarget1 = SD;
 }
     pass DepthMap
   {
     VertexShader = PostProcessVS;
     PixelShader = Out;
   }
     pass Past
   {
     VertexShader = PostProcessVS;
     PixelShader = S_BB;
     RenderTarget0 = SPC;
     RenderTarget1 = SPD;
   }
}
