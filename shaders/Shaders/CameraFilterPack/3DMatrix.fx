#include "ReShade.fxh"

uniform bool Debug <
	ui_type = "boolean";
	ui_tooltip = "Debug Mode [Matrix]";
> = true;

uniform float fixDistance <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.001;
	ui_label = "Distance [Matrix]";
	ui_tooltip = "Changes the distance the Matrix gets rendered";
> = 1.0;

uniform float _LightIntensity <
	ui_type = "drag";
	ui_min = -5.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Light Intensity [Matrix]";
	ui_tooltip = "The intensity of the Matrix lights";
> = 1.0;

uniform float _MatrixSize <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 6.0;
	ui_step = 0.001;
	ui_label = "Matrix Size [Matrix]";
	ui_tooltip = "The size of the Matrix texture";
> = 1.0;

uniform float _MatrixSpeed <
	ui_type = "drag";
	ui_min = -4.0;
	ui_max = 4.0;
	ui_step = 0.001;
	ui_label = "Speed [Matrix]";
	ui_tooltip = "The speed of the Matrix";
> = 1.0;

uniform float Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Fade [Matrix]";
	ui_tooltip = "The distance matrix fades.";
> = 1.0;

uniform float3 _MatrixColor <
	ui_type = "color";
	ui_label = "Matrix Color [Matrix]";
> = float3(0.0,1.0,0.0);

//SHADER START

texture tMatrixFX <source="CameraFilterPack/CameraFilterPack_3D_Matrix1.jpg";> { Width=1024; Height=1024; Format = RGBA8;};
sampler2D sMatrixFX { Texture=tMatrixFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _Timer < source = "timer"; >;

float GetLinearDepth(float2 coords)
{
	return ReShade::GetLinearizedDepth(coords);
}

float4 PS_Matrix(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
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
	uv*=_MatrixSize;
	float3 mx = tex2D(sMatrixFX,uv).r;
	mx -=1-_MatrixColor;

	uv.y+=t*0.5;
	uv *=0.8*_MatrixSize; 
	mx += tex2D(sMatrixFX,uv).r;
	uv.y+=t*0.5;
	uv *=0.8*_MatrixSize; 
	mx += tex2D(sMatrixFX,uv).g;
	uv.y+=t*0.5;
	uv *=0.8*_MatrixSize;
	mx += tex2D(sMatrixFX,uv).b;

	float md=mx*0.01*Fade;
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

technique Matrix {
	pass Matrix {
		VertexShader=PostProcessVS;
		PixelShader=PS_Matrix;
	}
}