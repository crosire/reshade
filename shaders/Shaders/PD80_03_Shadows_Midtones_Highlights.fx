/*
    Description : PD80 03 Shadows Midtones Highlights for Reshade https://reshade.me/
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
#include "PD80_00_Noise_Samplers.fxh"
#include "PD80_00_Blend_Modes.fxh"
#include "PD80_00_Base_Effects.fxh"

namespace pd80_SMH
{
    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int luma_mode < __UNIFORM_COMBO_INT1
        ui_label = "Luma Mode";
        ui_tooltip = "Luma Mode";
        ui_category = "Global";
        ui_items = "Use Average\0Use Perceived Luma\0Use Max Value\0";
        > = 2;
    uniform int separation_mode < __UNIFORM_COMBO_INT1
        ui_label = "Luma Separation Mode";
        ui_tooltip = "Luma Separation Mode";
        ui_category = "Global";
        ui_items = "Harsh Separation\0Smooth Separation\0";
        > = 0;
    uniform bool enable_dither <
        ui_label = "Enable Dithering";
        ui_tooltip = "Enable Dithering";
        ui_category = "Global";
        > = true;
    uniform float dither_strength <
        ui_type = "slider";
        ui_label = "Dither Strength";
        ui_tooltip = "Dither Strength";
        ui_category = "Global";
        ui_min = 0.0f;
        ui_max = 10.0f;
        > = 2.0;
    uniform float exposure_s <
        ui_label = "Exposure";
        ui_tooltip = "Shadow Exposure";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float contrast_s <
        ui_label = "Contrast";
        ui_tooltip = "Shadow Contrast";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float brightness_s <
        ui_label = "Brightness";
        ui_tooltip = "Shadow Brightness";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float3 blendcolor_s <
        ui_type = "color";
        ui_label = "Color";
        ui_tooltip = "Shadow Color";
        ui_category = "Shadow Adjustments";
        > = float3( 0.0,  0.365, 1.0 );
    uniform int blendmode_s < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Shadow Blendmode";
        ui_category = "Shadow Adjustments";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity_s <
        ui_label = "Opacity";
        ui_tooltip = "Shadow Opacity";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float tint_s <
        ui_label = "Tint";
        ui_tooltip = "Shadow Tint";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float saturation_s <
        ui_label = "Saturation";
        ui_tooltip = "Shadow Saturation";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float vibrance_s <
        ui_label = "Vibrance";
        ui_tooltip = "Shadow Vibrance";
        ui_category = "Shadow Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float exposure_m <
        ui_label = "Exposure";
        ui_tooltip = "Midtone Exposure";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float contrast_m <
        ui_label = "Contrast";
        ui_tooltip = "Midtone Contrast";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float brightness_m <
        ui_label = "Brightness";
        ui_tooltip = "Midtone Brightness";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float3 blendcolor_m <
        ui_type = "color";
        ui_label = "Color";
        ui_tooltip = "Midtone Color";
        ui_category = "Midtone Adjustments";
        > = float3( 0.98, 0.588, 0.0 );
    uniform int blendmode_m < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Midtone Blendmode";
        ui_category = "Midtone Adjustments";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity_m <
        ui_label = "Opacity";
        ui_tooltip = "Midtone Opacity";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float tint_m <
        ui_label = "Tint";
        ui_tooltip = "Midtone Tint";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float saturation_m <
        ui_label = "Saturation";
        ui_tooltip = "Midtone Saturation";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float vibrance_m <
        ui_label = "Vibrance";
        ui_tooltip = "Midtone Vibrance";
        ui_category = "Midtone Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float exposure_h <
        ui_label = "Exposure";
        ui_tooltip = "Highlight Exposure";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float contrast_h <
        ui_label = "Contrast";
        ui_tooltip = "Highlight Contrast";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float brightness_h <
        ui_label = "Brightness";
        ui_tooltip = "Highlight Brightness";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float3 blendcolor_h <
        ui_type = "color";
        ui_label = "Color";
        ui_tooltip = "Highlight Color";
        ui_category = "Highlight Adjustments";
        > = float3( 1.0, 1.0, 1.0 );
    uniform int blendmode_h < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Highlight Blendmode";
        ui_category = "Highlight Adjustments";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity_h <
        ui_label = "Opacity";
        ui_tooltip = "Highlight Opacity";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float tint_h <
        ui_label = "Tint";
        ui_tooltip = "Highlight Tint";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float saturation_h <
        ui_label = "Saturation";
        ui_tooltip = "Highlight Saturation";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float vibrance_h <
        ui_label = "Vibrance";
        ui_tooltip = "Highlight Vibrance";
        ui_category = "Highlight Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    uniform float2 pingpong < source = "pingpong"; min = 0; max = 128; step = 1; >;

    float getLuminance( in float3 x )
    {
        return dot( x, float3( 0.212656, 0.715158, 0.072186 ));
    }
    
    float curve( float x )
    {
        return x * x * x * ( x * ( x * 6.0f - 15.0f ) + 10.0f );
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_SMH(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        color.xyz         = saturate( color.xyz );

        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise      = dither( samplerRGBNoise, texcoord.xy, 3, enable_dither, dither_strength, 1, 0.5f );
        color.xyz          = saturate( color.xyz + dnoise.xyz );

        float pLuma       = 0.0f;
        switch( luma_mode )
        {
            case 0: // Use average
            {
                pLuma     = dot( color.xyz, float3( 0.333333f, 0.333334f, 0.333333f ));
            }
            break;
            case 1: // Use perceived luma
            {
                pLuma     = getLuminance( color.xyz );
            }
            break;
            case 2: // Use max
            {
                pLuma     = max( max( color.x, color.y ), color.z );
            }
            break;
        }
        
        float weight_s; float weight_h; float weight_m;

        switch( separation_mode )
        {
            /*
            Clear cutoff between shadows and highlights
            Maximizes precision at the loss of harsher transitions between contrasts
            Curves look like:

            Shadows                Highlights             Midtones
            ‾‾‾—_   	                         _—‾‾‾         _——‾‾‾——_
                 ‾‾——__________    __________——‾‾         ___—‾         ‾—___
            0.0.....0.5.....1.0    0.0.....0.5.....1.0    0.0.....0.5.....1.0
            
            */
            case 0:
            {
                weight_s  = curve( max( 1.0f - pLuma * 2.0f, 0.0f ));
                weight_h  = curve( max(( pLuma - 0.5f ) * 2.0f, 0.0f ));
                weight_m  = saturate( 1.0f - weight_s - weight_h );
            } break;

            /*
            Higher degree of blending between individual curves
            F.e. shadows will still have a minimal weight all the way into highlight territory
            Ensures smoother transition areas between contrasts
            Curves look like:

            Shadows                Highlights             Midtones
            ‾‾‾—_                                _—‾‾‾          __---__
                 ‾‾———————_____    _____———————‾‾         ___-‾‾       ‾‾-___
            0.0.....0.5.....1.0    0.0.....0.5.....1.0    0.0.....0.5.....1.0
            
            */
            case 1:
            {
                weight_s  = pow( 1.0f - pLuma, 4.0f );
                weight_h  = pow( pLuma, 4.0f );
                weight_m  = saturate( 1.0f - weight_s - weight_h );
            } break;
        }

        float3 cold       = float3( 0.0f,  0.365f, 1.0f ); //LBB
        float3 warm       = float3( 0.98f, 0.588f, 0.0f ); //LBA
        
        // Shadows
        color.xyz        = exposure( color.xyz, exposure_s * weight_s );
        color.xyz        = con( color.xyz, contrast_s * weight_s );
        color.xyz        = bri( color.xyz, brightness_s * weight_s );
        color.xyz        = blendmode( color.xyz, blendcolor_s.xyz, blendmode_s, opacity_s * weight_s );
        if( tint_s < 0.0f )
            color.xyz    = lerp( color.xyz, softlight( color.xyz, cold.xyz ), abs( tint_s * weight_s ));
        else
            color.xyz    = lerp( color.xyz, softlight( color.xyz, warm.xyz ), tint_s * weight_s );
        color.xyz        = sat( color.xyz, saturation_s * weight_s );
        color.xyz        = vib( color.xyz, vibrance_s   * weight_s );

        // Midtones
        color.xyz        = exposure( color.xyz, exposure_m * weight_m );
        color.xyz        = con( color.xyz, contrast_m   * weight_m );
        color.xyz        = bri( color.xyz, brightness_m * weight_m );
        color.xyz        = blendmode( color.xyz, blendcolor_m.xyz, blendmode_m, opacity_m * weight_m );
        if( tint_m < 0.0f )
            color.xyz    = lerp( color.xyz, softlight( color.xyz, cold.xyz ), abs( tint_m * weight_m ));
        else
            color.xyz    = lerp( color.xyz, softlight( color.xyz, warm.xyz ), tint_m * weight_m );
        color.xyz        = sat( color.xyz, saturation_m * weight_m );
        color.xyz        = vib( color.xyz, vibrance_m   * weight_m );

        // Highlights
        color.xyz        = exposure( color.xyz, exposure_h * weight_h );
        color.xyz        = con( color.xyz, contrast_h   * weight_h );
        color.xyz        = bri( color.xyz, brightness_h * weight_h );
        color.xyz        = blendmode( color.xyz, blendcolor_h.xyz, blendmode_h, opacity_h * weight_h );
        if( tint_h < 0.0f )
            color.xyz    = lerp( color.xyz, softlight( color.xyz, cold.xyz ), abs( tint_h * weight_h ));
        else
            color.xyz    = lerp( color.xyz, softlight( color.xyz, warm.xyz ), tint_h * weight_h );
        color.xyz        = sat( color.xyz, saturation_h * weight_h );
        color.xyz        = vib( color.xyz, vibrance_h   * weight_h );

        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_03_Shadows_Midtones_Highlights
    {
        pass prod80_pass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_SMH;
        }
    }
}


