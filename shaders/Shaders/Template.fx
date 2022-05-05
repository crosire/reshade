#include "ReShade.fxh"


/******************************************************************************
	Uniforms
******************************************************************************/
/*
////////////////////////// Effect //////////////////////////
uniform float fUIStrength <
	ui_type = "drag";
	ui_category = "Effect";
	ui_label = "Strength";
	ui_min = 0.0; ui_max = 1.0;
> = 1.0;
*/
/******************************************************************************
	Textures
******************************************************************************/
/*
texture2D texTemplate { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler2D SamplerTemplate { Texture = texTemplate; };
*/
/******************************************************************************
	Pixel Shader
******************************************************************************/
/*
void Texture_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord,out float3 color : SV_Target0) {
    color = tex2D(ReShade::BackBuffer, texcoord).rgb;
}

float3 Template_PS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    
	float3 color = tex2D(SamplerTemplate, texcoord).rgb;
    float3 luma = dot(color, float3(0.2126, 0.7151, 0.0721));

    return lerp(color, luma, fUIStrength);
}

technique Template
{
	pass {
		VertexShader = PostProcessVS;
		PixelShader = Texture_PS;
        RenderTarget0 = texTemplate;
	}
	pass {
		VertexShader = PostProcessVS;
		PixelShader = Template_PS;
	}
}
*/