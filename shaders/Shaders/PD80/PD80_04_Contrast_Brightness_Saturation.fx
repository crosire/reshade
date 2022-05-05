/*
    Description : PD80 04 Contrast Brightness Saturation for Reshade https://reshade.me/
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
#include "PD80_00_Color_Spaces.fxh"
#include "PD80_00_Blend_Modes.fxh"
#include "PD80_00_Base_Effects.fxh"

namespace pd80_conbrisat
{
    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
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
        > = 1.0;
    uniform float tint <
        ui_label = "Tint";
        ui_tooltip = "Tint";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float exposureN <
        ui_label = "Exposure";
        ui_tooltip = "Exposure";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float contrast <
        ui_label = "Contrast";
        ui_tooltip = "Contrast";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float brightness <
        ui_label = "Brightness";
        ui_tooltip = "Brightness";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float saturation <
        ui_label = "Saturation";
        ui_tooltip = "Saturation";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float vibrance <
        ui_label = "Vibrance";
        ui_tooltip = "Vibrance";
        ui_category = "Final Adjustments";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float huemid <
    	ui_label = "Color Hue";
        ui_tooltip = "Custom Color Hue";
        ui_category = "Custom Saturation Adjustments";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float huerange <
        ui_label = "Hue Range Selection";
        ui_tooltip = "Custom Hue Range Selection";
        ui_category = "Custom Saturation Adjustments";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.167;
    uniform float sat_custom <
        ui_label = "Custom Saturation Level";
        ui_tooltip = "Custom Saturation Level";
        ui_category = "Custom Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_r <
        ui_label = "Red Saturation";
        ui_tooltip = "Red Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_y <
        ui_label = "Yellow Saturation";
        ui_tooltip = "Yellow Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_g <
        ui_label = "Green Saturation";
        ui_tooltip = "Green Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_a <
        ui_label = "Aqua Saturation";
        ui_tooltip = "Aqua Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_b <
        ui_label = "Blue Saturation";
        ui_tooltip = "Blue Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_p <
        ui_label = "Purple Saturation";
        ui_tooltip = "Purple Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform float sat_m <
        ui_label = "Magenta Saturation";
        ui_tooltip = "Magenta Saturation";
        ui_category = "Color Saturation Adjustments";
        ui_type = "slider";
        ui_min = -2.0;
        ui_max = 2.0;
        > = 0.0;
    uniform bool enable_depth <
        ui_label = "Enable depth based adjustments.\nMake sure you have setup your depth buffer correctly.";
        ui_tooltip = "Enable depth based adjustments";
        ui_category = "Final Adjustments: Depth";
        > = false;
    uniform bool display_depth <
        ui_label = "Show depth texture";
        ui_tooltip = "Show depth texture";
        ui_category = "Final Adjustments: Depth";
        > = false;
    uniform float depthStart <
        ui_type = "slider";
        ui_label = "Change Depth Start Plane";
        ui_tooltip = "Change Depth Start Plane";
        ui_category = "Final Adjustments: Depth";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float depthEnd <
        ui_type = "slider";
        ui_label = "Change Depth End Plane";
        ui_tooltip = "Change Depth End Plane";
        ui_category = "Final Adjustments: Depth";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.1;
    uniform float depthCurve <
        ui_label = "Depth Curve Adjustment";
        ui_tooltip = "Depth Curve Adjustment";
        ui_category = "Final Adjustments: Depth";
        ui_type = "slider";
        ui_min = 0.05;
        ui_max = 8.0;
        > = 1.0;
    uniform float exposureD <
        ui_label = "Exposure Far";
        ui_tooltip = "Exposure Far";
        ui_category = "Final Adjustments: Far";
        ui_type = "slider";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float contrastD <
        ui_label = "Contrast Far";
        ui_tooltip = "Contrast Far";
        ui_category = "Final Adjustments: Far";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float brightnessD <
        ui_label = "Brightness Far";
        ui_tooltip = "Brightness Far";
        ui_category = "Final Adjustments: Far";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.5;
        > = 0.0;
    uniform float saturationD <
        ui_label = "Saturation Far";
        ui_tooltip = "Saturation Far";
        ui_category = "Final Adjustments: Far";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float vibranceD <
        ui_label = "Vibrance Far";
        ui_tooltip = "Vibrance Far";
        ui_category = "Final Adjustments: Far";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////

    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float getLuminance( in float3 x )
    {
        return dot( x, float3( 0.212656, 0.715158, 0.072186 ));
    }

    float curve( float x )
    {
        return x * x * ( 3.0 - 2.0 * x );
    }

    float3 channelsat( float3 col, float r, float y, float g, float a, float b, float p, float m, float hue )
    {
        float desat        = getLuminance( col.xyz );

        // Red          : 0.0
        // Orange       : 0.083
        // Yellow       : 0.167
        // Green        : 0.333
        // Cyan/Aqua    : 0.5
        // Blue         : 0.667
        // Purple       : 0.75
        // Magenta      : 0.833

        float weight_r     = curve( max( 1.0f - abs(  hue               * 6.0f ), 0.0f )) +
                             curve( max( 1.0f - abs(( hue - 1.0f      ) * 6.0f ), 0.0f ));
        float weight_y     = curve( max( 1.0f - abs(( hue - 0.166667f ) * 6.0f ), 0.0f ));
        float weight_g     = curve( max( 1.0f - abs(( hue - 0.333333f ) * 6.0f ), 0.0f ));
        float weight_a     = curve( max( 1.0f - abs(( hue - 0.5f      ) * 6.0f ), 0.0f ));
        float weight_b     = curve( max( 1.0f - abs(( hue - 0.666667f ) * 6.0f ), 0.0f ));
        float weight_p     = curve( max( 1.0f - abs(( hue - 0.75f     ) * 6.0f ), 0.0f ));
        float weight_m     = curve( max( 1.0f - abs(( hue - 0.833333f ) * 6.0f ), 0.0f ));

        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + r * weight_r, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + y * weight_y, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + g * weight_g, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + a * weight_a, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + b * weight_b, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + p * weight_p, 0.0f, 2.0f ));
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + m * weight_m, 0.0f, 2.0f ));

        return saturate( col.xyz );
    }

    float3 customsat( float3 col, float h, float range, float sat, float hue )
    {
        float desat        = getLuminance( col.xyz );
        float r            = rcp( range );
        float3 w           = max( 1.0f - abs(( hue - h        ) * r ), 0.0f );
        w.y                = max( 1.0f - abs(( hue + 1.0f - h ) * r ), 0.0f );
        w.z                = max( 1.0f - abs(( hue - 1.0f - h ) * r ), 0.0f );
        float weight       = curve( dot( w.xyz, 1.0f )) * sat;
        col.xyz            = lerp( desat, col.xyz, clamp( 1.0f + weight, 0.0f, 2.0f ));
        return saturate( col.xyz );
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_CBS(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( ReShade::BackBuffer, texcoord );
        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise    = dither( samplerRGBNoise, texcoord.xy, 6, enable_dither, dither_strength, 1, 0.5f );
        color.xyz        = saturate( color.xyz + dnoise.xyz );

        float depth      = ReShade::GetLinearizedDepth( texcoord ).x;
        depth            = smoothstep( depthStart, depthEnd, depth );
        depth            = pow( depth, depthCurve );
        float4 dnoise2   = dither( samplerGaussNoise, texcoord.xy, 0, 1, 1.0f, 0, 1.0f );
        depth            = saturate( depth + dnoise2.x );

        float3 cold      = float3( 0.0f,  0.365f, 1.0f ); //LBB
        float3 warm      = float3( 0.98f, 0.588f, 0.0f ); //LBA

        color.xyz        = ( tint < 0.0f ) ? lerp( color.xyz, blendLuma( cold.xyz, color.xyz ), abs( tint )) :
                                             lerp( color.xyz, blendLuma( warm.xyz, color.xyz ), tint );

        float3 dcolor    = color.xyz;
        color.xyz        = exposure( color.xyz, exposureN );
        color.xyz        = con( color.xyz, contrast   );
        color.xyz        = bri( color.xyz, brightness );
        color.xyz        = sat( color.xyz, saturation );
        color.xyz        = vib( color.xyz, vibrance   );

        dcolor.xyz       = exposure( dcolor.xyz, exposureD );
        dcolor.xyz       = con( dcolor.xyz, contrastD   );
        dcolor.xyz       = bri( dcolor.xyz, brightnessD );
        dcolor.xyz       = sat( dcolor.xyz, saturationD );
        dcolor.xyz       = vib( dcolor.xyz, vibranceD   );
        
        color.xyz        = lerp( color.xyz, dcolor.xyz, enable_depth * depth ); // apply based on depth

        float chue       = RGBToHSL( color.xyz ).x;
        color.xyz        = channelsat( color.xyz, sat_r, sat_y, sat_g, sat_a, sat_b, sat_p, sat_m, chue );
        color.xyz        = customsat( color.xyz, huemid, huerange, sat_custom, chue );

        color.xyz        = display_depth ? depth.xxx : color.xyz; // show depth

        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_ContrastBrightnessSaturation
    {
        pass ConBriSat
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_CBS;
        }
    }
}