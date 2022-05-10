#include "Reshade.fxh"

namespace DHUniformityCorrection {

//// uniform

	uniform bool bSolidColor <
	    ui_category = "Debug";
		ui_label = "Solid color";
	> = false;
	
	uniform float fSolidColor <
	    ui_category = "Debug";
		ui_label = "Brightness";
		ui_type = "slider";
	    ui_min = 0.0;
	    ui_max = 1.0;
	    ui_step = 0.1;
	> = 0.5;
	
	uniform float fCorrection <
	    ui_category = "Correction";
		ui_label = "Correction strength";
		ui_type = "slider";
	    ui_min = 0.0;
	    ui_max = 1.0;
	    ui_step = 0.05;
	> = true;

//// textures

	texture uniformityDefectTex < source = "dh_uniformity_correction.bmp"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; };
	sampler uniformityDefectSampler { Texture = uniformityDefectTex; };

//// Functions


//// PS

	void PS_Correction(float4 vpos : SV_Position, in float2 coords : TEXCOORD0, out float4 outPixel : SV_Target)
	{
		float4 color;
		if(bSolidColor) {
			color = fSolidColor;
			color.a = 1.0;
		} else {
			color = tex2D(ReShade::BackBuffer,coords);
		}
		
		float4 defect = tex2D(uniformityDefectSampler,coords);
	
		outPixel = clamp(color*(1.0+fCorrection*(1.0-defect)),0,1);
	}

//// Techniques

	technique DHUniformityCorrection <
	>
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_Correction;
		}
	}

}