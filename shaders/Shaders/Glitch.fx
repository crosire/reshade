#include "ReShade.fxh"

#define PI 3.14159265
#define TILE_SIZE 16.0

uniform float  Timer < source = "timer"; >;

#include "ReShadeUI.fxh"

uniform float Amount < __UNIFORM_SLIDER_FLOAT1
    ui_min = 0.0;
    ui_max = 10.0;
    ui_tooltip = "Glitch Amount [Glitch B]";
> = 1.0;

uniform bool bUseUV < __UNIFORM_COMBO_BOOL1
    ui_tooltip = "Use UV for Glitch [Glitch B]";
> = false;

float fmod(float a, float b) {
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}
float2 fmod(float2 a, float2 b) {
	float2 c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float3 rgb2hsv(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 hsv2rgb(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float3 posterize(float3 color, float steps)
{
    return floor(color * steps) / steps;
}

float quantize(float n, float steps)
{
    return floor(n * steps) / steps;
}

float4 downsample(sampler2D samp, float2 uv, float pixelSize)
{
    return tex2D(samp, uv - fmod(uv, float2(pixelSize,pixelSize) / ReShade::ScreenSize.xy));
}

float rand(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float noise(float p)
{
    float fl = floor(p);
  	float fc = frac(p);
    return lerp(rand(fl), rand(fl + 1.0), fc);
}

float rand(float2 n) 
{ 
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

float noise(float2 p)
{
    float2 ip = floor(p);
    float2 u = frac(p);
    u = u * u * (3.0 - 2.0 * u);

    float res = lerp(
        lerp(rand(ip), rand(ip + float2(1.0, 0.0)), u.x),
        lerp(rand(ip + float2(0.0,1.0)), rand(ip + float2(1.0,1.0)), u.x), u.y);
    return res * res;
}

float3 edge(sampler2D samp, float2 uv, float sampleSize)
{
    float dx = sampleSize / ReShade::ScreenSize.x;
    float dy = sampleSize / ReShade::ScreenSize.y;
    return (
    lerp(downsample(samp, uv - float2(dx, 0.0), sampleSize), downsample(samp, uv + float2(dx, 0.0), sampleSize), fmod(uv.x, dx) / dx) +
    lerp(downsample(samp, uv - float2(0.0, dy), sampleSize), downsample(samp, uv + float2(0.0, dy), sampleSize), fmod(uv.y, dy) / dy)    
    ).rgb / 2.0 - tex2D(samp, uv).rgb;
}

float3 distort(sampler2D samp, float2 uv, float edgeSize)
{
    float2 pixel = float2(1.0,1.0) / ReShade::ScreenSize.xy;
    float3 field = rgb2hsv(edge(samp, uv, edgeSize));
    float2 distort = pixel * sin((field.rb) * PI * 2.0);
    float shiftx = noise(float2(quantize(uv.y + 31.5, ReShade::ScreenSize.y / TILE_SIZE) * Timer*0.001, frac(Timer*0.001) * 300.0));
    float shifty = noise(float2(quantize(uv.x + 11.5, ReShade::ScreenSize.x / TILE_SIZE) * Timer*0.001, frac(Timer*0.001) * 100.0));
    float3 rgb = tex2D(samp, uv + (distort + (pixel - pixel / 2.0) * float2(shiftx, shifty) * (50.0 + 100.0 * Amount)) * Amount).rgb;
    float3 hsv = rgb2hsv(rgb);
    hsv.y = fmod(hsv.y + shifty * pow(Amount, 5.0) * 0.25, 1.0);
    return posterize(hsv2rgb(hsv), floor(lerp(256.0, pow(1.0 - hsv.z - 0.5, 2.0) * 64.0 * shiftx + 4.0, 1.0 - pow(1.0 - Amount, 5.0))));
}

float4 PS_Glitch ( float4 pos : SV_Position, float2 fragCoord : TEXCOORD0) : SV_Target
{
	float4 fragColor;
	float wow;
	float Amount;

	float2 texcoord = fragCoord * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	float2 uv = texcoord.xy / ReShade::ScreenSize.xy;
	if (bUseUV) {
		Amount = uv.x; // Just erase this line if you want to use the control at the top
	}
    wow = clamp(fmod(noise(Timer*0.001 + uv.y), 1.0), 0.0, 1.0) * 2.0 - 1.0;    
    float3 finalColor;
    finalColor += distort(ReShade::BackBuffer, uv, 8.0);
	return float4(finalColor, 1.0);
}

technique GlitchB {
    pass GlitchB {
        VertexShader=PostProcessVS;
        PixelShader=PS_Glitch;
    }
}
