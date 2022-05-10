#include "ReShade.fxh"

/*
    CRT Shader by EasyMode
    License: GPL

    A flat CRT shader ideally for 1080p or higher displays.

    Recommended Settings:

    Video
    - Aspect Ratio: 4:3
    - Integer Scale: Off

    Shader
    - Filter: Nearest
    - Scale: Don't Care

    Example RGB Mask Parameter Settings:

    Aperture Grille (Default)
    - Dot Width: 1
    - Dot Height: 1
    - Stagger: 0

    Lottes' Shadow Mask
    - Dot Width: 2
    - Dot Height: 1
    - Stagger: 3
*/

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Easymode]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Easymode]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Easymode]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Easymode]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float SHARPNESS_H <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Sharpness Horizontal [CRT-Easymode]";
> = 0.5;

uniform float SHARPNESS_V <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Sharpness Vertical [CRT-Easymode]";
> = 1.0;

uniform float MASK_STRENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Mask Strength [CRT-Easymode]";
> = 0.3;

uniform float MASK_DOT_WIDTH <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Mask Dot Width [CRT-Easymode]";
> = 1.0;

uniform float MASK_DOT_HEIGHT <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Mask Dot Height [CRT-Easymode]";
> = 1.0;

uniform float MASK_STAGGER <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Mask Stagger [CRT-Easymode]"; 
> = 0.0;

uniform float MASK_SIZE <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Mask Size [CRT-Easymode]";
> = 1.0;

uniform float SCANLINE_STRENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Scanline Strength [CRT-Easymode]";
> = 1.0;

uniform float SCANLINE_BEAM_WIDTH_MIN <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 5.0;
	ui_step = 0.5;
	ui_label = "Scanline Beam Width M [CRT-Easymode]";
> = 1.5;

uniform float SCANLINE_BEAM_WIDTH_MAX <
	ui_type = "drag";
	ui_min = 0.5;
	ui_max = 5.0;
	ui_step = 0.5;
	ui_label = "Scanline Beam Width Max. [CRT-Easymode]";
> = 1.5;

uniform float SCANLINE_BRIGHT_MIN <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Scanline Brightness M [CRT-Easymode]"; 
> = 0.35;

uniform float SCANLINE_BRIGHT_MAX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Scanline Brightness Max. [CRT-Easymode]";
> = 0.65;

uniform float SCANLINE_CUTOFF <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 1000.0;
	ui_step = 1.0;
	ui_label = "Scanline Cutoff [CRT-Easymode]";
> = 400.0;

uniform float GAMMA_INPUT <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Gamma Input [CRT-Easymode]";
> = 2.0;

uniform float GAMMA_OUTPUT <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "Gamma Output [CRT-Easymode]";
> = 1.8;

uniform float BRIGHT_BOOST <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 2.0;
	ui_step = 0.01;
	ui_label = "Brightness Boost [CRT-Easymode]";
> = 1.2;

uniform int DILATION <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 1.0;
	ui_label = "Dilation [CRT-Easymode]";
> = 1.0;

#define mod(x,y) (x-y*floor(x/y))

#define FIX(c) max(abs(c), 1e-5)
#define PI 3.141592653589
#define TEX2D(c) dilate(tex2D(ReShade::BackBuffer, c))
#define TEX2D_OGL(c) pow(dilate(tex2D(tex, c)).rgb, 1.0)
#define texture_size float2(texture_sizeX, texture_sizeY)
#define video_size float2(video_sizeX, video_sizeY)

uniform bool ENABLE_LANCZOS <
	ui_type = "combo";
	ui_label = "Enable Lanczos [CRT-Easymode]";
> = true;

float4 dilate(float4 col)
{
    float4 x = lerp(float4(1.0,1.0,1.0,1.0), col, DILATION);

    return col * x;
}

float curve_distance(float x, float sharp)
{

/*
    apply half-circle s-curve to distance for sharper (more pixelated) interpolation
    single line formula for Graph Toy:
    0.5 - sqrt(0.25 - (x - step(0.5, x)) * (x - step(0.5, x))) * sign(0.5 - x)
*/

    float x_step = step(0.5, x);
    float curve = 0.5 - sqrt(0.25 - (x - x_step) * (x - x_step)) * sign(0.5 - x);

    return lerp(x, curve, sharp);
}

float4x4 get_color_matrix(sampler2D tex, float2 co, float2 dx)
{
    return float4x4(TEX2D(co - dx), TEX2D(co), TEX2D(co + dx), TEX2D(co + 2.0 * dx));
}

float3 filter_lanczos(float4 coeffs, float4x4 color_matrix)
{
    float4 col = mul(coeffs, color_matrix);
    float4 sample_min = min(color_matrix[1], color_matrix[2]);
    float4 sample_max = max(color_matrix[1], color_matrix[2]);

    col = clamp(col, sample_min, sample_max);

    return col.rgb;
}

float3 filter_lanczos_ogl(sampler2D tex, float2 co, float2 tex_size)
{
    float2 dx = float2(1.0 / tex_size.x, 0.0);
    float2 pix_co = co * tex_size - float2(0.5, 0.0);
    float2 tex_co = (floor(pix_co) + float2(0.5, 0.0)) / tex_size;
    float2 dist = frac(pix_co);
    float4 coef = PI * float4(dist.x + 1.0, dist.x, dist.x - 1.0, dist.x - 2.0);

    coef = FIX(coef);
    coef = 2.0 * sin(coef) * sin(coef / 2.0) / (coef * coef);
    coef /= dot(coef, float4(1.0,1.0,1.0,1.0));

    float4 col1 = float4(TEX2D_OGL(tex_co), 1.0);
    float4 col2 = float4(TEX2D_OGL(tex_co + dx), 1.0);

    return mul(coef, float4x4(col1, col1, col2, col2)).rgb;
}

float4 PS_CRTEasymode(float4 vpos : SV_Position, float2 coords : TexCoord) : SV_Target
{
    float2 dx = float2(1.0 / texture_size.x, 0.0);
    float2 dy = float2(0.0, 1.0 / texture_size.y);
    float2 pix_co = coords * texture_size - float2(0.5, 0.5);
    float2 tex_co = (floor(pix_co) + float2(0.5, 0.5)) / texture_size;
    float2 dist = frac(pix_co);
    float curve_x;
    float3 col, col2;

	if(ENABLE_LANCZOS){
		curve_x = curve_distance(dist.x, SHARPNESS_H * SHARPNESS_H);

		float4 coeffs = PI * float4(1.0 + curve_x, curve_x, 1.0 - curve_x, 2.0 - curve_x);

		coeffs = FIX(coeffs);
		coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);
		coeffs /= dot(coeffs, float4(1.0,1.0,1.0,1.0));
		if (__RENDERER__ >= 0x10000 || __RENDERER__ == 0x0B100 || __RENDERER__ == 0x0A100){ //OGL and DX10/11 fix
				col = filter_lanczos_ogl(ReShade::BackBuffer, coords, texture_size).rgb;
				col2 = filter_lanczos_ogl(ReShade::BackBuffer, coords, texture_size).rgb;
		} else {
				col = filter_lanczos(coeffs, get_color_matrix(ReShade::BackBuffer, tex_co, dx));
				col2 = filter_lanczos(coeffs, get_color_matrix(ReShade::BackBuffer, tex_co + dy, dx));
		}	
	} else {
		curve_x = curve_distance(dist.x, SHARPNESS_H);

		col = lerp(TEX2D(tex_co).rgb, TEX2D(tex_co + dx).rgb, curve_x);
		col2 = lerp(TEX2D(tex_co + dy).rgb, TEX2D(tex_co + dx + dy).rgb, curve_x);
	}

    col = lerp(col, col2, curve_distance(dist.y, SHARPNESS_V));
    col = pow(col, float3(GAMMA_INPUT / (DILATION + 1.0),GAMMA_INPUT / (DILATION + 1.0),GAMMA_INPUT / (DILATION + 1.0)));

    float luma = dot(float3(0.2126, 0.7152, 0.0722), col);
    float bright = (max(col.r, max(col.g, col.b)) + luma) / 2.0;
    float scan_bright = clamp(bright, SCANLINE_BRIGHT_MIN, SCANLINE_BRIGHT_MAX);
    float scan_beam = clamp(bright * SCANLINE_BEAM_WIDTH_MAX, SCANLINE_BEAM_WIDTH_MIN, SCANLINE_BEAM_WIDTH_MAX);
    float scan_weight = 1.0 - pow(cos(coords.y * 2.0 * PI * texture_size.y) * 0.5 + 0.5, scan_beam) * SCANLINE_STRENGTH;

    float mask = 1.0 - MASK_STRENGTH;    
    float2 mod_fac = floor(coords * ReShade::ScreenSize * texture_size / (video_size * float2(MASK_SIZE, MASK_DOT_HEIGHT * MASK_SIZE)));
    int dot_no = int(mod((mod_fac.x + mod(mod_fac.y, 2.0) * MASK_STAGGER) / MASK_DOT_WIDTH, 3.0));
    float3 mask_weight;

    if (dot_no == 0) mask_weight = float3(1.0, mask, mask);
    else if (dot_no == 1) mask_weight = float3(mask, 1.0, mask);
    else mask_weight = float3(mask, mask, 1.0);

    if (video_size.y >= SCANLINE_CUTOFF) scan_weight = 1.0;

    col2 = col.rgb;
    col *= float3(scan_weight,scan_weight,scan_weight);
    col = lerp(col, col2, scan_bright);
    col *= mask_weight;
    col = pow(col, float3(1.0 / GAMMA_OUTPUT,1.0 / GAMMA_OUTPUT,1.0 / GAMMA_OUTPUT));

    return float4(col * BRIGHT_BOOST, 1.0);
}

technique EasymodeCRT {
	pass CRT_Easymode {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTEasymode;
	}
}