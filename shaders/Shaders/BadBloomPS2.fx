#include "ReShade.fxh"

#ifndef BAD_BLOOM_DOWN_SCALE
#define BAD_BLOOM_DOWN_SCALE 8
#endif

/*
uniform float fGamma <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Bloom Gamma";
> = 1.0;
*/

uniform float3 uColor <
	ui_label = "Color";
	ui_type = "color";
> = float3(1.0,1.0,1.0);

uniform float uAmount <
	ui_label = "Amount";
	ui_tooltip = "Default: 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 1.0;

uniform float uThreshold <
	ui_label = "Threshold";
	ui_tooltip = "Default: 2.0";
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 2.0;

uniform float2 uScale <
	ui_label = "Scale";
	ui_tooltip = "Default: 1.0 1.0";
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = float2(1, 1);

texture tBadBloom_Threshold {
	Width = BUFFER_WIDTH / BAD_BLOOM_DOWN_SCALE;
	Height = BUFFER_HEIGHT / BAD_BLOOM_DOWN_SCALE;
};
sampler sThreshold {
	Texture = tBadBloom_Threshold;
};

texture tBadBloom_Blur {
	Width = BUFFER_WIDTH / BAD_BLOOM_DOWN_SCALE;
	Height = BUFFER_HEIGHT / BAD_BLOOM_DOWN_SCALE;
};
sampler sBlur {
	Texture = tBadBloom_Blur;
};

float4 gamma(float4 col, float g)
{
    float i = 1.0 / g;
    return float4(pow(col.x, i)
              , pow(col.y, i)
              , pow(col.z, i)
			  , col.w);
}

float3 jodieReinhardTonemap(float3 c){
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float3 tc = c / (c + 1.0);

    return lerp(c / (l + 1.0), tc, tc);
}

float4 PS_Threshold(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	float4 color = tex2D(ReShade::BackBuffer, uv);
	//if(dot(color.rgb, float3(0.299, 0.587, 0.114)) > uThreshold) color = 0;
	color.rgb = pow(color.rgb, uThreshold);
	return color;
}

float4 PS_Blur(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	float4 color = tex2D(sThreshold,uv);
	float S = 8; //Evil Magic Number
	float2 pix = uScale * ReShade::PixelSize;

	color = tex2D(sThreshold, uv) * 0.204164;
	
	//H
    color += tex2D(sThreshold, uv + float2(pix.x * 8 * 1.407333,0)) * 0.304005;
    color += tex2D(sThreshold, uv - float2(pix.x * 4 * 1.407333,0)) * 0.304005;
    color += tex2D(sThreshold, uv + float2(pix.x * 2 * 3.294215,0)) * 0.093913;
    color += tex2D(sThreshold, uv - float2(pix.x * 1 * 3.294215,0)) * 0.093913;
	
	//V
    color += tex2D(sThreshold,( uv + float2(0,pix.y * 8 * 1.407333))) * 0.304005;
    color += tex2D(sThreshold,( uv - float2(0,pix.y * 4 * 1.407333))) * 0.304005;
    color += tex2D(sThreshold,( uv + float2(0,pix.y * 2 * 3.294215))) * 0.093913;
    color += tex2D(sThreshold,( uv - float2(0,pix.y * 1 * 3.294215))) * 0.093913;
	
	
	color *= 0.25;
	return color;
}

float4 PS_Blend(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
	//ORIGINAL
	float4 color = tex2D(ReShade::BackBuffer, uv);
	float4 blur = tex2D(sBlur, uv);
	color = mad(blur, uAmount * uColor, color);
	
	//TEST 1
	/*
	color = pow(abs(color),2.2);
	color = (color * uColor) + (blur * uAmount);
	color = pow(abs(color),rcp(2.2));
	
	color = gamma(color, fGamma);
	*/
	
	//TEST2
	/*
	float3 color = pow(tex2D(ReShade::BackBuffer, uv).rgba * uAmount, float3(2.2,2.2,2.2));
	color = pow(color, float3(2.2,2.2,2.2));
    color += pow(blur*uAmount, float3(2.2,2.2,2.2));
    color = pow(color, float3(1.0 / 2.2,1.0 / 2.2,1.0 / 2.2));
	color = jodieReinhardTonemap(color);
	return float4(color,1.0);
	*/
    
	return color;
}

technique BadBloomPS2 {
	pass Threshold {
		VertexShader = PostProcessVS;
		PixelShader = PS_Threshold;
		RenderTarget = tBadBloom_Threshold;
	}
	pass BlurPS2 {
		VertexShader = PostProcessVS;
		PixelShader = PS_Blur;
		RenderTarget = tBadBloom_Blur;
	}
	pass BlendPS2 {
		VertexShader = PostProcessVS;
		PixelShader = PS_Blend;
	}
}