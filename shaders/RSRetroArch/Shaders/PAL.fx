#include "ReShade.fxh"

/*
    pal-singlepass.slang
    svo's PAL single pass shader, ported to libretro
    ------------------------------------------------
    "Software composite video modulation/demodulation experiments
    The idea is to reproduce in GLSL shaders realistic composite-like
    artifacting by applying PAL modulation and demodulation.
    
    Digital texture, passed through the model of an analog channel,
    should suffer same effects as its analog counterpart and exhibit properties,
    such as dot crawl and colour bleeding, that may be desirable for faithful
    reproduction of look and feel of old computer games."
    
    https://github.com/svofski/CRT
    Copyright (c) 2016, Viacheslav Slavinsky
    All rights reserved.
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	
	Ported to ReShade by Matsilagi and luluco250
*/

#define MUL(A, B) mul(A, B)

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [PAL]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [PAL]";
> = 240.0;

uniform float FIR_GAIN <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "FIR lowpass gain [PAL]";
> = 1.5;

uniform float FIR_INVGAIN <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Inverse gain for luma recovery [PAL]";
> = 1.1;

uniform float PHASE_NOISE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Phase noise [PAL]";
> = 1.0;

#define texture_size float2(texture_sizeX,texture_sizeY)
uniform int frame_count < source = "framecount"; >;

/* Subcarrier frequency */
#define FSC          4433618.75

/* Line frequency */
#define FLINE        15625.

#define VISIBLELINES 312.

#define PI           3.14159265358

#define RGB_to_YIQ  float3x3( 0.299,    0.595716,  0.211456,\
                            0.587,   -0.274453, -0.522591,\
                            0.114,   -0.321263,  0.311135)

#define YIQ_to_RGB  float3x3( 1.0   ,   1.0,       1.0,\
                            0.9563,  -0.2721,   -1.1070,\
                            0.6210,  -0.6474,    1.7046)

#define RGB_to_YUV  float3x3( 0.299,   -0.14713,   0.615,\
                            0.587,   -0.28886,  -0.514991,\
                            0.114,    0.436,    -0.10001)

#define YUV_to_RGB  float3x3( 1.0,      1.0,       1.0,\
                            0.0,     -0.39465,   2.03211,\
                            1.13983, -0.58060,   0.0)
                            
#define fetch(ofs,center,invx) tex2D(tex, float2((ofs) * (invx) + center.x, center.y))

#define FIRTAPS 20.
static const float FIR1 = -0.008030271;
static const float FIR2 = 0.003107906;
static const float FIR3 = 0.016841352;
static const float FIR4 = 0.032545161;
static const float FIR5 = 0.049360136;
static const float FIR6 = 0.066256720;
static const float FIR7 = 0.082120150;
static const float FIR8 = 0.095848433;
static const float FIR9 = 0.106453014;
static const float FIR10 = 0.113151423;
static const float FIR11 = 0.115441842;
static const float FIR12 = 0.113151423;
static const float FIR13 = 0.106453014;
static const float FIR14 = 0.095848433;
static const float FIR15 = 0.082120150;
static const float FIR16 = 0.066256720;
static const float FIR17 = 0.049360136;
static const float FIR18 = 0.032545161;
static const float FIR19 = 0.016841352;
static const float FIR20 = 0.003107906;

/* subcarrier counts per scan line = FSC/FLINE = 283.7516 */
/* We save the reciprocal of this only to optimize it */
static const float counts_per_scanline_reciprocal = 1.0 / (FSC/FLINE);

#define mod(x,y) (x-y*floor(x/y))

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

/* http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/ */
float rand(float2 co)
{
    float a  = 12.9898;
    float b  = 78.233;
    float c  = 43758.5453;
    float dt = dot(co.xy, float2(a, b));
    float sn = mod(dt,3.14);

    return frac(sin(sn) * c);
}

float modulated(float invx, float2 xy, float sinwt, float coswt) 
{	
    float3 rgb = tex2D(ReShade::BackBuffer, float2((0.0) * (invx) + xy.x, xy.y)).xyz; 
    float3 yuv = MUL(RGB_to_YUV, rgb);

    return clamp(yuv.x + yuv.y * sinwt + yuv.z * coswt, 0.0, 1.0);    
}

float2 modem_uv(float width_ratio, float invx, float altv, float2 xy, float ofs, float2 texsize, float framecount) {
    float t  = (xy.x + ofs * invx) * texsize.x;
    float wt = t * 2. * PI / width_ratio;

    float sinwt = sin(wt);
    float coswt = cos(wt + altv);

    float3 rgb = tex2D(ReShade::BackBuffer, float2((ofs) * (invx) + xy.x, xy.y)).xyz;
    float3 yuv = MUL(RGB_to_YUV, rgb);
    float signal = clamp(yuv.x + yuv.y * sinwt + yuv.z * coswt, 0.0, 1.0);
    
    if (PHASE_NOISE != 0.)
    {
        /* .yy is horizontal noise, .xx looks bad, .xy is classic noise */
        float2 seed = xy.yy * float(framecount);
        wt        = wt + PHASE_NOISE * (rand(seed) - 0.5);
        sinwt     = sin(wt);
        coswt     = cos(wt + altv);
    }

    return float2(signal * sinwt, signal * coswt);
}

void PS_PAL(in float4 pos : SV_POSITION, in float2 texCoord : TEXCOORD0, out float4 col : COLOR)
{
    float2 xy      = texCoord;
    float width_ratio  = texture_sizeX * (counts_per_scanline_reciprocal);
    float height_ratio = texture_sizeY / VISIBLELINES;
    float altv         = fmod(floor(xy.y * VISIBLELINES + 0.5), 2.0) * PI;
    float invx         = 0.25 * (counts_per_scanline_reciprocal); // equals 4 samples per Fsc period

    // lowpass U/V at baseband
    float2 filtered = float2(0.0, 0.0);

	float2 uv;
	
	#define macro_loopz(c)	uv = modem_uv(width_ratio, invx, altv, xy, float(c) - FIRTAPS*0.5, float2(texture_sizeX,texture_sizeY), frame_count); \
        filtered += FIR_GAIN * uv * FIR##c;
		
	macro_loopz(1)
	macro_loopz(2)
	macro_loopz(3)
	macro_loopz(4)
	macro_loopz(5)
	macro_loopz(6)
	macro_loopz(7)
	macro_loopz(8)
	macro_loopz(9)
	macro_loopz(10)
	macro_loopz(11)
	macro_loopz(12)
	macro_loopz(13)
	macro_loopz(14)
	macro_loopz(15)
	macro_loopz(16)
	macro_loopz(17)
	macro_loopz(18)
	macro_loopz(19)
	macro_loopz(20)

    float t  = xy.x * texture_size.x;
    float wt = t * 2. * PI / width_ratio;

    float sinwt = sin(wt);
    float coswt = cos(wt + altv);

    float luma = modulated(invx, xy, sinwt, coswt) - FIR_INVGAIN * (filtered.x * sinwt + filtered.y * coswt);
    float3 yuv_result = float3(luma, filtered.x, filtered.y);

	col = float4(MUL(YUV_to_RGB, yuv_result), 1.0);
}

technique PAL
{
	pass PAL
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_PAL;
	}
}