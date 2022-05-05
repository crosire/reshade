/*
    Description : PD80 04 Magical Rectangle for Reshade https://reshade.me/
    Author      : prod80 (Bas Veth)
    License     : MIT, Copyright (c) 2020 prod80


    MIT License

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
#include "ReShadeUI.fxh"
#include "PD80_00_Blend_Modes.fxh"
#include "PD80_00_Color_Spaces.fxh"

namespace pd80_depthslicer
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform float depth_near <
        ui_type = "slider";
        ui_label = "Depth Near Plane";
        ui_tooltip = "Depth Near Plane";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float depthpos <
        ui_type = "slider";
        ui_label = "Depth Position";
        ui_tooltip = "Depth Position";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.015;
    uniform float depth_far <
        ui_type = "slider";
        ui_label = "Depth Far Plane";
        ui_tooltip = "Depth Far Plane";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float depth_smoothing <
        ui_type = "slider";
        ui_label = "Depth Smoothing";
        ui_tooltip = "Depth Smoothing";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.005;
    uniform float intensity <
        ui_type = "slider";
        ui_label = "Lightness";
        ui_tooltip = "Lightness";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float hue <
        ui_type = "slider";
        ui_label = "Hue";
        ui_tooltip = "Hue";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.083;
    uniform float saturation <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Saturation";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform int blendmode_1 < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Blendmode";
        ui_category = "Depth Slicer";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity <
        ui_type = "slider";
        ui_label = "Opacity";
        ui_tooltip = "Opacity";
        ui_category = "Depth Slicer";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 1.0;
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_DepthSlice(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        float depth       = ReShade::GetLinearizedDepth( texcoord ).x;

        float depth_np    = depthpos - depth_near;
        float depth_fp    = depthpos + depth_far;

        float dn          = smoothstep( depth_np - depth_smoothing, depth_np, depth );
        float df          = 1.0f - smoothstep( depth_fp, depth_fp + depth_smoothing, depth );
        
        float colorize    = 1.0f - ( dn * df );
        float a           = colorize;
        colorize          *= intensity;
        float3 b          = HSVToRGB( float3( hue, saturation, colorize ));
        color.xyz         = blendmode( color.xyz, b.xyz, blendmode_1, opacity * a );

        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_06_Depth_Slicer
    {
        pass prod80_pass0
        {
            VertexShader  = PostProcessVS;
            PixelShader   = PS_DepthSlice;
        }
    }
}


