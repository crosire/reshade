/*
    Description : PD80 04 Selective Color for Reshade https://reshade.me/
    Author      : prod80 (Bas Veth)
    License     : MIT, Copyright (c) 2020 prod80
    
    Additional credits
    - Based on the mathematical analysis provided here
      http://blog.pkh.me/p/22-understanding-selective-coloring-in-adobe-photoshop.html

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
#include "PD80_00_Base_Effects.fxh"

namespace pd80_selectivecolor
{

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int corr_method < __UNIFORM_COMBO_INT1
        ui_label = "Correction Method";
        ui_tooltip = "Correction Method";
        ui_category = "Selective Color";
        ui_items = "Absolute\0Relative\0"; //Do not change order; 0=Absolute, 1=Relative
        > = 1;
    uniform int corr_method2 < __UNIFORM_COMBO_INT1
        ui_label = "Correction Method Saturation";
        ui_tooltip = "Correction Method Saturation";
        ui_category = "Selective Color";
        ui_items = "Absolute\0Relative\0"; //Do not change order; 0=Absolute, 1=Relative
        > = 1;
    // Reds
    uniform float r_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Reds: Cyan";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float r_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Reds: Magenta";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float r_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Reds: Yellow";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float r_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Reds: Black";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float r_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Reds: Saturation";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float r_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Reds: Vibrance";
        ui_category = "Selective Color: Reds";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Yellows
    uniform float y_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Yellows: Cyan";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float y_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Yellows: Magenta";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float y_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Yellows: Yellow";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float y_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Yellows: Black";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float y_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Yellows: Saturation";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float y_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Yellows: Vibrance";
        ui_category = "Selective Color: Yellows";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    
    // Greens
    uniform float g_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Greens: Cyan";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float g_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Greens: Magenta";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float g_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Greens: Yellow";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float g_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Greens: Black";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float g_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Greens: Saturation";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float g_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Greens: Vibrance";
        ui_category = "Selective Color: Greens";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Cyans
    uniform float c_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Cyans: Cyan";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float c_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Cyans: Magenta";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float c_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Cyans: Yellow";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float c_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Cyans: Black";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float c_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Cyans: Saturation";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float c_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Cyans: Vibrance";
        ui_category = "Selective Color: Cyans";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Blues
    uniform float b_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Blues: Cyan";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float b_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Blues: Magenta";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float b_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Blues: Yellow";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float b_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Blues: Black";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float b_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Blues: Saturation";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float b_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Blues: Vibrance";
        ui_category = "Selective Color: Blues";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Magentas
    uniform float m_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Magentas: Cyan";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float m_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Magentas: Magenta";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float m_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Magentas: Yellow";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float m_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Magentas: Black";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float m_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Magentas: Saturation";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float m_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Magentas: Vibrance";
        ui_category = "Selective Color: Magentas";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Whites
    uniform float w_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Whites: Cyan";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float w_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Whites: Magenta";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float w_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Whites: Yellow";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float w_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Whites: Black";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float w_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Whites: Saturation";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float w_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Whites: Vibrance";
        ui_category = "Selective Color: Whites";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Neutrals
    uniform float n_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Neutrals: Cyan";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float n_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Neutrals: Magenta";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float n_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Neutrals: Yellow";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float n_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Neutrals: Black";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float n_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Neutrals: Saturation";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float n_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Neutrals: Vibrance";
        ui_category = "Selective Color: Neutrals";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    // Blacks
    uniform float bk_adj_cya <
        ui_type = "slider";
        ui_label = "Cyan";
        ui_tooltip = "Selective Color Blacks: Cyan";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float bk_adj_mag <
        ui_type = "slider";
        ui_label = "Magenta";
        ui_tooltip = "Selective Color Blacks: Magenta";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float bk_adj_yel <
        ui_type = "slider";
        ui_label = "Yellow";
        ui_tooltip = "Selective Color Blacks: Yellow";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float bk_adj_bla <
        ui_type = "slider";
        ui_label = "Black";
        ui_tooltip = "Selective Color Blacks: Black";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float bk_adj_sat <
        ui_type = "slider";
        ui_label = "Saturation";
        ui_tooltip = "Selective Color Blacks: Saturation";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float bk_adj_vib <
        ui_type = "slider";
        ui_label = "Vibrance";
        ui_tooltip = "Selective Color Blacks: Vibrance";
        ui_category = "Selective Color: Blacks";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    
    float mid( float3 c )
    {
        float sum = c.x + c.y + c.z;
        float mn = min( min( c.x, c.y ), c.z );
        float mx = max( max( c.x, c.y ), c.z );
        return sum - mn - mx;
    }

    float adjustcolor( float scale, float colorvalue, float adjust, float bk, int method )
    {
        /* 
        y(value, adjustment) = clamp((( -1 - adjustment ) * bk - adjustment ) * method, -value, 1 - value ) * scale
        absolute: method = 1.0f - colorvalue * 0
        relative: method = 1.0f - colorvalue * 1
        */
        return clamp((( -1.0f - adjust ) * bk - adjust ) * ( 1.0f - colorvalue * method ), -colorvalue, 1.0f - colorvalue) * scale;
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_SelectiveColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );

        // Clamp 0..1
        color.xyz         = saturate( color.xyz );

        // Need these a lot
        float min_value   = min( min( color.x, color.y ), color.z );
        float max_value   = max( max( color.x, color.y ), color.z );
        float mid_value   = mid( color.xyz );
        
        // Used for determining which pixels to adjust regardless of prior changes to color
        float3 orig       = color.xyz;

        // Scales
        float sRGB        = max_value - mid_value;
        float sCMY        = mid_value - min_value;
        float sNeutrals   = 1.0f - ( abs( max_value - 0.5f ) + abs( min_value - 0.5f ));
        float sWhites     = ( min_value - 0.5f ) * 2.0f;
        float sBlacks     = ( 0.5f - max_value ) * 2.0f;

        /*
        Create relative saturation levels.
        For example when saturating red channel you will manipulate yellow and magenta channels.
        So, to ensure there are no bugs and transitions are smooth, need to scale saturation with
        relative saturation of nearest colors. If difference between red and green is low ( color nearly yellow )
        you use this info to scale back red saturation on those pixels.
        This solution is not fool proof, but gives acceptable results almost always.
        */
        
        // Red is when maxvalue = x
        float r_d_m       = orig.x - orig.z;
        float r_d_y       = orig.x - orig.y;
        // Yellow is when minvalue = z
        float y_d         = mid_value - orig.z;
        // Green is when maxvalue = y
        float g_d_y       = orig.y - orig.x;
        float g_d_c       = orig.y - orig.z;
        // Cyan is when minvalue = x
        float c_d         = mid_value - orig.x;
        // Blue is when maxvalue = z
        float b_d_c       = orig.z - orig.y;
        float b_d_m       = orig.z - orig.x;
        // Magenta is when minvalue = y
        float m_d         = mid_value - orig.y;
        
        float r_delta     = 1.0f;
        float y_delta     = 1.0f;
        float g_delta     = 1.0f;
        float c_delta     = 1.0f;
        float b_delta     = 1.0f;
        float m_delta     = 1.0f;

        if( corr_method2 ) // Relative saturation
        {
            r_delta       = min( r_d_m, r_d_y );
            y_delta       = y_d;
            g_delta       = min( g_d_y, g_d_c );
            c_delta       = c_d;
            b_delta       = min( b_d_c, b_d_m );
            m_delta       = m_d;
        } 

        // Selective Color
        if( max_value == orig.x )
        {
            color.x       = color.x + adjustcolor( sRGB, color.x, r_adj_cya, r_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sRGB, color.y, r_adj_mag, r_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sRGB, color.z, r_adj_yel, r_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, r_adj_sat * r_delta );
            color.xyz     = vib( color.xyz, r_adj_vib * r_delta );
        }

        if( min_value == orig.z )
        {
            color.x       = color.x + adjustcolor( sCMY, color.x, y_adj_cya, y_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sCMY, color.y, y_adj_mag, y_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sCMY, color.z, y_adj_yel, y_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, y_adj_sat * y_delta );
            color.xyz     = vib( color.xyz, y_adj_vib * y_delta );
        }

        if( max_value == orig.y )
        {
            color.x       = color.x + adjustcolor( sRGB, color.x, g_adj_cya, g_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sRGB, color.y, g_adj_mag, g_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sRGB, color.z, g_adj_yel, g_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, g_adj_sat * g_delta );
            color.xyz     = vib( color.xyz, g_adj_vib * g_delta );
        }

        if( min_value == orig.x )
        {
            color.x       = color.x + adjustcolor( sCMY, color.x, c_adj_cya, c_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sCMY, color.y, c_adj_mag, c_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sCMY, color.z, c_adj_yel, c_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, c_adj_sat * c_delta );
            color.xyz     = vib( color.xyz, c_adj_vib * c_delta );
        }

        if( max_value == orig.z )
        {
            color.x       = color.x + adjustcolor( sRGB, color.x, b_adj_cya, b_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sRGB, color.y, b_adj_mag, b_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sRGB, color.z, b_adj_yel, b_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, b_adj_sat * b_delta );
            color.xyz     = vib( color.xyz, b_adj_vib * b_delta );
        }

        if( min_value == orig.y )
        {
            color.x       = color.x + adjustcolor( sCMY, color.x, m_adj_cya, m_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sCMY, color.y, m_adj_mag, m_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sCMY, color.z, m_adj_yel, m_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, m_adj_sat * m_delta );
            color.xyz     = vib( color.xyz, m_adj_vib * m_delta );
        }

        if( min_value >= 0.5f )
        {
            color.x       = color.x + adjustcolor( sWhites, color.x, w_adj_cya, w_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sWhites, color.y, w_adj_mag, w_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sWhites, color.z, w_adj_yel, w_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, w_adj_sat * smoothstep( 0.5f, 1.0f, min_value ));
            color.xyz     = vib( color.xyz, w_adj_vib * smoothstep( 0.5f, 1.0f, min_value ));
        }

        if( max_value > 0.0f && min_value < 1.0f )
        {
            color.x       = color.x + adjustcolor( sNeutrals, color.x, n_adj_cya, n_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sNeutrals, color.y, n_adj_mag, n_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sNeutrals, color.z, n_adj_yel, n_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, n_adj_sat );
            color.xyz     = vib( color.xyz, n_adj_vib );
        }

        if( max_value < 0.5f )
        {
            color.x       = color.x + adjustcolor( sBlacks, color.x, bk_adj_cya, bk_adj_bla, corr_method );
            color.y       = color.y + adjustcolor( sBlacks, color.y, bk_adj_mag, bk_adj_bla, corr_method );
            color.z       = color.z + adjustcolor( sBlacks, color.z, bk_adj_yel, bk_adj_bla, corr_method );
            color.xyz     = sat( color.xyz, bk_adj_sat * smoothstep( 0.5f, 0.0f, max_value ));
            color.xyz     = vib( color.xyz, bk_adj_vib * smoothstep( 0.5f, 0.0f, max_value ));
        }

        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_SelectiveColor
    {
        pass prod80_sc
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_SelectiveColor;
        }
    }
}