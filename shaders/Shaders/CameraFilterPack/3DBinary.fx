#include "ReShade.fxh"

uniform bool Debug <
	ui_type = "boolean";
	ui_tooltip = "Debug Mode [Binary]";
> = false;

uniform float fixDistance <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 0.001;
	ui_label = "Distance [Binary]";
	ui_tooltip = "Changes the distance the Binary gets rendered";
> = 1.0;

uniform float _LightIntensity <
	ui_type = "drag";
	ui_min = -5.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Light Intensity [Binary]";
	ui_tooltip = "The intensity of the Binary lights";
> = 1.0;

uniform float _MatrixSize <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 6.0;
	ui_step = 0.001;
	ui_label = "Binary Size [Binary]";
	ui_tooltip = "The size of the Binary texture";
> = 1.0;

uniform float _MatrixSpeed <
	ui_type = "drag";
	ui_min = -4.0;
	ui_max = 4.0;
	ui_step = 0.001;
	ui_label = "Speed [Binary]";
	ui_tooltip = "The speed of the Binary";
> = 1.0;

uniform float _Fade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Fade [Binary]";
	ui_tooltip = "The intensity of the Binary.";
> = 1.0;

uniform float _FadeFromBinary <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Fade Binary [Binary]";
	ui_tooltip = "The distance the Binary fades.";
> = 1.0;

uniform float3 _MatrixColor <
	ui_type = "color";
	ui_label = "Binary Color [Binary]";
> = float3(1.0,0.3,0.3);

//SHADER START

texture tBinaryFX <source="CameraFilterPack/CameraFilterPack_3D_Binary1.jpg";> { Width=512; Height=512; Format = RGBA8;};
sampler2D sBinaryFX { Texture=tBinaryFX; MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR; AddressU = REPEAT; AddressV = REPEAT; };

uniform int _Timer < source = "timer"; >;

float GetLinearDepth(float2 coords)
{
	return ReShade::GetLinearizedDepth(coords);
}

float4 PS_3DBinary(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 uv = texcoord.xy;
	float _Time = _Timer*0.001;
	
	float depth = GetLinearDepth(uv.xy) * RESHADE_DEPTH_LINEARIZATION_FAR_PLANE;
	depth/= fixDistance*10;
	depth = saturate(depth);
	depth = lerp(0.5,depth,_Fade);
	
	float t=_Time*_MatrixSpeed;
	float2 uv2=uv;
	depth=floor(depth*32)/32;
	uv/=depth+0.2-(_FadeFromBinary/2);
	uv.x-=t;
	uv*=float2(1,0.5)+_MatrixSize;
	float3 mx = tex2D(sBinaryFX,uv).r;
	mx -=1-_MatrixColor;
	float md=mx*0.01*_Fade;
	float3 txt = tex2D(ReShade::BackBuffer,uv2+float2(md,md));
	mx+=txt*2+depth*(_LightIntensity-1);
	mx=lerp(txt,mx,_Fade);

	if (Debug) {
		return depth;
	} else {
		return float4(mx,1.0);
	}

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Binary {
	pass Binary {
		VertexShader=PostProcessVS;
		PixelShader=PS_3DBinary;
	}
}