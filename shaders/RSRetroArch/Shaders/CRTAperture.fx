#include "ReShade.fxh"

/*
    CRT Shader by EasyMode
    License: GPL
*/

uniform float resX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Aperture]";
> = 320.0;

uniform float resY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Aperture]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Aperture]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Aperture]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform int SHARPNESS_IMAGE <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 5.0;
	ui_step = 1.0;
	ui_label = "Sharpness Image [CRT-Aperture]";
> =	1;

uniform int SHARPNESS_EDGES <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 5;
	ui_step = 1;
	ui_label = "Sharpness Edges [CRT-Aperture]";
> = 3;

uniform float GLOW_WIDTH <
	ui_type = "drag";
	ui_min = 0.05;
	ui_max = 0.65;
	ui_step = 0.05;
	ui_label = "Glow Width [CRT-Aperture]";
> =	0.5;

uniform float GLOW_HEIGHT <
	ui_type = "drag";
	ui_min = 0.05;
	ui_max = 0.65;
	ui_step = 0.05;
	ui_label = "Glow Height [CRT-Aperture]";
> =	0.5;

uniform float GLOW_HALATION <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Glow Halation [CRT-Aperture]";
> = 0.1;

uniform float GLOW_DIFFUSION <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Glow Diffusion [CRT-Aperture]";
> = 0.05;

uniform float MASK_COLORS <
	ui_type = "drag";
	ui_min = 2.0;
	ui_max = 3.0;
	ui_step = 1.0;
	ui_label = "Mask Colors [CRT-Aperture]";
> = 2.0;

uniform float MASK_STRENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Mask Strength [CRT-Aperture]";
> = 0.3;

uniform int MASK_SIZE <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 9;
	ui_step = 1;
	ui_label = "Mask Size [CRT-Aperture]";
> = 1;

uniform float SCANLINE_SIZE_MIN <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 1.5;
	ui_step = 0.05;
	ui_label = "Scanline Size Min. [CRT-Aperture]";
> = 0.5;

uniform float SCANLINE_SIZE_MAX <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 1.5;
	ui_step = 0.05;
	ui_label = "Scanline Size Max. [CRT-Aperture]";
> = 1.5;

uniform float GAMMA_INPUT <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Gamma Input [CRT-Aperture]";
> = 2.4;

uniform float GAMMA_OUTPUT <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Gamma Output [CRT-Aperture]";
> = 2.4;

uniform float BRIGHTNESS <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.05;
	ui_label = "Brightness [CRT-Aperture]";
> = 1.5;

#define FIX(c) max(abs(c), 1e-5)
#define PI 3.141592653589
#define TEX2D(c) pow(tex2D(tex, c).rgb, GAMMA_INPUT)
#define texture_size float2(resX, resY)
#define video_size float2(video_sizeX, video_sizeY)
#define mod(x,y) (x - y * trunc(x/y))

float3 blur(float3x3 m, float dist, float rad)
{
    float3 x = float3(dist - 1.0, dist, dist + 1.0) / rad;
    float3 w = exp2(x * x * -1.0);

    return (m[0] * w.x + m[1] * w.y + m[2] * w.z) / (w.x + w.y + w.z);
}

float3x3 get_color_matrix(sampler2D tex, float2 co, float2 dx)
{
    return float3x3(TEX2D(co - dx), TEX2D(co), TEX2D(co + dx));
}

float3 filter_gaussian(sampler2D tex, float2 co, float2 tex_size)
{
    float2 dx = float2(1.0 / tex_size.x, 0.0);
    float2 dy = float2(0.0, 1.0 / tex_size.y);
    float2 pix_co = co * tex_size;
    float2 tex_co = (floor(pix_co) + 0.5) / tex_size;
    float2 dist = (frac(pix_co) - 0.5) * -1.0;

    float3x3 line0 = get_color_matrix(tex, tex_co - dy, dx);
    float3x3 line1 = get_color_matrix(tex, tex_co, dx);
    float3x3 line2 = get_color_matrix(tex, tex_co + dy, dx);
    float3x3 column = float3x3(blur(line0, dist.x, GLOW_WIDTH),
                               blur(line1, dist.x, GLOW_WIDTH),
                               blur(line2, dist.x, GLOW_WIDTH));

    return blur(column, dist.y, GLOW_HEIGHT);
}

float3 filter_lanczos(sampler2D tex, float2 co, float2 tex_size, float sharp)
{
    tex_size.x *= sharp;

    float2 dx = float2(1.0 / tex_size.x, 0.0);
    float2 pix_co = co * tex_size - float2(0.5, 0.0);
    float2 tex_co = (floor(pix_co) + float2(0.5, 0.0)) / tex_size;
    float2 dist = frac(pix_co);
    float4 coef = PI * float4(dist.x + 1.0, dist.x, dist.x - 1.0, dist.x - 2.0);

    coef = FIX(coef);
    coef = 2.0 * sin(coef) * sin(coef / 2.0) / (coef * coef);
    coef /= dot(coef, float4(1.0,1.0,1.0,1.0));

    float4 col1 = float4(TEX2D(tex_co), 1.0);
    float4 col2 = float4(TEX2D(tex_co + dx), 1.0);

    return mul(coef, float4x4(col1, col1, col2, col2)).rgb;
}

float3 get_scanline_weight(float x, float3 col)
{
    float3 beam = lerp(float3(SCANLINE_SIZE_MIN,SCANLINE_SIZE_MIN,SCANLINE_SIZE_MIN), float3(SCANLINE_SIZE_MAX,SCANLINE_SIZE_MAX,SCANLINE_SIZE_MAX), col);
    float3 x_mul = 2.0 / beam;
    float3 x_offset = x_mul * 0.5;

    return smoothstep(0.0, 1.0, 1.0 - abs(x * x_mul - x_offset)) * x_offset;
}

float3 get_mask_weight(float x)
{
    float i = mod(floor(x * ReShade::ScreenSize.x * texture_size.x / (video_size.x * MASK_SIZE)), MASK_COLORS);

    if (i == 0.0) return lerp(float3(1.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), MASK_COLORS - 2.0);
    else if (i == 1.0) return float3(0.0, 1.0, 0.0);
    else return float3(0.0, 0.0, 1.0);
}

float4 PS_CRTAperture(float4 vpos : SV_Position, float2 co : TEXCOORD0) : SV_Target
{
    float3 col_glow = filter_gaussian(ReShade::BackBuffer, co, texture_size).rgb;
    float3 col_soft = filter_lanczos(ReShade::BackBuffer, co, texture_size, SHARPNESS_IMAGE).rgb;
    float3 col_sharp = filter_lanczos(ReShade::BackBuffer, co, texture_size, SHARPNESS_EDGES).rgb;
    float3 col = sqrt(col_sharp * col_soft);

    col *= get_scanline_weight(frac(co.y * texture_size.y), col_soft);
    col_glow = saturate(col_glow - col);
    col += col_glow * col_glow * GLOW_HALATION;
    col = lerp(col, col * get_mask_weight(co.x) * MASK_COLORS, MASK_STRENGTH);
    col += col_glow * GLOW_DIFFUSION;
    col = pow(col * BRIGHTNESS, 1.0 / GAMMA_OUTPUT);

    return float4(col, 1.0);
}

technique ApertureCRT {
	pass CRT_Aperture {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTAperture;
	}
}