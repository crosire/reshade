#include "Reshade.fxh"

namespace DH {

// Uniforms

uniform float fTargetFramerate <
	ui_label = "Target framerate";
	ui_type = "slider";
    ui_min = 1.0;
    ui_max = 240.0;
    ui_step = 0.1;
> = 60;

uniform float frametime < source = "frametime"; >;

// Textures
texture addTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
sampler addSampler { Texture = addTex; };

// Functions

float getTargetFrametimeMs() {
	return 1000.0/fTargetFramerate;
}

// Pixel shaders

void PS_add(in float4 position : SV_Position, in float2 coords : TEXCOORD, out float4 outPixel : SV_Target)
{
	float ftRatio = saturate(frametime/getTargetFrametimeMs());
	float4 color = tex2D(ReShade::BackBuffer,coords);

	color.a = ftRatio;
	
	outPixel = color;
}

void PS_display(in float4 position : SV_Position, in float2 coords : TEXCOORD, out float4 outPixel : SV_Target)
{
	float4 targetColor = tex2D(addSampler,coords);
	
	outPixel = targetColor;
}
	

	
// Techniques

	technique DH_Sync <
	>
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_add;
			RenderTarget = addTex;
			ClearRenderTargets = false;
			
			BlendEnable = true;
			BlendOp = ADD;
			BlendOpAlpha = ADD;

			// The data source and optional pre-blend operation used for blending.
			// Available values:
			//   ZERO, ONE,
			//   SRCCOLOR, SRCALPHA, INVSRCCOLOR, INVSRCALPHA
			//   DESTCOLOR, DESTALPHA, INVDESTCOLOR, INVDESTALPHA
			SrcBlend = SRCALPHA;
			SrcBlendAlpha = ONE;
			DestBlend = INVSRCALPHA;
			DestBlendAlpha = ONE;
			
		}
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_display;
		}
	}

}