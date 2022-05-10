/*
    Description : PD80 04 Black & White for Reshade https://reshade.me/
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

namespace pd80_blackandwhite
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
    uniform float curve_str <
        ui_type = "slider";
        ui_label = "Contrast Smoothness";
        ui_tooltip = "Contrast Smoothness";
        ui_category = "Global";
        ui_min = 1.0f;
        ui_max = 4.0f;
        > = 1.5f;
    uniform bool show_clip <
        ui_label = "Show Clipping Mask";
        ui_tooltip = "Show Clipping Mask";
        ui_category = "Global";
        > = false;
    uniform int bw_mode < __UNIFORM_COMBO_INT1
        ui_label = "Black & White Conversion";
        ui_tooltip = "Black & White Conversion";
        ui_category = "Black & White Techniques";
        ui_items = "Red Filter\0Green Filter\0Blue Filter\0High Contrast Red Filter\0High Contrast Green Filter\0High Contrast Blue Filter\0Infrared\0Maximum Black\0Maximum White\0Preserve Luminosity\0Neutral Green Filter\0Maintain Contrasts\0High Contrast\0Custom\0";
        > = 13;
    uniform float redchannel <
        ui_type = "slider";
        ui_label = "Custom: Red Weight";
        ui_tooltip = "Custom: Red Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = 0.2f;
    uniform float yellowchannel <
        ui_type = "slider";
        ui_label = "Custom: Yellow Weight";
        ui_tooltip = "Custom: Yellow Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = 0.4f;
    uniform float greenchannel <
        ui_type = "slider";
        ui_label = "Custom: Green Weight";
        ui_tooltip = "Custom: Green Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = 0.6f;
    uniform float cyanchannel <
        ui_type = "slider";
        ui_label = "Custom: Cyan Weight";
        ui_tooltip = "Custom: Cyan Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = 0.0f;
    uniform float bluechannel <
        ui_type = "slider";
        ui_label = "Custom: Blue Weight";
        ui_tooltip = "Custom: Blue Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = -0.6f;
    uniform float magentachannel <
        ui_type = "slider";
        ui_label = "Custom: Magenta Weight";
        ui_tooltip = "Custom: Magenta Weight";
        ui_category = "Black & White Techniques";
        ui_min = -2.0f;
        ui_max = 3.0f;
        > = -0.2f;
    uniform bool use_tint <
        ui_label = "Enable Tinting";
        ui_tooltip = "Enable Tinting";
        ui_category = "Tint";
        > = false;
    uniform float tinthue <
        ui_type = "slider";
        ui_label = "Tint Hue";
        ui_tooltip = "Tint Hue";
        ui_category = "Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.083f;
    uniform float tintsat <
        ui_type = "slider";
        ui_label = "Tint Saturation";
        ui_tooltip = "Tint Saturation";
        ui_category = "Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.12f;
    //// TEXTURES ///////////////////////////////////////////////////////////////////

    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    uniform float2 pingpong < source = "pingpong"; min = 0; max = 128; step = 1; >;

    // Credit to user 'iq' from shadertoy
    // See https://www.shadertoy.com/view/MdBfR1
    float curve( float x, float k )
    {
        float s = sign( x - 0.5f );
        float o = ( 1.0f + s ) / 2.0f;
        return o - 0.5f * s * pow( max( 2.0f * ( o - s * x ), 0.0f ), k );
    }

    float3 ProcessBW( float3 col, float r, float y, float g, float c, float b, float m )
    {
        float3 hsl         = RGBToHSL( col.xyz );
        // Inverse of luma channel to no apply boosts to intensity on already intense brightness (and blow out easily)
        float lum          = 1.0f - hsl.z;

        // Calculate the individual weights per color component in RGB and CMY
        // Sum of all the weights for a given hue is 1.0
        float weight_r     = curve( max( 1.0f - abs(  hsl.x               * 6.0f ), 0.0f ), curve_str ) +
                             curve( max( 1.0f - abs(( hsl.x - 1.0f      ) * 6.0f ), 0.0f ), curve_str );
        float weight_y     = curve( max( 1.0f - abs(( hsl.x - 0.166667f ) * 6.0f ), 0.0f ), curve_str );
        float weight_g     = curve( max( 1.0f - abs(( hsl.x - 0.333333f ) * 6.0f ), 0.0f ), curve_str );
        float weight_c     = curve( max( 1.0f - abs(( hsl.x - 0.5f      ) * 6.0f ), 0.0f ), curve_str );
        float weight_b     = curve( max( 1.0f - abs(( hsl.x - 0.666667f ) * 6.0f ), 0.0f ), curve_str );
        float weight_m     = curve( max( 1.0f - abs(( hsl.x - 0.833333f ) * 6.0f ), 0.0f ), curve_str );

        // No saturation (greyscale) should not influence B&W image
        float sat          = hsl.y * ( 1.0f - hsl.y ) + hsl.y;
        float ret          = hsl.z;
        ret                += ( hsl.z * ( weight_r * r ) * sat * lum );
        ret                += ( hsl.z * ( weight_y * y ) * sat * lum );
        ret                += ( hsl.z * ( weight_g * g ) * sat * lum );
        ret                += ( hsl.z * ( weight_c * c ) * sat * lum );
        ret                += ( hsl.z * ( weight_b * b ) * sat * lum );
        ret                += ( hsl.z * ( weight_m * m ) * sat * lum );

        return saturate( ret );
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_BlackandWhite(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        color.xyz         = saturate( color.xyz );

        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise      = dither( samplerRGBNoise, texcoord.xy, 4, enable_dither, dither_strength, 1, 0.5f );
        color.xyz          = saturate( color.xyz + dnoise.zyx );
        
        float red;  float yellow; float green;
        float cyan; float blue;   float magenta;
        
        switch( bw_mode )
        {
            case 0: // Red Filter
            {
                red      = 0.2f;
                yellow   = 0.5f;
                green    = -0.2f;
                cyan     = -0.6f;
                blue     = -1.0f;
                magenta  = -0.2f;
            }
            break;
            case 1: // Green Filter
            {
                red      = -0.5f;
                yellow   = 0.5f;
                green    = 1.2f;
                cyan     = -0.2f;
                blue     = -1.0f;
                magenta  = -0.5f;
            }
            break;
            case 2: // Blue Filter
            {
                red      = -0.2f;
                yellow   = 0.4f;
                green    = -0.6f;
                cyan     = 0.5f;
                blue     = 1.0f;
                magenta  = -0.2f;
            }
            break;
            case 3: // High Contrast Red Filter
            {
                red      = 0.5f;
                yellow   = 1.2f;
                green    = -0.5f;
                cyan     = -1.0f;
                blue     = -1.5f;
                magenta  = -1.0f;
            }
            break;
            case 4: // High Contrast Green Filter
            {
                red      = -1.0f;
                yellow   = 1.0f;
                green    = 1.2f;
                cyan     = -0.2f;
                blue     = -1.5f;
                magenta  = -1.0f;
            }
            break;
            case 5: // High Contrast Blue Filter
            {
                red      = -0.7f;
                yellow   = 0.4f;
                green    = -1.2f;
                cyan     = 0.7f;
                blue     = 1.2f;
                magenta  = -0.2f;
            }
            break;
            case 6: // Infrared
            {
                red      = -1.35f;
                yellow   = 2.35f;
                green    = 1.35f;
                cyan     = -1.35f;
                blue     = -1.6f;
                magenta  = -1.07f;
            }
            break;
            case 7: // Maximum Black
            {
                red      = -1.0f;
                yellow   = -1.0f;
                green    = -1.0f;
                cyan     = -1.0f;
                blue     = -1.0f;
                magenta  = -1.0f;
            }
            break;
            case 8: // Maximum White
            {
                red      = 1.0f;
                yellow   = 1.0f;
                green    = 1.0f;
                cyan     = 1.0f;
                blue     = 1.0f;
                magenta  = 1.0f;
            }
            break;
            case 9: // Preserve Luminosity
            {
                red      = -0.7f;
                yellow   = 0.9f;
                green    = 0.6f;
                cyan     = 0.1f;
                blue     = -0.4f;
                magenta  = -0.4f;
            }
            break;
            case 10: // Neutral Green Filter
            {
                red      = 0.2f;
                yellow   = 0.4f;
                green    = 0.6f;
                cyan     = 0.0f;
                blue     = -0.6f;
                magenta  = -0.2f;
            }
            break;
            case 11: // Maintain Contrasts
            {
                red      = -0.3f;
                yellow   = 1.0f;
                green    = -0.3f;
                cyan     = -0.6f;
                blue     = -1.0f;
                magenta  = -0.6f;
            }
            break;
            case 12: // High Contrast
            {
                red      = -0.3f;
                yellow   = 2.6f;
                green    = -0.3f;
                cyan     = -1.2f;
                blue     = -0.6f;
                magenta  = -0.4f;
            }
            break;
            case 13: // Custom Filter
            {
                red      = redchannel;
                yellow   = yellowchannel;
                green    = greenchannel;
                cyan     = cyanchannel;
                blue     = bluechannel;
                magenta  = magentachannel;
            }
            break;
            default:
            {
                red      = redchannel;
                yellow   = yellowchannel;
                green    = greenchannel;
                cyan     = cyanchannel;
                blue     = bluechannel;
                magenta  = magentachannel;
            }
            break;
        }
        // Do the Black & White
        color.xyz         = ProcessBW( color.xyz, red, yellow, green, cyan, blue, magenta );
        // Do the tinting
        color.xyz         = lerp( color.xyz, HSLToRGB( float3( tinthue, tintsat, color.x )), use_tint );
        if( show_clip )
        {
            float h       = 0.98f;
            float l       = 0.01f;
            color.xyz     = min( min( color.x, color.y ), color.z ) >= h ? lerp( color.xyz, float3( 1.0f, 0.0f, 0.0f ), smoothstep( h, 1.0f, min( min( color.x, color.y ), color.z ))) : color.xyz;
            color.xyz     = max( max( color.x, color.y ), color.z ) <= l ? lerp( float3( 0.0f, 0.0f, 1.0f ), color.xyz, smoothstep( 0.0f, l, max( max( color.x, color.y ), color.z ))) : color.xyz;
        }
        color.xyz         = saturate( color.xyz + dnoise.xyz );
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_Black_and_White
    {
        pass prod80_BlackandWhite
        {
            VertexShader  = PostProcessVS;
            PixelShader   = PS_BlackandWhite;
        }
    }
}
