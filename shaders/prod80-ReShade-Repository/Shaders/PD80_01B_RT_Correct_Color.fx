/*
    Description : PD80 01B RT Correct Color for Reshade https://reshade.me/
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

namespace pd80_correctcolor
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////
    #ifndef RT_PRECISION_LEVEL_0_TO_4
        #define RT_PRECISION_LEVEL_0_TO_4       0
    #endif

    #ifndef RT_ENABLE_HIGH_PERF_MODE
        #define RT_ENABLE_HIGH_PERF_MODE        0
    #endif

    #if( RT_ENABLE_HIGH_PERF_MODE > 1 || RT_ENABLE_HIGH_PERF_MODE < 0 )
        #error "RT_ENABLE_HIGH_PERF_MODE has a range of 0 to 1"
    #endif

    #if( RT_PRECISION_LEVEL_0_TO_4 > 4 || RT_ENABLE_HIGH_PERF_MODE < 0 )
        #error "RT_PRECISION_LEVEL_0_TO_4 has a range of 0 to 4"
    #endif

    //// DEFINES ////////////////////////////////////////////////////////////////////
#if( RT_PRECISION_LEVEL_0_TO_4 == 0 )
    #define RT_RES      1
    #define RT_MIPLVL   0
#elif( RT_PRECISION_LEVEL_0_TO_4 == 1 )
    #define RT_RES      2
    #define RT_MIPLVL   1
#elif( RT_PRECISION_LEVEL_0_TO_4 == 2 )
    #define RT_RES      4
    #define RT_MIPLVL   2
#elif( RT_PRECISION_LEVEL_0_TO_4 == 3 )
    #define RT_RES      8
    #define RT_MIPLVL   3
#else
    #define RT_RES      16
    #define RT_MIPLVL   4
#endif
    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int debug_mode < __UNIFORM_COMBO_INT1
        ui_label = "Debug Mode";
        ui_tooltip = "Debug Mode";
        ui_category = "Debug Mode";
        ui_items = "Default\0Min Color Texture\0Max Color Texture\0Mid Color Texture\0";
        > = 0;
    uniform bool enable_fade <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Time Based Fade";
        ui_tooltip = "Enable Time Based Fade";
        ui_category = "Global: Remove Tint";
        > = true;
    uniform bool freeze <
        ui_label = "Freeze Correction";
        ui_tooltip = "Freeze Correction";
        ui_category = "Global: Remove Tint";
        > = false;
    uniform float transition_speed <
        ui_type = "slider";
        ui_label = "Time Based Fade Speed";
        ui_tooltip = "Time Based Fade Speed";
        ui_category = "Global: Remove Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.5;
    uniform bool enable_dither <
        ui_label = "Enable Dithering";
        ui_tooltip = "Enable Dithering";
        ui_category = "Global: Remove Tint";
        > = true;
    uniform float dither_strength <
        ui_type = "slider";
        ui_label = "Dither Strength";
        ui_tooltip = "Dither Strength";
        ui_category = "Global: Remove Tint";
        ui_min = 0.0f;
        ui_max = 10.0f;
        > = 1.0;
    uniform bool rt_enable_whitepoint_correction <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Whitepoint Correction";
        ui_tooltip = "Enable Whitepoint Correction";
        ui_category = "Whitepoint: Remove Tint";
        > = true;
    uniform bool rt_whitepoint_respect_luma <
        ui_label = "Respect Luma";
        ui_tooltip = "Whitepoint: Respect Luma";
        ui_category = "Whitepoint: Remove Tint";
        > = true;
    uniform int rt_whitepoint_method < __UNIFORM_COMBO_INT1
        ui_label = "Color Detection Method";
        ui_category = "Whitepoint: Remove Tint";
        ui_items = "By Color Channel (auto-color)\0Find Light Color (auto-tone)\0";
        > = 0;
    uniform float rt_wp_str <
        ui_type = "slider";
        ui_label = "White Point Correction Strength";
        ui_tooltip = "Whitepoint: White Point Correction Strength";
        ui_category = "Whitepoint: Remove Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform float rt_wp_rl_str <
        ui_type = "slider";
        ui_label = "White Point Respect Luma Strength";
        ui_tooltip = "Whitepoint: White Point Respect Luma Strength";
        ui_category = "Whitepoint: Remove Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform bool rt_enable_blackpoint_correction <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Blackpoint Correction";
        ui_tooltip = "Enable Blackpoint Correction";
        ui_category = "Blackpoint: Remove Tint";
        > = true;
    uniform bool rt_blackpoint_respect_luma <
        ui_label = "Respect Luma";
        ui_tooltip = "Blackpoint: Respect Luma";
        ui_category = "Blackpoint: Remove Tint";
        > = false;
    uniform int rt_blackpoint_method < __UNIFORM_COMBO_INT1
        ui_label = "Color Detection Method";
        ui_category = "Blackpoint: Remove Tint";
        ui_items = "By Color Channel (auto-color)\0Find Dark Color  (auto-tone)\0";
        > = 1;
    uniform float rt_bp_str <
        ui_type = "slider";
        ui_label = "Black Point Correction Strength";
        ui_tooltip = "Blackpoint: Black Point Correction Strength";
        ui_category = "Blackpoint: Remove Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform float rt_bp_rl_str <
        ui_type = "slider";
        ui_label = "Black Point Respect Luma Strength";
        ui_tooltip = "Blackpoint: Black Point Respect Luma Strength";
        ui_category = "Blackpoint: Remove Tint";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
    uniform bool rt_enable_midpoint_correction <
        ui_text = "----------------------------------------------";
        ui_label = "Enable Midtone Correction";
        ui_tooltip = "Enable Midtone Correction";
        ui_category = "Midtone: Remove Tint";
        > = true;
    uniform bool rt_midpoint_respect_luma <
        ui_label = "Respect Luma";
        ui_tooltip = "Midtone: Respect Luma";
        ui_category = "Midtone: Remove Tint";
        > = true;
    uniform bool mid_use_alt_method <
        ui_label = "Use average Dark-Light as Mid";
        ui_tooltip = "Midtone: Use average Dark-Light as Mid";
        ui_category = "Midtone: Remove Tint";
        > = true;
    uniform float midCC_scale <
        ui_type = "slider";
        ui_label = "Midtone Correction Scale";
        ui_tooltip = "Midtone: Midtone Correction Scale";
        ui_category = "Midtone: Remove Tint";
        ui_min = 0.0f;
        ui_max = 5.0f;
        > = 0.5;

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texColor { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; MipLevels = 5; };
    texture texDS_1_Max { Width = 32; Height = 32; Format = RGBA16F; };
    texture texDS_1_Min { Width = 32; Height = 32; Format = RGBA16F; };
    texture texDS_1_Mid { Width = 32; Height = 32; Format = RGBA16F; };
    texture texDS_1x1 { Width = 6; Height = 2; Format = RGBA16F; };
    texture texPrevious { Width = 6; Height = 2; Format = RGBA16F; };

    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerColor { Texture = texColor; };
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
    sampler samplerDS_1_Mid
    {
        Texture = texDS_1_Mid;
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
    sampler samplerPrevious
    { 
        Texture   = texPrevious;
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
    float4 PS_WriteColor(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        return tex2D( ReShade::BackBuffer, texcoord );
    }
    //Downscale to 32x32 min/max color matrix
    void PS_MinMax_1( float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 minValue : SV_Target0, out float4 maxValue : SV_Target1, out float4 midValue : SV_Target2 )
    {
        float3 currColor;
        float3 minMethod0  = 1.0f;
        float3 minMethod1  = 1.0f;
        float3 maxMethod0  = 0.0f;
        float3 maxMethod1  = 0.0f;
        midValue           = 0.0f;

        float getMid;   float getMid2;
        float getMin;   float getMin2;
        float getMax;   float getMax2;

        float3 prevMin     = tex2D( samplerPrevious, float2( texcoord.x / 6.0f, texcoord.y )).xyz;
        float3 prevMax     = tex2D( samplerPrevious, float2(( texcoord.x + 4.0f ) / 6.0f, texcoord.y )).xyz;
        float middle       = dot( float2( dot( prevMin.xyz, 0.333333f ), dot( prevMax.xyz, 0.333333f )), 0.5f );
        middle             = ( mid_use_alt_method ) ? middle : 0.5f;

        // RenderTarget size is 32x32
        float pst          = 0.03125f;    // rcp( 32 )
        float hst          = 0.5f * pst;  // half size

        // Sample texture
        float2 stexSize    = float2( BUFFER_WIDTH/RT_RES, BUFFER_HEIGHT/RT_RES );
        uint OFFSET        = 1 + RT_ENABLE_HIGH_PERF_MODE * 3;
        float2 start       = floor(( texcoord.xy - hst ) * stexSize.xy );    // sample block start position
        float2 stop        = floor(( texcoord.xy + hst ) * stexSize.xy );    // ... end position

        [loop]
        for( int y = start.y; y < stop.y && y < stexSize.y; y += OFFSET )
        {
            for( int x = start.x; x < stop.x && x < stexSize.x; x += OFFSET )
            {
                currColor      = tex2Dfetch( samplerColor, int2( x, y ), RT_MIPLVL ).xyz;
                // Dark color detection methods
                // Per channel
                minMethod0.xyz = min( minMethod0.xyz, currColor.xyz );
                // By color
                getMin         = max( max( currColor.x, currColor.y ), currColor.z ) + dot( currColor.xyz, 1.0f );
                getMin2        = max( max( minMethod1.x, minMethod1.y ), minMethod1.z ) + dot( minMethod1.xyz, 1.0f );
                minMethod1.xyz = ( getMin2 >= getMin ) ? currColor.xyz : minMethod1.xyz;
                // Mid point detection
                getMid         = dot( abs( currColor.xyz - middle ), 1.0f );
                getMid2        = dot( abs( midValue.xyz - middle ), 1.0f );
                midValue.xyz   = ( getMid2 >= getMid ) ? currColor.xyz : midValue.xyz;
                // Light color detection methods
                // Per channel
                maxMethod0.xyz = max( currColor.xyz, maxMethod0.xyz );
                // By color
                getMax         = dot( currColor.xyz, 1.0f );
                getMax2        = dot( maxMethod1.xyz, 1.0f );
                maxMethod1.xyz = ( getMax >= getMax2 ) ? currColor.xyz : maxMethod1.xyz;
            }
        }

        minValue.xyz       = rt_blackpoint_method ? minMethod1.xyz : minMethod0.xyz;
        maxValue.xyz       = rt_whitepoint_method ? maxMethod1.xyz : maxMethod0.xyz;
        // Return
        minValue           = float4( minValue.xyz, 1.0f );
        maxValue           = float4( maxValue.xyz, 1.0f );
        midValue           = float4( midValue.xyz, 1.0f );
    }

    //Downscale to 32x32 to 1x1 min/max colors
    float4 PS_MinMax_1x1( float4 pos : SV_Position, float2 texcoord : TEXCOORD ) : SV_Target
    {
        float3 minColor; float3 maxColor; float3 midColor;
        float3 minValue; float3 maxValue; float3 midValue;
        float getMin;    float getMin2;
        float getMax;    float getMax2;
        float3 minMethod0  = 1.0f;
        float3 minMethod1  = 1.0f;
        float3 maxMethod0  = 0.0f;
        float3 maxMethod1  = 0.0f;
        midValue           = 0.0f;
        //Get texture resolution
        int2 SampleRes     = 32;
        float Sigma        = 0.0f;

        for( int y = 0; y < SampleRes.y; ++y )
        {
            for( int x = 0; x < SampleRes.x; ++x )
            {   
                // Dark color detection methods
                minColor       = tex2Dfetch( samplerDS_1_Min, int2( x, y ), 0 ).xyz;
                // Per channel
                minMethod0.xyz = min( minMethod0.xyz, minColor.xyz );
                // By color
                getMin         = max( max( minColor.x, minColor.y ), minColor.z ) + dot( minColor.xyz, 1.0f );
                getMin2        = max( max( minMethod1.x, minMethod1.y ), minMethod1.z ) + dot( minMethod1.xyz, 1.0f );
                minMethod1.xyz = ( getMin2 >= getMin ) ? minColor.xyz : minMethod1.xyz;
                // Mid point detection
                midColor       += tex2Dfetch( samplerDS_1_Mid, int2( x, y ), 0 ).xyz;
                Sigma          += 1.0f;
                // Light color detection methods
                maxColor       = tex2Dfetch( samplerDS_1_Max, int2( x, y ), 0 ).xyz;
                // Per channel
                maxMethod0.xyz = max( maxColor.xyz, maxMethod0.xyz );
                // By color
                getMax         = dot( maxColor.xyz, 1.0f );
                getMax2        = dot( maxMethod1.xyz, 1.0f );
                maxMethod1.xyz = ( getMax >= getMax2 ) ? maxColor.xyz : maxMethod1.xyz;
            }
        }

        minValue.xyz       = rt_blackpoint_method ? minMethod1.xyz : minMethod0.xyz;
        maxValue.xyz       = rt_whitepoint_method ? maxMethod1.xyz : maxMethod0.xyz;
        midValue.xyz       = midColor.xyz / Sigma;
        // When minValue = maxValue means the entire color is thrown out with correction
        // This is correct behavior because this means that color component has the same fixed value on ALL pixels
        // No game developer should ever make a coloring like that, but, you never know
        maxValue.xyz       = ( minValue.xyz >= maxValue.xyz ) ? float3( 1.0f, 1.0f, 1.0f ) : maxValue.xyz;
        // Try and avoid some flickering
        // Not really working consistently, too radical changes in min values sometimes
        float3 prevMin     = tex2D( samplerPrevious, float2( texcoord.x / 6.0f, texcoord.y )).xyz;
        float3 prevMid     = tex2D( samplerPrevious, float2(( texcoord.x + 2.0f ) / 6.0f, texcoord.y )).xyz;
        float3 prevMax     = tex2D( samplerPrevious, float2(( texcoord.x + 4.0f ) / 6.0f, texcoord.y )).xyz;
        float smoothf      = transition_speed * 4.0f + 1.0f;
        float time         = frametime * 0.001f;
        maxValue.xyz       = enable_fade ? interpolate( prevMax.xyz, maxValue.xyz, smoothf, time ) : maxValue.xyz;
        minValue.xyz       = enable_fade ? interpolate( prevMin.xyz, minValue.xyz, smoothf, time ) : minValue.xyz;
        midValue.xyz       = enable_fade ? interpolate( prevMid.xyz, midValue.xyz, smoothf, time ) : midValue.xyz;
        // Freeze Correction
        maxValue.xyz       = freeze ? prevMax.xyz : maxValue.xyz;
        minValue.xyz       = freeze ? prevMin.xyz : minValue.xyz;
        midValue.xyz       = freeze ? prevMid.xyz : midValue.xyz;
        // Return
        if( pos.x < 2 )
            return float4( minValue.xyz, 1.0f );
        else if( pos.x >= 2 && pos.x < 4 )
            return float4( midValue.xyz, 1.0f );
        else
            return float4( maxValue.xyz, 1.0f );
        return float4( 0.5, 0.5, 0.5, 1.0 );
    }

    float4 PS_RemoveTint(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color       = tex2D( samplerColor, texcoord );
        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise      = dither( samplerRGBNoise, texcoord.xy, 0, enable_dither, dither_strength, 1, 0.5f );
        color.xyz          = saturate( color.xyz + dnoise.xyz );
        // Grab min, max, mid values
        float3 minValue    = tex2D( samplerDS_1x1, float2( texcoord.x / 6.0f, texcoord.y )).xyz;
        float3 midValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 2.0f ) / 6.0f, texcoord.y )).xyz;
        float3 maxValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 4.0f ) / 6.0f, texcoord.y )).xyz;
        // Get middle correction method
        float middle       = dot( float2( dot( minValue.xyz, 0.333333f ), dot( maxValue.xyz, 0.333333f )), 0.5f );
        middle             = mid_use_alt_method ? middle : 0.5f;
        // Set min value
        minValue.xyz       = lerp( 0.0f, minValue.xyz, rt_bp_str );
        minValue.xyz       = rt_enable_blackpoint_correction ? minValue.xyz : 0.0f;
        // Set max value
        maxValue.xyz       = lerp( 1.0f, maxValue.xyz, rt_wp_str );
        maxValue.xyz       = rt_enable_whitepoint_correction ? maxValue.xyz : 1.0f;
        // Set mid value
        midValue.xyz       = midValue.xyz - middle;
        midValue.xyz       *= midCC_scale;
        midValue.xyz       = rt_enable_midpoint_correction ? midValue.xyz : 0.0f;
        // Main color correction
        color.xyz          = saturate( color.xyz - minValue.xyz ) / ( maxValue.xyz - minValue.xyz );
        // White Point luma preservation
        float avgMax       = dot( maxValue.xyz, 0.333333f );
        color.xyz          = lerp( color.xyz, color.xyz * avgMax, rt_whitepoint_respect_luma * rt_wp_rl_str );
        // Black Point luma preservation
        float avgMin       = dot( minValue.xyz, 0.333333f );
        color.xyz          = lerp( color.xyz, color.xyz * ( 1.0f - avgMin ) + avgMin, rt_blackpoint_respect_luma * rt_bp_rl_str );
        // Mid Point correction
        float avgCol       = dot( color.xyz, 0.333333f ); // Avg after main correction
        float avgMid       = dot( midValue.xyz, 0.333333f );
        avgCol             = 1.0f - abs( avgCol * 2.0f - 1.0f );
        color.xyz          = saturate( color.xyz - midValue.xyz * avgCol + avgMid * avgCol * rt_midpoint_respect_luma );
        // Debug
        switch( debug_mode )
        {
            case 0: { color.xyz = color.xyz; } break;
            case 1: { color.xyz = tex2D( samplerDS_1_Min, texcoord ).xyz; } break;
            case 2: { color.xyz = tex2D( samplerDS_1_Max, texcoord ).xyz; } break;
            case 3: { color.xyz = tex2D( samplerDS_1_Mid, texcoord ).xyz; } break;
        }
        return float4( color.xyz, 1.0f );
    }

    float4 PS_StorePrev( float4 pos : SV_Position, float2 texcoord : TEXCOORD ) : SV_Target
    {
        float3 minValue    = tex2D( samplerDS_1x1, float2( texcoord.x / 6.0f, texcoord.y )).xyz;
        float3 midValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 2.0f ) / 6.0f, texcoord.y )).xyz;
        float3 maxValue    = tex2D( samplerDS_1x1, float2(( texcoord.x + 4.0f ) / 6.0f, texcoord.y )).xyz;
        if( pos.x < 2 )
            return float4( minValue.xyz, 1.0f );
        else if( pos.x >= 2 && pos.x < 4 )
            return float4( midValue.xyz, 1.0f );
        else
            return float4( maxValue.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_01B_RT_Correct_Color
    < ui_tooltip = "Remove Tint/Color Cast\n\n"
			   "Automatically adjust Blackpoint, Whitepoint, and remove color tints/casts while enhancing contrast.\n"
               "Both correcting per individual channel, as well as Light/Dark colors are supported.\n"
               "This shader will not adjust tinting applied in gamma, and this is considered out of scope.\n\n"
			   
               "RT_PRECISION_LEVEL_0_TO_4\n"
               "Sets the precision level in detecting the white and black points. Higher levels mean less precision and more color removal.\n"
               "Too high values will remove significant amounts of color and may cause shifts in color, contrast, or banding artefacts.";>
    {
        pass prod80_pass0
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_WriteColor;
            RenderTarget       = texColor;
        }
        pass prod80_pass1
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_MinMax_1;
            RenderTarget0      = texDS_1_Min;
            RenderTarget1      = texDS_1_Max;
            RenderTarget2      = texDS_1_Mid;
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
            PixelShader        = PS_RemoveTint;
        }
        pass prod80_pass4
        {
            VertexShader       = PostProcessVS;
            PixelShader        = PS_StorePrev;
            RenderTarget       = texPrevious;
        }
    }
}