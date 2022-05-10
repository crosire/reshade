#include "ReShade.fxh"

uniform float x_off_r <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "X Offset Red [Technicolor]";
> = 0.05;

uniform float y_off_r <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Y Offset Red [Technicolor]";
> = 0.05;

uniform float x_off_g <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "X Offset Green [Technicolor]";
> = -0.05;

uniform float y_off_g <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Y Offset Green [Technicolor]";
> = -0.05;

uniform float x_off_b <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "X Offset Blue [Technicolor]";
> = -0.05;

uniform float y_off_b <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Y Offset Blue [Technicolor]";
> = 0.05;

uniform float grain_str <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 16.0;
	ui_step = 1.0;
	ui_label = "Grain Strength [Technicolor]";
> = 12.0;

uniform bool lut_toggle <
	ui_type = "boolean";
	ui_label = "LUT Toggle [Technicolor]";
> = true;

uniform bool hotspot <
	ui_type = "boolean";
	ui_label = "Hotspot Toggle [Technicolor]";
> = true;

uniform bool vignette <
	ui_type = "boolean";
	ui_label = "Vignette Toggle [Technicolor]";
> = true;

uniform bool noise_toggle <
	ui_type = "boolean";
	ui_label = "Film Scratches";
> = true;

uniform int FrameCount < source = "framecount"; >;

texture texCMYKLUT < source = "CMYK.png"; > { Width = 256; Height = 16; Format = RGBA8; };
sampler	SamplerCMYKLUT 	{ Texture = texCMYKLUT; };

texture texFilmNoise < source = "film_noise1.png"; > { Width = 910; Height = 480; Format = RGBA8; };
sampler	SamplerFilmNoise {Texture = texFilmNoise;};

#define mod(x,y) (x-y*floor(x/y))

//https://www.shadertoy.com/view/4sXSWs strength= 16.0
float filmGrain(float2 uv, float strength, float timer ){       
    float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * ((mod(timer, 800.0) + 10.0) * 10.0);
	return  (mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
}

float hash( float n ){
    return frac(sin(n)*43758.5453123);
}

void PS_CMYK_LUT(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 res : SV_Target0)
{

	float4 color = tex2D(ReShade::BackBuffer, texcoord.xy);
	float2 texelsize = 1.0 / 16;
	texelsize.x /= 16;
	
	float3 lutcoord = float3((color.xy*16-color.xy+0.5)*texelsize.xy,color.z*16-color.z);
	
	float lerpfact = frac(lutcoord.z);
	lutcoord.x += (lutcoord.z-lerpfact)*texelsize.y;

	float3 lutcolor = lerp(tex2D(SamplerCMYKLUT, lutcoord.xy).xyz, tex2D(SamplerCMYKLUT, float2(lutcoord.x+texelsize.y,lutcoord.y)).xyz,lerpfact);

	if (lut_toggle){
	color.xyz = lerp(normalize(color.xyz), normalize(lutcolor.xyz), 1.0) * 
	            lerp(length(color.xyz),    length(lutcolor.xyz),    1.0);
	} else {
		color.xyz = lerp(normalize(color.xyz), normalize(lutcolor.xyz), 0.0) * 
	            lerp(length(color.xyz),    length(lutcolor.xyz),    0.0);
	}

	res.xyz = color.xyz;
	res.w = 1.0;
}

void PS_Technicolor_Noise(in float4 pos : SV_POSITION, in float2 texcoord : TEXCOORD0, out float4 gl_FragCol : COLOR0)
{
// a simple calculation for the vignette/hotspot effects
	float2 middle = texcoord.xy - 0.5;
	float len = length(middle);
	float vig = smoothstep(0.0, 1.0, len);

// create the noise effects from a LUT of actual film noise
	float4 film_noise1 = tex2D(SamplerFilmNoise, texcoord.xx *
		sin(hash(mod(float(FrameCount), 47.0))));
	float4 film_noise2 = tex2D(SamplerFilmNoise, texcoord.xy *
		cos(hash(mod(float(FrameCount), 92.0))));

	float2 red_coord = texcoord + 0.01 * float2(x_off_r, y_off_r);
	float3 red_light = tex2D(ReShade::BackBuffer, red_coord).rgb;
	float2 green_coord = texcoord + 0.01 * float2(x_off_g, y_off_g);
	float3 green_light = tex2D(ReShade::BackBuffer, green_coord).rgb;
	float2 blue_coord = texcoord + 0.01 * float2(x_off_r, y_off_r);
	float3 blue_light = tex2D(ReShade::BackBuffer, blue_coord).rgb;

	float3 film = float3(red_light.r, green_light.g, blue_light.b);
	film += filmGrain(texcoord.xy, grain_str, float(FrameCount)); // Film grain

	film *= (vignette > 0.5) ? (1.0 - vig) : 1.0; // Vignette
	film += ((1.0 - vig) * 0.2) * hotspot; // Hotspot

// Apply noise effects (or not)
	if (hash(float(FrameCount)) > 0.99 && noise_toggle > 0.5)
		gl_FragCol = float4(lerp(film, film_noise1.rgb, film_noise1.a), 1.0);
	else if (hash(float(FrameCount)) < 0.01 && noise_toggle > 0.5)
		gl_FragCol = float4(lerp(film, film_noise2.rgb, film_noise2.a), 1.0);
	else
		gl_FragCol = float4(film, 1.0);
} 

technique Technicolor
{
	pass Technicolor_P1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_CMYK_LUT;
	}
	pass Technicolor_P2
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_Technicolor_Noise;
	}
}