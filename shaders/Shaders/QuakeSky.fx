#include "ReShade.fxh"

#ifndef QUAKESKY_TEX_RES
#define QUAKESKY_TEX_RES 128
#endif

#ifndef QUAKESKY_ADDRESS
#define QUAKESKY_ADDRESS REPEAT
#endif

#ifndef QUAKESKY_FILTER
#define QUAKESKY_FILTER POINT
#endif

static const float sky_ps = 1.0 / QUAKESKY_TEX_RES;

uniform float fScale <
    ui_label = "Scale";
    ui_type = "drag";
    ui_min = 1;
    ui_max = 10;
    ui_step = 0.001;
> = 1.0;

uniform float fDistortion <
    ui_label = "Distortion";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 10.0;
    ui_step = 0.001;
> = 1.0;

uniform float2 fSpeed_Background <
    ui_label = "Background Speed";
    ui_type = "drag";
    ui_min = -10;
    ui_max = 10;
    ui_step = 0.001;
> = 1.0;

uniform float2 fSpeed_Foreground <
    ui_label = "Foreground Speed";
    ui_type = "drag";
    ui_min = -10;
    ui_max = 10;
    ui_step = 0.001;
> = 2.0;

uniform float fInfinity <
    ui_label = "Sky Distance";
    ui_tooltip = "Distance where to render the sky.\n" 
                 "Anything after this will be replaced by the sky.";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.001;
> = 1.0;

uniform int iSkyTexture <
	ui_label = "Sky Texture";
	ui_tooltip = "Choose between the included Quake sky textures.";
	ui_type = "drag";
	ui_min = 0;
	ui_max = 2;
	ui_step = 1;
> = 0;

uniform float time <source="timer";>;

texture tQuakeSky_Background <source="sky1_0.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
texture tQuakeSky_Foreground <source="sky1_1.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
texture tQuakeSky_Background1 <source="sky2_0.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
texture tQuakeSky_Foreground1 <source="sky2_1.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
texture tQuakeSky_Background2 <source="sky3_0.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
texture tQuakeSky_Foreground2 <source="sky3_1.png";> { Width = QUAKESKY_TEX_RES; Height = QUAKESKY_TEX_RES; };
sampler sQuakeSky_Background { 
    Texture = tQuakeSky_Background; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};
sampler sQuakeSky_Foreground { 
    Texture = tQuakeSky_Foreground; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};
sampler sQuakeSky_Background1 { 
    Texture = tQuakeSky_Background1; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};
sampler sQuakeSky_Foreground1 { 
    Texture = tQuakeSky_Foreground1; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};
sampler sQuakeSky_Background2 { 
    Texture = tQuakeSky_Background2; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};
sampler sQuakeSky_Foreground2 { 
    Texture = tQuakeSky_Foreground2; 
    AddressU = QUAKESKY_ADDRESS; AddressV = QUAKESKY_ADDRESS; 
    MagFilter = QUAKESKY_FILTER; MinFilter = QUAKESKY_FILTER;
};

//#define scaleUV(uv, scale) ((uv - 0.5) * scale + 0.5)

float2 distortUV(float2 uv) {
    uv -= 0.5;
    float r2 = uv.x * uv.x + uv.y * uv.y;
    float f = 1 + r2 * -fDistortion;
    uv = f * uv;
    uv += 0.5;
    return uv;
}

float4 PS_QuakeSky(
    float4 pos  : SV_POSITION,
    float2 uv   : TEXCOORD
) : SV_TARGET {
    float3 col = tex2D(ReShade::BackBuffer, uv).rgb;

    float4 uv_sky = (distortUV(uv) * ReShade::ScreenSize * sky_ps).xyxy;
    uv_sky -= (time * 0.001) * float4(fSpeed_Background, fSpeed_Foreground);
    uv_sky *= 1.0 / fScale;
    //uv_sky = scaleUV(uv_sky, 1.0 / fScale);

	
    float3 sky_bg = tex2D(sQuakeSky_Background, uv_sky.xy).rgb;
    float4 sky_fg = tex2D(sQuakeSky_Foreground, uv_sky.zw).rgba; //include alpha because we'll use it for blending
	
	if(iSkyTexture == 1){
		sky_bg = tex2D(sQuakeSky_Background1, uv_sky.xy).rgb;
		sky_fg = tex2D(sQuakeSky_Foreground1, uv_sky.zw).rgba; //include alpha because we'll use it for blending
	} else if (iSkyTexture == 2){
	    sky_bg = tex2D(sQuakeSky_Background2, uv_sky.xy).rgb;
		sky_fg = tex2D(sQuakeSky_Foreground2, uv_sky.zw).rgba; //include alpha because we'll use it for blending
	}

    sky_bg = lerp(sky_bg, sky_fg.rgb, sky_fg.a);

    float depth = ReShade::GetLinearizedDepth(uv);
    col = depth >= fInfinity ? sky_bg : col;

    return float4(col, 1.0);
}

technique QuakeSky {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_QuakeSky;
    }
}
