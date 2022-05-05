/*
    Description : PD80 01 HQ Bloom for Reshade https://reshade.me/
    Author      : prod80 (Bas Veth)
    License     : MIT, Copyright (c) 2020 prod80

    Additional credits (exposure)
    - Padraic Hennessy for the logic
      https://placeholderart.wordpress.com/2014/11/21/implementing-a-physically-based-camera-manual-exposure/
    - Padraic Hennessy for the logic
      https://placeholderart.wordpress.com/2014/12/15/implementing-a-physically-based-camera-automatic-exposure/
    - MJP and David Neubelt for the method
      https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/Exposure.hlsl
      License: MIT, Copyright (c) 2016 MJP

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
#include "PD80_00_Base_Effects.fxh"

namespace pd80_hqbloom
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    // Funky stuff
    #ifndef BLOOM_ENABLE_CA
        #define BLOOM_ENABLE_CA         0
    #endif

    // Min: 0, Max: 3 | Bloom Quality, 0 is best quality (full screen) and values higher than that will progessively use lower resolution texture. Value 3 will use 1/4th screen resolution texture size
    // 0 = Fullscreen   - Ultra
    // 1 = 1/4th size   - High
    // 2 = 1/16th size  - Medium
    // Default = High quality (1) as difference is nearly impossible to tell during gameplay, and performance 60% faster than Ultra (0)
    // Using medium quality can show some artifacts because of the low resolution of texture upscaled to fullscreen when using CA, however it's very fast
    #ifndef BLOOM_QUALITY_0_TO_2
        #define BLOOM_QUALITY_0_TO_2	1
    #endif
    
    #ifndef BLOOM_LOOPCOUNT
        #define BLOOM_LOOPCOUNT         300
    #endif
    
    #ifndef BLOOM_LIMITER
        #define BLOOM_LIMITER           0.0001
    #endif

    #ifndef BLOOM_USE_FOCUS_BLOOM
        #define BLOOM_USE_FOCUS_BLOOM   0
    #endif

    // Dodgy code that should avoid some compilation errors that seem to happen sometimes for no particular reason
    #if( BLOOM_QUALITY_0_TO_2 > 2 )
        #define BLOOM_MIPLVL    2
    #elif( BLOOM_QUALITY_0_TO_2 < 0 )
        #define BLOOM_MIPLVL    0
    #else
        #define BLOOM_MIPLVL    BLOOM_QUALITY_0_TO_2
    #endif

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform bool debugBloom <
        ui_label  = "Show only bloom on screen";
        ui_category = "Bloom debug";
        > = false;
    uniform float dither_strength <
    	ui_label = "Bloom Dither Stength";
    	ui_tooltip = "Bloom Dither Stength";
    	ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 10.0;
        > = 2.0;
    uniform float BloomMix <
        ui_label = "Bloom Mix";
        ui_tooltip = "Bloom Mix";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.5;
    uniform float BloomLimit <
        ui_label = "Bloom Threshold";
        ui_tooltip = "Bloom Threshold";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.333;
    uniform float GreyValue <
        ui_label = "Bloom Exposure 50% Greyvalue";
        ui_tooltip = "Bloom Exposure 50% Greyvalue";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.333;
    uniform float bExposure <
        ui_label = "Bloom Exposure";
        ui_tooltip = "Bloom Exposure";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = -1.0;
        ui_max = 5.0;
        > = 0.0;
#if( BLOOM_USE_FOCUS_BLOOM )
    uniform float fBloomStrength <
        ui_label = "Bloom Width (Focus Center) Bias";
        ui_tooltip = "Bloom Width (Focus Center) Bias";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.25;
    uniform float BlurSigmaNarrow <
        ui_label = "Bloom Width (Focus Center)";
        ui_tooltip = "Bloom Width (Focus Center)";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 10.0;
        ui_max = 40.0;
        > = 15.0;
#endif
    uniform float BlurSigma <
        ui_label = "Bloom Width";
        ui_tooltip = "Bloom Width";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 10.0;
        ui_max = 300.0;
        > = 30.0;
    uniform float BloomSaturation <
        ui_label = "Bloom Add Saturation";
        ui_tooltip = "Bloom Add Saturation";
        ui_category = "Bloom";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 2.0;
        > = 0.0;
    #if( BLOOM_ENABLE_CA == 0 )
    uniform bool enableBKelvin <
        ui_label  = "Enable Bloom Color Temp (K)";
        ui_tooltip = "Enable Bloom Color Temp (K)";
        ui_category = "Bloom Color Temperature";
        > = false;
    uniform uint BKelvin <
        ui_type = "slider";
        ui_label = "Bloom Color Temp (K)";
        ui_tooltip = "Bloom Color Temp (K)";
        ui_category = "Bloom Color Temperature";
        ui_min = 1000;
        ui_max = 40000;
        > = 6500;
    #endif
    #if( BLOOM_ENABLE_CA == 1 )
    uniform int CA_type < __UNIFORM_COMBO_INT1
        ui_label = "Chromatic Aberration Type";
        ui_tooltip = "Chromatic Aberration Type";
        ui_category = "Chromatic Aberration";
        ui_items = "Center Weighted Radial\0Center Weighted Longitudinal\0Full screen Radial\0Full screen Longitudinal\0";
        > = 0;
    uniform bool use_only_ca <
        ui_label  = "Use only CA";
        ui_tooltip = "Use only CA";
        ui_category = "Chromatic Aberration";
        > = false;
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
        > = 60.0;
    uniform float CA_strength <
        ui_type = "slider";
        ui_label = "CA Effect Strength";
        ui_tooltip = "CA Effect Strength";
        ui_category = "Chromatic Aberration";
        ui_min = 0.0f;
        ui_max = 5.0f;
        > = 0.5;
    #endif
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texPrepLOD { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; MipLevels = 5; };
    texture texBLuma { Width = 256; Height = 256; Format = R16F; MipLevels = 9; };
    texture texBAvgLuma { Format = R16F; };
    texture texBPrevAvgLuma { Format = R16F; };
    #if( BLOOM_ENABLE_CA == 1 )
    texture texCABloom { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
    #endif
    #if( BLOOM_QUALITY_0_TO_2 == 0 )
        texture texBloomIn { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
        texture texBloomH { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
        texture texBloom { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
        texture texBloomAll { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
    #if( BLOOM_USE_FOCUS_BLOOM )
        texture texBloomHF { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
        texture texBloomF { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
    #endif
        #define SWIDTH   BUFFER_WIDTH
        #define SHEIGHT  BUFFER_HEIGHT
    #elif( BLOOM_QUALITY_0_TO_2 == 1 )
        #define SWIDTH   ( BUFFER_WIDTH / 2 )
        #define SHEIGHT  ( BUFFER_HEIGHT / 2 )
        texture texBloomIn { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomH { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloom { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomAll { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
    #if( BLOOM_USE_FOCUS_BLOOM )
        texture texBloomF { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomHF { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
    #endif
    #else
        #define SWIDTH   ( BUFFER_WIDTH / 4 )
        #define SHEIGHT  ( BUFFER_HEIGHT / 4 )
        texture texBloomIn { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomH { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloom { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomAll { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
    #if( BLOOM_USE_FOCUS_BLOOM )
        texture texBloomF { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
        texture texBloomHF { Width = SWIDTH; Height = SHEIGHT; Format = RGBA16F; };
    #endif
    #endif

    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerLODColor { Texture = texPrepLOD; };
    sampler samplerLinColor { Texture = ReShade::BackBufferTex; SRGBTexture = true; };
    sampler samplerBLuma { Texture = texBLuma; };
    sampler samplerBAvgLuma { Texture = texBAvgLuma; };
    sampler samplerBPrevAvgLuma { Texture = texBPrevAvgLuma; };
    sampler samplerBloomIn
    {
        Texture = texBloomIn;
        AddressU = BORDER;
	    AddressV = BORDER;
	    AddressW = BORDER;
    };
    sampler samplerBloomH
    {
        Texture = texBloomH;
        AddressU = BORDER;
	    AddressV = BORDER;
	    AddressW = BORDER;
    };
    #if( BLOOM_USE_FOCUS_BLOOM )
    sampler samplerBloomHF
    {
        Texture = texBloomHF;
        AddressU = BORDER;
	    AddressV = BORDER;
	    AddressW = BORDER;
    };
    sampler samplerBloomF { Texture = texBloomF; };
    #endif
    #if( BLOOM_ENABLE_CA == 1 )
    sampler samplerCABloom { Texture = texCABloom; };
    #endif
    sampler samplerBloom { Texture = texBloom; };
    sampler samplerBloomAll { Texture = texBloomAll; };
    //// DEFINES ////////////////////////////////////////////////////////////////////
    uniform float frametime < source = "frametime"; >;
    #define LumCoeff float3(0.212656, 0.715158, 0.072186)
    #define PI 3.141592f
    #define LOOPCOUNT 500.0f
    #define aspect float( BUFFER_WIDTH * BUFFER_RCP_HEIGHT )
    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float getLuminance( in float3 x )
    {
        return dot( x, LumCoeff );
    }

    float Log2Exposure( in float avgLuminance, in float GreyValue )
    {
        float exposure   = 0.0f;
        avgLuminance     = max(avgLuminance, 0.000001f);
        // GreyValue should be 0.148 based on https://placeholderart.wordpress.com/2014/11/21/implementing-a-physically-based-camera-manual-exposure/
        // But more success using higher values >= 0.5
        float linExp     = GreyValue / avgLuminance;
        exposure         = log2( linExp );
        return exposure;
    }

    float3 CalcExposedColor( in float3 color, in float avgLuminance, in float offset, in float GreyValue )
    {
        float exposure   = Log2Exposure( avgLuminance, GreyValue );
        exposure         += offset; //offset = exposure
        return exp2( exposure ) * color;
    }

    float3 screen( in float3 c, in float3 b )
    { 
        return 1.0f - ( 1.0f - c ) * ( 1.0f - b );
    }

    //// COMPUTE SHADERS ////////////////////////////////////////////////////////////
    // Not supported in ReShade (?)

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float PS_WriteBLuma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( samplerLinColor, texcoord );
        float luma       = getLuminance( color.xyz );
        luma             = max( luma, BloomLimit ); // Bloom threshold
        return log2( luma );
    }

    float PS_AvgBLuma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float luma       = tex2Dlod( samplerBLuma, float4(0.5f, 0.5f, 0, 8 )).x;
        luma             = exp2( luma );
        float prevluma   = tex2D( samplerBPrevAvgLuma, float2( 0.5f, 0.5f )).x;
        float fps        = max( 1000.0f / frametime, 0.001f );
        fps              *= 0.5f; //approx. 1 second delay to change luma between bright and dark
        float avgLuma    = lerp( prevluma, luma, saturate( 1.0f / fps )); 
        return avgLuma;
    }
    
    float4 PS_PrepLOD(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        return tex2D( ReShade::BackBuffer, texcoord );
    }

    float4 PS_BloomIn(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2Dlod( samplerLODColor, float4( texcoord.xy, 0, BLOOM_MIPLVL ));
        float luma       = tex2D( samplerBAvgLuma, float2( 0.5f, 0.5f )).x;
        luma             = clamp( luma, 0.000001f, 0.999999f );
        color.xyz        = saturate( color.xyz - luma ) / saturate( 1.0f - luma );
        color.xyz        = CalcExposedColor( color.xyz, luma, bExposure, GreyValue );
        return float4( color.xyz, 1.0f ); 
    }

    #if( !BLOOM_USE_FOCUS_BLOOM )
    float4 PS_GaussianH(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( samplerBloomIn, texcoord );
        float px         = rcp( SWIDTH );
        float SigmaSum   = 0.0f;
        float pxlOffset  = 1.5f;
        float2 buffSigma = 0.0f;
        #if( BLOOM_QUALITY_0_TO_2 == 0 )
            float bSigma = BlurSigma;
        #elif( BLOOM_QUALITY_0_TO_2 == 1 )
            float bSigma = BlurSigma * 0.5f;
        #else
            float bSigma = BlurSigma * 0.25f;
        #endif
        //Gaussian Math
        float3 Sigma;
        Sigma.x          = 1.0f / ( sqrt( 2.0f * PI ) * bSigma );
        Sigma.y          = exp( -0.5f / ( bSigma * bSigma ));
        Sigma.z          = Sigma.y * Sigma.y;

        //Center Weight
        color.xyz        *= Sigma.x;
        //Adding to total sum of distributed weights
        SigmaSum         += Sigma.x;
        //Setup next weight
        Sigma.xy         *= Sigma.yz;

        [loop]
        for( int i = 0; i < BLOOM_LOOPCOUNT && Sigma.x > BLOOM_LIMITER; ++i )
        {
            buffSigma.x  = Sigma.x * Sigma.y;
            buffSigma.y  = Sigma.x + buffSigma.x;
            color        += tex2Dlod( samplerBloomIn, float4( texcoord.xy + float2( pxlOffset * px, 0.0f ), 0, 0 )) * buffSigma.y;
            color        += tex2Dlod( samplerBloomIn, float4( texcoord.xy - float2( pxlOffset * px, 0.0f ), 0, 0 )) * buffSigma.y;
            SigmaSum     += ( 2.0f * Sigma.x + 2.0f * buffSigma.x );
            pxlOffset    += 2.0f;
            Sigma.xy     *= Sigma.yz;
            Sigma.xy     *= Sigma.yz;
        }

        color            /= SigmaSum;
        return color;
    }
    #endif

    #if( BLOOM_USE_FOCUS_BLOOM )
    void PS_GaussianH(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 bloom1 : SV_Target0, out float4 bloom2 : SV_Target1)
    {
        // This part of shader writes both wide and narrow gaussian horizontal during the same pass to avoid sampling same texture twice
        // Bloom quality setting
        #if( BLOOM_QUALITY_0_TO_2 == 0 )
            float Mult   = 1.0f;
        #elif( BLOOM_QUALITY_0_TO_2 == 1 )
            float Mult   = 0.5f;
        #else
            float Mult   = 0.25f;
        #endif

        // Required variables
        float px         = rcp( SWIDTH );
        float pxlOffset  = 1.5f;
        
        // Color
        bloom1           = tex2D( samplerBloomIn, texcoord );
        bloom2           = bloom1;

        // Wide bloom
        float b1_Sum     = 0.0f;
        float2 b1_tSigma = 0.0f;
        float b1_Width   = BlurSigma * Mult;
        float3 b1_Sigma;
        b1_Sigma.x       = 1.0f / ( sqrt( 2.0f * PI ) * b1_Width );
        b1_Sigma.y       = exp( -0.5f / ( b1_Width * b1_Width ));
        b1_Sigma.z       = b1_Sigma.y * b1_Sigma.y;
        bloom1           *= b1_Sigma.x;
        b1_Sum           += b1_Sigma.x;
        b1_Sigma.xy      *= b1_Sigma.yz;

        // Narrow bloom
        float b2_Sum     = 0.0f;
        float2 b2_tSigma = 0.0f;
        float b2_Width   = BlurSigmaNarrow * Mult;
        float3 b2_Sigma;
        b2_Sigma.x       = 1.0f / ( sqrt( 2.0f * PI ) * b2_Width );
        b2_Sigma.y       = exp( -0.5f / ( b2_Width * b2_Width ));
        b2_Sigma.z       = b2_Sigma.y * b2_Sigma.y;
        bloom2           *= b2_Sigma.x;
        b2_Sum           += b2_Sigma.x;
        b2_Sigma.xy      *= b2_Sigma.yz;

        // Temp variables
        float4 temp1;
        float4 temp2;

        [loop]
        for( int i = 0; i < BLOOM_LOOPCOUNT && b1_Sigma.x > BLOOM_LIMITER; ++i )
        {
            // Setup weights (wide)
            b1_tSigma.x  = b1_Sigma.x * b1_Sigma.y;
            b1_tSigma.y  = b1_Sigma.x + b1_tSigma.x;
            // Setup weights (narrow)
            b2_tSigma.x  = b2_Sigma.x * b2_Sigma.y;
            b2_tSigma.y  = b2_Sigma.x + b2_tSigma.x;
            // Fetch
            temp1        = tex2Dlod( samplerBloomIn, float4( texcoord.xy + float2( pxlOffset * px, 0.0f ), 0, 0 ));
            temp2        = tex2Dlod( samplerBloomIn, float4( texcoord.xy - float2( pxlOffset * px, 0.0f ), 0, 0 ));
            // Merge
            bloom1       += temp1 * b1_tSigma.y;
            bloom1       += temp2 * b1_tSigma.y;
            bloom2       += temp1 * b2_tSigma.y;
            bloom2       += temp2 * b2_tSigma.y;
            // Sum
            b1_Sum       += 2.0f * b1_tSigma.y;
            b2_Sum       += 2.0f * b2_tSigma.y;
            // Next offset
            pxlOffset    += 2.0f;
            // Next weights
            b1_Sigma.xy  *= b1_Sigma.yz;
            b1_Sigma.xy  *= b1_Sigma.yz;
            b2_Sigma.xy  *= b2_Sigma.yz;
            b2_Sigma.xy  *= b2_Sigma.yz;
        }

        bloom1           /= b1_Sum;
        bloom2           /= b2_Sum;
    }
    #endif

    float4 PS_GaussianV(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( samplerBloomH, texcoord );
        float py         = rcp( SHEIGHT );
        float SigmaSum   = 0.0f;
        float pxlOffset  = 1.5f;
        float2 buffSigma = 0.0f;
        #if( BLOOM_QUALITY_0_TO_2 == 0 )
            float bSigma = BlurSigma;
        #elif( BLOOM_QUALITY_0_TO_2 == 1 )
            float bSigma = BlurSigma * 0.5f;
        #else
            float bSigma = BlurSigma * 0.25f;
        #endif
        //Gaussian Math
        float3 Sigma;
        Sigma.x          = 1.0f / ( sqrt( 2.0f * PI ) * bSigma );
        Sigma.y          = exp( -0.5f / ( bSigma * bSigma ));
        Sigma.z          = Sigma.y * Sigma.y;

        //Center Weight
        color.xyz        *= Sigma.x;
        //Adding to total sum of distributed weights
        SigmaSum         += Sigma.x;
        //Setup next weight
        Sigma.xy         *= Sigma.yz;

        [loop]
        for( int i = 0; i < BLOOM_LOOPCOUNT && Sigma.x > BLOOM_LIMITER; ++i )
        {
            buffSigma.x  = Sigma.x * Sigma.y;
            buffSigma.y  = Sigma.x + buffSigma.x;
            color        += tex2Dlod( samplerBloomH, float4( texcoord.xy + float2( 0.0f, pxlOffset * py ), 0, 0 )) * buffSigma.y;
            color        += tex2Dlod( samplerBloomH, float4( texcoord.xy - float2( 0.0f, pxlOffset * py ), 0, 0 )) * buffSigma.y;
            SigmaSum     += ( 2.0f * Sigma.x + 2.0f * buffSigma.x );
            pxlOffset    += 2.0f;
            Sigma.xy     *= Sigma.yz;
            Sigma.xy     *= Sigma.yz;
        }

        color            /= SigmaSum;
        return color;
    }

    #if( BLOOM_USE_FOCUS_BLOOM )
    float4 PS_GaussianVF(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( samplerBloomHF, texcoord );
        float py         = rcp( SHEIGHT );
        float SigmaSum   = 0.0f;
        float pxlOffset  = 1.5f;
        float2 buffSigma = 0.0f;
        #if( BLOOM_QUALITY_0_TO_2 == 0 )
            float bSigma = BlurSigmaNarrow;
        #elif( BLOOM_QUALITY_0_TO_2 == 1 )
            float bSigma = BlurSigmaNarrow * 0.5f;
        #else
            float bSigma = BlurSigmaNarrow * 0.25f;
        #endif
        //Gaussian Math
        float3 Sigma;
        Sigma.x          = 1.0f / ( sqrt( 2.0f * PI ) * bSigma );
        Sigma.y          = exp( -0.5f / ( bSigma * bSigma ));
        Sigma.z          = Sigma.y * Sigma.y;

        //Center Weight
        color.xyz        *= Sigma.x;
        //Adding to total sum of distributed weights
        SigmaSum         += Sigma.x;
        //Setup next weight
        Sigma.xy         *= Sigma.yz;

        [loop]
        for( int i = 0; i < BLOOM_LOOPCOUNT && Sigma.x > BLOOM_LIMITER; ++i )
        {
            buffSigma.x  = Sigma.x * Sigma.y;
            buffSigma.y  = Sigma.x + buffSigma.x;
            color        += tex2Dlod( samplerBloomHF, float4( texcoord.xy + float2( 0.0f, pxlOffset * py ), 0, 0 )) * buffSigma.y;
            color        += tex2Dlod( samplerBloomHF, float4( texcoord.xy - float2( 0.0f, pxlOffset * py ), 0, 0 )) * buffSigma.y;
            SigmaSum     += ( 2.0f * Sigma.x + 2.0f * buffSigma.x );
            pxlOffset    += 2.0f;
            Sigma.xy     *= Sigma.yz;
            Sigma.xy     *= Sigma.yz;
        }

        color            /= SigmaSum;
        return color;
    }
    #endif

    float4 PS_Combine(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 widebloom = tex2D( samplerBloom, texcoord );
        #if( BLOOM_USE_FOCUS_BLOOM )
        float4 narrbloom = tex2D( samplerBloomF, texcoord );
        return saturate( widebloom * ( 1.0 - fBloomStrength ) + narrbloom * fBloomStrength );
        #else
        return widebloom;
        #endif
    }

    #if( BLOOM_ENABLE_CA )
    float4 PS_CA(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = 0.0f;
        float3 orig       = tex2D( samplerBloomAll, texcoord ).xyz;
        float px          = BUFFER_RCP_WIDTH;
        float py          = BUFFER_RCP_HEIGHT;

        float2 coords     = texcoord.xy * 2.0f - 1.0f;
        float2 uv         = coords.xy;
        coords.xy         /= float2( 1.0f / aspect, 1.0f );
        float2 caintensity= length( coords.xy ); // * 2.0f for higher weight in center
        caintensity.y     = caintensity.x * caintensity.x + 1.0f;
        caintensity.x     = 1.0f - ( 1.0f / ( caintensity.y * caintensity.y ));

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

        float3 huecolor   = 0.0f;
        float3 temp       = 0.0f;
        float o1          = 7.0f;
        float o2          = 0.0f;
        float3 d          = 0.0f;

        // Scale CA (hackjob!)
        float caWidth     = CA * ( max( BUFFER_WIDTH, BUFFER_HEIGHT ) / 1920.0f ); // Scaled for 1920, raising resolution in X or Y should raise scale

        float offsetX     = px * c * caintensity.x;
        float offsetY     = py * s * caintensity.x;

        for( float i = 0; i < 8; ++i )
        {
            huecolor.xyz  = HUEToRGB( i / 8.0f );
            o2            = lerp( -caWidth, caWidth, i / o1 );
            temp.xyz      = tex2D( samplerBloomAll, texcoord.xy + float2( o2 * offsetX, o2 * offsetY )).xyz;
            color.xyz     += temp.xyz * huecolor.xyz;
            d.xyz         += huecolor.xyz;
        }
        color.xyz           /= dot( d.xyz, 0.333333f ); // seems so-so OK
        color.xyz           = lerp( orig.xyz, color.xyz, CA_strength );
        color.xyz           = lerp( color.xyz, color.xyz - orig.xyz, use_only_ca );
        return float4( color.xyz, 1.0f );
    }
    #endif

    float4 PS_Gaussian(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        #if( !BLOOM_ENABLE_CA )
        float4 bloom     = tex2D( samplerBloomAll, texcoord );
        #endif
        #if( BLOOM_ENABLE_CA )
        float4 bloom     = tex2D( samplerCABloom, texcoord );
        #endif
        float4 color     = tex2D( ReShade::BackBuffer, texcoord );
        // Dither
        float4 dnoise    = dither( samplerRGBNoise, texcoord.xy, 0, 1, dither_strength, 1, 2.0f - ( 1.0f - BloomLimit ) );
        float3 steps     = smoothstep( 0.0f, 0.012f, bloom.xyz );
        bloom.xyz        = saturate( bloom.xyz + dnoise.xyz * steps.xyz );

        #if( BLOOM_ENABLE_CA == 0 )
        if( enableBKelvin )
        {
            float3 K       = KelvinToRGB( BKelvin );
            float3 bLum    = RGBToHSL( bloom.xyz );
            float3 retHSV  = RGBToHSL( bloom.xyz * K.xyz );
            bloom.xyz      = HSLToRGB( float3( retHSV.xy, bLum.z ));
        }
        #endif
        // Vibrance
        bloom.xyz        = vib( bloom.xyz, BloomSaturation );
        float3 bcolor    = screen( color.xyz, bloom.xyz );
        color.xyz        = lerp( color.xyz, bcolor.xyz, BloomMix );
        color.xyz        = debugBloom ? bloom.xyz : color.xyz; // render only bloom to screen
        return float4( color.xyz, 1.0f );
    }

    float PS_PrevAvgBLuma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float avgLuma    = tex2D( samplerBAvgLuma, float2( 0.5f, 0.5f )).x;
        return avgLuma;
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_02_Bloom
        < ui_tooltip = "Bloom\n\n"
			   "Bloom is an effect that causes diffraction of light around bright reflective or emittive sources\n\n"
               "Preprocessor Settings\n\n"
               "BLOOM_ENABLE_CA: Enables a chromatic aberration effect on bloom\n\n"
               "BLOOM_QUALITY_0_TO_2: Sets the quality, 0 is full (and heavy!), 2 is low and very fast,\n"
               "1 is high quality and best trade off between quality and performance\n\n"
               "BLOOM_LOOPCOUNT: Limit to the amount of loops of the Width effect. Wider blooms may need higher\n"
               "values (eg. max width is 300, this value should be 300)\n\n"
               "BLOOM_LIMITER: Limiter to the bloom. Wider blooms may need lower values or the bloom starts to look\n"
               "rectangular (eg. 0.0001 (default) is good to about width 100, after that start to decrease this value)\n\n"
               "BLOOM_USE_FOCUS_BLOOM: Enables another pass to add a narrow bloom on top of the wide bloom";>
    {
        pass BLuma
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_WriteBLuma;
            RenderTarget   = texBLuma;
        }
        pass AvgBLuma
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_AvgBLuma;
            RenderTarget   = texBAvgLuma;
        }
        pass PrepLod
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_PrepLOD;
            RenderTarget   = texPrepLOD;
        }
        pass BloomIn
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_BloomIn;
            RenderTarget   = texBloomIn;
        }
    #if( BLOOM_USE_FOCUS_BLOOM )
        pass GaussianH
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_GaussianH;
            RenderTarget0  = texBloomH;
            RenderTarget1  = texBloomHF;
        }
    #endif
    #if( !BLOOM_USE_FOCUS_BLOOM )
        pass GaussianH
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_GaussianH;
            RenderTarget   = texBloomH;
        }
    #endif
        pass GaussianV
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_GaussianV;
            RenderTarget   = texBloom;
        }
    #if( BLOOM_USE_FOCUS_BLOOM )
        pass GaussianVF
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_GaussianVF;
            RenderTarget   = texBloomF;
        }
    #endif
        pass Combine
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_Combine;
            RenderTarget   = texBloomAll;
        }
        #if( BLOOM_ENABLE_CA == 0 )
        pass GaussianBlur
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_Gaussian;
        }
        #endif
        #if( BLOOM_ENABLE_CA == 1 )
        pass AddCA
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_CA;
            RenderTarget   = texCABloom;
        }
        pass GaussianBlur
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_Gaussian;
        }
        #endif
        pass PreviousBLuma
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_PrevAvgBLuma;
            RenderTarget   = texBPrevAvgLuma;
        }
    }
}