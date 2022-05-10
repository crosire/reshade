/*
    Description : PD80 01 Color Gamut for Reshade https://reshade.me/
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

namespace pd80_colorconversion
{
    //// PREPROCESSOR DEFINITIONS ///////////////////////////////////////////////////

    //// UI ELEMENTS ////////////////////////////////////////////////////////////////
    uniform int colorgamut < 
        ui_type = "combo";
        ui_items= "No Change\0Adobe RGB\0Apple RGB\0Best RGB\0Beta RGB\0Bruce RGB\0CIE RGB\0Color Match\0Don\0ECI\0Ekta Space PS5\0NTSC\0PAL-SECAM\0ProPhoto\0SMPTE-C\0Wide Gamut RGB\0";
        ui_label = "Color Gamut Selection";
        ui_tooltip = "Color Gamut Selection";
        > = 0;
    //// TEXTURES ///////////////////////////////////////////////////////////////////
    
    //// SAMPLERS ///////////////////////////////////////////////////////////////////

    //// DEFINES ////////////////////////////////////////////////////////////////////

    //D65
    static const float3x3 sRGB_To_XYZ = float3x3(
    0.4124564,  0.3575761,  0.1804375,
    0.2126729,  0.7151522,  0.0721750,
    0.0193339,  0.1191920,  0.9503041);

    //D65 (0)
    static const float3x3 XYZ_To_sRGB = float3x3(
    3.2404542, -1.5371385, -0.4985314,
   -0.9692660,  1.8760108,  0.0415560,
    0.0556434, -0.2040259,  1.0572252);

    //D65 (1)
    static const float3x3 XYZ_To_AdobeRGB = float3x3(
    2.0413690, -0.5649464, -0.3446944,
   -0.9692660,  1.8760108,  0.0415560,
    0.0134474, -0.1183897,  1.0154096);

    //D65 (2)
    static const float3x3 XYZ_To_AppleRGB = float3x3(
    2.9515373, -1.2894116, -0.4738445,
   -1.0851093,  1.9908566,  0.0372026,
    0.0854934, -0.2694964,  1.0912975);

    //D50 (3)
    static const float3x3 XYZ_To_BestRGB = float3x3(
    1.7552599, -0.4836786, -0.2530000,
   -0.5441336,  1.5068789,  0.0215528,
    0.0063467, -0.0175761,  1.2256959);

    //D50 (4)
    static const float3x3 XYZ_To_BetaRGB = float3x3(
    1.6832270, -0.4282363, -0.2360185,
   -0.7710229,  1.7065571,  0.0446900,
    0.0400013, -0.0885376,  1.2723640);

    //D65 (5)
    static const float3x3 XYZ_To_BruceRGB = float3x3(
    2.7454669, -1.1358136, -0.4350269,
   -0.9692660,  1.8760108,  0.0415560,
    0.0112723, -0.1139754,  1.0132541);

    //E (6)
    static const float3x3 XYZ_To_CIERGB = float3x3(
    2.3706743, -0.9000405, -0.4706338,
   -0.5138850,  1.4253036,  0.0885814,
    0.0052982, -0.0146949,  1.0093968);

    //D50 (7)
    static const float3x3 XYZ_To_ColorMatch = float3x3(
    2.6422874, -1.2234270, -0.3930143,
   -1.1119763,  2.0590183,  0.0159614,
    0.0821699, -0.2807254,  1.4559877);

    //D50 (8)
    static const float3x3 XYZ_To_Don = float3x3(
    1.7603902, -0.4881198, -0.2536126,
   -0.7126288,  1.6527432,  0.0416715,
    0.0078207, -0.0347411,  1.2447743);

    //D50 (9)
    static const float3x3 XYZ_To_ECI = float3x3(
    1.7827618, -0.4969847, -0.2690101,
   -0.9593623,  1.9477962, -0.0275807,
    0.0859317, -0.1744674,  1.3228273);

    //D50 (10)
    static const float3x3 XYZ_To_EktaSpacePS5 = float3x3(
    2.0043819, -0.7304844, -0.2450052,
   -0.7110285,  1.6202126,  0.0792227,
    0.0381263, -0.0868780,  1.2725438);

    //C (11)
    static const float3x3 XYZ_To_NTSC = float3x3(
    1.9099961, -0.5324542, -0.2882091,
   -0.9846663,  1.9991710, -0.0283082,
    0.0583056, -0.1183781,  0.8975535);

    //D65 (12)
    static const float3x3 XYZ_To_PALSECAM = float3x3(
    3.0628971, -1.3931791, -0.4757517,
   -0.9692660,  1.8760108,  0.0415560,
    0.0678775, -0.2288548,  1.0693490);

    //D50 (13)
    static const float3x3 XYZ_To_ProPhoto = float3x3(
    1.3459433, -0.2556075, -0.0511118,
   -0.5445989,  1.5081673,  0.0205351,
    0.0000000,  0.0000000,  1.2118128);

    //D65 (14)
    static const float3x3 XYZ_To_SMPTEC = float3x3(
    3.5053960, -1.7394894, -0.5439640,
   -1.0690722,  1.9778245,  0.0351722,
    0.0563200, -0.1970226,  1.0502026);

    //D50 (15)
    static const float3x3 XYZ_To_WideGamutRGB = float3x3(
    1.4628067, -0.1840623, -0.2743606,
   -0.5217933,  1.4472381,  0.0677227,
    0.0349342, -0.0968930,  1.2884099);

    //D65 to D50
    static const float3x3 D65_To_D50 = float3x3(
    1.0478112,  0.0228866, -0.0501270,
    0.0295424,  0.9904844, -0.0170491,
   -0.0092345,  0.0150436,  0.7521316);

    //D65 to E
    static const float3x3 D65_To_E = float3x3(
    1.0502616,  0.0270757, -0.0232523,
    0.0390650,  0.9729502, -0.0092579,
   -0.0024047,  0.0026446,  0.918087);

    //D65 to C
    static const float3x3 D65_To_C = float3x3(
    1.0097785,  0.0070419,  0.0127971,
    0.0123113,  0.9847094,  0.0032962,
    0.0038284, -0.0072331,  1.0891639);

    //// FUNCTIONS //////////////////////////////////////////////////////////////////
    float3 LinearTosRGB( float3 color )
    {
        float3 x = color * 12.92f;
        float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

        float3 clr = color;
        clr.r = color.r < 0.0031308f ? x.r : y.r;
        clr.g = color.g < 0.0031308f ? x.g : y.g;
        clr.b = color.b < 0.0031308f ? x.b : y.b;

        return clr;
    }

    float3 SRGBToLinear( float3 color )
    {
        float3 x = color / 12.92f;
        float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

        float3 clr = color;
        clr.r = color.r <= 0.04045f ? x.r : y.r;
        clr.g = color.g <= 0.04045f ? x.g : y.g;
        clr.b = color.b <= 0.04045f ? x.b : y.b;

        return clr;
    }

    //// PIXEL SHADERS //////////////////////////////////////////////////////////////
    float4 PS_ColorConversion(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float4 color      = tex2D( ReShade::BackBuffer, texcoord );
        color.xyz         = SRGBToLinear( color.xyz );
        color.xyz         = mul( sRGB_To_XYZ, color.xyz );
        float3x3 gamut;
        switch( colorgamut )
        {
            case 0:  { gamut = XYZ_To_sRGB;         } break;
            case 1:  { gamut = XYZ_To_AdobeRGB;     } break;
            case 2:  { gamut = XYZ_To_AppleRGB;     } break;
            case 3:  { gamut = XYZ_To_BestRGB;      } break;
            case 4:  { gamut = XYZ_To_BetaRGB;      } break;
            case 5:  { gamut = XYZ_To_BruceRGB;     } break;
            case 6:  { gamut = XYZ_To_CIERGB;       } break;
            case 7:  { gamut = XYZ_To_ColorMatch;   } break;
            case 8:  { gamut = XYZ_To_Don;          } break;
            case 9:  { gamut = XYZ_To_ECI;          } break;
            case 10: { gamut = XYZ_To_EktaSpacePS5; } break;
            case 11: { gamut = XYZ_To_NTSC;         } break;
            case 12: { gamut = XYZ_To_PALSECAM;     } break;
            case 13: { gamut = XYZ_To_ProPhoto;     } break;
            case 14: { gamut = XYZ_To_SMPTEC;       } break;
            case 15: { gamut = XYZ_To_WideGamutRGB; } break;
        }
        float3x3 refwhitemat = float3x3(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0);
        if( colorgamut == 3  ) refwhitemat = D65_To_D50;
        if( colorgamut == 4  ) refwhitemat = D65_To_D50;
        if( colorgamut == 6  ) refwhitemat = D65_To_E;
        if( colorgamut == 7  ) refwhitemat = D65_To_D50;
        if( colorgamut == 8  ) refwhitemat = D65_To_D50;
        if( colorgamut == 9  ) refwhitemat = D65_To_D50;
        if( colorgamut == 10 ) refwhitemat = D65_To_D50;
        if( colorgamut == 11 ) refwhitemat = D65_To_C;
        if( colorgamut == 13 ) refwhitemat = D65_To_D50;
        if( colorgamut == 15 ) refwhitemat = D65_To_D50;
        color.xyz         = mul( gamut, mul( refwhitemat, color.xyz ));
        color.xyz         = LinearTosRGB( color.xyz );
        return float4( color.xyz, 1.0f );
    }

    //// TECHNIQUES /////////////////////////////////////////////////////////////////
    technique prod80_01_Color_Gamut
    {
        pass prod80_pass0
        {
            VertexShader   = PostProcessVS;
            PixelShader    = PS_ColorConversion;
        }
    }
}


