/**
Checkerboard Interlace PS, version 1.0.0
All rights (c) 2021 Jakub Maksymilian Fober (the Author)

Licensed under the Creative Commons CC BY-SA 3.0,
license available online at http://creativecommons.org/licenses/by-sa/3.0/
*/

#include "ReShade.fxh"
#include "ReShadeUI.fxh"


// Menu items

uniform bool HighFPS < __UNIFORM_INPUT_BOOL1
	ui_label = "Enable high FPS mode";
	ui_tooltip =
		"Interpolates 4-frames instead of two."
		"\nUse only when game runs at 120 FPS or more";
> = false;

uniform uint CheckerboardPattern < __UNIFORM_COMBO_INT1
	ui_spacing = 1u;
	ui_category = "High FPS mode settings";
	ui_category_closed = true;
	ui_label = "Pattern shape";
	ui_tooltip =
		"Works only in high FPS mode!"
		"\n\nInterlace pattern shape type";
	ui_items = "Z\0N'\0U\0D\0A\0C\0";
> = 0;

uniform bool ReversePattern < __UNIFORM_INPUT_BOOL1
	ui_category = "High FPS mode settings";
	ui_label = "Reverse sampling order";
	ui_tooltip = "Reverse pattern winding";
> = false;

uniform bool DisplayPattern < __UNIFORM_INPUT_BOOL1
	ui_category = "Debug settings";
	ui_category_closed = true;
	ui_label = "Display pattern";
	ui_tooltip = "Display pattern";
> = false;

uniform uint PatternScale < __UNIFORM_SLIDER_INT1
	ui_category = "Debug settings";
	ui_label = "Pattern size (preview)";
	ui_tooltip = "Decrease preview resolution for debugging";
	ui_min = 1u; ui_max = 32u;
> = 8u;

// System variable
uniform uint FRAME_INDEX < source = "framecount"; >;


// Textures

// Full-screen texture with alpha as interpolation target
texture interlaceTex < pooled = false; >
{
	Width  = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA8;
};

// Full-screen texture sampler without interpolation
sampler interlaceSampler
{
	Texture = interlaceTex;
	MinFilter = POINT;
	MagFilter = POINT;
};


// Constants

// Left-right, top-bottom
static const float4x4 PixelMask[6] =
{
	// Z pattern
	float4x4(
		float4(
			1f, 0f,
			0f, 0f), // Frame 0
		float4(
			0f, 1f,
			0f, 0f), // Frame 1
		float4(
			0f, 0f,
			1f, 0f), // Frame 2
		float4(
			0f, 0f,
			0f, 1f)  // Frame 3
	),
	// N' pattern
	float4x4(
		float4(
			1f, 0f,
			0f, 0f), // Frame 0
		float4(
			0f, 0f,
			1f, 0f), // Frame 1
		float4(
			0f, 1f,
			0f, 0f), // Frame 2
		float4(
			0f, 0f,
			0f, 1f)  // Frame 3
	),
	// U pattern
	float4x4(
		float4(
			1f, 0f,
			0f, 0f), // Frame 0
		float4(
			0f, 0f,
			1f, 0f), // Frame 1
		float4(
			0f, 0f,
			0f, 1f), // Frame 2
		float4(
			0f, 1f,
			0f, 0f)  // Frame 3
	),
	// D pattern
	float4x4(
		float4(
			1f, 0f,
			0f, 0f), // Frame 0
		float4(
			0f, 1f,
			0f, 0f), // Frame 1
		float4(
			0f, 0f,
			0f, 1f), // Frame 2
		float4(
			0f, 0f,
			1f, 0f)  // Frame 3
	),
	// A pattern
	float4x4(
		float4(
			0f, 0f,
			1f, 0f), // Frame 0
		float4(
			1f, 0f,
			0f, 0f), // Frame 1
		float4(
			0f, 1f,
			0f, 0f), // Frame 2
		float4(
			0f, 0f,
			0f, 1f)  // Frame 3
	),
	// C pattern
	float4x4(
		float4(
			0f, 1f,
			0f, 0f), // Frame 0
		float4(
			1f, 0f,
			0f, 0f), // Frame 1
		float4(
			0f, 0f,
			1f, 0f), // Frame 2
		float4(
			0f, 0f,
			0f, 1f)  // Frame 3
	)
	// ...more
};

// Limited pattern for 2-frame interpolation
static const float2x4 altPixelMask = float2x4(
	float4(
		1f, 0f,
		0f, 1f),
	float4(
		0f, 1f,
		1f, 0f)
);


// Shaders

// Render full-screen triangle without texture coordinates
void FullscreenTriangleVS(uint vertexID : SV_VertexID, out float4 position : SV_Position)
{
	// Initialize values
	position.z = 0f; // Not used
	position.w = 1f; // Not used

	// Constant vertex positions
	static const float2 positionList[3] =
	{
		float2(-1f, 1f),
		float2( 3f, 1f),
		float2(-1f,-3f)
	};

	// Read the position for each vertex of a triangle
	position.xy = positionList[vertexID];
}

// Save pixels in interlaced checkerboard pattern
void CheckerboardSavePS(float4 pos : SV_Position, out float4 color : SV_Target)
{
	// Get pixel position index
	uint2 texelCoord = uint2(pos.xy);
	// Sample background texture
	if (DisplayPattern)
	{
		static const float4x3 pixelColors = float4x3(
			float3(1f, 0f, 0f), // Red
			float3(0f, 1f, 0f), // Green
			float3(0f, 0f, 1f), // Blue
			float3(1f, 1f, 1f)  // White
		)*(235f/255f); // Broadcast safe range for beauty
		// Set different color for each frame
		color.rgb = pixelColors[HighFPS? FRAME_INDEX%4u : FRAME_INDEX%2u];
		// Decrease resolution
		texelCoord /= PatternScale;
	}
	else color.rgb = tex2Dfetch(ReShade::BackBuffer, texelCoord).rgb;
	// Save alpha pattern
	if (HighFPS)
		color.a = PixelMask
			[CheckerboardPattern] // Pattern index
			[ReversePattern? 3u-FRAME_INDEX%4u : FRAME_INDEX%4u] // Frame index
			[(texelCoord.y%2u)*2u+(texelCoord.x%2u)] // Pixel 2x2 index
		;
	else
		color.a = altPixelMask
			[FRAME_INDEX%2u] // Frame index
			[(texelCoord.y%2u)*2u+(texelCoord.x%2u)] // Pixel 2x2 index
		;
}

// Display interlaced texture
void CheckerboardDisplayPS(float4 pos : SV_Position, out float3 color : SV_Target)
{
	// Sample background texture
	color = tex2Dfetch(interlaceSampler, uint2(pos.xy)).rgb;
}


// Output

technique CheckerboardInterlace <
	ui_label = "Checkerboard interlacing";
	ui_tooltip =
		"Adds checkerboard 2 or 4-frame motion-blur"
		"\n\nUnder the Creative Commons CC BY-SA 3.0 license"
		"\n(c) 2021 Jakub Maksymilian Fober";
>
{
	pass SaveFrame
	{
		BlendEnable = true;
		SrcBlend = SRCALPHA;     // Foreground
		DestBlend = INVSRCALPHA; // Background

		RenderTarget = interlaceTex;

		VertexShader = FullscreenTriangleVS;
		PixelShader = CheckerboardSavePS;
	}
	pass DisplayFinal
	{
		VertexShader = FullscreenTriangleVS;
		PixelShader = CheckerboardDisplayPS;
	}
}