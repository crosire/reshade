/*
    Description : PD80 02 LUT Creator for Reshade https://reshade.me/
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

/*
    This shader overlays the screen with a 512x512 high quality LUT.
    One can run effects on this LUT using Reshade and export the results in a PNG
    screenshot. You can take this screenshot into your favorite image editor and
    make it into a 4096x64 LUT texture that can be read by Reshade's LUT shaders.
    This way you can safe performance by using a texture to apply your favorite
    effects instead of Reshade's color manipulation shaders.

    To use:
    1. Start game with Reshade
    2. Make sure PD80_03_LUT_Creator.fx is the first shader in your effect chain and any effects that shouldn't be enabled are disabled; Bloom, DoF, Noise/Grain, Dither, MXAO, SMAA, SSR, ...
    3. Adjust the game coloring, contrasts, brightness, gamma, etc. to taste using whatever Reshade effects you please. Once happy make a screenshot in .png format
    4. Exit game and load screenshot in Photoshop or your preferred image editing program
    5. Carefully cut and paste the individual rows into a 4096x64 size image
    6. Save the new image as a .png
    7. Back in the game open whatever LUT shader you have available (f.e. PD80_02_MultiLUT_2.0.fx) and load your new LUT file name with the correct parameters (Tile size 64, Number of tiles 64, Number of LUTs 1)
    8. Disable the effects used to make the LUT (or effects are applied twice)
    
    Profit.
*/

// "HALD_64.png"
// "pd80_neutral-lut.png"

#include "ReShade.fxh"

#ifndef PD80_LC_TEXTURE_NAME
    #define PD80_LC_TEXTURE_NAME    "pd80_neutral-lut.png"
#endif

#ifndef PD80_LC_TEXTURE_WIDTH
    #define PD80_LC_TEXTURE_WIDTH   512.0
#endif

#ifndef PD80_LC_TEXTURE_HEIGHT
    #define PD80_LC_TEXTURE_HEIGHT  512.0
#endif

namespace pd80_lutoverlay
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////

    //// TEXTURES ///////////////////////////////////////////////////////////////////
    texture texPicture < source = PD80_LC_TEXTURE_NAME; > { Width = PD80_LC_TEXTURE_WIDTH; Height = PD80_LC_TEXTURE_HEIGHT; Format = RGBA8; };
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////
    sampler samplerPicture {
        Texture = texPicture;
        AddressU = CLAMP;
    	AddressV = CLAMP;
	    AddressW = CLAMP;
        };

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    
    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_OverlayLUT(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float2 coords   = float2( BUFFER_WIDTH, BUFFER_HEIGHT ) / float2( PD80_LC_TEXTURE_WIDTH, PD80_LC_TEXTURE_HEIGHT );
        coords.xy      *= texcoord.xy;
        float3 lut      = tex2D( samplerPicture, coords ).xyz;
        float3 color    = tex2D( ReShade::BackBuffer, texcoord ).xyz;
        float2 cutoff   = float2( BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT ) * float2( PD80_LC_TEXTURE_WIDTH, PD80_LC_TEXTURE_HEIGHT );
        color           = ( texcoord.y > cutoff.y || texcoord.x > cutoff.x ) ? color : lut;
        
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_02_LUT_Creator
    < ui_tooltip =  "This shader overlays the screen with a 512x512 high quality LUT.\n"
                    "One can run effects on this LUT using Reshade and export the results in a PNG\n"
                    "screenshot. You can take this screenshot into your favorite image editor and\n"
                    "make it into a 4096x64 LUT texture that can be read by Reshade's LUT shaders.\n"
                    "This way you can safe performance by using a texture to apply your favorite\n"
                    "effects instead of Reshade's color manipulation shaders.";>
    {
        pass prod80_pass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_OverlayLUT;
        }
    }
}


