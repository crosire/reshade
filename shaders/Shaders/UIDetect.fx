//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ReShade effect file
// UIDetect by brussell
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/*
This shader can be used to toggle effects depending on the visibility
of UI elements. Unlike UIMask, that uses a mask to decide which area of
the screen should be effect-free, this one automatically turns off effects
for the whole screen. It's useful for games, where one wants to use post-
processing like DOF, CA or AO, which however shouldn't be active when
certain UI elements are displayed (e.g. inventory, map, dialoque boxes,
options menu etc.). Each UI is characterized by a number of user defined
pixels, while the workflow to get their values could be like this:

-take a screenshot without any effects when the UI is visible
-open the screenshot in an image processing tool
-look for a static and opaque area in the UI layer that is usually
 out of reach for user actions like the mouse cursor, tooltips etc.
 (usually somewhere in a corner of the screen)
-use a color picker tool and choose two, three or more pixels,
 which are near to each other but differ greatly in color and brightness,
 and note the pixels coordinates and RGB values (thus choose pixels that
 do not likely occur in non-UI game situations, so that effects couldn't
 get toggled accidently when there is no UI visible)

After that, write the pixel coordinates and RGB values to UIDetect.fxh
(see further description there)

Effect ordering:
-UIDetect	                        //must be first in pipeline (needs unaltered backbuffer)
...	effects that affect UIs
-UIDetect_Before	                //place before effects that shouldn't affect UI
...	effects that should not affect UIs
-UIDetect_After	                    //place after effects that shouldn't affect UI
...	effects that affect UIs

Drawbacks:
-does not work for transparent or nonstatic UIs obviously
-pixels are only valid for one game resolution
-pixel values can change with different hardware anti aliasing settings
*/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "ReShade.fxh"

//textures and samplers
texture texColor_Orig { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
sampler Color_Orig { Texture = texColor_Orig; };

texture texUIDetect { Width = 1; Height = 1; Format = R8; };
sampler UIDetect { Texture = texUIDetect; };

//pixel shaders
float PS_UIDetect(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {
    #include "UIDetect.fxh"

    float diff;
    float ui_detected = 0;
    float uilayer = 1;
    float nextuilayer = 0;

    for (int i=0; i < PIXELNUMBER; i++)
    {
        [branch]
        if (UIPixelCoord[i].z - uilayer == 0){
            if (nextuilayer == 0){
                diff = pow(dot(tex2Dlod(ReShade::BackBuffer, float4(UIPixelCoord[i].xy * ReShade::PixelSize.xy, 0, 0)).xyz - UIPixelRGB[i].xyz / 255.0, 0.333), 2);
                if (diff < 0.00001) {
                    ui_detected = 1;
                }else{
                    ui_detected = 0;
                    nextuilayer = 1;
                }
            }
        }else{
            if (ui_detected == 1){ return ui_detected; }
            uilayer += 1;
            nextuilayer = 0;
            i -= 1;
        }
    }
    return ui_detected;
}

float4 PS_StoreColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {
    return  tex2D(ReShade::BackBuffer, texcoord);
}

float4 PS_RestoreColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {
    float4 color = tex2D(UIDetect, float2(0,0)).x == 1.0 ? tex2D(Color_Orig, texcoord) : tex2D(ReShade::BackBuffer, texcoord);
    return color;
}

//techniques
technique UIDetect {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_UIDetect;
        RenderTarget = texUIDetect;
    }
}

technique UIDetect_Before {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_StoreColor;
        RenderTarget = texColor_Orig;
    }
}

technique UIDetect_After {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_RestoreColor;
    }
}
