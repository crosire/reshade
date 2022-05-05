#include "ReShade.fxh"

uniform bool Debug <
	ui_type = "boolean";
	ui_tooltip = "Debug Mode [Computer]";
> = true;

uniform float fixDistance <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.001;
	ui_label = "Distance [Computer]";
	ui_tooltip = "Changes the distance the grid gets rendered";
> = 1.0;

uniform float _LightIntensity <
	ui_type = "drag";
	ui_min = -5.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Light Intensity [Computer]";
	ui_tooltip = "The intensity of the Grid lights";
> = 1.0;

uniform float _MatrixSize <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 6.0;
	ui_step = 0.001;
	ui_label = "Grid Size [Computer]";
	ui_tooltip = "The size of the Grid texture";
> = 1.0;

uniform float _MatrixSpeed <
	ui_type = "drag";
	ui_min = -4.0;
	ui_max = 4.0;
	ui_step = 0.001;
	ui_label = "Speed [Computer]";
	ui_tooltip = "The speed of the Grid";
> = 1.0;

uniform float Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Fade [Computer]";
	ui_tooltip = "The distance grid fades.";
> = 1.0;

uniform float3 _MatrixColor <
	ui_type = "color";
	ui_label = "Grid Color [Computer]";
> = float3(0.0,0.5,1.0);

//SHADER START

texture tComputerFX <source="CameraFilterPack/CameraFilterPack_3D_Computer1.jpg";> { Width=256; Height=256; Format = RGBA8;};
sampler2D sComputerFX { Texture=tComputerFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _Timer < source = "timer"; >;

float GetLinearDepth(float2 coords)
{
	return ReShade::GetLinearizedDepth(coords);
}

float4 PS_3DComputer(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 uv = texcoord.xy;
	
	float depth = GetLinearDepth(uv.xy) * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
	depth/= fixDistance*10;
	depth = saturate(depth);
	depth = lerp(0.5,depth,Fade);
	
	float t=(_Timer*0.001)*_MatrixSpeed;
	
	float2 uv2=uv;
	uv/=depth+0.2;
	uv.y+=t;
	uv*=float2(1,0.5)+_MatrixSize;
	float3 mx = tex2D(sComputerFX,uv).r;
	mx -=1-_MatrixColor;



	float md=mx*0.02*Fade;
	float3 txt = tex2D(ReShade::BackBuffer,uv2+float2(md,md));
	mx+=txt+depth*0.25*_LightIntensity;

	mx=lerp(txt,mx,Fade);

	
	if (Debug) {
		return depth;
	} else {
		return float4(mx,1.0);
	}

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Computer {
	pass Computer {
		VertexShader=PostProcessVS;
		PixelShader=PS_3DComputer;
	}
}