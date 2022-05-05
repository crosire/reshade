/*
    Description : PD80 03 Color Space Curves for Reshade https://reshade.me/
    Author      : prod80 (Bas Veth)
    License     : MIT, Copyright (c) 2020 prod80

    Additional Credits
    http://technorgb.blogspot.com/2018/02/hyperbola-tone-mapping.html

    LAB Conversion adopted from
    http://www.brucelindbloom.com/

    For the curves code:
    Copyright (c) 2018 ishiyama, MIT License
    Please see https://www.shadertoy.com/view/4tjcD1
    

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

namespace pd80_cscurves
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

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
    uniform int color_space <
        ui_type = "combo";
        ui_category = "Global";
        ui_label = "Color Space";
        ui_tooltip = "Color Space";
        ui_items = "RGB-W\0L* a* b*\0HSL\0HSV\0";
    > = 1;
    // Greys
    uniform float pos0_shoulder_grey <
        ui_type = "slider";
        ui_label = "Shoulder Position X";
        ui_tooltip = "Shoulder Position X";
        ui_category = "Contrast Curves";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.8;
    uniform float pos1_shoulder_grey <
        ui_type = "slider";
        ui_label = "Shoulder Position Y";
        ui_tooltip = "Shoulder Position Y";
        ui_category = "Contrast Curves";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.8;
    uniform float pos0_toe_grey <
        ui_type = "slider";
        ui_label = "Toe Position X";
        ui_tooltip = "Toe Position X";
        ui_category = "Contrast Curves";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.2;
    uniform float pos1_toe_grey <
        ui_type = "slider";
        ui_label = "Toe Position Y";
        ui_tooltip = "Toe Position Y";
        ui_category = "Contrast Curves";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.2;
    // Saturation
    uniform float colorsat <
        ui_type = "slider";
        ui_category = "Color Control";
        ui_label = "Saturation";
        ui_tooltip = "Saturation";
        ui_min = -1.0;
        ui_max = 1.0;
    > = 0.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// STRUCTURES /////////////////////////////////////////////////////////////////
    struct TonemapParams
    {
        float3 mToe;
        float2 mMid;
        float3 mShoulder;
        float2 mBx;
    };

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float3 Tonemap(const TonemapParams tc, float3 x)
    {
        float3 toe = - tc.mToe.x / (x + tc.mToe.y) + tc.mToe.z;
        float3 mid = tc.mMid.x * x + tc.mMid.y;
        float3 shoulder = - tc.mShoulder.x / (x + tc.mShoulder.y) + tc.mShoulder.z;
        float3 result = ( x >= tc.mBx.x ) ? mid : toe;
        result = ( x >= tc.mBx.y ) ? shoulder : result;
        return result;
    }


    float4 setBoundaries( float tx, float ty, float sx, float sy )
    {
        if( tx > sx )
            tx = sx;
        if( ty > sy )
            ty = sy;
        return float4( tx, ty, sx, sy );
    }

    void PrepareTonemapParams(float2 p1, float2 p2, float2 p3, out TonemapParams tc)
    {
        float denom = p2.x - p1.x;
        denom = abs(denom) > 1e-5 ? denom : 1e-5;
        float slope = (p2.y - p1.y) / denom;
        {
            tc.mMid.x = slope;
            tc.mMid.y = p1.y - slope * p1.x;
        }
        {
            float denom = p1.y - slope * p1.x;
            denom = abs(denom) > 1e-5 ? denom : 1e-5;
            tc.mToe.x = slope * p1.x * p1.x * p1.y * p1.y / (denom * denom);
            tc.mToe.y = slope * p1.x * p1.x / denom;
            tc.mToe.z = p1.y * p1.y / denom;
        }
        {
            float denom = slope * (p2.x - p3.x) - p2.y + p3.y;
            denom = abs(denom) > 1e-5 ? denom : 1e-5;
            tc.mShoulder.x = slope * pow(p2.x - p3.x, 2.0) * pow(p2.y - p3.y, 2.0) / (denom * denom);
            tc.mShoulder.y = (slope * p2.x * (p3.x - p2.x) + p3.x * (p2.y - p3.y) ) / denom;
            tc.mShoulder.z = (-p2.y * p2.y + p3.y * (slope * (p2.x - p3.x) + p2.y) ) / denom;
        }
        tc.mBx = float2(p1.x, p2.x);
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_CSCurves(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        
        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise     = dither( samplerRGBNoise, texcoord.xy, 1, enable_dither, dither_strength, 1, 0.5f );
        color.xyz         = saturate( color.xyz + dnoise.xyz );
        
        // Prepare curves
        float4 grey       = setBoundaries( pos0_toe_grey, pos1_toe_grey, pos0_shoulder_grey, pos1_shoulder_grey );
        TonemapParams tc;
        PrepareTonemapParams( grey.xy, grey.zw, float2( 1.0f, 1.0f ), tc );

        // RGBW
        float rgb_luma    = min( min( color.x, color.y ), color.z );
        float temp_luma   = rgb_luma;
        float3 rgb_chroma = color.xyz - rgb_luma;
        rgb_luma          = Tonemap( tc, rgb_luma.xxx ).x;
        rgb_chroma       *= ( colorsat + 1.0f );

        // LAB
        float3 lab_color  = pd80_srgb_to_lab( color.xyz );
        lab_color.x       = Tonemap( tc, lab_color.xxx ).x;
        lab_color.yz     *= ( colorsat + 1.0f );
        
        // HSL
        float3 hsl_color  = RGBToHSL( color.xyz );
        hsl_color.z       = Tonemap( tc, hsl_color.zzz ).z;
        hsl_color.y      *= ( colorsat + 1.0f );

        // HSV
        float3 hsv_color  = RGBToHSV( color.xyz );
        hsv_color.z       = Tonemap( tc, hsv_color.zzz ).z;
        hsv_color.y      *= ( colorsat + 1.0f );

        switch( color_space )
        {
            case 0: { color.xyz = saturate( rgb_chroma.xyz + rgb_luma ); } break;
            case 1: { color.xyz = pd80_lab_to_srgb( lab_color.xyz );     } break;
            case 2: { color.xyz = HSLToRGB( saturate( hsl_color.xyz ));  } break;
            case 3: { color.xyz = HSVToRGB( saturate( hsv_color.xyz ));  } break;
        }

        color.xyz         = saturate( color.xyz + dnoise.wxy );
        return float4(color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_03_Color_Space_Curves
    {
        pass prod80_CCpass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_CSCurves;
        }
    }
}