#include "ReShade.fxh"

uniform float fRollingFlicker_Factor <
    ui_label = "Rolling Flicker Factor [CRTRefresh]";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.25;

uniform float fRollingFlicker_VSyncTime <
    ui_label = "Rolling Flicker V Sync Time [CRTRefresh]";
    ui_type = "drag";
    ui_min = -144.0;
    ui_max = 144.0;
> = 1.0;

uniform float fTimer <source="timer";>;

float fmod(float a, float b) {
    float c = frac(abs(a / b)) * abs(b);
    return (a < 0) ? -c : c;
}

float wrap(float f, float f_min, float f_max) {
    return (f < f_min) ? (f_max - fmod(f_min - f, f_max - f_min)) : (f_min + fmod(f - f_min, f_max - f_min));
}

void RollingFlicker(inout float3 col, float2 uv) {
    float t = fTimer * fRollingFlicker_VSyncTime * 0.001;
    float y = fmod(-t, 1.0);
    float rolling_flicker = uv.y + y;
    rolling_flicker = wrap(rolling_flicker, 0.0, 1.0);
    col *= lerp(1.0, rolling_flicker, fRollingFlicker_Factor);
}

float4 PS_RefreshRate (float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  float3 rgb = tex2D(ReShade::BackBuffer, texcoord).rgb;
  RollingFlicker(rgb,texcoord);
  return float4(rgb,1.0);
}

technique CRTRefresh
{
	pass RefreshPass
	{
     VertexShader = PostProcessVS;
     PixelShader  = PS_RefreshRate;
   }  
}