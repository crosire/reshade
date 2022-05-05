#include "ReShade.fxh"

uniform float fTimeOfDay <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Time of Day [Sky Replacer]";
> = 1.0;

// Valid from 1000 to 40000 K (and additionally 0 for pure full white)
float3 colorTemperatureToRGB(float temperature){
	// Values from: http://blenderartists.org/forum/showthread.php?270332-OSL-Goodness&p=2268693&viewfull=1#post2268693   
	float3x3 m;
	
	if (temperature <= 6500.0)
		m = float3x3(
			float3(0.0, -2902.1955373783176, -8257.7997278925690),
			float3(0.0, 1669.5803561666639, 2575.2827530017594),
			float3(1.0, 1.3302673723350029, 1.8993753891711275)
		);
	else
		m = float3x3(
			float3(1745.0425298314172, 1216.6168361476490, -8257.7997278925690),
			float3(-2666.3474220535695, -2173.1012343082230, 2575.2827530017594),
			float3(0.55995389139931482, 0.70381203140554553, 1.8993753891711275)
		);
	
	return lerp(
		saturate(
			m[0] / (clamp(temperature, 1000.0, 40000.0) + m[1]) + m[2]
		),
		1.0,
		smoothstep(1000.0, 0.0, temperature)
	);
}

float4 PS_SkyReplacer(float4 pos  : SV_POSITION, float2 uv   : TEXCOORD) : SV_TARGET
{
	float3 col = tex2D(ReShade::BackBuffer, uv).rgb;
	float intensity = fTimeOfDay;
	float temp = lerp(0.0, intensity, 0.327);
	temp *= 39000 + 1000;
	float3 sky_bg = colorTemperatureToRGB(temp);
    float depth = ReShade::GetLinearizedDepth(uv);
    col = depth >= 1.0 ? sky_bg : col;

    return float4(col, 1.0);
}

technique SkyReplacer
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader  = PS_SkyReplacer;
	}
}