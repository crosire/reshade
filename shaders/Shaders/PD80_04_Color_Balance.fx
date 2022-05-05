/*
    Description : PD80 04 Color Balance for Reshade https://reshade.me/
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

namespace pd80_colorbalance
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENS /////////////////////////////////////////////////////////////////
    uniform bool preserve_luma <
        ui_label = "Preserve Luminosity";
        ui_tooltip = "Preserve Luminosity";
        ui_category = "Color Balance";
    > = true;
    uniform int separation_mode < __UNIFORM_COMBO_INT1
        ui_label = "Luma Separation Mode";
        ui_tooltip = "Luma Separation Mode";
        ui_category = "Color Balance";
        ui_items = "Harsh Separation\0Smooth Separation\0";
        > = 0;
    uniform float s_RedShift <
        ui_label = "Cyan <--> Red";
        ui_tooltip = "Shadows: Cyan <--> Red";
        ui_category = "Shadows";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float s_GreenShift <
        ui_label = "Magenta <--> Green";
        ui_tooltip = "Shadows: Magenta <--> Green";
        ui_category = "Shadows";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float s_BlueShift <
        ui_label = "Yellow <--> Blue";
        ui_tooltip = "Shadows: Yellow <--> Blue";
        ui_category = "Shadows";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float m_RedShift <
        ui_label = "Cyan <--> Red";
        ui_tooltip = "Midtones: Cyan <--> Red";
        ui_category = "Midtones";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float m_GreenShift <
        ui_label = "Magenta <--> Green";
        ui_tooltip = "Midtones: Magenta <--> Green";
        ui_category = "Midtones";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float m_BlueShift <
        ui_label = "Yellow <--> Blue";
        ui_tooltip = "Midtones: Yellow <--> Blue";
        ui_category = "Midtones";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float h_RedShift <
        ui_label = "Cyan <--> Red";
        ui_tooltip = "Highlights: Cyan <--> Red";
        ui_category = "Highlights";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float h_GreenShift <
        ui_label = "Magenta <--> Green";
        ui_tooltip = "Highlights: Magenta <--> Green";
        ui_category = "Highlights";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float h_BlueShift <
        ui_label = "Yellow <--> Blue";
        ui_tooltip = "Highlights: Yellow <--> Blue";
        ui_category = "Highlights";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////
    #define ES_RGB   float3( 1.0 - float3( 0.299, 0.587, 0.114 ))
    #define ES_CMY   float3( dot( ES_RGB.yz, 0.5 ), dot( ES_RGB.xz, 0.5 ), dot( ES_RGB.xy, 0.5 ))

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float3 curve( float3 x )
    {
        return x * x * ( 3.0f - 2.0f * x );
    }

    float3 ColorBalance( float3 c, float3 shadows, float3 midtones, float3 highlights )
    {
        // For highlights
        float luma = dot( c.xyz, float3( 0.333333f, 0.333334f, 0.333333f ));
        
        // Determine the distribution curves between shadows, midtones, and highlights
        float3 dist_s; float3 dist_h;

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
                dist_s.xyz  = curve( max( 1.0f - c.xyz * 2.0f, 0.0f ));
                dist_h.xyz  = curve( max(( c.xyz - 0.5f ) * 2.0f, 0.0f ));
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
                dist_s.xyz  = pow( 1.0f - c.xyz, 4.0f );
                dist_h.xyz  = pow( c.xyz, 4.0f );
            } break;
        }

        // Get luminosity offsets
        // One could omit this whole code part in case no luma should be preserved
        float3 s_rgb = 1.0f;
        float3 m_rgb = 1.0f;
        float3 h_rgb = 1.0f;

        if( preserve_luma )
        {
            s_rgb    = shadows > 0.0f     ? ES_RGB * shadows      : ES_CMY * abs( shadows );
            m_rgb    = midtones > 0.0f    ? ES_RGB * midtones     : ES_CMY * abs( midtones );
            h_rgb    = highlights > 0.0f  ? ES_RGB * highlights   : ES_CMY * abs( highlights );
        }
        float3 mids  = saturate( 1.0f - dist_s.xyz - dist_h.xyz );
        float3 highs = dist_h.xyz * ( highlights.xyz * h_rgb.xyz * ( 1.0f - luma ));
        float3 newc  = c.xyz * ( dist_s.xyz * shadows.xyz * s_rgb.xyz + mids.xyz * midtones.xyz * m_rgb.xyz ) * ( 1.0f - c.xyz ) + highs.xyz;
        return saturate( c.xyz + newc.xyz );
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_ColorBalance(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        color.xyz         = saturate( color.xyz );
        color.xyz         = ColorBalance( color.xyz, float3( s_RedShift, s_GreenShift, s_BlueShift ), 
                                                     float3( m_RedShift, m_GreenShift, m_BlueShift ),
                                                     float3( h_RedShift, h_GreenShift, h_BlueShift ));
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_ColorBalance
    {
        pass prod80_pass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_ColorBalance;
        }
    }
}


