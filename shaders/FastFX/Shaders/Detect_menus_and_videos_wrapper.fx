/*------------------.
| :: Description :: |
'-------------------/

	Detect_menus_and_videos_wrapper.fx

	Author: Robert Jessop
	License: MIT

	About:
	Wraps any set of shaders with menu/video detection. Output of the wrapped shaders will be thrown away and replaced with original image if depth is 0. Requires depth buffer. 
	
	You need to enable both parts. Put "Before" first in the list, then your shaders, then "After".
	
	If it doesn't work then make sure your depth buffer is setup correctly (using DisplayDepth) and check if your game actually has non-zero depth during gameplay and zero depth buffer during menus/videos.
    
    	Based partly on https://github.com/CeeJayDK/SweetFX/blob/master/Shaders/Splitscreen.fx by CeeJay.dk (MIT licence)
	
	Ideas for future improvement:
    *

	History:
	(*) Feature (+) Improvement (x) Bugfix (-) Information (!) Compatibility
	
	Version 1.0 - this.
    
*/


/*---------------.
| :: Includes :: |
'---------------*/

#include "ReShade.fxh"

uniform bool detect_sky <
    ui_category = "Detect_menus_and_videos_wrapper";
    ui_label = "Detect sky too";
    ui_tooltip = "Will also show original image where depth==1 (i.e. don't apply shaders to sky.)";
    ui_type = "radio";
> = false;

namespace Detect_menus_and_videos_wrapper {

/*-------------------------.
| :: Texture and sampler:: |
'-------------------------*/

texture Before { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler Before_sampler { Texture = Before; };


/*-------------.
| :: Effect :: |
'-------------*/

float4 PS_Before(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    return tex2D(ReShade::BackBuffer, texcoord);
}

float4 PS_After(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    float4 color; 
	
	float depth = ReShade::GetLinearizedDepth(texcoord);

    if(depth==0 || (detect_sky && depth==1)) color = tex2D(Before_sampler, texcoord);
	else color = tex2D(ReShade::BackBuffer, texcoord);

    return color;
}


/*-----------------.
| :: Techniques :: |
'-----------------*/

technique Before <
	ui_tooltip = "Wraps any set of shaders with menu/video detection. Output of the wrapped shaders will be thrown away and replaced with original image if depth is 0. Requires depth buffer.\n\nPut \"Before\" first, then your shaders, then \"After\".\n\nIf it doesn't work then make sure your depth buffer is setup correctly (using DisplayDepth) and check if your game actually has non-zero depth during gameplay and zero depth buffer during menus/videos.";
	>
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_Before;
        RenderTarget = Before;
    }
}

technique After <
	ui_tooltip = "Wraps any set of shaders with menu/video detection. Output of the wrapped shaders will be thrown away and replaced with original image if depth is 0. Requires depth buffer.\n\nPut \"Before\" first, then your shaders, then \"After\".\n\nIf it doesn't work then make sure your depth buffer is setup correctly (using DisplayDepth) and check if your game actually has non-zero depth during gameplay and zero depth buffer during menus/videos.";
	>
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_After;
    }
}


} //namespace end
