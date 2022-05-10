/*
 * Original shader by Ganossa
 * 
 * ReShade Community edits by:
 * - MartyMcFly
 * - Otis_Inf
 * - prod80
 */

// Input
#define _MERGE(a, b) a##b
#define PD80_TexName        _MERGE( PD80_Technique_Name, Tex )
#define PD80_Sampler        _MERGE( PD80_Technique_Name, Samp )
#define PD80_NameSpace      _MERGE( PD80_Technique_Name, Name )

#include "ReShade.fxh"
#include "PD80_00_Noise_Samplers.fxh"
#include "PD80_00_Color_Spaces.fxh"

namespace PD80_NameSpace
{

    //// UI Elements /////////////////////////////////////////////////
    uniform bool enable_dither <
        ui_label = "Enable Dithering";
        ui_tooltip = "Enable Dithering";
        > = true;
    uniform float dither_strength <
        ui_type = "slider";
        ui_label = "Dither Strength";
        ui_tooltip = "Dither Strength";
        ui_min = 0.0f;
        ui_max = 10.0f;
        > = 1.0;
    uniform int PD80_LutSelector < 
        ui_type = "combo";
        ui_items= PD80_Drop_Down_Menu;
        ui_label = "LUT Selection";
        ui_tooltip = "The LUT to use for color transformation.";
        > = 0;
    uniform float PD80_MixChroma <
        ui_type = "slider";
        ui_min = 0.00; ui_max = 1.00;
        ui_label = "LUT Chroma";
        ui_tooltip = "LUT Chroma";
        > = 1.00;
    uniform float PD80_MixLuma <
        ui_type = "slider";
        ui_min = 0.00; ui_max = 1.00;
        ui_label = "LUT Luma";
        ui_tooltip = "LUT Luma";
        > = 1.00;
    uniform float PD80_Intensity <
        ui_type = "slider";
        ui_min = 0.00; ui_max = 1.00;
        ui_label = "LUT Intensity";
        ui_tooltip = "Intensity of LUT effect";
        > = 1.00;
    uniform float3 ib <
        ui_type = "color";
        ui_label = "LUT Black IN Level";
        ui_tooltip = "LUT Black IN Level";
        ui_category = "LUT Levels";
        > = float3(0.0, 0.0, 0.0);
    uniform float3 iw <
        ui_type = "color";
        ui_label = "LUT White IN Level";
        ui_tooltip = "LUT White IN Level";
        ui_category = "LUT Levels";
        > = float3(1.0, 1.0, 1.0);
    uniform float3 ob <
        ui_type = "color";
        ui_label = "LUT Black OUT Level";
        ui_tooltip = "LUT Black OUT Level";
        ui_category = "LUT Levels";
        > = float3(0.0, 0.0, 0.0);
    uniform float3 ow <
        ui_type = "color";
        ui_label = "LUT White OUT Level";
        ui_tooltip = "LUT White OUT Level";
        ui_category = "LUT Levels";
        > = float3(1.0, 1.0, 1.0);
    uniform float ig <
        ui_label = "LUT Gamma Adjustment";
        ui_tooltip = "LUT Gamma Adjustment";
        ui_category = "LUT Levels";
        ui_type = "slider";
        ui_min = 0.05;
        ui_max = 10.0;
        > = 1.0;

    //// Textures Samplers ///////////////////////////////////////////
    texture PD80_TexName < source = PD80_LUT_File_Name; >
    { 
        Width = PD80_Tile_SizeXY * PD80_Tile_Amount;
        Height = PD80_Tile_SizeXY * PD80_LUT_Amount;
    };
    sampler	PD80_Sampler { Texture = PD80_TexName; };

    //// Functions ///////////////////////////////////////////////////
    float3 levels( float3 color, float3 blackin, float3 whitein, float gamma, float3 outblack, float3 outwhite )
    {
        float3 ret       = saturate( color.xyz - blackin.xyz ) / max( whitein.xyz - blackin.xyz, 0.000001f );
        ret.xyz          = pow( ret.xyz, gamma );
        ret.xyz          = ret.xyz * saturate( outwhite.xyz - outblack.xyz ) + outblack.xyz;
        return ret;
    }

    //// Pixel Shader ////////////////////////////////////////////////
    void PS_MultiLUT_v2( float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target0 )
    {
        color            = tex2D( ReShade::BackBuffer, texcoord.xy );
        float4 dnoise    = dither( samplerRGBNoise, texcoord.xy, 10, enable_dither, dither_strength, 1, 0.5f );

        float2 texelsize = rcp( PD80_Tile_SizeXY );
        texelsize.x     /= PD80_Tile_Amount;

        float3 lutcoord  = float3(( color.xy * PD80_Tile_SizeXY - color.xy + 0.5f ) * texelsize.xy, color.z * PD80_Tile_SizeXY - color.z );
        lutcoord.y      /= PD80_LUT_Amount;
        lutcoord.y      += ( float( PD80_LutSelector ) / PD80_LUT_Amount );
        float lerpfact   = frac( lutcoord.z );
        lutcoord.x      += ( lutcoord.z - lerpfact ) * texelsize.y;

        float3 lutcolor  = lerp( tex2D( PD80_Sampler, lutcoord.xy ).xyz, tex2D( PD80_Sampler, float2( lutcoord.x + texelsize.y, lutcoord.y )).xyz, lerpfact );
        lutcolor.xyz     = levels( lutcolor.xyz,    saturate( ib.xyz + dnoise.xyz ),
                                                    saturate( iw.xyz + dnoise.yzx ),
                                                    ig, 
                                                    saturate( ob.xyz + dnoise.zxy ), 
                                                    saturate( ow.xyz + dnoise.wxz ));
        float3 lablut    = pd80_srgb_to_lab( lutcolor.xyz );
        float3 labcol    = pd80_srgb_to_lab( color.xyz );
        float newluma    = lerp( labcol.x, lablut.x, PD80_MixLuma );
        float2 newAB     = lerp( labcol.yz, lablut.yz, PD80_MixChroma );
        lutcolor.xyz     = pd80_lab_to_srgb( float3( newluma, newAB ));
        color.xyz        = lerp( color.xyz, saturate( lutcolor.xyz + dnoise.wzx ), PD80_Intensity );
    }

    //// Technique ///////////////////////////////////////////////////
    technique PD80_Technique_Name
    {
        pass MultiLUT_Apply
        {
            VertexShader = PostProcessVS;
            PixelShader  = PS_MultiLUT_v2;
        }
    }
}