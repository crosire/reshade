/*
    Description : PD80 01A Correct Contrast for Reshade https://reshade.me/
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

namespace pd80_correctcontrast
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform bool enable_fade <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Time Based Fade";
        ui_tooltip = "Enable Time Based Fade";
        ui_category = "Global: Correct Contrasts";
        > = true;
    uniform bool freeze <
        ui_label = "Freeze Correction";
        ui_tooltip = "Freeze Correction";
        ui_category = "Global: Correct Contrasts";
        > = false;
    uniform float transition_speed <
        ui_type = "slider";
        ui_label = "Time Based Fade Speed";
        ui_tooltip = "Time Based Fade Speed";
        ui_category = "Global: Correct Contrasts";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.5;
    uniform bool rt_enable_whitepoint_correction <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Whitepoint Correction";
        ui_tooltip = "Enable Whitepoint Correction";
        ui_category = "Whitepoint Correction";
        > = false;
    uniform float rt_wp_str <
        ui_type = "slider";
        ui_label = "White Point Correction Strength";
        ui_tooltip = "White Point Correction Strength";
        ui_category = "Whitepoint Correction";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform bool rt_enable_blackpoint_correction <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Blackpoint Correction";
        ui_tooltip = "Enable Blackpoint Correction";
        ui_category = "Blackpoint Correction";
        > = true;
    uniform float rt_bp_str <
        ui_type = "slider";
        ui_label = "Black Point Correction Strength";
        ui_tooltip = "Black Point Correction Strength";
        ui_category = "Blackpoint Correction";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texDS_1_Max { Width = 32; Height = 32; Format = RGBA16F; };
    texture texDS_1_Min { Width = 32; Height = 32; Format = RGBA16F; };
    texture texPrevious { Width = 4; Height = 2; Format = RGBA16F; };
    texture texDS_1x1 { Width = 4; Height = 2; Format = RGBA16F; };

    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerDS_1_Max
    { 
        Texture = texDS_1_Max;
        MipFilter = POINT;
        MinFilter = POINT;
        MagFilter = POINT;
    };
    sampler samplerDS_1_Min
    {
        Texture = texDS_1_Min;
        MipFilter = POINT;
        MinFilter = POINT;
        MagFilter = POINT;
    };
    sampler samplerPrevious
    { 
        Texture   = texPrevious;
        MipFilter = POINT;
        MinFilter = POINT;
        MagFilter = POINT;
    };
    sampler samplerDS_1x1
    {
        Texture   = texDS_1x1;
        MipFilter = POINT;
        MinFilter = POINT;
        MagFilter = POINT;
    };

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    uniform float frametime < source = "frametime"; >;

    float3 interpolate( float3 o, float3 n, float factor, float ft )
    {
        return lerp( o.xyz, n.xyz, 1.0f - exp( -factor * ft ));
    }


    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    //Downscale to 32x32 min/max color matrix
    void PS_MinMax_1( float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 minValue : SV_Target0, out float4 maxValue : SV_Target1 )
    {
        float3 currColor;
        minValue.xyz       = 1.0f;
        maxValue.xyz       = 0.0f;

        // RenderTarget size is 32x32
        float pst          = 0.03125f;    // rcp( 32 )
        float hst          = 0.5f * pst;  // half size

        // Sample texture
        float2 stexSize    = float2( BUFFER_WIDTH, BUFFER_HEIGHT );
        float2 start       = floor(( texcoord.xy - hst ) * stexSize.xy );    // sample block start position
        float2 stop        = floor(( texcoord.xy + hst ) * stexSize.xy );    // ... end position

        for( int y = start.y; y < stop.y; ++y )
        {
            for( int x = start.x; x < stop.x; ++x )
            {
                currColor    = tex2Dfetch( ReShade::BackBuffer, int2( x, y ), 0 ).xyz;
                // Dark color detection methods
                minValue.xyz = min( minValue.xyz, currColor.xyz );
                // Light color detection methods
                maxValue.xyz = max( currColor.xyz, maxValue.xyz );
            }
        }
        // Return
        minValue           = float4( minValue.xyz, 1.0f );
        maxValue           = float4( maxValue.xyz, 1.0f );
    }

    //Downscale to 32x32 to 1x1 min/max colors
    float4 PS_MinMax_1x1( float4 pos : SV_Position, float2 texcoord : TEXCOORD ) : SV_Target
    {
        float3 minColor; float3 maxColor;
        float3 minValue    = 1.0f;
        float3 maxValue    = 0.0f;
        //Get texture resolution
        uint SampleRes     = 32;
        float Sigma        = 0.0f;

        for( int y = 0; y < SampleRes; ++y )
        {
            for( int x = 0; x < SampleRes; ++x )
            {   
                // Dark color detection methods
                minColor     = tex2Dfetch( samplerDS_1_Min, int2( x, y), 0 ).xyz;
                minValue.xyz = min( minValue.xyz, minColor.xyz );
                // Light color detection methods
                maxColor     = tex2Dfetch( samplerDS_1_Max, int2( x, y ), 0 ).xyz;
                maxValue.xyz = max( maxColor.xyz, maxValue.xyz );
            }
        }

        //Try and avoid some flickering
        //Not really working, too radical changes in min values sometimes
        float3 prevMin     = tex2D( samplerPrevious, float2( texcoord.x / 4.0f, texcoord.y )).xyz;
        float3 prevMax     = tex2D( samplerPrevious, float2(( texcoord.x + 2.0f ) / 4.0, texcoord.y )).xyz;
        float smoothf      = transition_speed * 4.0f + 0.5f;
        float time         = frametime * 0.001f;
        maxValue.xyz       = enable_fade ? interpolate( prevMax.xyz, maxValue.xyz, smoothf, time ) : maxValue.xyz;
        minValue.xyz       = enable_fade ? interpolate( prevMin.xyz, minValue.xyz, smoothf, time ) : minValue.xyz;
        // Freeze Correction
        maxValue.xyz       = freeze ? prevMax.xyz : maxValue.xyz;
        minValue.xyz       = freeze ? prevMin.xyz : minValue.xyz;
        // Return
        if( pos.x < 2 )
            return float4( minValue.xyz, 1.0f );
        else
            return float4( maxValue.xyz, 1.0f );
    }

    float4 PS_CorrectContrast(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color       = tex2D( ReShade::BackBuffer, texcoord );
        float3 minValue    = tex2D( samplerDS_1x1, float2( texcoord.x / 4.0f, texcoord.y )).xyz;
        float3 maxValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 2.0f ) / 4.0f, texcoord.y )).xyz;
        // Black/White Point Change
        float adjBlack     = min( min( minValue.x, minValue.y ), minValue.z );    
        float adjWhite     = max( max( maxValue.x, maxValue.y ), maxValue.z );
        // Set min value
        adjBlack           = lerp( 0.0f, adjBlack, rt_bp_str );
        adjBlack           = rt_enable_blackpoint_correction ? adjBlack : 0.0f;
        // Set max value
        adjWhite           = lerp( 1.0f, adjWhite, rt_wp_str );
        adjWhite           = rt_enable_whitepoint_correction ? adjWhite : 1.0f;
        // Main color correction
        color.xyz          = saturate( color.xyz - adjBlack ) / saturate( adjWhite - adjBlack );
        //color.xyz          = tex2D( samplerDS_1_Max, texcoord ).xyz; // Debug
        //color.xyz          = tex2D( samplerDS_1_Min, texcoord ).xyz; 
        return float4( color.xyz, 1.0f );
    }

    float4 PS_StorePrev( float4 pos : SV_Position, float2 texcoord : TEXCOORD ) : SV_Target
    {
        float3 minValue    = tex2D( samplerDS_1x1, float2( texcoord.x / 4.0f, texcoord.y )).xyz;
        float3 maxValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 2.0f ) / 4.0f, texcoord.y )).xyz;
        if( pos.x < 2 )
            return float4( minValue.xyz, 1.0f );
        else
            return float4( maxValue.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_01A_RT_Correct_Contrast
    {
        pass prod80_pass1
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_MinMax_1;
            RenderTarget0      = texDS_1_Min;
            RenderTarget1      = texDS_1_Max;
        }
        pass prod80_pass2
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_MinMax_1x1;
            RenderTarget       = texDS_1x1;
        }
        pass prod80_pass3
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_CorrectContrast;
        }
        pass prod80_pass4
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_StorePrev;
            RenderTarget       = texPrevious;
        }
    }
}