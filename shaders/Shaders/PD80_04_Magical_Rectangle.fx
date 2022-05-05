/*
    Description : PD80 04 Magical Rectangle for Reshade https://reshade.me/
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
#include "PD80_00_Color_Spaces.fxh"
#include "PD80_00_Base_Effects.fxh"

namespace pd80_magicalrectangle
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int shape < __UNIFORM_COMBO_INT1
        ui_label = "Shape";
        ui_tooltip = "Shape";
        ui_category = "Shape Manipulation";
        ui_items = "Square\0Circle\0";
        > = 0;
    uniform bool invert_shape <
        ui_label = "Invert Shape";
        ui_tooltip = "Invert Shape";
        ui_category = "Shape Manipulation";
        > = false;
    uniform uint rotation <
        ui_type = "slider";
        ui_label = "Rotation Factor";
        ui_tooltip = "Rotation Factor";
        ui_category = "Shape Manipulation";
        ui_min = 0;
        ui_max = 360;
        > = 45;
    uniform float2 center <
        ui_type = "slider";
        ui_label = "Center";
        ui_tooltip = "Center";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 1.0;
        > = float2( 0.5, 0.5 );
    uniform float ret_size_x <
        ui_type = "slider";
        ui_label = "Horizontal Size";
        ui_tooltip = "Horizontal Size";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 0.5;
        > = 0.125;
    uniform float ret_size_y <
        ui_type = "slider";
        ui_label = "Vertical Size";
        ui_tooltip = "Vertical Size";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 0.5;
        > = 0.125;
    uniform float depthpos <
        ui_type = "slider";
        ui_label = "Depth Position";
        ui_tooltip = "Depth Position";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float smoothing <
        ui_type = "slider";
        ui_label = "Edge Smoothing";
        ui_tooltip = "Edge Smoothing";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.01;
    uniform float depth_smoothing <
        ui_type = "slider";
        ui_label = "Depth Smoothing";
        ui_tooltip = "Depth Smoothing";
        ui_category = "Shape Manipulation";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.002;
    uniform float dither_strength <
        ui_type = "slider";
        ui_label = "Dither Strength";
        ui_tooltip = "Dither Strength";
        ui_category = "Shape Manipulation";
        ui_min = 0.0f;
        ui_max = 10.0f;
        > = 0.0;
    uniform float3 reccolor <
        ui_text = "-------------------------------------\n"
                  "Use Opacity and Blend Mode to adjust\n"
                  "Shape controls the Shape coloring\n"
                  "Image controls the underlying picture\n"
                  "-------------------------------------";
        ui_type = "color";
        ui_label = "Shape: Color";
        ui_tooltip = "Shape: Color";
        ui_category = "Shape Coloration";
        > = float3( 0.5, 0.5, 0.5 );
    uniform float mr_exposure <
        ui_type = "slider";
        ui_label = "Image: Exposure";
        ui_tooltip = "Image: Exposure";
        ui_category = "Shape Coloration";
        ui_min = -4.0;
        ui_max = 4.0;
        > = 0.0;
    uniform float mr_contrast <
        ui_type = "slider";
        ui_label = "Image: Contrast";
        ui_tooltip = "Image: Contrast";
        ui_category = "Shape Coloration";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float mr_brightness <
        ui_type = "slider";
        ui_label = "Image: Brightness";
        ui_tooltip = "Image: Brightness";
        ui_category = "Shape Coloration";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float mr_hue <
        ui_type = "slider";
        ui_label = "Image: Hue";
        ui_tooltip = "Image: Hue";
        ui_category = "Shape Coloration";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float mr_saturation <
        ui_type = "slider";
        ui_label = "Image: Saturation";
        ui_tooltip = "Image: Saturation";
        ui_category = "Shape Coloration";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform float mr_vibrance <
        ui_type = "slider";
        ui_label = "Image: Vibrance";
        ui_tooltip = "Image: Vibrance";
        ui_category = "Shape Coloration";
        ui_min = -1.0;
        ui_max = 1.0;
        > = 0.0;
    uniform bool enable_gradient <
        ui_label = "Enable Gradient";
        ui_tooltip = "Enable Gradient";
        ui_category = "Shape Gradient";
        > = false;
    uniform bool gradient_type <
        ui_label = "Gradient Type";
        ui_tooltip = "Gradient Type";
        ui_category = "Shape Gradient";
        > = false;
    uniform float gradient_curve <
        ui_type = "slider";
        ui_label = "Gradient Curve";
        ui_tooltip = "Gradient Curve";
        ui_category = "Shape Gradient";
        ui_min = 0.001;
        ui_max = 2.0;
        > = 0.25;
    uniform float intensity_boost <
        ui_type = "slider";
        ui_label = "Intensity Boost";
        ui_tooltip = "Intensity Boost";
        ui_category = "Intensity Boost";
        ui_min = 1.0;
        ui_max = 4.0;
        > = 1.0;
    uniform int blendmode_1 < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Blendmode";
        ui_category = "Shape Blending";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity <
        ui_type = "slider";
        ui_label = "Opacity";
        ui_tooltip = "Opacity";
        ui_category = "Shape Blending";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 1.0;
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texMagicRectangle { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };

    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerMagicRectangle { Texture = texMagicRectangle; };

    //// DEFINES ////////////////////////////////////////////////////////////////////
    #define ASPECT_RATIO float( BUFFER_WIDTH * BUFFER_RCP_HEIGHT )

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    uniform bool hasdepth < source = "bufready_depth"; >;

    float3 hue( float3 res, float shift, float x )
    {
        float3 hsl = RGBToHSL( res.xyz );
        hsl.x = frac( hsl.x + ( shift + 1.0f ) / 2.0f - 0.5f );
        hsl.xyz = HSLToRGB( hsl.xyz );
        return lerp( res.xyz, hsl.xyz, x );
    }

    float curve( float x )
    {
        return x * x * x * ( x * ( x * 6.0f - 15.0f ) + 10.0f );
    }

    //// VERTEX SHADER //////////////////////////////////////////////////////////////
    /*
    Adding texcoord2 in vextex shader which is a rotated texcoord
    */
    void PPVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD, out float2 texcoord2 : TEXCOORD2)
    {
        PostProcessVS(id, position, texcoord);
        float2 uv;
        uv.x         = ( id == 2 ) ? 2.0 : 0.0;
	    uv.y         = ( id == 1 ) ? 2.0 : 0.0;
        uv.xy        -= center.xy;
        uv.y         /= ASPECT_RATIO;
        float dim    = ceil( sqrt( BUFFER_WIDTH * BUFFER_WIDTH + BUFFER_HEIGHT * BUFFER_HEIGHT )); // Diagonal size
        float maxlen = min( BUFFER_WIDTH, BUFFER_HEIGHT );
        dim          = dim / maxlen; // Scalar
        uv.xy        /= dim;
        float sin    = sin( radians( rotation ));
        float cos    = cos( radians( rotation ));
        texcoord2.x  = ( uv.x * cos ) + ( uv.y * (-sin));
        texcoord2.y  = ( uv.x * sin ) + ( uv.y * cos );
        texcoord2.xy += float2( 0.5f, 0.5f ); // Transform back
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_Layer_1( float4 pos : SV_Position, float2 texcoord : TEXCOORD, float2 texcoord2 : TEXCOORD2 ) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        // Depth stuff
        float depth       = ReShade::GetLinearizedDepth( texcoord ).x;
        // Sizing
        float dim         = ceil( sqrt( BUFFER_WIDTH * BUFFER_WIDTH + BUFFER_HEIGHT * BUFFER_HEIGHT )); // Diagonal size
        float maxlen      = max( BUFFER_WIDTH, BUFFER_HEIGHT );
        dim               = dim / maxlen; // Scalar with screen diagonal
        float2 uv         = texcoord2.xy;
        uv.xy             = uv.xy * 2.0f - 1.0f; // rescale to -1..0..1 range
        uv.xy             /= ( float2( ret_size_x + ret_size_x * smoothing, ret_size_y + ret_size_y * smoothing ) * dim ); // scale rectangle
        switch( shape )
        {
            case 0: // square
            { uv.xy       = uv.xy; } break;
            case 1: // circle
            { uv.xy       = lerp( dot( uv.xy, uv.xy ), dot( uv.xy, -uv.xy ), gradient_type ); } break;
        }
        uv.xy             = ( uv.xy + 1.0f ) / 2.0f; // scale back to 0..1 range
        
        // Using smoothstep to create values from 0 to 1, 1 being the drawn shape around center
        // First makes bottom and left side, then flips coord to make top and right side: x | 1 - x
        // Do some funky stuff with gradients
        // Finally make a depth fade
        float2 bl         = smoothstep( 0.0f, 0.0f + smoothing, uv.xy );
        float2 tr         = smoothstep( 0.0f, 0.0f + smoothing, 1.0f - uv.xy );
        if( enable_gradient )
        {
            if( gradient_type )
            {
                bl        = smoothstep( 0.0f, 0.0f + smoothing, uv.xy ) * pow( abs( uv.y ), gradient_curve );
            }
            tr            = smoothstep( 0.0f, 0.0f + smoothing, 1.0f - uv.xy ) * pow( abs( uv.x ), gradient_curve );
        }
        float depthfade   = smoothstep( depthpos - depth_smoothing, depthpos + depth_smoothing, depth );
        depthfade         = lerp( 1.0f, depthfade, hasdepth );
        // Combine them all
        float R           = bl.x * bl.y * tr.x * tr.y * depthfade;
        R                 = ( invert_shape ) ? 1.0f - R : R;
        // Blend the borders
        float intensity   = RGBToHSV( reccolor.xyz ).z;
        color.xyz         = lerp( color.xyz, saturate( color.xyz * saturate( 1.0f - R ) + R * intensity ), R );
        // Add to color, use R for Alpha
        return float4( color.xyz, R );
    }	

    float4 PS_Blend(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 orig       = tex2D( ReShade::BackBuffer, texcoord );
        float3 color;
        float4 layer_1    = saturate( tex2D( samplerMagicRectangle, texcoord ));
        // Dither
        // Input: sampler, texcoord, variance(int), enable_dither(bool), dither_strength(float), motion(bool), swing(float)
        float4 dnoise     = dither( samplerRGBNoise, texcoord.xy, 7, 1, dither_strength, 1, 0.5f );
        layer_1.xyz       = saturate( layer_1.xyz + dnoise.xyz );

        orig.xyz          = exposure( orig.xyz, mr_exposure * layer_1.w );
        orig.xyz          = con( orig.xyz, mr_contrast * layer_1.w );
        orig.xyz          = bri( orig.xyz, mr_brightness * layer_1.w );
        orig.xyz          = hue( orig.xyz, mr_hue, layer_1.w );
        orig.xyz          = sat( orig.xyz, mr_saturation * layer_1.w );
        orig.xyz          = vib( orig.xyz, mr_vibrance * layer_1.w );
        orig.xyz          = saturate( orig.xyz );
        // Doing some HSL color space conversions to colorize
        layer_1.xyz       = saturate( layer_1.xyz * intensity_boost );
        layer_1.xyz       = RGBToHSV( layer_1.xyz );
        float2 huesat     = RGBToHSV( reccolor.xyz ).xy;
        layer_1.xyz       = HSVToRGB( float3( huesat.xy, layer_1.z ));
        layer_1.xyz       = saturate( layer_1.xyz );
        // Blend mode with background
        color.xyz         = blendmode( orig.xyz, layer_1.xyz, blendmode_1, saturate( layer_1.w ) * opacity );
        // Output to screen
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_Magical_Rectangle
    < ui_tooltip = "The Magical Rectangle\n\n"
                   "This shader gives you a rectangular shape on your screen that you can manipulate in 3D space.\n"
                   "It can blend on depth, blur edges, change color, change blending, change shape, and so on.\n"
                   "It will allow you to manipulate parts of the scene in various ways. Not withstanding; add mist,\n"
                   "remove mist, change clouds, create backgrounds, draw flares, add contrasts, change hues, etc. in ways\n"
                   "another shader will not be able to do.\n\n"
                   "This shader requires access to depth buffer for full functionality!";>
    {
        pass prod80_pass0
        {
            VertexShader   = PPVS;
            PixelShader    = PS_Layer_1;
            RenderTarget   = texMagicRectangle;
        }
        pass prod80_pass1
        {
            VertexShader   = PPVS;
            PixelShader    = PS_Blend;
        }
    }
}


