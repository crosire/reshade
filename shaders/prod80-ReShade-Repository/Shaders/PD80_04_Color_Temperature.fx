/*
    Description : PD80 04 Color Temperature for Reshade https://reshade.me/
    Author      : prod80 (Bas Veth)
    License     : MIT, Copyright (c) 2020 prod80

    Additional credits
    - Taller Helland
      http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
    - Renaud Bédard https://www.shadertoy.com/view/lsSXW1
      License: https://creativecommons.org/licenses/by/3.0/

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
#include "PD80_00_Color_Spaces.fxh"

namespace pd80_colortemp
{
    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform uint Kelvin <
        ui_label = "Color Temp (K)";
        ui_tooltip = "Color Temp (K)";
        ui_category = "Kelvin";
        ui_type = "slider";
        ui_min = 1000;
        ui_max = 40000;
        > = 6500;
    uniform float LumPreservation <
        ui_label = "Luminance Preservation";
        ui_tooltip = "Luminance Preservation";
        ui_category = "Kelvin";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 1.0;
    uniform float kMix <
        ui_label = "Mix with Original";
        ui_tooltip = "Mix with Original";
        ui_category = "Kelvin";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 1.0;

    //// TEXTURES ///////////////////////////////////////////////////////////////////

    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //// FUNCTIONS //////////////////////////////////////////////////////////////////

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_ColorTemp(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color     = tex2D( ReShade::BackBuffer, texcoord );
        float3 kColor    = KelvinToRGB( Kelvin );
        float3 oLum      = RGBToHSL( color.xyz );
        float3 blended   = lerp( color.xyz, color.xyz * kColor.xyz, kMix );
        float3 resHSV    = RGBToHSL( blended.xyz );
        float3 resRGB    = HSLToRGB( float3( resHSV.xy, oLum.z ));
        color.xyz        = lerp( blended.xyz, resRGB.xyz, LumPreservation );
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_04_ColorTemperature
    {
        pass ColorTemp
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_ColorTemp;
        }
    }
}