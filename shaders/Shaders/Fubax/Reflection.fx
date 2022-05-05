/*
Reflection PS (c) 2019 Jacob Maximilian Fober

This work is licensed under the Creative Commons 
Attribution-NonCommercial-NoDerivatives 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-nc-nd/4.0/ 

For inquiries please contact jakub.m.fober@pm.me
*/

/*
This shader maps reflection vector of a normal surface
from a camera onto reflection texture in equisolid
360 degrees projection (mirror ball)

If you want to create reflection texture from
equirectangular 360 panorama, visit following:
https://github.com/Fubaxiusz/shadron-shaders/blob/master/Shaders/Equirect2Equisolid.shadron

It's a shader script for Shadron image-editing software,
avaiable here:
https://www.arteryengine.com/shadron/
*/

/*
Depth Map sampler is from ReShade.fxh by Crosire.
Normal Map generator is from DisplayDepth.fx by CeeJay.
Soft light blending mode is from pegtop.net
*/

// version 1.2.1


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

#ifndef ReflectionRes
	#define ReflectionRes 512
#endif
#ifndef ReflectionImage
	#define ReflectionImage "matcap.png"
#endif

uniform int FOV < __UNIFORM_SLIDER_INT1
	ui_label = "Field of View (horizontal)";
	ui_min = 1; ui_max = 170;
	ui_category = "Depth settings";
> = 90;

uniform float FarPlane <
	ui_label = "Far Plane adjustment";
	ui_tooltip = "Adjust Normal Map strength";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1000.0; ui_step = 0.2;
	ui_category = "Depth settings";
> = 1000.0;

uniform bool Skip4Background <
	ui_label = "Show background";
	ui_tooltip = "Mask reflection for:\nDEPTH = 1.0";
	ui_category = "Depth settings";
> = true;

uniform int BlendingMode <
	ui_label = "Mix mode";
	ui_type = "combo";
	ui_items = "Normal mode\0Multiply\0Screen\0Overlay\0Soft Light\0";
	ui_category = "Texture settings";
> = 0;

uniform float BlendingAmount < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Blending";
	ui_tooltip = "Blend with background";
	ui_min = 0.0; ui_max = 1.0;
	ui_category = "Texture settings";
> = 1.0;

uniform bool AlphaBlending <
	ui_label = "Use alpha transparency";
	ui_tooltip = "Use texture transparency channel (Alpha)";
	ui_category = "Texture settings";
> = false;

#include "ReShade.fxh"

texture ReflectionTex < source = ReflectionImage; > {Width = ReflectionRes; Height = ReflectionRes;};
sampler ReflectionSampler
{
	Texture = ReflectionTex;
	AddressU = BORDER;
	AddressV = BORDER;
	Format = RGBA8;
};


	  //////////////////////
	 /// BLENDING MODES ///
	//////////////////////

float3 Multiply(float3 A, float3 B){ return A*B; }

float3 Screen(float3 A, float3 B){ return A+B-A*B; } // By JMF

float3 Overlay(float3 A, float3 B)
{
	// Algorithm by JMF
	float3 HL[4] = { max(A, 0.5), max(B, 0.5), min(A, 0.5), min(B, 0.5) };
	return ( HL[0] + HL[1] - HL[0]*HL[1] + HL[2]*HL[3] )*2.0 - 1.5;
}

float3 SoftLight(float3 A, float3 B)
{
	float3 A2 = pow(A, 2.0);
	return 2.0*A*B+A2-2.0*A2*B;
}


	  //////////////
	 /// SHADER ///
	//////////////

// Get depth function from ReShade.fxh with custom Far Plane
float GetDepth(float2 TexCoord)
{
	#if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
		TexCoord.y = 1.0 - TexCoord.y;
	#endif
	float Depth = tex2Dlod( ReShade::DepthBuffer, float4(TexCoord, 0, 0) ).x;
	
	#if RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
		const float C = 0.01;
		Depth = (exp(Depth * log(C + 1.0)) - 1.0) / C;
	#endif
	#if RESHADE_DEPTH_INPUT_IS_REVERSED
		Depth = 1.0 - Depth;
	#endif

	const float N = 1.0;
	Depth /= FarPlane - Depth * (FarPlane - N);

	return Depth;
}

// Normal map (OpenGL oriented) generator from DisplayDepth.fx
float3 NormalVector(float2 texcoord)
{
	float3 offset = float3(ReShade::PixelSize.xy, 0.0);
	float2 posCenter = texcoord.xy;
	float2 posNorth  = posCenter - offset.zy;
	float2 posEast   = posCenter + offset.xz;

	float3 vertCenter = float3(posCenter - 0.5, 1.0) * GetDepth(posCenter);
	float3 vertNorth  = float3(posNorth - 0.5,  1.0) * GetDepth(posNorth);
	float3 vertEast   = float3(posEast - 0.5,   1.0) * GetDepth(posEast);

	return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5;
}

// Sample Matcap texture
float4 GetReflection(float2 TexCoord)
{
	// Get aspect ratio
	float Aspect = ReShade::AspectRatio;

	// Sample normal pass
	float3 Normal = NormalVector(TexCoord);
	Normal.xy = Normal.xy * 2.0 - 1.0;

	// Get ray vector from camera to the visible geometry
	float3 CameraRay;
	CameraRay.xy = TexCoord * 2.0 - 1.0;
	CameraRay.y /= Aspect; // Correct aspect ratio
	CameraRay.z = 1.0 / tan(radians(FOV*0.5)); // Scale frustum Z position from FOV

	// Get reflection vector from camera to geometry surface
	float3 OnSphereCoord = normalize( reflect(CameraRay, Normal) );

	// Convert cartesian coordinates to equisolid polar
	float2 EquisolidPolar, EquisolidCoord;
	EquisolidPolar.x = length( OnSphereCoord + float3(0.0, 0.0, 1.0) ) * 0.5;
	EquisolidPolar.y = atan2(OnSphereCoord.y, OnSphereCoord.x);
	// Convert polar to UV coordinates
	EquisolidCoord.x = EquisolidPolar.x * cos(EquisolidPolar.y);
	EquisolidCoord.y = EquisolidPolar.x * sin(EquisolidPolar.y);
	EquisolidCoord = EquisolidCoord * 0.5 + 0.5;

	// Sample matcap texture
	float4 ReflectionTexture = tex2D(ReflectionSampler, EquisolidCoord);

	return ReflectionTexture;
}

float3 ReflectionPS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	if(Skip4Background) if( GetDepth(texcoord)==1.0 ) return tex2D(ReShade::BackBuffer, texcoord).rgb;
	if( !bool(BlendingMode) && !AlphaBlending && BlendingAmount == 1.0) return GetReflection(texcoord).rgb;

	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float4 Reflection = GetReflection(texcoord);

	switch(BlendingMode)
	{
		case 1:{ Reflection.rgb = Multiply(Reflection.rgb, Display); break; } // Multiply
		case 2:{ Reflection.rgb = Screen(Reflection.rgb, Display); break; } // Screen
		case 3:{ Reflection.rgb = Overlay(Reflection.rgb, Display); break; } // Overlay
		case 4:{ Reflection.rgb = SoftLight(Reflection.rgb, Display); break; } // Soft Light
	}

	if( !AlphaBlending && BlendingAmount == 1.0) return Reflection.rgb;
	return lerp(Display, Reflection.rgb, Reflection.a * BlendingAmount);
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique Reflection < ui_label = "Reflection (matcap)";
ui_tooltip = "Apply reflection texture to a geometry...\n"
	"To change used texture, define in global preprocessor definitions:\n\n"
	"   ReflectionImage 'YourImage.png'\n\n"
	"To change texture resolution, define\n\n"
	"   ReflectionRes 32"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ReflectionPS;
	}
}
