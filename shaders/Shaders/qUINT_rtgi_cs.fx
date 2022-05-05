/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg    

    * Unauthorized copying of this file, via any medium is strictly prohibited
 	* Proprietary and confidential

=============================================================================*/

#if((__RENDERER__ < 0xb000) || ((__RENDERER__ >= 0x10000) && (__RENDERER__ < 0x14300)))
 #error "Game API does not support compute shaders with required feature set. Please use RTGI 0.21 instead."
#endif

#if __RESHADE__ < 40900
 #error "Update ReShade to at least 4.9.0"
#endif

/*=============================================================================
	Preprocessor settings
=============================================================================*/

#ifndef SMOOTHNORMALS
 #define SMOOTHNORMALS 			0   //[0 to 3]      0: off | 1: enables some filtering of the normals derived from depth buffer to hide 3d model blockyness
#endif

#ifndef FADEOUT_MODE
 #define FADEOUT_MODE 			0   //[0 to 3]      0: smoothstep original* using distance vs depth | 1: linear | 2: biquadratic | 3: exponential
#endif

#ifndef INFINITE_BOUNCES
 #define INFINITE_BOUNCES       0   //[0 or 1]      If enabled, path tracer samples previous frame GI as well, causing a feedback loop to simulate secondary bounces, causing a more widespread GI.
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform int UIHELP <
	ui_type = "radio";
	ui_label = " ";	
	ui_text ="This shader adds ray traced / ray marched global illumination to games\nby traversing the height field described by the depth map of the game.\n\nHover over the settings below to display more information.\n\n          >>>>>>>>>> IMPORTANT <<<<<<<<<      \n\nIf the shader appears to do nothing when enabled, make sure ReShade's\ndepth access is properly set up - no output without proper input.\n\n          >>>>>>>>>> IMPORTANT <<<<<<<<<      ";
	ui_category = ">>>> OVERVIEW / HELP (click me) <<<<";
	ui_category_closed = true;
>;

uniform float RT_SAMPLE_RADIUS <
	ui_type = "drag";
	ui_min = 0.5; ui_max = 20.0;
    ui_step = 0.01;
    ui_label = "Ray Length";
	ui_tooltip = "Maximum ray length, directly affects\nthe spread radius of shadows / bounce lighting";
    ui_category = "Ray Tracing";
> = 4.0;

uniform float RT_SAMPLE_RADIUS_FAR <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.01;
    ui_label = "Extended Ray Length Multiplier";
	ui_tooltip = "Increases ray length in the background to achieve ultra wide light bounces.";
    ui_category = "Ray Tracing";
> = 0.0;

uniform int RT_RAY_AMOUNT <
	ui_type = "slider";
	ui_min = 1; ui_max = 20;
    ui_label = "Amount of Rays";
    ui_tooltip = "Amount of rays launched per pixel in order to\nestimate the global illumination at this location.\nAmount of noise to filter is proportional to sqrt(rays).";
    ui_category = "Ray Tracing";
> = 3;

uniform int RT_RAY_STEPS <
	ui_type = "slider";
	ui_min = 1; ui_max = 40;
    ui_label = "Amount of Steps per Ray";
    ui_tooltip = "RTGI performs step-wise raymarching to check for ray hits.\nFewer steps may result in rays skipping over small details.";
    ui_category = "Ray Tracing";
> = 12;

uniform float RT_Z_THICKNESS <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 4.0;
    ui_step = 0.01;
    ui_label = "Z Thickness";
	ui_tooltip = "The shader can't know how thick objects are, since it only\nsees the side the camera faces and has to assume a fixed value.\n\nUse this parameter to remove halos around thin objects.";
    ui_category = "Ray Tracing";
> = 0.5;

uniform bool RT_HIGHP_LIGHT_SPREAD <
    ui_label = "Enable precise light spreading";
    ui_tooltip = "Rays accept scene intersections within a small error margin.\nEnabling this will snap rays to the actual hit location.\nThis results in sharper but more realistic lighting.";
    ui_category = "Ray Tracing";
> = true;

uniform bool RT_BACKFACE_MIRROR <
    ui_label = "Enable simulation of backface lighting";
    ui_tooltip = "RTGI can only simulate light bouncing of the objects visible on the screen.\nTo estimate light coming from non-visible sides of otherwise visible objects,\nthis feature will just take the front-side color instead.";
    ui_category = "Ray Tracing";
> = false;

uniform bool RT_USE_FALLBACK <
    ui_label = "Enable ray miss fallback (experimental)";
    ui_tooltip = "RTGI normally only accepts actual ray intersection locations. To not waste already completed computing effort,\nenabling this will cause invalid samples to be treated like a regular SSAO sample and possible GI influence will be recovered.\n!!EXPERIMENTAL!!";
    ui_category = "Ray Tracing";
> = false;

uniform float RT_AO_AMOUNT <
	ui_type = "drag";
	ui_min = 0; ui_max = 10.0;
    ui_step = 0.01;
    ui_label = "Ambient Occlusion Intensity";
    ui_category = "Blending";
> = 4.0;

uniform float RT_IL_AMOUNT <
	ui_type = "drag";
	ui_min = 0; ui_max = 10.0;
    ui_step = 0.01;
    ui_label = "Bounce Lighting Intensity";
    ui_category = "Blending";
> = 4.0;

#if INFINITE_BOUNCES != 0
    uniform float RT_IL_BOUNCE_WEIGHT <
        ui_type = "drag";
        ui_min = 0; ui_max = 2.0;
        ui_step = 0.01;
        ui_label = "Next Bounce Weight";
        ui_category = "Blending";
    > = 0.0;
#endif

uniform float2 RT_FADE_DEPTH <
	ui_type = "drag";
    ui_label = "Fade Out Start / End";
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Distance where GI starts to fade out | is completely faded out.";
    ui_category = "Blending";
> = float2(0.0, 0.5);

uniform int RT_DEBUG_VIEW <
	ui_type = "radio";
    ui_label = "Enable Debug View";
	ui_items = "None\0Lighting Channel\0Normal Channel\0";
	ui_tooltip = "Different debug outputs";
    ui_category = "Debug";
> = 0;

uniform int UIHELP2 <
	ui_type = "radio";
	ui_label = " ";	
	ui_text ="Description for preprocessor definitions:\n\nINFINITE_BOUNCES\n0: off\n1: allows the light to reflect more than once.\n\nFADEOUT_MODE\n0-3: multiple different fadeout modes\n\nSMOOTHNORMALS\n0: off\n1: enables normal map filtering, reduces blockyness on low poly surfaces.";
	ui_category = ">>>> PREPROCESSOR DEFINITION GUIDE (click me) <<<<";
	ui_category_closed = false;
>;
/*
uniform float4 tempF1 <
    ui_type = "drag";
    ui_min = -100.0;
    ui_max = 100.0;
> = float4(1,1,1,1);

uniform float4 tempF2 <
    ui_type = "drag";
    ui_min = -100.0;
    ui_max = 100.0;
> = float4(1,1,1,1);

uniform float4 tempF3 <
    ui_type = "drag";
    ui_min = -100.0;
    ui_max = 100.0;
> = float4(1,1,1,1);
*/
/*=============================================================================
	Textures, Samplers, Globals, Structs
=============================================================================*/

#include "qUINT\Global.fxh"
#include "qUINT\Depth.fxh"
#include "qUINT\Projection.fxh"
#include "qUINT\Normal.fxh"
#include "qUINT\Random.fxh"
#include "qUINT\RaySorting.fxh"

uniform uint FRAMECOUNT  < source = "framecount"; >;
uniform float FRAMETIME   < source = "frametime"; >;

//log2 macro for uints up to 16 bit
#define CONST_LOG2(v)   (((uint(v) >> 1u) != 0u) + ((uint(v) >> 2u) != 0u) + ((uint(v) >> 3u) != 0u) + ((uint(v) >> 4u) != 0u) + ((uint(v) >> 5u) != 0u) + ((uint(v) >> 6u) != 0u) + ((uint(v) >> 7u) != 0u) + ((uint(v) >> 8u) != 0u) + ((uint(v) >> 9u) != 0u) + ((uint(v) >> 10u) != 0u) + ((uint(v) >> 11u) != 0u) + ((uint(v) >> 12u) != 0u) + ((uint(v) >> 13u) != 0u) + ((uint(v) >> 14u) != 0u) + ((uint(v) >> 15u) != 0u))

//integer divide, rounding up
#define CEIL_DIV(num, denom) (((num - 1) / denom) + 1)

//for 1920x1080, use 3 mip levels
//double the screen size, use one mip level more
//log2(1920/240) = 3
//log2(3840/240) = 4
#define MIP_AMT 	CONST_LOG2(BUFFER_WIDTH / 240)
#define MIP_BIAS_IL	2

texture ZTex            { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = R16F;      MipLevels = MIP_AMT;};
texture NTex            { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGB10A2; };
texture CTex            { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGB10A2;   MipLevels = MIP_AMT + MIP_BIAS_IL;};
texture GBufferTex0     { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GBufferTex1     { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GBufferTex2     { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GITex0	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GITex1	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GITex2	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture FilterTex0      { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture FilterTex1      { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };

sampler sZTex           { Texture = ZTex;           };
sampler sNTex           { Texture = NTex;           };
sampler sCTex           { Texture = CTex;           };
sampler sGBufferTex0	{ Texture = GBufferTex0;	};
sampler sGBufferTex1	{ Texture = GBufferTex1;	};
sampler sGBufferTex2	{ Texture = GBufferTex2;	};
sampler sGITex0       	{ Texture = GITex0;         };
sampler sGITex1       	{ Texture = GITex1;         };
sampler sGITex2       	{ Texture = GITex2;         };
sampler sFilterTex0     { Texture = FilterTex0;     };
sampler sFilterTex1     { Texture = FilterTex1;     };

//DX 11.0 does not support more than 8 UAV bind slots, do it the old fashioned way until ReShade toggles these per entry point
//Alternative would be 2 techniques in separate files, but that's ugly... Overhead of doing the resource creation per PS
//is not that bad anyways. The storage revolver I use is much more effective at reducing overhead and code complexion.
//storage stZTex          { Texture = ZTex;         };
//storage stNTex          { Texture = NTex;         };
//storage stCTex          { Texture = CTex;         };
storage stGBufferTex0	{ Texture = GBufferTex0;	};
storage stGBufferTex1	{ Texture = GBufferTex1;	};
storage stGBufferTex2	{ Texture = GBufferTex2;	};
storage stGITex0       	{ Texture = GITex0;         };
storage stGITex1       	{ Texture = GITex1;         };
storage stGITex2       	{ Texture = GITex2;         };
storage stFilterTex0    { Texture = FilterTex0;     };
storage stFilterTex1    { Texture = FilterTex1;     };

texture JitterTex < source = "bluenoise.png"; > { Width = 32; 			  Height = 32; 				Format = RGBA8; };
sampler	sJitterTex          { Texture = JitterTex; AddressU = WRAP; AddressV = WRAP;};
sampler ColorInputLinear 	{ Texture = ColorInputTex; };

struct CSIN 
{
    uint3 groupthreadid     : SV_GroupThreadID;         //XYZ idx of thread inside group
    uint3 groupid           : SV_GroupID;               //XYZ idx of group inside dispatch
    uint3 dispatchthreadid  : SV_DispatchThreadID;      //XYZ idx of thread inside dispatch
    uint threadid           : SV_GroupIndex;            //flattened idx of thread inside group
};

struct VSOUT
{
	float4 vpos : SV_Position;
    float2 uv   : TEXCOORD0;
};

/*=============================================================================
	Functions
=============================================================================*/

float4 safe_tex2Dfetch(sampler src, int2 coord)
{
    uint2 size = tex2Dsize(src);
    coord = clamp(coord, 0, size - 1); //CLAMP mode
    return tex2Dfetch(src, coord);
}

float2 pixel_idx_to_uv(uint2 pos, float2 texture_size)
{
    float2 inv_texture_size = rcp(texture_size);
    return pos * inv_texture_size + 0.5 * inv_texture_size;
}

void unpack_hdr(inout float3 color)
{
    color  = saturate(color);
    color = color * rcp(1.01 - saturate(color)); 
    //color = sRGB2AP1(color);
}

void pack_hdr(inout float3 color)
{
    //color = AP12sRGB(color);
    color = 1.01 * color * rcp(color + 1.0);
    color  = saturate(color);
}

float3 dither(in VSOUT i)
{
    const float2 magicdot = float2(0.75487766624669276, 0.569840290998);
    const float3 magicadd = float3(0, 0.025, 0.0125) * dot(magicdot, 1);

    const int bit_depth = 8; //TODO: add BUFFER_COLOR_DEPTH once it works
    const float lsb = exp2(bit_depth) - 1;

    float3 dither = frac(dot(i.vpos.xy, magicdot) + magicadd);
    dither /= lsb;
    
    return dither;
}

void store_data_revolver(storage st0, storage st1, storage st2, uint2 coord, float4 res)
{
    uint writeslot = FRAMECOUNT % 3u;
         if(writeslot == 0) tex2Dstore(st0, coord, res);
    else if(writeslot == 1) tex2Dstore(st1, coord, res);
    else if(writeslot == 2) tex2Dstore(st2, coord, res);
}

float4 fetch_data_revolver(sampler s0, sampler s1, sampler s2, uint2 coord)
{
    uint readslot = FRAMECOUNT % 3u;
    float4 o = 0;

         if(readslot == 0) o = tex2Dfetch(s0, coord, 0);
    else if(readslot == 1) o = tex2Dfetch(s1, coord, 0);
    else if(readslot == 2) o = tex2Dfetch(s2, coord, 0);

    return o;
}

float fade_distance(in VSOUT i)
{
    float distance = saturate(length(Projection::uv_to_proj(i.uv)) / RESHADE_DEPTH_LINEARIZATION_FAR_PLANE);
    float fade;
#if(FADEOUT_MODE == 1)
    fade = saturate((RT_FADE_DEPTH.y - distance) / (RT_FADE_DEPTH.y - RT_FADE_DEPTH.x + 1e-6));
#elif(FADEOUT_MODE == 2)
    fade = saturate((RT_FADE_DEPTH.y - distance) / (RT_FADE_DEPTH.y - RT_FADE_DEPTH.x + 1e-6));
    fade *= fade; 
    fade *= fade;
#elif(FADEOUT_MODE == 3)
    fade = exp(-distance * rcp(RT_FADE_DEPTH.y * RT_FADE_DEPTH.y * 8.0 + 0.001) + RT_FADE_DEPTH.x);
#else
    fade = smoothstep(RT_FADE_DEPTH.y+0.001, RT_FADE_DEPTH.x, distance);  
#endif

    return fade;    
}

float3 smooth_normals(float2 iuv)
{
    const float max_n_n = 0.63;
    const float max_v_s = 0.65;
    const float max_c_p = 0.5;
    const float searchsize = 0.0125;
    const int dirs = 5;

    float4 gbuf_center = float4(tex2Dlod(sNTex, iuv, 0).xyz * 2.0 - 1.0, tex2Dlod(sZTex, iuv, 0).x); //tex2D(sRTGITempTex1, i.uv);

    float3 n_center = gbuf_center.xyz;
    float3 p_center = Projection::uv_to_proj(iuv, gbuf_center.w);
    float radius = searchsize + searchsize * rcp(p_center.z) * 2.0;
    float worldradius = radius * p_center.z;

    int steps = clamp(ceil(radius * 300.0) + 1, 1, 7);
    float3 n_sum = 0.001 * n_center;

    for(float j = 0; j < dirs; j++)
    {
        float2 dir; sincos(radians(360.0 * j / dirs + 0.666), dir.y, dir.x);

        float3 n_candidate = n_center;
        float3 p_prev = p_center;

        for(float stp = 1.0; stp <= steps; stp++)
        {
            float fi = stp / steps;   
            fi *= fi * rsqrt(fi);

            float offs = fi * radius;
            offs += length(BUFFER_PIXEL_SIZE);

            float2 uv = iuv + dir * offs * BUFFER_ASPECT_RATIO;            
            if(!all(saturate(uv - uv*uv))) break;

            float4 gbuf = float4(tex2Dlod(sNTex, uv, 0).xyz * 2.0 - 1.0, tex2Dlod(sZTex, uv, 0).x);//tex2Dlod(sRTGITempTex1, float4(uv, 0, 0));
            float3 n = gbuf.xyz;
            float3 p = Projection::uv_to_proj(uv, gbuf.w);

            float3 v_increment  = normalize(p - p_prev);

            float ndotn         = dot(n, n_center); 
            float vdotn         = dot(v_increment, n_center); 
            float v2dotn        = dot(normalize(p - p_center), n_center); 
          
            ndotn *= max(0, 1.0 + fi *0.5 * (1.0 - abs(v2dotn)));

            if(abs(vdotn)  > max_v_s || abs(v2dotn) > max_c_p) break;       

            if(ndotn > max_n_n)
            {
                float d = distance(p, p_center) / worldradius;
                float w = saturate(4.0 - 2.0 * d) * smoothstep(max_n_n, lerp(max_n_n, 1.0, 2), ndotn); //special recipe
                w = stp < 1.5 && d < 2.0 ? 1 : w;  //special recipe       
                n_candidate = lerp(n_candidate, n, w);
                n_candidate = normalize(n_candidate);
            }

            p_prev = p;
            n_sum += n_candidate;
        }
    }

    n_sum = normalize(n_sum);
    return n_sum;
}

float3 ggx_vndf(float2 uniform_disc, float2 alpha, float3 v)
{
	//scale by alpha, 3.2
	float3 Vh = normalize(float3(alpha * v.xy, v.z));
	//point on projected area of hemisphere
	float2 p = uniform_disc;
	p.y = lerp(sqrt(1.0 - p.x*p.x), 
		       p.y,
		       Vh.z * 0.5 + 0.5);

	float3 Nh =  float3(p.xy, sqrt(saturate(1.0 - dot(p, p)))); //150920 fixed sqrt() of z

	//reproject onto hemisphere
	Nh = mul(Nh, Normal::base_from_vector(Vh));

	//revert scaling
	Nh = normalize(float3(alpha * Nh.xy, saturate(Nh.z)));

	return Nh;
}

float3 schlick_fresnel(float vdoth, float3 f0)
{
	vdoth = saturate(vdoth);
	return lerp(pow(vdoth, 5), 1, f0);
}

float ggx_g2_g1(float3 l, float3 v, float2 alpha)
{
	//smith masking-shadowing g2/g1, v and l in tangent space
	l.xy *= alpha;
	v.xy *= alpha;
	float nl = length(l);
	float nv = length(v);

    float ln = l.z * nv;
    float lv = l.z * v.z;
    float vn = v.z * nl;
    //in tangent space, v.z = ndotv and l.z = ndotl
    return (ln + lv) / (vn + ln + 1e-7);
}

/*=============================================================================
	Shader entry points
=============================================================================*/

VSOUT VS_Main(in uint id : SV_VertexID)
{
    VSOUT o;
    VS_FullscreenTriangle(id, o.vpos, o.uv); //use original fullscreen triangle VS
    return o;
}

/* Soon...

#define GRP_SIZE_X    16
#define GRP_SIZE_Y    16

groupshared min16float depth_samples[(GRP_SIZE_X + 2 * 2) * (GRP_SIZE_Y + 2 * 2)];

void CS_MakeInputs(in CSIN i)
{
    //todo: check and improve bank conflicts, this is super unoptimized

    const uint2 group_size = uint2(GRP_SIZE_X, GRP_SIZE_Y);

    //sampling every odd pixel, but with 2 pixels padding in each direction
    //grid size = (group size / 2) + 2 //padding both sides once
    //spans group size + 4 pixels but is half the size, as only every second pixel is gathered on
    const uint2 gather_grid_size = (group_size / 2) + 2;

    //get position of thread inside this grid
    uint2 gather_grid_pos;
    gather_grid_pos.x = i.threadid % gather_grid_size.x;
    gather_grid_pos.y = i.threadid / gather_grid_size.x;

    //transform to offset to group topleft corner
    uint2 gather_sample_pos = gather_grid_pos * 2 - 2;
    
    //transform to pixel position in texture
    uint2 gather_sample_pos_global = i.groupid.xy * group_size + gather_sample_pos; 

    //transform to uv on texture
    float2 gather_sample_uv = pixel_idx_to_uv(gather_sample_pos_global, BUFFER_SCREEN_SIZE);
    
    if(i.threadid < gather_grid_size.x * gather_grid_size.y)
    {
        //float4 depths = tex2DgatherR(sZTex, gather_sample_uv);
        float2 corrected_uv = Depth::correct_uv(gather_sample_uv); //fixed for lookup    

    #if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
        corrected_uv.y -= BUFFER_PIXEL_SIZE.y * 0.5;    //shift upwards since gather looks down and right
    #endif

        min16float4 depth_texels = tex2DgatherR(DepthInput, corrected_uv);

    #if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
        depth_texels = depth_texels.wzyx; //flip gather order vertically if the sampling is upside down
    #endif

        depth_texels = Depth::linearize_depths(depth_texels);

        //offsets for xyzw components
        const uint2 offsets[4] = {uint2(0, 1), uint2(1, 1), uint2(1, 0), uint2(0, 0)};

        [unroll]
        for(int j = 0; j < 4; j++)
        {
            uint2 pos_in_mem_grid = gather_grid_pos * 2 + offsets[j];
            uint pos_in_mem = pos_in_mem_grid.y * (gather_grid_size.x * 2) + pos_in_mem_grid.x; //unroll to 1D
            depth_samples[pos_in_mem] = depth_texels[j];
        }

        [branch]
        if(    gather_sample_pos.x >= 0
            && gather_sample_pos.y >= 0
            && gather_sample_pos.x <= GRP_SIZE_X - 2 
            && gather_sample_pos.y <= GRP_SIZE_Y - 2 )
        {     
            //and while we're at it, sampling fullscreen every odd pixel, might as well downsample color.
            float4 color = tex2Dlod(ColorInputLinear, gather_sample_uv + BUFFER_PIXEL_SIZE * 0.5, 0);
            color *= saturate(100.0 - dot(depth_texels, 99.0 * 0.25));
            tex2Dstore(stCTex, gather_sample_pos_global / 2, color);
        }
    }

    barrier();

    //center, top, bottom, left, right
    const uint2 sample_offsets[5] = {uint2(0,0),uint2(0,-1),uint2(0,1),uint2(-1,0),uint2(1,0)};
    float3 proj_pos[5]; //cannot be min16

    [unroll]
    for(int j = 0; j < 5; j++)
    {
        uint2 pos_in_mem_grid = i.groupthreadid.xy + uint2(2, 2); //mem grid start is offset by -2, -2
        pos_in_mem_grid += sample_offsets[j];

        uint pos_in_mem = pos_in_mem_grid.y * (gather_grid_size.x * 2) + pos_in_mem_grid.x; //unroll to 1D
        min16float depth = depth_samples[pos_in_mem];

        float2 offset_uv = pixel_idx_to_uv(i.dispatchthreadid.xy + sample_offsets[j], BUFFER_SCREEN_SIZE);

        proj_pos[j] = Projection::uv_to_proj(offset_uv, Projection::depth_to_z(depth));
    }    

    //casting after conversion is fine - before, precision loss is too vast
    min16float3 delta_y_1 = proj_pos[0] - proj_pos[1]; //center - top
    min16float3 delta_y_2 = proj_pos[2] - proj_pos[0]; //bottom - center
    min16float3 delta_y = abs(delta_y_1.z) < abs(delta_y_2.z) ? delta_y_1 : delta_y_2;

    min16float3 delta_x_1 = proj_pos[0] - proj_pos[3]; //center - left
    min16float3 delta_x_2 = proj_pos[4] - proj_pos[0]; //right - center    
    min16float3 delta_x = abs(delta_x_1.z) < abs(delta_x_2.z) ? delta_x_1 : delta_x_2;
     
    min16float3 normal = normalize(cross(delta_y, delta_x)); 

    tex2Dstore(stNTex, i.dispatchthreadid.xy, normal.xyzz * 0.5 + 0.5);
    tex2Dstore(stZTex, i.dispatchthreadid.xy, proj_pos[0].z);

//if smooth normals are enabled, we create this buffer from scratch anyways
#if SMOOTHNORMALS == 0
    tex2Dstore(stGBufferTexC, i.dispatchthreadid.xy, float4(normal, proj_pos[0].z));
#endif
}

void CS_SmoothNormals(in CSIN i)
{
    tex2Dstore(stGBufferTexC, i.dispatchthreadid, smoothnormals);
}
*/

void PS_MakeInputs(in VSOUT i, out MRT3 o)
{ 
    o.t0 = Depth::get_linear_depth(i.uv);
    o.t1 = tex2D(ColorInputLinear, i.uv);
    o.t1 *= saturate(999.0 - o.t0.x * 1000.0); //mask sky
    o.t0 = Projection::depth_to_z(o.t0.x);
    o.t2.xyz = Normal::normal_from_depth(i.uv) * 0.5 + 0.5;
    o.t2.w = 1;
}

struct HybridRay 
{
    float2 origin_2D;
    float3 origin_3D;
    float2 dir_2D;
    float3 dir_3D;
    float length_2D;
    float length_3D;
    float2 pos_2D;
    float3 pos_3D;
    float width;
};

void CS_RTMain(in CSIN i)
{ 
    float3 jitter        = tex2Dfetch(sJitterTex,  i.dispatchthreadid.xy        & 0x1F).xyz;
    jitter = frac(jitter + tex2Dfetch(sJitterTex, (i.dispatchthreadid.xy >> 5u) & 0x1F).xyz);
    
    float2 uv = pixel_idx_to_uv(i.dispatchthreadid.xy, BUFFER_SCREEN_SIZE);
    float3 n = normalize(tex2Dlod(sNTex, uv, 0).xyz * 2.0 - 1.0);
    float3 p = Projection::uv_to_proj(uv); //can't hurt to have best data..

#if SMOOTHNORMALS != 0
    n = smooth_normals(uv);
#endif

    float4 gbuffer = float4(n.xyz, p.z);
    store_data_revolver(stGBufferTex0, stGBufferTex1, stGBufferTex2, i.dispatchthreadid.xy, gbuffer);

    SampleSet sampleset = ray_sorting(FRAMECOUNT, jitter.x); 

    p *= 0.999;
	p += n * Projection::z_to_depth(p.z);

    float4 rtgi = 0;

    for(uint r = 0; r < RT_RAY_AMOUNT; r++)
    {
        HybridRay ray;
        ray.origin_3D = p; 
  
        //lambert cosine distribution without TBN reorientation
        ray.dir_3D.z = (r + sampleset.index) / RT_RAY_AMOUNT * 2.0 - 1.0; 
        ray.dir_3D.xy = sampleset.dir_xy * sqrt(1.0 - ray.dir_3D.z * ray.dir_3D.z); //build sphere
        ray.dir_3D = normalize(ray.dir_3D + n);
        sampleset.dir_xy = mul(sampleset.dir_xy, sampleset.nextdir);

        ray.dir_2D = normalize(ray.dir_3D.xy * BUFFER_ASPECT_RATIO);
        ray.width = 0;

        float stepjitter = Random::goldenweyl1(r, jitter.y);       
        
        min16float STEP_CURVE = 8;
        const min16float MAX_SCREEN_RADIUS = 1.414; //should be length of diagonal screen, but a bit lower does not hurt

        min16float intersected = 0;
        min16float3 prevpos = ray.origin_3D;
        min16float prevdelta = 0;
        min16float3 pos;

        //min16float ray_termination_length = RT_SAMPLE_RADIUS*RT_SAMPLE_RADIUS;
        min16float ray_termination_length = RT_SAMPLE_RADIUS * RT_SAMPLE_RADIUS;
        ray_termination_length *= 1.0 + 100.0 * saturate(Projection::z_to_depth(p.z) * RT_SAMPLE_RADIUS_FAR);
 
        ray.origin_2D = uv + BUFFER_PIXEL_SIZE * ray.dir_2D / max(abs(ray.dir_2D.x), abs(ray.dir_2D.y)) * 2.0;

        for(uint s = 1; s <= RT_RAY_STEPS; s++)
        {
            min16float lambda = (s + stepjitter) * rcp(RT_RAY_STEPS);
            lambda = (exp2(STEP_CURVE * lambda) - 1) * rcp(exp2(STEP_CURVE) - 1);           

            ray.length_2D = lambda * MAX_SCREEN_RADIUS;
            ray.pos_2D = ray.origin_2D + ray.dir_2D * ray.length_2D ;

            if(!all(saturate(-ray.pos_2D * ray.pos_2D + ray.pos_2D))) break;

            ray.width = clamp(log2(length(ray.dir_2D * ray.length_2D * BUFFER_SCREEN_SIZE)) - 3.0, 0, MIP_AMT);

            pos = Projection::uv_to_proj(ray.pos_2D, sZTex, ray.width);
            ray.length_3D = length(cross(0 - ray.origin_3D, normalize(pos))) / length(cross(ray.dir_3D, normalize(pos)));

            if(ray.length_3D > ray_termination_length) break;     

            ray.pos_3D = ray.origin_3D + ray.dir_3D * ray.length_3D;           

            min16float delta = pos.z - ray.pos_3D.z;
            min16float z_thickness = (RT_Z_THICKNESS * RT_Z_THICKNESS) * (ray.length_3D + 0.2) * 10.0;    

            [branch]
            if(abs(delta * 2.0 + z_thickness) < z_thickness)
            {
                intersected = 1;
                
                //can only interpolate if ray did not pass behind an object, otherwise interpolation point will be on the object and thus the object will leak light through itself away from the camera
                ray.pos_2D = prevdelta < 0 ? ray.pos_2D : Projection::proj_to_uv(lerp(prevpos, pos, prevdelta / (prevdelta + abs(delta)))); //no need to check for screen boundaries, current and previous step are inside screen, so must be something inbetween
                
                if(RT_HIGHP_LIGHT_SPREAD) 
                    ray.dir_3D = normalize(lerp(pos - ray.origin_3D, ray.dir_3D,saturate(ray.length_3D * 0.25)));

                break;
            }

            prevdelta = delta;
            prevpos = pos;
        }

        if(RT_USE_FALLBACK && intersected < 0.01) //no intersection but we are inside the screen still
        {
            float3 delta = pos - ray.pos_3D;
            delta /= ray_termination_length;
            float falloff = saturate(1.0 - dot(delta, delta) * 0.5);         
            intersected = falloff * 0.25 * all(saturate(-ray.pos_2D * ray.pos_2D + ray.pos_2D));
        }

        rtgi.w += intersected;

        if(intersected < 0.01) 
            continue;

        min16float3 albedo           = tex2Dlod(sCTex, ray.pos_2D, ray.width + MIP_BIAS_IL).rgb; unpack_hdr(albedo);

#if INFINITE_BOUNCES != 0
        float3 nextbounce       = tex2Dlod(sFilterTex0, ray.pos_2D, 0).rgb; unpack_hdr(nextbounce);            
        albedo += nextbounce * RT_IL_BOUNCE_WEIGHT;
#endif

        min16float3 intersect_normal = tex2Dlod(sNTex, ray.pos_2D, 0).xyz * 2.0 - 1.0;
        min16float backface_check = saturate(dot(-intersect_normal, ray.dir_3D) * 64.0);

        backface_check = RT_BACKFACE_MIRROR ? lerp(backface_check, 1.0, 0.1) : backface_check;
        albedo *= backface_check;

        rtgi.rgb += albedo;
    }

    rtgi /= RT_RAY_AMOUNT;
    pack_hdr(rtgi.xyz);

    store_data_revolver(stGITex0, stGITex1, stGITex2, i.dispatchthreadid.xy, rtgi);
}

void CS_Combine(in CSIN i)
{
    min16float4 gi[3], gbuf[3];
    gi[0] = tex2Dfetch(sGITex0, i.dispatchthreadid.xy);
    gi[1] = tex2Dfetch(sGITex1, i.dispatchthreadid.xy);
    gi[2] = tex2Dfetch(sGITex2, i.dispatchthreadid.xy);
    gbuf[0] = tex2Dfetch(sGBufferTex0, i.dispatchthreadid.xy);
    gbuf[1] = tex2Dfetch(sGBufferTex1, i.dispatchthreadid.xy);
    gbuf[2] = tex2Dfetch(sGBufferTex2, i.dispatchthreadid.xy);

    //technically, one of the above is a duplicate, but not deciding which textures to fetch
    //and just letting the weighting do its work is faster.

    min16float4 gbuf_reference = fetch_data_revolver(sGBufferTex0, sGBufferTex1, sGBufferTex2, i.dispatchthreadid.xy); //tex2Dfetch(sGBufferTexC, i.dispatchthreadid.xy); 
    min16float4 combined = 0;
	min16float sumweight = 0.0001;

    for(uint j = 0; j < 3; j++)
    {
        min16float4 delta = abs(gbuf_reference - gbuf[j]);		
        min16float time_delta = FRAMETIME; 
		time_delta = max(time_delta, 1.0) / 16.7; //~1 for 60 fps, expected range
		delta /= time_delta;

        const min16float normal_sensitivity = 2.0;
		const min16float z_sensitivity = 1.0;

        min16float d = dot(delta, min16float4(delta.xyz * normal_sensitivity, z_sensitivity)); //normal squared, depth linear
		min16float w = exp(-d);

        combined += gi[j] * w;
		sumweight += w;
    }
    combined /= sumweight;
    tex2Dstore(stFilterTex0, i.dispatchthreadid.xy, combined);
}

#define FILTER_THREADGROUP_SIZE     16
#define FILTER_KERNEL_RADIUS        2 //need same padding
#define FILTER_SHARED_KERNELSIZE    (FILTER_THREADGROUP_SIZE + 2 * FILTER_KERNEL_RADIUS)

#define FILTER_KERNEL_DIAMETER      (2 * FILTER_KERNEL_RADIUS + 1)

struct FilterSample
{
    min16float4 gbuffer;
    min16float4 val;
};

groupshared FilterSample filter_samples[FILTER_SHARED_KERNELSIZE * FILTER_SHARED_KERNELSIZE];

void atrous_compute(in CSIN i, sampler gisrc, storage gidst, uint iter)
{ 
    uint groupsize = FILTER_THREADGROUP_SIZE;

    //                       //spacing between texels        //shift entire blocks                         //interleave inside sectors
    //uint2 pixel_position = (i.groupthreadid.xy << iter) + (i.groupid.xy >> iter) * (groupsize << iter) + (i.groupid.xy & ((1 << iter) - 1)); 

    int2 group_shift = (i.groupid.xy >> iter) * (groupsize << iter) + (i.groupid.xy & ((1u << iter) - 1u));
    int2 pixel_position = (i.groupthreadid.xy << iter) + group_shift;

    uint2 grid_size = FILTER_SHARED_KERNELSIZE;

    uint threadid = i.threadid;

    [loop]
    while(threadid < FILTER_SHARED_KERNELSIZE * FILTER_SHARED_KERNELSIZE)
    {
        int2 grid_pos, texel_position;
        grid_pos.x = threadid % grid_size.x;
        grid_pos.y = threadid / grid_size.x;
        grid_pos -= FILTER_KERNEL_RADIUS;
        texel_position = (grid_pos << iter) + group_shift;

        FilterSample s;
#if SMOOTHNORMALS != 0
        s.gbuffer = min16float4(fetch_data_revolver(sGBufferTex0, sGBufferTex1, sGBufferTex2, texel_position));
#else //tiny bit faster
        s.gbuffer = min16float4(safe_tex2Dfetch(sNTex, texel_position).xyz * 2 - 1, safe_tex2Dfetch(sZTex, texel_position).x);
#endif
        s.val = min16float4(safe_tex2Dfetch(gisrc, texel_position));

        filter_samples[threadid] = s;
        threadid += FILTER_THREADGROUP_SIZE*FILTER_THREADGROUP_SIZE;
    }

    barrier(); 

    int2 center_grididx = i.groupthreadid.xy + FILTER_KERNEL_RADIUS;
    int2 topleft_grididx = center_grididx - FILTER_KERNEL_RADIUS;
    uint topleft_idx = topleft_grididx.x + topleft_grididx.y * FILTER_SHARED_KERNELSIZE;

    FilterSample center = filter_samples[center_grididx.x + center_grididx.y * FILTER_SHARED_KERNELSIZE];

    min16float4 value_sum = center.val * 0.01; 
    min16float weight_sum = 0.01; 

    min16float4 sigma_z = min16float4(0.7,0.7,0.7,0.7);
    min16float4 sigma_n = min16float4(0.75,1.5,1.5,5);
    min16float4 sigma_v = min16float4(0.035,0.6,1.4,5);

    min16float expectederror = sqrt(RT_RAY_AMOUNT);

    for(int x = 0; x < FILTER_KERNEL_DIAMETER; x++)
    {
        FilterSample t[FILTER_KERNEL_DIAMETER];
        [unroll] for(int j = 0; j < FILTER_KERNEL_DIAMETER; j++) t[j] = filter_samples[topleft_idx + j];

        topleft_idx += FILTER_SHARED_KERNELSIZE;

        for(int y = 0; y < FILTER_KERNEL_DIAMETER; y++)
        {
            FilterSample tap = t[y];

            min16float wz = sigma_z[iter] * 16.0 *  (1.0 - tap.gbuffer.w / center.gbuffer.w);
            wz = saturate(0.5 - lerp(wz, abs(wz), 0.75));

            min16float wn = saturate(dot(tap.gbuffer.xyz, center.gbuffer.xyz) * (sigma_n[iter] + 1) - sigma_n[iter]);
            min16float wi = dot(abs(tap.val - center.val), min16float4(0.3, 0.59, 0.11, 3.0));
    
            wi = exp(-wi * wi * 2.0 * sigma_v[iter] * expectederror);

            wn = lerp(wn, 1, saturate(wz * 1.42 - 0.42));
            min16float w = saturate(wz * wn * wi);
            value_sum += tap.val * w;
            weight_sum += w;
        }
    }
    min16float4 result = value_sum / weight_sum;
    tex2Dstore(gidst, pixel_position, result);
}

void CS_Atrous0(in CSIN i) { atrous_compute(i, sFilterTex0, stFilterTex1, 0); }
void CS_Atrous1(in CSIN i) { atrous_compute(i, sFilterTex1, stFilterTex0, 1); }
void CS_Atrous2(in CSIN i) { atrous_compute(i, sFilterTex0, stFilterTex1, 2); }
void CS_Atrous3(in CSIN i) { atrous_compute(i, sFilterTex1, stFilterTex0, 3); }

void PSMain(in VSOUT i, out float4 o : SV_Target0)
{
    o = tex2D(sFilterTex0, i.uv);
}

void PS_Blending(in VSOUT i, out float4 o : SV_Target0)
{
    float4 gi = tex2D(sFilterTex0, i.uv);
    float3 color = tex2D(ColorInput, i.uv).rgb;

    unpack_hdr(color);
    unpack_hdr(gi.rgb);

    color = RT_DEBUG_VIEW == 1 ? 1 : color; 

    float similarity = distance(normalize(color + 0.00001), normalize(gi.rgb + 0.00001));
	similarity = saturate(similarity * 3.0);
	gi.rgb = lerp(dot(gi.rgb, 0.3333), gi.rgb, saturate(similarity * 0.5 + 0.5));  
   
    float fade = fade_distance(i);  
    gi *= fade; 
 
    color += lerp(color, dot(color, 0.333), saturate(1 - dot(color, 3.0))) * gi.rgb * RT_IL_AMOUNT * RT_IL_AMOUNT; //apply GI
    color = color / (1.0 + gi.w * RT_AO_AMOUNT);    

    pack_hdr(color.rgb);

    //dither a little bit as large scale lighting might exhibit banding
    color += dither(i);

    color = RT_DEBUG_VIEW == 2 ? tex2D(sGBufferTex0, i.uv).xyz * float3(0.5, 0.5, -0.5) + 0.5 : color;
    o = float4(color, 1);
}

/*=============================================================================
	Techniques
=============================================================================*/

technique RTGlobalIlluminationCS
< ui_tooltip = "              >> qUINT::RTGI 0.22 CS <<\n\n"
               "         EARLY ACCESS -- PATREON ONLY\n"
               "Official versions only via patreon.com/mcflypg\n"
               "\nRTGI is written by Marty McFly / Pascal Gilcher\n"
               "Early access, featureset might be subject to change"; >
{
    /*pass
    {
        ComputeShader = CS_MakeInputs<GRP_SIZE_X, GRP_SIZE_Y>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, GRP_SIZE_X);
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, GRP_SIZE_Y);  
    }*/
    pass
	{
		VertexShader = VS_Main;
        PixelShader  = PS_MakeInputs;
        RenderTarget0 = ZTex;
        RenderTarget1 = CTex;
        RenderTarget2 = NTex;
	}
    pass
    {
        ComputeShader = CS_RTMain<8, 8>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, 8);
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, 8);  
    } 
    pass
    {
        ComputeShader = CS_Combine<8, 8>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, 8);
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, 8);  
    } 
    pass
    {
        ComputeShader = CS_Atrous0<FILTER_THREADGROUP_SIZE, FILTER_THREADGROUP_SIZE>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, FILTER_THREADGROUP_SIZE) + 1;
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, FILTER_THREADGROUP_SIZE) + 1;  
    }
    pass
    {
        ComputeShader = CS_Atrous1<FILTER_THREADGROUP_SIZE, FILTER_THREADGROUP_SIZE>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, FILTER_THREADGROUP_SIZE) + 1;
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, FILTER_THREADGROUP_SIZE) + 1;  
    }
    pass
    {
        ComputeShader = CS_Atrous2<FILTER_THREADGROUP_SIZE, FILTER_THREADGROUP_SIZE>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, FILTER_THREADGROUP_SIZE) + 1;
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, FILTER_THREADGROUP_SIZE) + 1;  
    }
    pass
    {
        ComputeShader = CS_Atrous3<FILTER_THREADGROUP_SIZE, FILTER_THREADGROUP_SIZE>;
    	DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, FILTER_THREADGROUP_SIZE) + 1;
    	DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, FILTER_THREADGROUP_SIZE) + 1;  
    }
    pass
	{
		VertexShader = VS_Main;
        PixelShader  = PS_Blending;
	}
}
