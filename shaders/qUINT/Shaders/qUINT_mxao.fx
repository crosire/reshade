/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg

    Ambient Obscurance with Indirect Lighting "MXAO"
    by Marty McFly / P.Gilcher
    part of qUINT shader library for ReShade 4

    Copyright (c) Pascal Gilcher / Marty McFly. All rights reserved.

=============================================================================*/

/*=============================================================================
	Preprocessor settings
=============================================================================*/

#ifndef MXAO_MIPLEVEL_AO
 #define MXAO_MIPLEVEL_AO		0	//[0 to 2]      Miplevel of AO texture. 0 = fullscreen, 1 = 1/2 screen width/height, 2 = 1/4 screen width/height and so forth. Best results: IL MipLevel = AO MipLevel + 2
#endif

#ifndef MXAO_MIPLEVEL_IL
 #define MXAO_MIPLEVEL_IL		2	//[0 to 4]      Miplevel of IL texture. 0 = fullscreen, 1 = 1/2 screen width/height, 2 = 1/4 screen width/height and so forth.
#endif

#ifndef MXAO_ENABLE_IL
 #define MXAO_ENABLE_IL			0	//[0 or 1]	    Enables Indirect Lighting calculation. Will cause a major fps hit.
#endif

#ifndef MXAO_SMOOTHNORMALS
 #define MXAO_SMOOTHNORMALS     0   //[0 or 1]      This feature makes low poly surfaces smoother, especially useful on older games.
#endif

#ifndef MXAO_TWO_LAYER
 #define MXAO_TWO_LAYER         0   //[0 or 1]      Splits MXAO into two separate layers that allow for both large and fine AO.
#endif

#ifndef MXAO_HIGH_QUALITY
 #define MXAO_HIGH_QUALITY      0   //[0 or 1]      Enables a different, more physically accurate but slower SSAO mode. Based on Ground Truth Ambient Occlusion by Activision. No IL yet.
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform int MXAO_GLOBAL_SAMPLE_QUALITY_PRESET <
	ui_type = "combo";
    ui_label = "Sample Quality";
	ui_items = "Very Low  (4 samples)\0Low       (8 samples)\0Medium    (16 samples)\0High      (24 samples)\0Very High (32 samples)\0Ultra     (64 samples)\0Maximum   (255 samples)\0Auto      (variable)\0";
	ui_tooltip = "Global quality control, main performance knob. Higher radii might require higher quality.";
    ui_category = "Global";
> = 2;

uniform float MXAO_SAMPLE_RADIUS <
	ui_type = "drag";
	ui_min = 0.5; ui_max = 20.0;
    ui_label = "Sample Radius";
	ui_tooltip = "Sample radius of MXAO, higher means more large-scale occlusion with less fine-scale details.";  
    ui_category = "Global";      
> = 2.5;

#if (MXAO_HIGH_QUALITY==0)
    uniform float MXAO_SAMPLE_NORMAL_BIAS <
        ui_type = "drag";
        ui_min = 0.0; ui_max = 0.8;
        ui_label = "Normal Bias";
        ui_tooltip = "Occlusion Cone bias to reduce self-occlusion of surfaces that have a low angle to each other.";
        ui_category = "Global";
    > = 0.2;
#else
    #define MXAO_SAMPLE_NORMAL_BIAS 0       //don't break PS which needs this, cleaner this way
#endif

uniform float MXAO_GLOBAL_RENDER_SCALE <
	ui_type = "drag";
    ui_label = "Render Size Scale";
	ui_min = 0.50; ui_max = 1.00;
    ui_tooltip = "Factor of MXAO resolution, lower values greatly reduce performance overhead but decrease quality.\n1.0 = MXAO is computed in original resolution\n0.5 = MXAO is computed in 1/2 width 1/2 height of original resolution\n...";
    ui_category = "Global";
> = 1.0;

uniform float MXAO_SSAO_AMOUNT <
	ui_type = "drag";
	ui_min = 0.00; ui_max = 4.00;
    ui_label = "Ambient Occlusion Amount";        
	ui_tooltip = "Intensity of AO effect. Can cause pitch black clipping if set too high.";
    ui_category = "Ambient Occlusion";
> = 1.00;

#if(MXAO_ENABLE_IL != 0)
uniform float MXAO_SSIL_AMOUNT <
    ui_type = "drag";
    ui_min = 0.00; ui_max = 12.00;
    ui_label = "Indirect Lighting Amount";
    ui_tooltip = "Intensity of IL effect. Can cause overexposured white spots if set too high.";
    ui_category = "Indirect Lighting";
> = 4.00;

uniform float MXAO_SSIL_SATURATION <
    ui_type = "drag";
    ui_min = 0.00; ui_max = 3.00;
    ui_label = "Indirect Lighting Saturation";
    ui_tooltip = "Controls color saturation of IL effect.";
    ui_category = "Indirect Lighting";
> = 1.00;
#endif

#if (MXAO_TWO_LAYER != 0)
    uniform float MXAO_SAMPLE_RADIUS_SECONDARY <
        ui_type = "drag";
        ui_min = 0.1; ui_max = 1.00;
        ui_label = "Fine AO Scale";
        ui_tooltip = "Multiplier of Sample Radius for fine geometry. A setting of 0.5 scans the geometry at half the radius of the main AO.";
        ui_category = "Double Layer";
    > = 0.2;

    uniform float MXAO_AMOUNT_FINE <
        ui_type = "drag";
        ui_min = 0.00; ui_max = 1.00;
        ui_label = "Fine AO intensity multiplier";
        ui_tooltip = "Intensity of small scale AO / IL.";
        ui_category = "Double Layer";
    > = 1.0;

    uniform float MXAO_AMOUNT_COARSE <
        ui_type = "drag";
        ui_min = 0.00; ui_max = 1.00;
        ui_label = "Coarse AO intensity multiplier";
        ui_tooltip = "Intensity of large scale AO / IL.";
        ui_category = "Double Layer";
    > = 1.0;
#endif

uniform int MXAO_BLEND_TYPE <
	ui_type = "slider";
	ui_min = 0; ui_max = 3;
    ui_label = "Blending Mode";
	ui_tooltip = "Different blending modes for merging AO/IL with original color.\0Blending mode 0 matches formula of MXAO 2.0 and older.";
    ui_category = "Blending";
> = 0;

uniform float MXAO_FADE_DEPTH_START <
	ui_type = "drag";
    ui_label = "Fade Out Start";
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Distance where MXAO starts to fade out. 0.0 = camera, 1.0 = sky. Must be less than Fade Out End.";
    ui_category = "Blending";
> = 0.05;

uniform float MXAO_FADE_DEPTH_END <
	ui_type = "drag";
    ui_label = "Fade Out End";
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Distance where MXAO completely fades out. 0.0 = camera, 1.0 = sky. Must be greater than Fade Out Start.";
    ui_category = "Blending";
> = 0.4;

uniform int MXAO_DEBUG_VIEW_ENABLE <
	ui_type = "combo";
    ui_label = "Enable Debug View";
	ui_items = "None\0AO/IL channel\0Normal vectors\0";
	ui_tooltip = "Different debug outputs";
    ui_category = "Debug";
> = 0;

/*=============================================================================
	Textures, Samplers, Globals
=============================================================================*/

#define RESHADE_QUINT_COMMON_VERSION_REQUIRE    202
#define RESHADE_QUINT_EFFECT_DEPTH_REQUIRE      //effect requires depth access
#include "qUINT_common.fxh"

texture2D MXAO_ColorTex 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; MipLevels = 3+MXAO_MIPLEVEL_IL;};
texture2D MXAO_DepthTex 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = R16F;  MipLevels = 3+MXAO_MIPLEVEL_AO;};
texture2D MXAO_NormalTex	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; MipLevels = 3+MXAO_MIPLEVEL_IL;};

sampler2D sMXAO_ColorTex	{ Texture = MXAO_ColorTex;	};
sampler2D sMXAO_DepthTex	{ Texture = MXAO_DepthTex;	};
sampler2D sMXAO_NormalTex	{ Texture = MXAO_NormalTex;	};

texture2D CommonTex0 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; };
sampler2D sCommonTex0	{ Texture = CommonTex0;	};

texture2D CommonTex1 	{ Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; };
sampler2D sCommonTex1	{ Texture = CommonTex1;	};

#if(MXAO_ENABLE_IL != 0)
 #define BLUR_COMP_SWIZZLE xyzw
#else
 #define BLUR_COMP_SWIZZLE w
#endif

/*=============================================================================
	Vertex Shader
=============================================================================*/

struct MXAO_VSOUT
{
	float4                  vpos        : SV_Position;
    float4                  uv          : TEXCOORD0;
    nointerpolation float   samples     : TEXCOORD1;
    nointerpolation float3  uvtoviewADD : TEXCOORD4;
    nointerpolation float3  uvtoviewMUL : TEXCOORD5;
};

struct BlurData
{
	float4 key;
	float4 mask;
};

MXAO_VSOUT VS_MXAO(in uint id : SV_VertexID)
{
    MXAO_VSOUT MXAO;

    MXAO.uv.x = (id == 2) ? 2.0 : 0.0;
    MXAO.uv.y = (id == 1) ? 2.0 : 0.0;
    MXAO.uv.zw = MXAO.uv.xy / MXAO_GLOBAL_RENDER_SCALE;
    MXAO.vpos = float4(MXAO.uv.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    static const int samples_per_preset[8] = {4, 8, 16, 24, 32, 64, 255, 8 /*overridden*/};
    MXAO.samples   = samples_per_preset[MXAO_GLOBAL_SAMPLE_QUALITY_PRESET];
    
    MXAO.uvtoviewADD = float3(-1.0,-1.0,1.0);
    MXAO.uvtoviewMUL = float3(2.0,2.0,0.0);

#if 0
    static const float FOV = 75; //vertical FoV
    MXAO.uvtoviewADD = float3(-tan(radians(FOV * 0.5)).xx,1.0) * qUINT::ASPECT_RATIO.yxx;
   	MXAO.uvtoviewMUL = float3(-2.0 * MXAO.uvtoviewADD.xy,0.0);
#endif

    return MXAO;
}

/*=============================================================================
	Functions
=============================================================================*/

float3 get_position_from_uv(in float2 uv, in MXAO_VSOUT MXAO)
{
    return (uv.xyx * MXAO.uvtoviewMUL + MXAO.uvtoviewADD) * qUINT::linear_depth(uv) * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
}

float3 get_position_from_uv_mipmapped(in float2 uv, in MXAO_VSOUT MXAO, in int miplevel)
{
    return (uv.xyx * MXAO.uvtoviewMUL + MXAO.uvtoviewADD) * tex2Dlod(sMXAO_DepthTex, float4(uv.xyx, miplevel)).x;
}

void spatial_blur_data(inout BlurData o, in sampler inputsampler, in float inputscale, in float4 uv)
{
	o.key = tex2Dlod(inputsampler, uv * inputscale);
	o.mask = tex2Dlod(sMXAO_NormalTex, uv);
	o.mask.xyz = o.mask.xyz * 2 - 1;
}

float compute_spatial_tap_weight(in BlurData center, in BlurData tap)
{
	float depth_term = saturate(1 - abs(tap.mask.w - center.mask.w));
	float normal_term = saturate(dot(tap.mask.xyz, center.mask.xyz) * 16 - 15);
	return depth_term * normal_term;
}

float4 blur_filter(in MXAO_VSOUT MXAO, in sampler inputsampler, in float inputscale, in float radius, in int blursteps)
{
	float4 blur_uv = float4(MXAO.uv.xy, 0, 0);

    BlurData center, tap;
	spatial_blur_data(center, inputsampler, inputscale, blur_uv);

	float4 blursum 			= center.key;
	float4 blursum_noweight = center.key;
	float blurweight = 1;

    static const float2 offsets[8] = 
    {
    	float2(1.5,0.5),float2(-1.5,-0.5),float2(-0.5,1.5),float2(0.5,-1.5),
        float2(1.5,2.5),float2(-1.5,-2.5),float2(-2.5,1.5),float2(2.5,-1.5)
    };

    float2 blur_offsetscale = qUINT::PIXEL_SIZE / inputscale * radius;

	[unroll]
	for(int i = 0; i < blursteps; i++) 
	{
		blur_uv.xy = MXAO.uv.xy + offsets[i] * blur_offsetscale;
		spatial_blur_data(tap, inputsampler, inputscale, blur_uv);

		float tap_weight = compute_spatial_tap_weight(center, tap);

		blurweight += tap_weight;
		blursum.BLUR_COMP_SWIZZLE += tap.key.BLUR_COMP_SWIZZLE * tap_weight;
		blursum_noweight.BLUR_COMP_SWIZZLE += tap.key.BLUR_COMP_SWIZZLE;
	}

	blursum.BLUR_COMP_SWIZZLE /= blurweight;
	blursum_noweight.BLUR_COMP_SWIZZLE /= 1 + blursteps;

	return lerp(blursum.BLUR_COMP_SWIZZLE, blursum_noweight.BLUR_COMP_SWIZZLE, blurweight < 2);
}

void sample_parameter_setup(in MXAO_VSOUT MXAO, in float scaled_depth, in float layer_id, out float scaled_radius, out float falloff_factor)
{
    scaled_radius  = 0.25 * MXAO_SAMPLE_RADIUS / (MXAO.samples * (scaled_depth + 2.0));
    falloff_factor = -1.0/(MXAO_SAMPLE_RADIUS * MXAO_SAMPLE_RADIUS);

    #if(MXAO_TWO_LAYER != 0)
        scaled_radius  *= lerp(1.0, MXAO_SAMPLE_RADIUS_SECONDARY + 1e-6, layer_id);
        falloff_factor *= lerp(1.0, 1.0 / (MXAO_SAMPLE_RADIUS_SECONDARY * MXAO_SAMPLE_RADIUS_SECONDARY + 1e-6), layer_id);
    #endif
}

void smooth_normals(inout float3 normal, in float3 position, in MXAO_VSOUT MXAO)
{
    float2 scaled_radius = 0.018 / position.z * qUINT::ASPECT_RATIO;
    float3 neighbour_normal[4] = {normal, normal, normal, normal};

    [unroll]
    for(int i = 0; i < 4; i++)
    {
        float2 direction;
        sincos(6.28318548 * 0.25 * i, direction.y, direction.x);

        [unroll]
        for(int direction_step = 1; direction_step <= 5; direction_step++)
        {
            float search_radius = exp2(direction_step);
            float2 tap_uv = MXAO.uv.zw + direction * search_radius * scaled_radius;

            float3 temp_normal = tex2Dlod(sMXAO_NormalTex, float4(tap_uv, 0, 0)).xyz * 2.0 - 1.0;
            float3 temp_position = get_position_from_uv_mipmapped(tap_uv, MXAO, 0);

            float3 position_delta = temp_position - position;
            float distance_weight = saturate(1.0 - dot(position_delta, position_delta) * 20.0 / search_radius);
            float normal_angle = dot(normal, temp_normal);
            float angle_weight = smoothstep(0.3, 0.98, normal_angle) * smoothstep(1.0, 0.98, normal_angle); //only take normals into account that are NOT equal to the current normal.

            float total_weight = saturate(3.0 * distance_weight * angle_weight / search_radius);

            neighbour_normal[i] = lerp(neighbour_normal[i], temp_normal, total_weight);
        }
    }

    normal = normalize(neighbour_normal[0] + neighbour_normal[1] + neighbour_normal[2] + neighbour_normal[3]);
}

/*=============================================================================
	Pixel Shaders
=============================================================================*/

void PS_InputBufferSetup(in MXAO_VSOUT MXAO, out float4 color : SV_Target0, out float4 depth : SV_Target1, out float4 normal : SV_Target2)
{
    float3 single_pixel_offset = float3(qUINT::PIXEL_SIZE.xy, 0);

	float3 position          =              get_position_from_uv(MXAO.uv.xy, MXAO);
	float3 position_delta_x1 = - position + get_position_from_uv(MXAO.uv.xy + single_pixel_offset.xz, MXAO);
	float3 position_delta_x2 =   position - get_position_from_uv(MXAO.uv.xy - single_pixel_offset.xz, MXAO);
	float3 position_delta_y1 = - position + get_position_from_uv(MXAO.uv.xy + single_pixel_offset.zy, MXAO);
	float3 position_delta_y2 =   position - get_position_from_uv(MXAO.uv.xy - single_pixel_offset.zy, MXAO);

	position_delta_x1 = lerp(position_delta_x1, position_delta_x2, abs(position_delta_x1.z) > abs(position_delta_x2.z));
	position_delta_y1 = lerp(position_delta_y1, position_delta_y2, abs(position_delta_y1.z) > abs(position_delta_y2.z));

	float deltaz = abs(position_delta_x1.z * position_delta_x1.z - position_delta_x2.z * position_delta_x2.z)
				 + abs(position_delta_y1.z * position_delta_y1.z - position_delta_y2.z * position_delta_y2.z);

	normal  = float4(normalize(cross(position_delta_y1, position_delta_x1)) * 0.5 + 0.5, deltaz);
    color 	= tex2D(qUINT::sBackBufferTex, MXAO.uv.xy);
	depth 	= qUINT::linear_depth(MXAO.uv.xy) * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;   
}

void PS_StencilSetup(in MXAO_VSOUT MXAO, out float4 color : SV_Target0)
{        
    if(    qUINT::linear_depth(MXAO.uv.zw) >= MXAO_FADE_DEPTH_END
        || 0.25 * 0.5 * MXAO_SAMPLE_RADIUS / (tex2D(sMXAO_DepthTex, MXAO.uv.zw).x + 2.0) * BUFFER_HEIGHT < 1.0
        || MXAO.uv.z > 1.0
        || MXAO.uv.w > 1.0
        ) discard;

    color = 1.0;
}

void PS_AmbientObscurance(in MXAO_VSOUT MXAO, out float4 color : SV_Target0)
{
	float3 position = get_position_from_uv_mipmapped(MXAO.uv.zw, MXAO, 0);
    float3 normal = tex2D(sMXAO_NormalTex, MXAO.uv.zw).xyz * 2.0 - 1.0;

    float sample_jitter = dot(floor(MXAO.vpos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;

    float  layer_id = (MXAO.vpos.x + MXAO.vpos.y) % 2.0;

#if(MXAO_SMOOTHNORMALS != 0)
    smooth_normals(normal, position, MXAO);
#endif
    float linear_depth = position.z / RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;        
    position += normal * linear_depth;

    if(MXAO_GLOBAL_SAMPLE_QUALITY_PRESET == 7) MXAO.samples = 2 + floor(0.05 * MXAO_SAMPLE_RADIUS / linear_depth);

    float scaled_radius;
    float falloff_factor;
    sample_parameter_setup(MXAO, position.z, layer_id, scaled_radius, falloff_factor);

    float2 tap_uv, sample_dir;
    sincos(2.3999632 * 16 * sample_jitter, sample_dir.x, sample_dir.y); //2.3999632 * 16
    sample_dir *= scaled_radius;   

    color = 0.0;

    [loop]
    for(int i = 0; i < MXAO.samples; i++)
    {                    
        tap_uv = MXAO.uv.zw + sample_dir.xy * qUINT::ASPECT_RATIO * (i + sample_jitter);   
        sample_dir.xy = mul(sample_dir.xy, float2x2(0.76465, -0.64444, 0.64444, 0.76465)); //cos/sin 2.3999632 * 16            

        float sample_mip = saturate(scaled_radius * i * 20.0) * 3.0;
           
    	float3 delta_v = -position + get_position_from_uv_mipmapped(tap_uv, MXAO, sample_mip + MXAO_MIPLEVEL_AO);                
        float v2 = dot(delta_v, delta_v);
        float vn = dot(delta_v, normal) * rsqrt(v2);

        float sample_ao = saturate(1.0 + falloff_factor * v2) * saturate(vn - MXAO_SAMPLE_NORMAL_BIAS);
#if(MXAO_ENABLE_IL != 0)
        [branch]
        if(sample_ao > 0.1)
        {
                float3 sample_il = tex2Dlod(sMXAO_ColorTex, float4(tap_uv, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz;
                float3 sample_normal = tex2Dlod(sMXAO_NormalTex, float4(tap_uv, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz * 2.0 - 1.0;
                
                sample_il *= sample_ao;
                sample_il *= 0.5 + 0.5*saturate(dot(sample_normal, -delta_v * v2));

                color += float4(sample_il, sample_ao);
        }
#else
        color.w += sample_ao;
#endif
    }

    color = saturate(color / ((1.0 - MXAO_SAMPLE_NORMAL_BIAS) * MXAO.samples) * 2.0);
    color = color.BLUR_COMP_SWIZZLE;
    color = sqrt(color);

#if(MXAO_TWO_LAYER != 0)
    color *= lerp(MXAO_AMOUNT_COARSE, MXAO_AMOUNT_FINE, layer_id); 
#endif
}

void PS_AmbientObscuranceHQ(in MXAO_VSOUT MXAO, out float4 color : SV_Target0)
{
	float3 position = get_position_from_uv_mipmapped(MXAO.uv.zw, MXAO, 0);
	float3 normal 	= normalize(tex2D(sMXAO_NormalTex, MXAO.uv.zw).xyz * 2.0 - 1.0); //fixes black lines	

#if(MXAO_SMOOTHNORMALS != 0)
    smooth_normals(normal, position, MXAO);
#endif

	float3 viewdir 	= normalize(-position);

	int directions = 2 + floor(MXAO.samples / 32) * 2;
	int stepshalf = MXAO.samples / (directions * 2);
	
	float angle_correct = 1 - viewdir.z * viewdir.z; 
	float scaled_radius = MXAO_SAMPLE_RADIUS / position.z / stepshalf * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
	float falloff_factor = 0.25 * rcp(MXAO_SAMPLE_RADIUS * MXAO_SAMPLE_RADIUS);	

	float sample_jitter = dot(floor(MXAO.vpos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;

	float dir_phi = 3.14159265 / directions;
	float2 sample_dir; sincos(dir_phi * sample_jitter * 6, sample_dir.y, sample_dir.x);
	float2x2 rot_dir = float2x2(cos(dir_phi),-sin(dir_phi),
                                sin(dir_phi),cos(dir_phi));

	color = 0;

	[loop]
	for(float i = 0; i < directions; i++)
	{
		sample_dir = mul(sample_dir, rot_dir);
		float2 start = sample_dir * sample_jitter;

		float3 sliceDir = float3(sample_dir, 0);
		float2 h = -1.0;

#if(MXAO_ENABLE_IL != 0)
		float3 il[2];il[0] = 0;il[1] = 0;
#endif
		[loop]
		for(int j = 0; j < stepshalf; j++)
		{
			float4 tap_uv = MXAO.uv.zwzw + scaled_radius * qUINT::PIXEL_SIZE.xyxy * start.xyxy * float4(1,1,-1,-1);
			float sample_mip = saturate(scaled_radius * j * 0.01) * 3.0;

			float3 delta_v[2];
			delta_v[0] = -position + get_position_from_uv_mipmapped(tap_uv.xy, MXAO, sample_mip + MXAO_MIPLEVEL_AO);  
			delta_v[1] = -position + get_position_from_uv_mipmapped(tap_uv.zw, MXAO, sample_mip + MXAO_MIPLEVEL_AO); 

			float2  v2 = float2(dot(delta_v[0], delta_v[0]), 
														dot(delta_v[1], delta_v[1]));

            float2 inv_distance = rsqrt(v2);

			float2 sample_h = float2(dot(delta_v[0], viewdir), 
								     dot(delta_v[1], viewdir)) * inv_distance;

			float2 falloff = saturate(v2 * falloff_factor);
			sample_h = lerp(sample_h, h, falloff);

#if(MXAO_ENABLE_IL != 0)
			float3 sample_il[2], sample_normal[2]; sample_il[0] = 0; sample_il[1] = 0;

			[branch]
			if(falloff.x < 0.8)
			{
				sample_il[0] = tex2Dlod(sMXAO_ColorTex, float4(tap_uv.xy, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz;
				sample_normal[0] = tex2Dlod(sMXAO_NormalTex, float4(tap_uv.xy, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz * 2.0 - 1.0;
				sample_il[0] *= saturate(-inv_distance.x * dot(delta_v[0], sample_normal[0]));
				sample_il[0] = lerp(sample_il[0], il[0], saturate( v2.x * falloff_factor));
			}
			[branch]
			if(falloff.y < 0.8)
			{
	            sample_il[1] = tex2Dlod(sMXAO_ColorTex, float4(tap_uv.zw, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz;
	            sample_normal[1] = tex2Dlod(sMXAO_NormalTex, float4(tap_uv.zw, 0, sample_mip + MXAO_MIPLEVEL_IL)).xyz * 2.0 - 1.0;       
	            sample_il[1] *= saturate(-inv_distance.y * dot(delta_v[1], sample_normal[1]));
	            sample_il[1] = lerp(sample_il[1], il[1], saturate( v2.y * falloff_factor));
			}
#endif

			h.xy = (sample_h > h) ? sample_h : lerp(sample_h, h, 0.75);	

#if(MXAO_ENABLE_IL != 0)
            il[0] = (sample_h.x > h.x) ? sample_il[0] : lerp(sample_il[0], il[0], 0.75);	 
            il[1] = (sample_h.y > h.y) ? sample_il[1] : lerp(sample_il[1], il[1], 0.75);
#endif
			start += sample_dir;
		}

		float3 normal_slice_plane = normalize(cross(sliceDir, viewdir));
		float3 tangent = cross(viewdir, normal_slice_plane);
		float3 proj_normal = normal - normal_slice_plane * dot(normal, normal_slice_plane); 

		float proj_length = length(proj_normal);
		float cos_gamma = clamp(dot(proj_normal, viewdir) * rcp(proj_length), -1.0, 1.0);
		float gamma = -sign(dot(proj_normal, tangent)) * acos(cos_gamma);

		h = acos(min(h, 1));

		h.x = gamma + max(-h.x - gamma, -1.5707963);
		h.y = gamma + min( h.y - gamma,  1.5707963);

        h *= 2;		

		float2 sample_ao = cos_gamma + h * sin(gamma) - cos(h - gamma);
		color.w += proj_length * dot(sample_ao, 0.25); 
#if(MXAO_ENABLE_IL != 0)
		color.rgb += proj_length * sample_ao.x * 0.25 * il[0];
		color.rgb += proj_length * sample_ao.y * 0.25 * il[1];
#endif
	}

    color /= directions;
    color.w = 1 - color.w;
	color = color.BLUR_COMP_SWIZZLE;
	color = sqrt(color);
}

void PS_SpatialFilter1(in MXAO_VSOUT MXAO, out float4 color : SV_Target0)
{
    color = blur_filter(MXAO, sCommonTex0, MXAO_GLOBAL_RENDER_SCALE, 0.75, 4);
}

void PS_SpatialFilter2(MXAO_VSOUT MXAO, out float4 color : SV_Target0)
{
    float4 ssil_ssao = blur_filter(MXAO, sCommonTex1, 1, 1.0 / MXAO_GLOBAL_RENDER_SCALE, 8);
    ssil_ssao *= ssil_ssao;
	color = tex2D(sMXAO_ColorTex, MXAO.uv.xy);

    static const float3 lumcoeff = float3(0.2126, 0.7152, 0.0722);
    float scenedepth = qUINT::linear_depth(MXAO.uv.xy);        
    float colorgray = dot(color.rgb, lumcoeff);
    float blendfact = 1.0 - colorgray;

#if(MXAO_ENABLE_IL != 0)
	ssil_ssao.xyz  = lerp(dot(ssil_ssao.xyz, lumcoeff), ssil_ssao.xyz, MXAO_SSIL_SATURATION) * MXAO_SSIL_AMOUNT * 2.0;
#else
    ssil_ssao.xyz = 0.0;
#endif

#if(MXAO_HIGH_QUALITY == 0)
	ssil_ssao.w  = 1.0 - pow(saturate(1.0 - ssil_ssao.w), MXAO_SSAO_AMOUNT * 2.0);
#else
    ssil_ssao.w  = 1.0 - pow(saturate(1.0 - ssil_ssao.w), MXAO_SSAO_AMOUNT);
#endif
    ssil_ssao    *= 1.0 - smoothstep(MXAO_FADE_DEPTH_START, MXAO_FADE_DEPTH_END, scenedepth * float4(2.0, 2.0, 2.0, 1.0));

    if(MXAO_BLEND_TYPE == 0)
    {
        color.rgb -= (ssil_ssao.www - ssil_ssao.xyz) * blendfact * color.rgb;
    }
    else if(MXAO_BLEND_TYPE == 1)
    {
        color.rgb = color.rgb * saturate(1.0 - ssil_ssao.www * blendfact * 1.2) + ssil_ssao.xyz * blendfact * colorgray * 2.0;
    }
    else if(MXAO_BLEND_TYPE == 2)
    {
        float colordiff = saturate(2.0 * distance(normalize(color.rgb + 1e-6),normalize(ssil_ssao.rgb + 1e-6)));
        color.rgb = color.rgb + ssil_ssao.rgb * lerp(color.rgb, dot(color.rgb, 0.3333), colordiff) * blendfact * blendfact * 4.0;
        color.rgb = color.rgb * (1.0 - ssil_ssao.www * (1.0 - dot(color.rgb, lumcoeff)));
    }
    else if(MXAO_BLEND_TYPE == 3)
    {
        color.rgb *= color.rgb;
        color.rgb -= (ssil_ssao.www - ssil_ssao.xyz) * color.rgb;
        color.rgb = sqrt(color.rgb);
    }

    if(MXAO_DEBUG_VIEW_ENABLE == 1)
    {
        color.rgb = max(0.0, 1.0 - ssil_ssao.www + ssil_ssao.xyz);
        color.rgb *= (MXAO_ENABLE_IL != 0) ? 0.5 : 1.0;
    }
    else if(MXAO_DEBUG_VIEW_ENABLE == 2)
    {      
        color.rgb = tex2D(sMXAO_NormalTex, MXAO.uv.xy).xyz;
        color.b = 1-color.b; //looks nicer
    }
       
    color.a = 1.0;        
}

/*=============================================================================
	Techniques
=============================================================================*/

technique MXAO 
< ui_tooltip = "                     >> qUINT::MXAO <<\n\n"
			   "MXAO is a screen-space ambient occlusion shader.\n"
               "It adds diffuse shading to object corners to give more depth\n"
               "and detail to the scene. Check out the preprocessor options to\n"
               "get access to more functionality.\n"
               "\nMake sure to move MXAO to the very top of your shader list for\n"
               "maximum compatibility with other shaders.\n"
               "\nMXAO is written by Marty McFly / Pascal Gilcher"; >
{
    pass
	{
		VertexShader = VS_MXAO;
		PixelShader  = PS_InputBufferSetup;
		RenderTarget0 = MXAO_ColorTex;
		RenderTarget1 = MXAO_DepthTex;
		RenderTarget2 = MXAO_NormalTex;
	}
    pass
    {
        VertexShader = VS_MXAO;
		PixelShader  = PS_StencilSetup;
        /*Render Target is Backbuffer*/
        ClearRenderTargets = true;
		StencilEnable = true;
		StencilPass = REPLACE;
        StencilRef = 1;
    }
#if(MXAO_HIGH_QUALITY != 0)
    pass
    {
        VertexShader = VS_MXAO;
        PixelShader  = PS_AmbientObscuranceHQ;
        RenderTarget = CommonTex0;
        ClearRenderTargets = true;
        StencilEnable = true;
        StencilPass = KEEP;
        StencilFunc = EQUAL;
        StencilRef = 1;
    }
#else
    pass
    {
        VertexShader = VS_MXAO;
        PixelShader  = PS_AmbientObscurance;
        RenderTarget = CommonTex0;
        ClearRenderTargets = true;
        StencilEnable = true;
        StencilPass = KEEP;
        StencilFunc = EQUAL;
        StencilRef = 1;
    }
#endif
    pass
	{
		VertexShader = VS_MXAO;
		PixelShader  = PS_SpatialFilter1;
        RenderTarget = CommonTex1;
	}
	pass
	{
		VertexShader = VS_MXAO;
		PixelShader  = PS_SpatialFilter2;
        /*Render Target is Backbuffer*/
	}
}