/*
	UI Mask Creator
	By: Lord Of Lunacy
	
	This shader makes it possible to quickly create a UI mask ingame without making use
	of a photo editor by directly drawing over the UI, so that it can then be saved with
	a screenshot.
*/


#include "ReShade.fxh"

uniform int Radius<
	ui_label = "Brush Radius";
	ui_min = 0; ui_max = 256;
	ui_tooltip = "Hold down spacebar while adjusting so that you\n"
				"don't draw behind the overlay on accident.";
	ui_type = "slider";
> = 16;

uniform bool SquareCursor <
	ui_label = "Square Cursor";
	ui_tooltip = "Hold down spacebar while adjusting so that you\n"
				"don't draw behind the overlay on accident.";
> = 0;

uniform bool ReadyForScreenshot <
	ui_label = "Ready For Screenshot";
	ui_tooltip = "This turns the parts of the screen not covered by \n"
				"the mask white, and disables the cursor in preparation \n"
				"for a screenshot. \n \n"
				"Hold down spacebar while adjusting so that you\n"
				"don't draw behind the overlay on accident.";
> = 0;

uniform float2 MouseCoord < source = "mousepoint";>;
uniform float2 MouseDelta < source = "mousedelta";>;

uniform bool LeftMouseButtonDown <source = "mousebutton"; keycode = 0;>;
uniform bool LeftMouseButtonPressed <source = "mousebutton"; keycode = 0; mode = "press";>;

uniform bool RightMouseButtonDown <source = "mousebutton"; keycode = 1;>;
uniform bool RightMouseButtonPressed <source = "mousebutton"; keycode = 1; mode = "press";>;

uniform bool SpaceBarDown <source = "key"; keycode = 0x20;>;
uniform bool CtrlDown <source = "key"; keycode = 0x11;>;
uniform bool Reset <source = "key"; keycode = 0x52;>;



texture MaskLayer {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
sampler sMaskLayer {Texture = MaskLayer;};


bool WithinRadius(float2 texcoord)
{
	float2 coordinates = texcoord * BUFFER_SCREEN_SIZE;
	float2 coordinateDelta = abs(coordinates - MouseCoord);
	int distanceFromCursor;
	if (SquareCursor)
	{
		distanceFromCursor = max(coordinateDelta.x, coordinateDelta.y);
	}
	else
	{
		distanceFromCursor = sqrt(dot(1, coordinateDelta * coordinateDelta));
	}
	
	return distanceFromCursor <= Radius;
}

void CursorPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float4 output : SV_Target)
{
	if(ReadyForScreenshot) discard;
	output = tex2D(ReShade::BackBuffer, texcoord);
	output *=  1 - (tex2D(sMaskLayer, texcoord).rrrr);
	if(WithinRadius(texcoord)) output = lerp(output, 1, 0.333);
}

void WriteToMaskLayerPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float output : SV_Target)
{
	if(SpaceBarDown || CtrlDown || ReadyForScreenshot) discard;
	else if (Reset) output = 0;
	else if(WithinRadius(texcoord))
	{
		if(LeftMouseButtonDown) output = 1;
		else if(RightMouseButtonDown) output = 0;
		else discard;
	}
	else discard;
}

void OutputMaskLayerPS(float4 pos : SV_Position, float2 texcoord : Texcoord, out float4 output : SV_Target)
{
	if(ReadyForScreenshot)
	{
		output = 1 - tex2D(sMaskLayer, texcoord).rrrr;
	}
	else discard;
}


technique UIMaskCreator<
	ui_tooltip = "Press R to reset the mask layer, left click to mask, right click to erase, \n"
				"and use spacebar when changing settings to prevent it from writing behind the \n"
				"overlay.\n\n"
				"You can use a blur shader such as GaussianBlur.fx to soften HUD edges once you are\n"
				"ready to screenshot the mask.";>
				
{
	pass Cursor
	{
		VertexShader = PostProcessVS;
		PixelShader = CursorPS;
	}
	pass WriteToMaskLayer
	{
		VertexShader = PostProcessVS;
		PixelShader = WriteToMaskLayerPS;
		RenderTarget = MaskLayer;
	}
	pass OutputMaskLayerPS
	{
		VertexShader = PostProcessVS;
		PixelShader = OutputMaskLayerPS;
	}
}
		