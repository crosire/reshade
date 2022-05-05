///////////////////////////////////////////////////////////////////////////////
//
//ReShade Shader: ColorfulPoster
//https://github.com/Daodan317081/reshade-shaders
//
//BSD 3-Clause License
//
//Copyright (c) 2018-2019, Alexander Federwisch
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions are met:
//
//* Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//
//* Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//* Neither the name of the copyright holder nor the names of its
//  contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

#define UI_CATEGORY_POSTERIZATION "Posterization"
#define UI_CATEGORY_COLOR "Color"
#define UI_CATEGORY_EFFECT "Effect"

/******************************************************************************
    Uniforms
******************************************************************************/

////////////////////////// Posterization //////////////////////////
uniform int iUILumaLevels <
    ui_type = "drag";
    ui_category = UI_CATEGORY_POSTERIZATION;
    ui_label = "Luma Posterize Levels";
    ui_min = 1; ui_max = 20;
> = 16;

uniform int iUIStepType <
    ui_type = "combo";
    ui_category = UI_CATEGORY_POSTERIZATION;
    ui_label = "Curve Type";
    ui_items = "Linear\0Smoothstep\0Logistic\0Sigmoid\0";
> = 2;

uniform float fUIStepContinuity <
    ui_type = "drag";
    ui_category = UI_CATEGORY_POSTERIZATION;
    ui_label = "Continuity";
    ui_tooltip = "Broken up <-> Connected";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
> = 1.0;

uniform float fUISlope <
    ui_type = "drag";
    ui_category = UI_CATEGORY_POSTERIZATION;
    ui_label = "Slope Logistic Curve";
    ui_min = 0.0; ui_max = 40.0;
    ui_step = 0.1;
> = 13.0;

uniform bool iUIDebugOverlayPosterizeLevels <
    ui_category = UI_CATEGORY_POSTERIZATION;
    ui_label = "Show Posterization as Curve (Magenta)";
> = 0;

////////////////////////// Color //////////////////////////

uniform float fUITint <
    ui_type = "drag";
    ui_category = UI_CATEGORY_COLOR;
    ui_label = "Tint Strength";
    ui_min = 0.0; ui_max = 1.0;
> = 1.0;

////////////////////////// Effect //////////////////////////

uniform float fUIStrength <
    ui_type = "drag";
    ui_category = UI_CATEGORY_EFFECT;
    ui_label = "Strength";
    ui_min = 0.0; ui_max = 1.0;
> = 1.0;

/******************************************************************************
    Functions
******************************************************************************/

#define MAX_VALUE(v) max(v.x, max(v.y, v.z))

float Posterize(float x, int numLevels, float continuity, float slope, int type) {
    float stepheight = 1.0 / numLevels;
    float stepnum = floor(x * numLevels);
    float frc = frac(x * numLevels);
    float step1 = floor(frc) * stepheight;
    float step2;

    if(type == 1)
        step2 = smoothstep(0.0, 1.0, frc) * stepheight;
    else if(type == 2)
        step2 = (1.0 / (1.0 + exp(-slope*(frc - 0.5)))) * stepheight;
    else if(type == 3)
        step2 = (frc < 0.5 ? pow(frc, slope) * pow(2.0, slope) * 0.5 : 1.0 - pow(1.0 - frc, slope) * pow(2.0, slope) * 0.5) * stepheight;
    else
        step2 = frc * stepheight;

    return lerp(step1, step2, continuity) + stepheight * stepnum;
}

float4 RGBtoCMYK(float3 color) {
    float3 CMY;
    float K;
    K = 1.0 - max(color.r, max(color.g, color.b));
    CMY = (1.0 - color - K) / (1.0 - K);
    return float4(CMY, K);
}

float3 CMYKtoRGB(float4 cmyk) {
    return (1.0.xxx - cmyk.xyz) * (1.0 - cmyk.w);
}

float3 DrawDebugCurve(float3 background, float2 texcoord, float value, float3 color, float curveDiv) {
    float p = exp(-(BUFFER_HEIGHT/curveDiv) * length(texcoord - float2(texcoord.x, 1.0 - value)));
    return lerp(background, color, saturate(p));
}

/******************************************************************************
    Pixel Shader
******************************************************************************/
float3 ColorfulPoster_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    static const float3 LumaCoeff = float3(0.2126, 0.7151, 0.0721);
    /*******************************************************
        Get BackBuffer
    *******************************************************/
    float3 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgb;

    /*******************************************************
        Calculate chroma and luma; posterize luma
    *******************************************************/
    float luma = dot(backbuffer, LumaCoeff);
    float3 chroma = backbuffer - luma;
    float3 lumaPoster = Posterize(luma, iUILumaLevels, fUIStepContinuity, fUISlope, iUIStepType).rrr;

    /*******************************************************
        Color
    *******************************************************/
    float3 mask, image, colorLayer;

    //Convert RGB to CMYK, add cyan tint, set K to 0.0
    float4 backbufferCMYK = RGBtoCMYK(backbuffer);
    backbufferCMYK.xyz += float3(0.2, -0.1, -0.2);
    backbufferCMYK.w = 0.0;

    //Convert back to RGB
    mask = CMYKtoRGB(saturate(backbufferCMYK));
    
    //add luma to chroma
    image = chroma + lumaPoster;

    //Blend with 'hard light'
    colorLayer = lerp(2*image*mask, 1.0 - 2.0 * (1.0 - image) * (1.0 - mask), step(0.5, luma.r));
    colorLayer = lerp(image, colorLayer, fUITint);

    /*******************************************************
        Create result
    *******************************************************/
    float3 result = lerp(backbuffer, colorLayer, fUIStrength);

    if(iUIDebugOverlayPosterizeLevels == 1) {
        float value = Posterize(texcoord.x, iUILumaLevels, fUIStepContinuity, fUISlope, iUIStepType);
        result = DrawDebugCurve(result, texcoord, value, float3(1.0, 0.0, 1.0), 1.0);        
    }
        
    /*******************************************************
        Set overall strength and return
    *******************************************************/
    return result;
}

technique ColorfulPoster
{
    pass {
        VertexShader = PostProcessVS;
        PixelShader = ColorfulPoster_PS;
    }
}
