// === PARAMETERS

uniform int2 display_size <
	ui_type = "input";
	ui_label = "Display Size";
	ui_tooltip = "The virtual screen size in pixels";
> = int2(320, 240);

uniform float2 pixel_mask_scale <
	ui_type = "input";
	ui_label = "Pixel Mask Scale";
	ui_tooltip = "Number of times to repeat pixel mask image";
> = float2(160, 90);

uniform float rf_noise <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_label = "RF Noise";
	ui_tooltip = "Apply noise to signal when in RF mode";
> = 0.1;

uniform float luma_sharpen <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 4.0;
	ui_label = "Luma Sharpen";
	ui_tooltip = "Apply sharpen filter to luma signal";
> = 1.0;

uniform bool EnableTVCurvature <
	ui_label = "Enable TV Curvature";
	ui_tooltip = "Enables a CRT Curvature effect";
> = true;

uniform float tv_curvature <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "TV Curvature";
	ui_tooltip = "Apply CRT style curve to screen";
> = 0.5;

uniform int3 colorbits <
	ui_type = "drag";
	ui_min = 2;
	ui_max = 8;
	ui_label = "Color Bits";
	ui_tooltip = "Bits per color channel";
> = int3( 8, 8, 8 );

uniform bool EnableRollingFlicker <
	ui_label = "Enable Rolling Flicker";
	ui_tooltip = "Enables a Flicker effect to simulate mismatch between TV and Camera VSync";
> = true;

uniform float fRollingFlicker_Factor <
    ui_label = "Rolling Flicker Factor";
    ui_type = "drag";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.25;

uniform float fRollingFlicker_VSyncTime <
    ui_label = "Rolling Flicker V Sync Time";
    ui_type = "drag";
    ui_min = -20.0;
    ui_max = 20.0;
> = 1.0;

uniform bool EnablePixelMask <
	ui_label = "Enable Pixel Mask";
	ui_tooltip = "Enables a CRT-Like Pixel-Mask";
> = true;
	
uniform float pixelMaskBrightness <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 2.0;
	ui_label = "Pixel Mask Brightness";
	ui__tooltip = "Brightness of the Pixel Mask";
> = 1.5;

uniform bool EnableBurstCountAnimation <
	ui_label = "Enable Burst Counter";
	ui_tooltip = "Enables an animated burst counter from the NTSC Signal";
> = true;

uniform int framecount < source = "framecount"; >;

// === END PARAMETERS

// === CONSTS AND UNIFORMS

#define PI 3.14159265
#define SCANLINE_PHASE_OFFSET (PI * 0.66667)
#define CHROMA_MOD_FREQ (PI / 3.0)
#define CHROMA_AMP 1.5
#define ENCODE_GAMMA (1.0 / 2.2)

#define SATURATION 1.0
#define BRIGHTNESS 1.0
#define chroma_mod (2.0 * SATURATION / CHROMA_AMP)

#define NTSC_GAMMA 1.0

#define RGB_SHIFT float4( 0.02, 0.02, 0.02, 0.0 )
#define RGB_SCALE ( 1.0 - RGB_SHIFT )

uniform float fTimer <source="timer";>;

static const float3x3 rgb2yiq = float3x3(
	0.299, 0.587, 0.114,
	0.596, -0.275, -0.321,
	0.221, -0.523, 0.311
);

static const float3x3 yiq2rgb = float3x3(
	1.0, 0.956, 0.621,
	1.0, -0.272, -0.647,
	1.0, -1.106, 1.703
);

// === END CONSTS AND UNIFORMS

// === HELPER FUNCTIONS

float fmod(float a, float b) {
    float c = frac(abs(a / b)) * abs(b);
    return (a < 0) ? -c : c;
}

float wrap(float f, float f_min, float f_max) {
    return (f < f_min) ? (f_max - fmod(f_min - f, f_max - f_min)) : (f_min + fmod(f - f_min, f_max - f_min));
}

float rand(float3 myVector)
{
	return frac(sin(dot(myVector, float3(12.9898, 78.233, 45.5432))) * 43758.5453);
}

float3 quantize_rgb( float3 rgb )
{
	float3 q = float3(
		1 << colorbits.r,
		1 << colorbits.g,
		1 << colorbits.b
	);

	rgb *= q;
	rgb = floor( rgb );
	rgb /= q;
	
	return rgb;
}

// === END HELPER FUNCTIONS

// === VERTEX SHADERS

void RetroTV_VS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float2 pix_no : TEXCOORD1, out float2 mask_uv : TEXCOORD2)
{
    texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	
	pix_no = texcoord * display_size * float2( 4.0, 1.0 );
	mask_uv = texcoord * pixel_mask_scale;
}

// === END VERTEX SHADERS

// === PIXEL SHADERS

//Rolling Flicker that simulates VSync mismatch
void RollingFlicker(inout float3 col, float2 uv) {
    float t = fTimer * fRollingFlicker_VSyncTime * 0.001;
    float y = fmod(-t, 1.0);
    float rolling_flicker = uv.y + y;
    rolling_flicker = wrap(rolling_flicker, 0.0, 1.0);
    col *= lerp(1.0, rolling_flicker, fRollingFlicker_Factor);
}

// samples target0 and encodes in a composite signal plus added RF noise
float4 RFEncodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float3 rgb = tex2D(samplerTarget0, texcoord).rgb;
	rgb = quantize_rgb( rgb );
	
	float3 yiq = mul( rgb2yiq, rgb );
	
	float chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + 0.0);
	
	if (EnableBurstCountAnimation){
		chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + framecount);
	}
	
	float mod_phase = chroma_phase + pix_no.x * CHROMA_MOD_FREQ;
	
	float i_mod = cos(mod_phase);
	float q_mod = sin(mod_phase);

	yiq.y *= i_mod * CHROMA_AMP;
	yiq.z *= q_mod * CHROMA_AMP;
	
	float signal = dot(yiq, float3(1.0, 1.0, 1.0));
	
	float rmod = 1.0 - (sin(texcoord.x * 320) * 0.05);
	float noise = (rand(float3(texcoord, (framecount%60) * 0.16)) * rmod * 2 - 1) * rf_noise;
	signal += noise;
	
	float3 out_color = signal.xxx * float3(BRIGHTNESS, i_mod * chroma_mod, q_mod * chroma_mod);
	return float4( out_color, 1.0 );
}

// samples target0 and encodes in an s-video signal
float4 SVideoEncodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float3 rgb = tex2D(samplerTarget0, texcoord).rgb;
	rgb = quantize_rgb( rgb );
	
	float3 yiq = mul( rgb2yiq, rgb );
	
	float chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + 0.0);
	
	if (EnableBurstCountAnimation){
		chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + framecount);
	}
	float mod_phase = chroma_phase + pix_no.x * CHROMA_MOD_FREQ;
	
	float i_mod = cos(mod_phase);
	float q_mod = sin(mod_phase);

	yiq.y *= i_mod * CHROMA_AMP;
	yiq.z *= q_mod * CHROMA_AMP;
	
	float signal = dot(yiq, float3(1.0, 1.0, 1.0));
	
	float3 out_color = float3( yiq.x, signal, signal ) * float3(BRIGHTNESS, i_mod * chroma_mod, q_mod * chroma_mod);
	return float4( out_color, 1.0 );
}

// samples target0 and encodes in a composite signal
float4 CompositeEncodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float3 rgb = tex2D(samplerTarget0, texcoord).rgb;
	rgb = quantize_rgb( rgb );
	
	float3 yiq = mul( rgb2yiq, rgb );
	
	float chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + 0.0);
	
	if (EnableBurstCountAnimation){
		chroma_phase = SCANLINE_PHASE_OFFSET * (pix_no.y + framecount);
	}
	
	float mod_phase = chroma_phase + pix_no.x * CHROMA_MOD_FREQ;
	
	float i_mod = cos(mod_phase);
	float q_mod = sin(mod_phase);

	yiq.y *= i_mod * CHROMA_AMP;
	yiq.z *= q_mod * CHROMA_AMP;
	
	float signal = dot(yiq, float3(1.0, 1.0, 1.0));
	
	float3 out_color = signal.xxx * float3(BRIGHTNESS, i_mod * chroma_mod, q_mod * chroma_mod);
	return float4( out_color, 1.0 );
}

// samples target1 and decodes into YIQ
float4 RFDecodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float _LumaFilter[9] =
	{
		-0.0020, -0.0009, 0.0038, 0.0178, 0.0445,
		0.0817, 0.1214, 0.1519, 0.1634
	};

	float _ChromaFilter[9] =
	{
		0.0046, 0.0082, 0.0182, 0.0353, 0.0501,
		0.0832, 0.1062, 0.1222, 0.1280
	};

	float2 one_x = float2( 1.0 / 1280, 0.0 );
	float3 signal = float3( 0.0, 0.0, 0.0 );
	
	for (int idx = 0; idx < 8; idx++)
	{
		float offset = float(idx);

		float3 sums = tex2D(samplerTarget1, texcoord + ( ( offset - 8.0 ) * one_x * 1.5 )) +
			tex2D(samplerTarget1, texcoord + ( ( 8.0 - offset ) * one_x * 1.5 ));

		signal += sums * float3(_LumaFilter[idx], _ChromaFilter[idx], _ChromaFilter[idx]);
	}
	signal += tex2D(samplerTarget1, texcoord) *
		float3(_LumaFilter[8], _ChromaFilter[8], _ChromaFilter[8]);
		
	return float4( signal, 1.0 );
}

// samples target1 and decodes into YIQ
float4 CompositeDecodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float _LumaFilter[9] =
	{
		-0.0020, -0.0009, 0.0038, 0.0178, 0.0445,
		0.0817, 0.1214, 0.1519, 0.1634
	};

	float _ChromaFilter[9] =
	{
		0.0046, 0.0082, 0.0182, 0.0353, 0.0501,
		0.0832, 0.1062, 0.1222, 0.1280
	};

	float2 one_x = float2( 1.0 / 1280, 0.0 );
	float3 signal = float3( 0.0, 0.0, 0.0 );
	
	for (int idx = 0; idx < 8; idx++)
	{
		float offset = float(idx);

		float3 sums = tex2D(samplerTarget1, texcoord + ( ( offset - 8.0 ) * one_x )) +
			tex2D(samplerTarget1, texcoord + ( ( 8.0 - offset ) * one_x ));

		signal += sums * float3(_LumaFilter[idx], _ChromaFilter[idx], _ChromaFilter[idx]);
	}
	signal += tex2D(samplerTarget1, texcoord) *
		float3(_LumaFilter[8], _ChromaFilter[8], _ChromaFilter[8]);
		
	return float4( signal, 1.0 );
}

// samples target1 and decodes into YIQ
float4 SVideoDecodePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float _LumaFilter[9] =
	{
		-0.0020, -0.0009, 0.0038, 0.0178, 0.0445,
		0.0817, 0.1214, 0.1519, 0.1634
	};

	float _ChromaFilter[9] =
	{
		0.0046, 0.0082, 0.0182, 0.0353, 0.0501,
		0.0832, 0.1062, 0.1222, 0.1280
	};

	float2 one_x = float2( 1.0 / 1280, 0.0 );
	float3 signal = float3( 0.0, 0.0, 0.0 );
	
	for (int idx = 0; idx < 8; idx++)
	{
		float offset = float(idx);

		float3 sums = tex2D(samplerTarget1, texcoord + ( ( offset - 8.0 ) * one_x )) +
			tex2D(samplerTarget1, texcoord + ( ( 8.0 - offset ) * one_x ));

		signal += sums * float3(0.0, _ChromaFilter[idx], _ChromaFilter[idx]);
	}
	signal += tex2D(samplerTarget1, texcoord) *
		float3(1.0, _ChromaFilter[8], _ChromaFilter[8]);
		
	return float4( signal, 1.0 );
}

float4 CompositeFinalPS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float2 one_x = float2( 1.0 / 1280, 0.0 ) * 2.0;
	
	float3 yiq = tex2D(samplerTarget0, texcoord);
	float3 yiq2 = tex2D(samplerTarget0, texcoord + one_x);
	float3 yiq3 = tex2D(samplerTarget0, texcoord - one_x);
	
	// for realism this should be a scanline-based comb filter, but that doesn't seem to look quite right
	// so for now it's a naive horizontal convolution instead
	yiq.x += (yiq.x * luma_sharpen * 2) + (yiq2.x * -1 * luma_sharpen) + (yiq3.x * -1 * luma_sharpen);
	
	float3 rgb = mul( yiq2rgb, yiq );
	
	//apply rolling flicker
	if (EnableRollingFlicker){
		RollingFlicker(rgb, texcoord);
	}
	
	// apply pixel mask & tv border
	if (EnablePixelMask) {
		rgb *= tex2D( sampler_pixel_mask, mask_uv ).rgb * pixelMaskBrightness;
	}
	
	if (EnableTVCurvature) {
		rgb *= tex2D( sampler_tv_border, texcoord ).rgb;
	}
	
	return float4( rgb, 1.0 );
}

// VGA / SCAR-T, Just blits to the screen i guess.
float4 VGAFinalPS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target {
	float3 rgb = tex2D(ReShade::BackBuffer, texcoord).rgb;
	rgb = quantize_rgb( rgb );
	
	//apply rolling flicker
	if (EnableRollingFlicker){
		RollingFlicker(rgb, texcoord);
	}
	
	// apply pixel mask & tv border
	if (EnablePixelMask) {
		rgb *= tex2D( sampler_pixel_mask, mask_uv ).rgb * pixelMaskBrightness;
	}
	
	// apply tv curvature
	if (EnableTVCurvature) {
		rgb *= tex2D( sampler_tv_border, texcoord ).rgb;
	}

	return float4(rgb.x, rgb.y, rgb.z, 1.0) * RGB_SCALE + RGB_SHIFT;
}

float4 SVideoFinalPS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0, float2 pix_no : TEXCOORD1, float2 mask_uv : TEXCOORD2) : SV_Target
{
	float3 yiq = tex2D(samplerTarget0, texcoord);
	float3 rgb = mul( yiq2rgb, yiq );
	
	//Apply Rolling Flicker
	if (EnableRollingFlicker){
		RollingFlicker(rgb, texcoord);
	}
	
	// apply pixel mask & tv border
	if (EnablePixelMask) {
		rgb *= tex2D( sampler_pixel_mask, mask_uv ).rgb * pixelMaskBrightness;
	}
	
	if (EnableTVCurvature) {
		rgb *= tex2D( sampler_tv_border, texcoord ).rgb;
	}
	
	return float4( rgb, 1.0 );
}

float4 TVCurvaturePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	if (EnableTVCurvature) {
		float2 coords = texcoord;
		coords = ( coords - 0.5 ) * 2.0;
	
		float2 intensity = float2( tv_curvature, tv_curvature ) * 0.1;
	
		float2 realCoordOffs;
		realCoordOffs.x = (coords.y * coords.y) * intensity.y * (coords.x);
		realCoordOffs.y = (coords.x * coords.x) * intensity.x * (coords.y);
	
		float2 uv = texcoord + realCoordOffs;	
		return tex2D(samplerTarget1, uv);
	} else {
		return tex2D(samplerTarget1, texcoord);
	}
}

// === END PIXEL SHADERS