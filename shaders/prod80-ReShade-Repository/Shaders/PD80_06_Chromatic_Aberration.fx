/*
    Description : PD80 06 Chromatic Aberration for Reshade https://reshade.me/
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

#if __RENDERER__ == 0x9000
    #ifndef CA_sampleSTEPS
        #define CA_sampleSTEPS     24  // [0 to 96]
    #endif
        #define CA_DX9_MODE        1
#else
        #define CA_DX9_MODE        0
#endif

namespace pd80_ca
{
    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int CA_type < __UNIFORM_COMBO_INT1
        ui_label = "Chromatic Aberration Type";
        ui_tooltip = "Chromatic Aberration Type";
        ui_category = "Chromatic Aberration";
        ui_items = "Center Weighted Radial\0Center Weighted Longitudinal\0Full screen Radial\0Full screen Longitudinal\0";
        > = 0;
    uniform int degrees <
        ui_type = "slider";
        ui_label = "CA Rotation Offset";
        ui_tooltip = "CA Rotation Offset";
        ui_category = "Chromatic Aberration";
        ui_min = 0;
        ui_max = 360;
        ui_step = 1;
        > = 135;
    uniform float CA <
        ui_type = "slider";
        ui_label = "CA Global Width";
        ui_tooltip = "CA Global Width";
        ui_category = "Chromatic Aberration";
        ui_min = -150.0f;
        ui_max = 150.0f;
        > = -12.0;
#if CA_DX9_MODE == 0
    uniform int sampleSTEPS <
        ui_type = "slider";
        ui_label = "Number of Hues";
        ui_tooltip = "Number of Hues";
        ui_category = "Chromatic Aberration";
        ui_min = 8;
        ui_max = 96;
        ui_step = 1;
        > = 24;
#endif
    uniform float CA_strength <
        ui_type = "slider";
        ui_label = "CA Effect Strength";
        ui_tooltip = "CA Effect Strength";
        ui_category = "Chromatic Aberration";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform bool show_CA <
        ui_label = "CA Show Center / Vignette";
        ui_tooltip = "CA Show Center / Vignette";
        ui_category = "CA: Center Weighted";
        > = false;
    uniform float3 vignetteColor <
        ui_type = "color";
        ui_label = "Vignette Color";
        ui_tooltip = "Vignette Color";
        ui_category = "CA: Center Weighted";
        > = float3(0.0, 0.0, 0.0);
    uniform float CA_width <
        ui_type = "slider";
        ui_label = "CA Width";
        ui_tooltip = "CA Width";
        ui_category = "CA: Center Weighted";
        ui_min = 0.0f;
        ui_max = 5.0f;
        > = 1.0;
    uniform float CA_curve <
        ui_type = "slider";
        ui_label = "CA Curve";
        ui_tooltip = "CA Curve";
        ui_category = "CA: Center Weighted";
        ui_min = 0.1f;
        ui_max = 12.0f;
        > = 1.0;
    uniform float oX <
        ui_type = "slider";
        ui_label = "CA Center (X)";
        ui_tooltip = "CA Center (X)";
        ui_category = "CA: Center Weighted";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float oY <
        ui_type = "slider";
        ui_label = "CA Center (Y)";
        ui_tooltip = "CA Center (Y)";
        ui_category = "CA: Center Weighted";
        ui_min = -1.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform float CA_shapeX <
        ui_type = "slider";
        ui_label = "CA Shape (X)";
        ui_tooltip = "CA Shape (X)";
        ui_category = "CA: Center Weighted";
        ui_min = 0.2f;
        ui_max = 6.0f;
        > = 1.0;
    uniform float CA_shapeY <
        ui_type = "slider";
        ui_label = "CA Shape (Y)";
        ui_tooltip = "CA Shape (Y)";
        ui_category = "CA: Center Weighted";
        ui_min = 0.2f;
        ui_max = 6.0f;
        > = 1.0;
    uniform bool enable_depth_int <
        ui_label = "Intensity: Enable depth based adjustments.\nMake sure you have setup your depth buffer correctly.";
        ui_tooltip = "Intensity: Enable depth based adjustments";
        ui_category = "Final Adjustments: Depth";
        > = false;
    uniform bool enable_depth_width <
        ui_label = "Width: Enable depth based adjustments.\nMake sure you have setup your depth buffer correctly.";
        ui_tooltip = "Width: Enable depth based adjustments";
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

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float3 HUEToRGB( float H )
    {
        return saturate( float3( abs( H * 6.0f - 3.0f ) - 1.0f,
                                 2.0f - abs( H * 6.0f - 2.0f ),
                                 2.0f - abs( H * 6.0f - 4.0f )));
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_CA(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = 0.0f;
        float px          = BUFFER_RCP_WIDTH;
        float py          = BUFFER_RCP_HEIGHT;
        float aspect      = float( BUFFER_WIDTH * BUFFER_RCP_HEIGHT );
        float3 orig       = tex2D( ReShade::BackBuffer, texcoord ).xyz;
        float depth       = ReShade::GetLinearizedDepth( texcoord ).x;
        depth             = smoothstep( depthStart, depthEnd, depth );
        depth             = pow( depth, depthCurve );
        float CA_width_n  = CA_width;
        if( enable_depth_width )
            CA_width_n    *= depth;

        //float2 coords     = clamp( texcoord.xy * 2.0f - float2( oX + 1.0f, oY + 1.0f ), -1.0f, 1.0f );
        float2 coords     = texcoord.xy * 2.0f - float2( oX + 1.0f, oY + 1.0f ); // Let it ripp, and not clamp!
        float2 uv         = coords.xy;
        coords.xy         /= float2( CA_shapeX / aspect, CA_shapeY );
        float2 caintensity= length( coords.xy ) * CA_width_n;
        caintensity.y     = caintensity.x * caintensity.x + 1.0f;
        caintensity.x     = 1.0f - ( 1.0f / ( caintensity.y * caintensity.y ));
        caintensity.x     = pow( caintensity.x, CA_curve );

        int degreesY      = degrees;
        float c           = 0.0f;
        float s           = 0.0f;
        switch( CA_type )
        {
            // Radial: Y + 90 w/ multiplying with uv.xy
            case 0:
            {
                degreesY      = degrees + 90 > 360 ? degreesY = degrees + 90 - 360 : degrees + 90;
                c             = cos( radians( degrees )) * uv.x;
                s             = sin( radians( degreesY )) * uv.y;
            }
            break;
            // Longitudinal: X = Y w/o multiplying with uv.xy
            case 1:
            {
                c             = cos( radians( degrees ));
                s             = sin( radians( degreesY ));
            }
            break;
            // Full screen Radial
            case 2:
            {
                degreesY      = degrees + 90 > 360 ? degreesY = degrees + 90 - 360 : degrees + 90;
                caintensity.x = 1.0f;
                c             = cos( radians( degrees )) * uv.x;
                s             = sin( radians( degreesY )) * uv.y;
            }
            break;
            // Full screen Longitudinal
            case 3:
            {
                caintensity.x = 1.0f;
                c             = cos( radians( degrees ));
                s             = sin( radians( degreesY ));
            }
            break;
        }
        
        //Apply based on scene depth
        if( enable_depth_int )
            caintensity.x *= depth;

        float3 huecolor   = 0.0f;
        float3 temp       = 0.0f;
#if CA_DX9_MODE == 1
        float o1          = CA_sampleSTEPS - 1.0f;
#else
        float o1          = sampleSTEPS - 1.0f;
#endif
        float o2          = 0.0f;
        float3 d          = 0.0f;

        // Scale CA (hackjob!)
        float caWidth     = CA * ( max( BUFFER_WIDTH, BUFFER_HEIGHT ) / 1920.0f ); // Scaled for 1920, raising resolution in X or Y should raise scale

        float offsetX     = px * c * caintensity.x;
        float offsetY     = py * s * caintensity.x;
#if CA_DX9_MODE == 1
        float sampst      = CA_sampleSTEPS;
        for( float i = 0; i < CA_sampleSTEPS; i++ )
#else
        float sampst      = sampleSTEPS;
        for( float i = 0; i < sampleSTEPS; i++ )
#endif
        {
            huecolor.xyz  = HUEToRGB( i / sampst );
            o2            = lerp( -caWidth, caWidth, i / o1 );
            temp.xyz      = tex2D( ReShade::BackBuffer, texcoord.xy + float2( o2 * offsetX, o2 * offsetY )).xyz;
            color.xyz     += temp.xyz * huecolor.xyz;
            d.xyz         += huecolor.xyz;
        }
        //color.xyz         /= ( sampleSTEPS / 3.0f * 2.0f ); // Too crude and doesn't work with low sampleSTEPS ( too dim )
        color.xyz           /= dot( d.xyz, 0.333333f ); // seems so-so OK
        color.xyz           = lerp( orig.xyz, color.xyz, CA_strength );
        color.xyz           = lerp( color.xyz, vignetteColor.xyz * caintensity.x + ( 1.0f - caintensity.x ) * color.xyz, show_CA );
        color.xyz           = display_depth ? depth.xxx : color.xyz;
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_06_ChromaticAberration
    {
        pass prod80_CA
        {
            VertexShader  = PostProcessVS;
            PixelShader   = PS_CA;
        }
    }
}
