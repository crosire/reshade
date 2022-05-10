/*
    Description : PD80 06 Posterize Pixelate for Reshade https://reshade.me/
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

namespace pd80_posterizepixelate
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int number_of_levels <
        ui_label = "Number of Levels";
        ui_tooltip = "Number of Levels";
        ui_category = "Posterize";
        ui_type = "slider";
        ui_min = 2;
        ui_max = 255;
        > = 255;
	uniform int pixel_size <
		ui_label = "Pixel Size";
        ui_tooltip = "Pixel Size";
        ui_category = "Pixelate";
        ui_type = "slider";
        ui_min = 1;
        ui_max = 9;
        > = 1;
    uniform float effect_strength <
        ui_type = "slider";
        ui_label = "Effect Strength";
        ui_tooltip = "Effect Strength";
        ui_category = "Posterize Pixelate";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 1.0;
	uniform float border_str <
        ui_type = "slider";
        ui_label = "Border Strength";
        ui_tooltip = "Border Strength";
        ui_category = "Posterize Pixelate";
        ui_min = 0.0f;
        ui_max = 1.0f;
        > = 0.0;
    uniform bool enable_dither <
        ui_label = "Enable Dithering";
        ui_tooltip = "Enable Dithering";
        ui_category = "Posterize Pixelate";
        > = false;
    uniform bool dither_motion <
        ui_label = "Dither Motion";
        ui_tooltip = "Dither Motion";
        ui_category = "Posterize Pixelate";
        > = true;
    uniform float dither_strength <
        ui_type = "slider";
        ui_label = "Dither Strength";
        ui_tooltip = "Dither Strength";
        ui_category = "Posterize Pixelate";
        ui_min = 0.0f;
        ui_max = 10.0f;
        > = 1.0;
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texMipMe { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; MipLevels = 9; };
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerMipMe
    {
        Texture = texMipMe;
        MipFilter = POINT;
        MinFilter = POINT;
        MagFilter = POINT;
    };
	
    //// DEFINES ////////////////////////////////////////////////////////////////////
    #define aspect      float( BUFFER_WIDTH * BUFFER_RCP_HEIGHT )
    
    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    uniform float2 pingpong < source = "pingpong"; min = 0; max = 128; step = 1; >;

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_MipMe(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        return tex2D( ReShade::BackBuffer, texcoord );
    }

    float4 PS_Posterize(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float3 color      = tex2Dlod( samplerMipMe, float4( texcoord.xy, 0.0f, pixel_size - 1 )).xyz;
        float exp         = exp2( pixel_size - 1 );
        float rcp_exp     = max( rcp( exp ) - 0.00001f, 0.0f ); //  - 0.00001f because fp precision comes into play
        float2 bwbh       = float2( floor( BUFFER_WIDTH / exp ), floor( BUFFER_HEIGHT / exp ));
        // Dither
        float2 tx         = bwbh.xy / 512.0f;
        tx.xy             *= texcoord.xy;
        float3 dnoise     = tex2D( samplerRGBNoise, tx ).xyz;
        float mot         = dither_motion ? pingpong.x + 9 : 1.0f;
        dnoise.xyz        = frac( dnoise + 0.61803398875f * mot );
        dnoise.xyz        = dnoise * 2.0f - 1.0f;
        color.xyz         = enable_dither ? saturate( color.xyz + dnoise.xyz * ( dither_strength / number_of_levels )) : color.xyz;
        // Dither end
        float3 orig       = color.xyz;
        color.xyz         = floor( color.xyz * number_of_levels ) / ( number_of_levels - 1 );
        float2 uv         = frac( texcoord.xy * bwbh.xy );
        float grade       = ( uv.x <= rcp_exp ) ? 1.0 : 0.0; 
        grade            += ( uv.y <= rcp_exp ) ? 1.0 : 0.0;
        color.xyz         = lerp( color.xyz, lerp( color.xyz, 0.0f, border_str * saturate( pixel_size - 1 )), saturate( grade ));
        color.xyz         = lerp( orig.xyz, color.xyz, effect_strength );
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_06_Posterize_Pixelate
    {
        pass prod80_pass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_MipMe;
            RenderTarget   = texMipMe;
        }
        pass prod80_pass1
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_Posterize;
        }
    }
}