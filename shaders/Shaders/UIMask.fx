/*
    Simple UIMask shader by luluco250

    I have no idea why this was never ported back to ReShade 3.0 from 2.0,
    but if you missed it, here it is.

    It doesn't feature the auto mask from the original shader.
    
    It does feature a new multi-channnel masking feature. UI masks can now contain
    separate 'modes' within each of the three color channels.
    
    For example, you can have the regular hud on the red channel (the default one),
    a mask for an inventory screen on the green channel and a mask for a quest menu
    on the blue channel. You can then use keyboard keys to toggle each channel on or off.
    
    Multiple channels can be active at once, they'll just add up to mask the image.
    
    Simple/legacy masks are not affected by this, they'll work just as you'd expect,
    so you can still make simple black and white masks that use all color channels, it'll
    be no different than just having it on a single channel.

    Tips:
    
    --You can adjust how much it will affect your HUD by changing "Mask Intensity".
    
    --You don't actually need to place the UIMask_Bottom technique at the bottom of
      your shader pipeline, if you have any effects that don't necessarily affect 
      the visibility of the HUD you can place it before that.
      For instance, if you use color correction shaders like LUT, you might want
      to place UIMask_Bottom just before that.

    --Preprocessor flags:
      --UIMASK_MULTICHANNEL:
        Enables having up to three different masks on each color channel.
      --UIMASK_TOGGLEKEY_RED:
        Keycode that toggles the red channel of the mask.
      --UIMASK_TOGGLEKEY_BLUE:
        Keycode that toggles the blue channel of the mask.
      --UIMASK_TOGGLEKEY_GREEN:
        Keycode that toggles the green channel of the mask.
      
    --Refer to this page for keycodes: 
      https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx

    --To make a custom mask:

      1-Take a screenshot of your game with the HUD enabled,
       preferrably with any effects disabled for maximum visibility.

      2-Open the screenshot with your preferred image editor program, I use GIMP.

      3-Make a background white layer if there isn't one already.
        Be sure to leave it behind your actual screenshot for the while.

      4-Make an empty layer for the mask itself, you can call it "mask".

      5-Having selected the mask layer, paint the places where HUD constantly is,
        such as health bars, important messages, minimaps etc.
      
      6-Delete or make your screenshot layer invisible.

      7-Before saving your mask, let's do some gaussian blurring to improve it's look and feel:
        For every step of blurring you want to do, make a new layer, such as:
        Mask - Blur16x16
        Mask - Blur8x8
        Mask - Blur4x4
        Mask - Blur2x2
        Mask - NoBlur
        You should use your image editor's default gaussian blurring filter, if there is one.
        This avoids possible artifacts and makes the mask blend more easily on the eyes.
        You may not need this if your mask is accurate enough and/or the HUD is simple enough.

      8-Now save the final image as "UIMask.png" in your textures folder and you're done!

    
    MIT Licensed:

    Copyright (c) 2017 Lucas Melo

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "ReShade.fxh"

#ifndef UIMASK_MULTICHANNEL
    #define UIMASK_MULTICHANNEL 0
#endif

#ifndef UIMASK_TOGGLEKEY_RED
    #define UIMASK_TOGGLEKEY_RED 0x67 //Numpad 7
#endif

#ifndef UIMASK_TOGGLEKEY_GREEN
    #define UIMASK_TOGGLEKEY_GREEN 0x68 //Numpad 8
#endif

#ifndef UIMASK_TOGGLEKEY_BLUE
    #define UIMASK_TOGGLEKEY_BLUE 0x69 //Numpad 9
#endif

#if !UIMASK_MULTICHANNEL
    #define TEXFORMAT R8
#else
    #define TEXFORMAT RGBA8
#endif

#include "ReShadeUI.fxh"

uniform float fMask_Intensity < __UNIFORM_SLIDER_FLOAT1
    ui_label = "Mask Intensity";
    ui_tooltip = "How much to mask effects to the original image.";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 1.0;

uniform bool bDisplayMask <
    ui_label = "Display Mask";
    ui_tooltip = 
        "Display the mask texture.\n"
        "Useful for testing multiple channels or simply the mask itself.";
> = false;

#if UIMASK_MULTICHANNEL
uniform bool toggle_red <source="key"; keycode=UIMASK_TOGGLEKEY_RED; toggle=true;>;
uniform bool toggle_green <source="key"; keycode=UIMASK_TOGGLEKEY_GREEN; toggle=true;>;
uniform bool toggle_blue <source="key"; keycode=UIMASK_TOGGLEKEY_BLUE; toggle=true;>;
#endif

texture tUIMask_Backup { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
texture tUIMask_Mask <source="UIMask.png";> { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=TEXFORMAT; };

sampler sUIMask_Mask { Texture = tUIMask_Mask; };
sampler sUIMask_Backup { Texture = tUIMask_Backup; };

float4 PS_Backup(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target {
    return tex2D(ReShade::BackBuffer, uv);
}

float4 PS_ApplyMask(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target {
    float3 col = tex2D(ReShade::BackBuffer, uv).rgb;
    float3 bak = tex2D(sUIMask_Backup, uv).rgb;
    
    #if !UIMASK_MULTICHANNEL
    float mask = tex2D(sUIMask_Mask, uv).r;
    #else
    float3 mask_rgb = tex2D(sUIMask_Mask, uv).rgb;
    //This just works, it basically adds masking with each channel that has been toggled.
    //'toggle_red' is inverted so it defaults to 'true' upon start.
    float mask = saturate(1.0 - dot(1.0 - mask_rgb, float3(!toggle_red, toggle_green, toggle_blue)));
    #endif

    mask = lerp(1.0, mask, fMask_Intensity);
    col = lerp(bak, col, mask);
    col = bDisplayMask ? mask : col;
    
    return float4(col, 1.0);
}

technique UIMask_Top {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_Backup;
        RenderTarget = tUIMask_Backup;
    }
}

technique UIMask_Bottom {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_ApplyMask;
    }
}
