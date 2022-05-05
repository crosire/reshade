/*=============================================================================

	ReShade 4 effect file
    github.com/martymcmodding

	Support me:
   		paypal.me/mcflypg
   		patreon.com/mcflypg

    Path Traced Global Illumination 

    by Marty McFly / P.Gilcher
    part of qUINT shader library for ReShade 4

    CC BY-NC-ND 3.0 licensed.

    PATREON ONLY FILE - DO NOT SHARE

=============================================================================*/

/*=============================================================================
	Preprocessor settings
=============================================================================*/

#ifndef INFINITE_BOUNCES
 #define INFINITE_BOUNCES       0   //[0 or 1]      If enabled, path tracer samples previous frame GI as well, causing a feedback loop to simulate secondary bounces, causing a more widespread GI.
#endif

#ifndef SKYCOLOR_MODE
 #define SKYCOLOR_MODE          0   //[0 to 2]      0: skycolor feature disabled | 1: manual skycolor | 2: dynamic skycolor
#endif

/*=============================================================================
	UI Uniforms
=============================================================================*/

uniform float RT_SIZE_SCALE <
	ui_type = "drag";
	ui_min = 0.5; ui_max = 1.0;
    ui_step = 0.5;
    ui_label = "GI Render Resolution Scale";
    ui_category = "Global";
> = 1.0;

uniform float RT_SAMPLE_RADIUS <
	ui_type = "drag";
	ui_min = 0.5; ui_max = 20.0;
    ui_step = 0.01;
    ui_label = "Ray Length";
	ui_tooltip = "Maximum ray length, directly affects\nthe spread radius of shadows / indirect lighing";
    ui_category = "Path Tracing";
> = 15.0;

uniform int RT_RAY_AMOUNT <
	ui_type = "slider";
	ui_min = 1; ui_max = 20;
    ui_label = "Ray Amount";
    ui_category = "Path Tracing";
> = 10;

uniform int RT_RAY_STEPS <
	ui_type = "slider";
	ui_min = 1; ui_max = 20;
    ui_label = "Ray Step Amount";
    ui_category = "Path Tracing";
> = 10;

uniform float RT_Z_THICKNESS <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 10.0;
    ui_step = 0.01;
    ui_label = "Z Thickness";
	ui_tooltip = "The shader can't know how thick objects are, since it only\nsees the side the camera faces and has to assume a fixed value.\n\nUse this parameter to remove halos around thin objects.";
    ui_category = "Path Tracing";
> = 1.0;

#if SKYCOLOR_MODE != 0

#if SKYCOLOR_MODE == 1
uniform float3 SKY_COLOR <
	ui_type = "color";
	ui_label = "Sky Color";
    ui_category = "Blending";
> = float3(1.0, 0.0, 0.0);
#endif

#if SKYCOLOR_MODE == 2
uniform float SKY_COLOR_SAT <
	ui_type = "drag";
	ui_min = 0; ui_max = 5.0;
    ui_step = 0.01;
    ui_label = "Auto Sky Color Saturation";
    ui_category = "Blending";
> = 1.0;
#endif

uniform float SKY_COLOR_AMBIENT_MIX <
	ui_type = "drag";
	ui_min = 0; ui_max = 1.0;
    ui_step = 0.01;
    ui_label = "Sky Color Ambient Mix";
    ui_tooltip = "How much of the occluded ambient color is considered skycolor\n\nIf 0, Ambient Occlusion removes white ambient color,\nif 1, Ambient Occlusion only removes skycolor";
    ui_category = "Blending";
> = 1.0;

uniform float SKY_COLOR_AMT <
	ui_type = "drag";
	ui_min = 0; ui_max = 10.0;
    ui_step = 0.01;
    ui_label = "Sky Color Intensity";
    ui_category = "Blending";
> = 4.0;
#endif

uniform float RT_AO_AMOUNT <
	ui_type = "drag";
	ui_min = 0; ui_max = 10.0;
    ui_step = 0.01;
    ui_label = "Ambient Occlusion Intensity";
    ui_category = "Blending";
> = 1.0;

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
        ui_min = 0; ui_max = 5.0;
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
	ui_items = "None\0Lighting Channel\0";
	ui_tooltip = "Different debug outputs";
    ui_category = "Debug";
> = 0;

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
	Textures, Samplers, Globals
=============================================================================*/

#include "qUINT_common.fxh"

uniform int framecount < source = "framecount"; >;
uniform float frametime < source = "frametime"; >;

#define MIP_BIAS_IL 2

texture ZTex 	            { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = R16F;  MipLevels = 3;};
texture NormalTex 	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA8; MipLevels = 3 + MIP_BIAS_IL;};
texture GITex	            { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GITexPrev	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GITexTemp	        { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture GBufferTexPrev      { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = RGBA16F; };
texture HistoryConfidence   { Width = BUFFER_WIDTH;   Height = BUFFER_HEIGHT;   Format = R8; };

texture SkyCol          { Width = 1;   Height = 1;   Format = RGBA8; };
texture SkyColPrev      { Width = 1;   Height = 1;   Format = RGBA8; };
sampler2D sSkyCol	    { Texture = SkyCol;	};
sampler2D sSkyColPrev	{ Texture = SkyColPrev;	};

texture JitterTex           < source = "bluenoise.png"; > { Width = 32; Height = 32; Format = RGBA8; };

sampler2D sZTex	            { Texture = ZTex;	    };
sampler2D sNormalTex	    { Texture = NormalTex;	};
sampler2D sGITex	        { Texture = GITex;	        };
sampler2D sGITexPrev	    { Texture = GITexPrev;	    };
sampler2D sGITexTemp	    { Texture = GITexTemp;	    };
sampler2D sGBufferTexPrev	{ Texture = GBufferTexPrev;	};
sampler2D sHistoryConfidence	{ Texture = HistoryConfidence;	};

sampler	sJitterTex          { Texture = JitterTex; AddressU = WRAP; AddressV = WRAP;};

/*=============================================================================
	Vertex Shader
=============================================================================*/

struct VSOUT
{
	float4                  vpos        : SV_Position;
    float2                  uv          : TEXCOORD0;
    float4                  uv_scaled   : TEXCOORD1;
};

VSOUT VS_RT(in uint id : SV_VertexID)
{
    VSOUT o;

    o.uv.x = (id == 2) ? 2.0 : 0.0;
    o.uv.y = (id == 1) ? 2.0 : 0.0;

    o.uv_scaled.xy = o.uv / RT_SIZE_SCALE;
    o.uv_scaled.zw = o.uv * RT_SIZE_SCALE;

    o.vpos = float4(o.uv.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

/*=============================================================================
	Functions
=============================================================================*/

struct Ray 
{
    float3 pos;
    float3 dir;
    float2 uv;
    float len;
};

struct MRT
{
    float4 gi   : SV_Target0;
    float4 gbuf : SV_Target1;
};

struct RTConstants
{
    float3 pos;
    float3 normal;
    float3x3 mtbn;
    int nrays;
    int nsteps;
};

#include "RTGI/Projection.fxh"
#include "RTGI/Normal.fxh"
#include "RTGI/RaySorting.fxh"


void unpack_hdr(inout float3 color)
{
    color /= 1.01 - color; //min(min(color.r, color.g), color.b);//max(max(color.r, color.g), color.b);
}

void pack_hdr(inout float3 color)
{
    color /= 1.01 + color; //min(min(color.r, color.g), color.b);//max(max(color.r, color.g), color.b);
}

float compute_history_confidence(MRT curr, MRT prev, inout float historyconfidence)
{
    float4 gbuf_delta = abs(curr.gbuf - prev.gbuf);

    historyconfidence = dot(gbuf_delta.xyz, gbuf_delta.xyz) * 10 + gbuf_delta.w;
    historyconfidence = exp(-historyconfidence);

    return lerp(0.25, 0.9, saturate(1 - historyconfidence));
}

float4 fetch_gbuffer(in float2 uv)
{
    return float4(tex2Dlod(sNormalTex, float4(uv, 0, 0)).xyz * 2 - 1, 
                  tex2Dlod(sZTex, float4(uv, 0, 0)).x/* / RESHADE_DEPTH_LINEARIZATION_FAR_PLANE*/);

}

float4 atrous(int iter, sampler gi, VSOUT i)
{
    float4 center_gbuf = fetch_gbuffer(i.uv);
    float4 center_gi = tex2D(gi, i.uv);

    float historyconfidence = tex2D(sHistoryConfidence, i.uv_scaled.zw).x;

    float4 weighted_value_sum = 0; //center_gi * 0.00001;
    float weight_sum = 0.00001;

    float size[4] = {1.5, 3, 6, 12};
    float error_thresh_val[4] = {1.3, 8, 13, 17};//0.06 0.68 0.5

    for(int x = -1; x <= 1; x++)
    for(int y = -1; y <= 1; y++)
    {
        float2 grid_pos = float2(x, y) * size[iter] * qUINT::PIXEL_SIZE;

        float2 tap_uv       = i.uv + grid_pos;  
        float4 tap_gi       = tex2Dlod(gi, float4(tap_uv, 0, 0));
        float4 tap_gbuf     = fetch_gbuffer(tap_uv);

        float wz = exp(-abs(tap_gbuf.w - center_gbuf.w) * 0.25);
        float wn = pow(saturate(dot(tap_gbuf.xyz, center_gbuf.xyz)), 64);
        float wi = exp(-abs(dot(tap_gi, float4(0.9, 1.77, 0.33, 3.0)) - dot(center_gi, float4(0.9, 1.77, 0.33, 3.0))) * error_thresh_val[iter]);

        wn = saturate(wn + 0.2 * wz * wz);
        wi = lerp(wi, saturate(wi*5), pow(wz*wz*wn*wn, 10));
        wi = lerp(wi, 1, saturate(1 - historyconfidence  * 1.2));

        float w = wz * wn * wi;

        weighted_value_sum += tap_gi * w;
        weight_sum += w;
    }

    //return center_gi;
    return weighted_value_sum / weight_sum;
}

/*=============================================================================
	Pixel Shaders
=============================================================================*/

void PS_Deferred(in VSOUT i, out float4 normal : SV_Target0, out float depth : SV_Target1)
{	
    normal  = float4(normal_from_depth(i) * 0.5 + 0.5, 1); 
    depth   = depth_to_z(qUINT::linear_depth(i.uv));
}

void PS_RTMain(in VSOUT i, out float4 o : SV_Target0, out float historyconfidence : SV_Target1)
{
    RTConstants rtconstants;
    rtconstants.pos     = uv_to_proj(i.uv_scaled.xy);
    rtconstants.normal  = tex2D(sNormalTex, i.uv_scaled.xy).xyz * 2 - 1;
    rtconstants.mtbn    = calculate_tbn_matrix(rtconstants.normal);
    rtconstants.nrays   = RT_RAY_AMOUNT;
    rtconstants.nsteps  = RT_RAY_STEPS;  

    float depth = z_to_depth(rtconstants.pos.z); 
    rtconstants.pos = rtconstants.pos * 0.998 + rtconstants.normal * depth;

    MRT curr, prev;     

    float2x2 raygen_matrix;    
    float2 raygen_init;    
    float sampleset_start_norm;
    float2 bluenoise = tex2Dfetch(sJitterTex, int4(i.vpos.xy % tex2Dsize(sJitterTex), 0, 0)).xy;
    ray_sorting(i, framecount, bluenoise.x, raygen_matrix, raygen_init, sampleset_start_norm);

    curr.gi = 0;
    float invthickness = rcp(RT_SAMPLE_RADIUS * RT_SAMPLE_RADIUS * RT_Z_THICKNESS);    

    [loop]
    for(int r = 0; r < 0 + rtconstants.nrays; r++)
    {
        Ray ray; 
        ray.dir.z = (r + sampleset_start_norm) / rtconstants.nrays;
        ray.dir.xy = 1 - ray.dir.z;
        ray.dir = sqrt(ray.dir);
        ray.dir.xy *= raygen_init;
        ray.dir = mul(ray.dir, rtconstants.mtbn);
        ray.len = RT_SAMPLE_RADIUS * RT_SAMPLE_RADIUS;

        raygen_init = mul(raygen_init, raygen_matrix); 

        float intersected = 0, mip = 0; int s = 0; bool inside_screen = 1;

        //HLSL SM3 compiler bug workaround
        //The compiler generates an unused and empty int register
        //and uses that one to iterate the loop instead of using the
        //UI uniform, so the loop runs precisely 0 times as the register 
        //is empty. This workaround forces the compiler to generate
        //a loop that runs 255 times by default and conditionally breaks.
        while(s++ < 0 + rtconstants.nsteps && inside_screen)
        {
            float lambda = float(s - bluenoise.y) / rtconstants.nsteps; //normalized position in ray [0, 1]  
            ray.pos = rtconstants.pos + ray.dir * lambda * ray.len;

            ray.uv = proj_to_uv(ray.pos);
            inside_screen = all(saturate(-ray.uv * ray.uv + ray.uv));

            //another SM3 "bug"fix. Under DX9, the entire mip chain is generated
            //even though only 3 are specified, making this code access too high indices
            //by default. Manually clamping makes this work uniform across API versions
            mip = min(length((ray.uv - i.uv_scaled.xy) * qUINT::ASPECT_RATIO.yx) * 28, 3);
            float3 delta = uv_to_proj(ray.uv, sZTex, mip) - ray.pos;
            
            delta *= invthickness;

            [branch]
            if(delta.z < 0 && delta.z > -1)
            {                
                intersected = inside_screen * exp2(-3 * lambda * lambda); 
                s = 100;
            }       
        }

        curr.gi.w += intersected;    

        [branch]
        if(RT_IL_AMOUNT > 0 && intersected > 0.05)
        {
            float3 albedo 			= tex2Dlod(qUINT::sBackBufferTex, 	float4(ray.uv, 0, mip + MIP_BIAS_IL)).rgb; unpack_hdr(albedo);
            float3 intersect_normal = tex2Dlod(sNormalTex,              float4(ray.uv, 0, mip + MIP_BIAS_IL)).xyz * 2.0 - 1.0;

#if INFINITE_BOUNCES != 0
            float3 nextbounce 		= tex2Dlod(sGITexPrev, float4(ray.uv, 0, 0)).rgb; unpack_hdr(nextbounce);            
            albedo += nextbounce * RT_IL_BOUNCE_WEIGHT;
#endif
            curr.gi.rgb += albedo * intersected * saturate(dot(-intersect_normal, ray.dir));
        }
    }

    curr.gi /= rtconstants.nrays; 
    pack_hdr(curr.gi.rgb);

    curr.gbuf = float4(rtconstants.normal, rtconstants.pos.z);  
    prev.gi = tex2D(sGITexTemp, i.uv_scaled.xy);
    prev.gbuf = tex2D(sGBufferTexPrev, i.uv_scaled.xy);

    float alpha = compute_history_confidence(curr, prev, historyconfidence);
    o = lerp(prev.gi, curr.gi, alpha);
}

void PS_Copy(in VSOUT i, out MRT o)
{	
    o.gi    = tex2D(sGITex, i.uv_scaled.zw);
    o.gbuf  = fetch_gbuffer(i.uv);

    if(qUINT::linear_depth(i.uv.xy) >= max(RT_FADE_DEPTH.x, RT_FADE_DEPTH.y) //theoretically only .y but users might swap it...
    ) discard;
}

void PS_Filter0(in VSOUT i, out float4 o : SV_Target0)
{
    o = atrous(0, sGITexPrev, i);
}
void PS_Filter1(in VSOUT i, out float4 o : SV_Target0)
{
    o = atrous(1, sGITexTemp, i);
}
void PS_Filter2(in VSOUT i, out float4 o : SV_Target0)
{
    o = atrous(2, sGITex, i);
}
void PS_Filter3(in VSOUT i, out float4 o : SV_Target0)
{
    o = atrous(3, sGITexTemp, i);
}

void PS_Output(in VSOUT i, out float4 o : SV_Target0)
{
    float4 gi = tex2D(sGITex, i.uv);//sGITexTemp
    float3 color = tex2D(qUINT::sBackBufferTex, i.uv).rgb;

    unpack_hdr(color);
    unpack_hdr(gi.rgb);    

    if(RT_DEBUG_VIEW == 1) color.rgb = 1;

#if SKYCOLOR_MODE != 0
 #if SKYCOLOR_MODE == 1
    float3 skycol = SKY_COLOR;
 #else
    float3 skycol = tex2Dfetch(sSkyCol, 0).rgb;
    skycol = lerp(dot(skycol, 0.333), skycol, SKY_COLOR_SAT * 0.2);
 #endif

    float fade = smoothstep(RT_FADE_DEPTH.y, RT_FADE_DEPTH.x, qUINT::linear_depth(i.uv));   
    gi *= fade;  
    skycol *= fade;  

    color = color * (1.0 + gi.rgb * RT_IL_AMOUNT * RT_IL_AMOUNT); //apply GI
    color = color / (1.0 + lerp(1.0, skycol, SKY_COLOR_AMBIENT_MIX) * gi.w * RT_AO_AMOUNT); //apply AO as occlusion of skycolor
    color = color * (1.0 + skycol * SKY_COLOR_AMT);
#else
    color = color * (1.0 + gi.rgb * RT_IL_AMOUNT * RT_IL_AMOUNT); //apply GI
    color = color / (1.0 + gi.w * RT_AO_AMOUNT);
#endif

    pack_hdr(color.rgb);
    o = float4(color, 1);
}

void PS_ReadSkycol(in VSOUT i, out float4 o : SV_Target0)
{
    float2 gridpos;
    gridpos.x = framecount % 64;
    gridpos.y = floor(framecount / 64) % 64;

    float2 unormgridpos = gridpos / 64.0;

    int searchsize = 10;

    float4 skycolor = 0.0;

    for(float x = 0; x < searchsize; x++)
    for(float y = 0; y < searchsize; y++)
    {
        float2 loc = (float2(x, y) + unormgridpos) * rcp(searchsize);

        float z = qUINT::linear_depth(loc);
        float issky = z == 1;

        skycolor += float4(tex2Dlod(qUINT::sBackBufferTex, float4(loc, 0, 0)).rgb, 1) * issky;
    }

    skycolor.rgb /= skycolor.w + 0.000001;

    float4 prevskycolor = tex2D(sSkyColPrev, 1);

    bool skydetectedthisframe = skycolor.w > 0.000001;
    bool skydetectedatall = prevskycolor.w; //0 if skycolor has not been read yet at all

    float interp = 0;

    //no skycol yet stored, now we have skycolor, use it
    if(!skydetectedatall && skydetectedthisframe)
        interp = 1;

    if(skydetectedatall && skydetectedthisframe)
        interp = saturate(0.1 * 0.01 * frametime);

    o.rgb = lerp(prevskycolor.rgb, skycolor.rgb, interp);
    o.w = skydetectedthisframe || skydetectedatall;
}

void PS_CopyPrevSkycol(in VSOUT i, out float4 o : SV_Target0)
{
    o = tex2D(sSkyCol, 1.0);
}

void PS_StencilSetup(in VSOUT i, out float4 o : SV_Target0)
{   
    o = tex2D(qUINT::sBackBufferTex, i.uv);

    if(qUINT::linear_depth(i.uv_scaled.xy) >= max(RT_FADE_DEPTH.x, RT_FADE_DEPTH.y) //theoretically only .y but users might swap it...
    || max(i.uv_scaled.x, i.uv_scaled.y) > 1
    ) discard;    
}

/*=============================================================================
	Techniques
=============================================================================*/

#define CREATE_STENCIL StencilEnable = true; \
                       StencilPass = REPLACE; \
                       StencilRef = 1;


#define USE_STENCIL     ClearRenderTargets = true; \
                        StencilEnable = true; \
                        StencilPass = KEEP; \
                        StencilFunc = EQUAL; \
                        StencilRef = 1;

technique RTGlobalIllumination
{
#if SKYCOLOR_MODE == 2
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_ReadSkycol;
        RenderTarget = SkyCol;
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_CopyPrevSkycol;
        RenderTarget = SkyColPrev;
	}
#endif
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Deferred;
        RenderTarget0 = NormalTex;
        RenderTarget1 = ZTex;
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_StencilSetup;
        ClearRenderTargets = false;
		CREATE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_RTMain;
        RenderTarget0 = GITex;
        RenderTarget1 = HistoryConfidence;
        USE_STENCIL
	}  
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Copy;
        RenderTarget0 = GITexPrev;
        RenderTarget1 = GBufferTexPrev;
        ClearRenderTargets = true;
		CREATE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Filter0;
        RenderTarget = GITexTemp;
        USE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Filter1;
        RenderTarget = GITex;
        USE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Filter2;
        RenderTarget = GITexTemp;
        USE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Filter3;
        RenderTarget = GITex;
        USE_STENCIL
	}
    pass
	{
		VertexShader = VS_RT;
		PixelShader  = PS_Output;
	}
}