/*

Source: https://www.shadertoy.com/view/wlScWG

Original by Hatchling @ ShaderToy

Ported by Lucas Melo (luluco250)

Fixed by Matsilagi

*/

#include "ReShade.fxh"

static const float PI = 3.14159265359;

// Adjust these values to control the look of the encoding.

// Increasing this value increases ringing artifacts. Careful, higher values are
// expensive.

uniform int WINDOW_RADIUS <
	ui_type = "drag";
	ui_min = 0;
	ui_max = 255;
	ui_step = 1;
	ui_label = "Window Radius [NTSCCustom]";
	ui_tooltip = "Increasing this value increases ringing artifacts. Higher values are more expensive";
> = 20;

// Simulated AM signal transmission.
uniform float AM_CARRIERSIGNAL_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "AM Signal Wavelength [NTSCCustom]";
	ui_tooltip = "Simulated AM signal transmission.";
> = 2.0;

uniform float AM_DECODE_HIGHPASS_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "AM Decode Highpass Wavelength [NTSCCustom]";
	ui_tooltip = "Simulated AM signal transmission.";
> = 2.0;

uniform float AM_DEMODULATE_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "AM Demodulate Wavelength [NTSCCustom]";
	ui_tooltip = "Simulated AM signal transmission.";
> = 2.0;

// Wavelength of the color signal.
uniform float COLORBURST_WAVELENGTH_ENCODER <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Encoder Color Signal Wavelength [NTSCCustom]";
	ui_tooltip = "Wavelength of the color signal.";
> = 2.5;

uniform float COLORBURST_WAVELENGTH_DECODER <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Decoder Color Signal Wavelength [NTSCCustom]";
	ui_tooltip = "Wavelength of the color signal.";
> = 2.5;

// Lowpassing of luminance before encoding.
// If this value is less than the colorburst wavelength,
// luminance values will be interpreted as chrominance,
// resulting in color fringes near edges.
uniform float YLOWPASS_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 255.0;
	ui_step = 0.001;
	ui_label = "Encode Lowpass Wavelength (Y) [NTSCCustom]";
	ui_tooltip = "Lowpassing of luminance before encoding. If this value is less than the colorburst wavelength, it will have color fringes on the edges";
> = 1.0;

// The higher these values are, the more smeary colors will be.

uniform float ILOWPASS_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 255.0;
	ui_step = 0.001;
	ui_label = "Encode Lowpass Wavelength (I) [NTSCCustom]";
	ui_tooltip = "The higher these values are, the more smeary colors will be.";
> = 8.0;

uniform float QLOWPASS_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 255.0;
	ui_step = 0.001;
	ui_label = "Encode Lowpass Wavelength (Q) [NTSCCustom]";
	ui_tooltip = "The higher these values are, the more smeary colors will be.";
> = 11.0;

// The higher this value, the blurrier the image.
uniform float DECODE_LOWPASS_WAVELENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 255.0;
	ui_step = 0.001;
	ui_label = "Decode Lowpass Wavelength [NTSCCustom]";
	ui_tooltip = "The higher this value, the blurrier the image.";
> = 2.0;

// Change the overall scale of the NTSC-style encoding and decoding artifacts.
uniform float NTSC_SCALE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "NTSC Artifacts Scale [NTSCCustom]";
	ui_tooltip = "Change the overall scale of the NTSC-style encoding and decoding artifacts.";
> = 1.0;

static const float PHASE_ALTERNATION = 3.1415927; //0.0

// Amount of TV static.
uniform float NOISE_STRENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Noise Strength [NTSCCustom]";
	ui_tooltip = "Amount of TV static.";
> = 0.1;

// Saturation control.
uniform float SATURATION <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.001;
	ui_label = "Saturation [NTSCCustom]";
	ui_tooltip = "Saturation control.";
> = 2.0;

// Offsets shape of window. This can make artifacts smear to one side or the
// other.
uniform float WINDOW_BIAS <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Window Bias [NTSCCustom]";
	ui_tooltip = "Offsets shape of window. This can make artifacts smear to one side or the other.";
> = 0.0;

static const float3x3 MatrixRGBToYIQ = float3x3(0.299, 0.587, 0.114,
												0.595,-0.274,-0.3213,
												0.2115,-0.5227, 0.3112);

static const float3x3 MatrixYIQToRGB = float3x3(1.0,  0.956,  0.619,
												1.0, -0.272, -0.647,
												1.0, -1.106, 1.703);

uniform int FrameCount <source = "framecount";>;
uniform float Timer <source = "timer";>;

texture target0_ntsc_custom
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};
sampler s0ntsc { Texture = target0_ntsc_custom; };

texture target1_ntsc_custom
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};
sampler s1ntsc { Texture = target1_ntsc_custom; };

// RNG algorithm credit: https://www.shadertoy.com/view/wtSyWm
#if (__RENDERER__ != 0x9000)
	uint wang_hash(inout uint seed)
	{
		seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
		seed *= uint(9);
		seed = seed ^ (seed >> 4);
		seed *= uint(0x27d4eb2d);
		seed = seed ^ (seed >> 15);

		return seed;
	}
#endif

float Hash(inout float seed)
{
	//return seed = frac(sin(seed) * 93457.5453);
	seed = 1664525 * seed + 1013904223;

	return seed;
}

float RandomFloat01(inout float state)
{
	#if (__RENDERER__ != 0x9000)
		return float(wang_hash(state)) / 4294967296.0;
	#else
		return sin(Hash(state) / 4294967296.0) * 0.5 + 0.5;
	#endif
}

float RandomFloat01(float2 uv)
{
	static const float A = 23.2345;
	static const float B = 84.1234;
	static const float C = 56758.9482;

    return frac(sin(dot(uv, float2(A, B))) * C + (Timer * 0.001));
}

// NOTE: Window functions expect a range from -1 to 1.
float Sinc(float x)
{
	if (x == 0.0)
		return 1.0;

    x *= PI;

	return sin(x) / x;
}

float WindowCosine(float x)
{
    x = atan(x);
    x += WINDOW_BIAS;
    x = tan(x);

	return cos(PI * x) * 0.5 + 0.5;
}

float Encode(
	sampler sp,
	float2 uv,
	float pixelWidth,
	bool alternatePhase)
{
    float3 yiq = 0.0;
    float windowWeight = 0.0;

	for (int i = -WINDOW_RADIUS; i <= WINDOW_RADIUS; i++)
    {
		// Extend padding by one since we don't want to include a sample at the
		// very edge, which will be 0.
        float window = WindowCosine(i / float(WINDOW_RADIUS + 1));

		float3 sincYiq = float3(
			Sinc(i / YLOWPASS_WAVELENGTH) / YLOWPASS_WAVELENGTH,
			Sinc(i / ILOWPASS_WAVELENGTH) / ILOWPASS_WAVELENGTH,
			Sinc(i / QLOWPASS_WAVELENGTH) / QLOWPASS_WAVELENGTH);

        float2 uvWithOffset = float2(uv.x + i * pixelWidth, uv.y);

        float3 yiqSample = mul(
			MatrixRGBToYIQ,
			saturate(tex2D(sp, uvWithOffset).rgb));
			// clamp(0.0, 1.0, tex2D(sp, uvWithOffset).xyz));

		yiq += yiqSample * sincYiq * window;
        windowWeight += window;
    }
    //yiq /= windowWeight;

    float phase = uv.x * PI / (COLORBURST_WAVELENGTH_ENCODER * pixelWidth);

    if (alternatePhase)
        phase += PHASE_ALTERNATION;

    float phaseAM = uv.x * PI / (AM_CARRIERSIGNAL_WAVELENGTH * pixelWidth);

	float s, c;
	sincos(phase, s, c);

    return (yiq.x + s * yiq.y + c * yiq.z) * sin(phaseAM);
}

float DecodeAM(sampler sp, float2 uv, float pixelWidth)
{
    float originalSignal = tex2D(sp, uv).x;
    float phaseAM = uv.x * PI / (AM_DEMODULATE_WAVELENGTH * pixelWidth);
    float decoded = 0.0;
    float windowWeight = 0.0;

	#if (__RENDERER != 0x10000)
		[unroll]
	#endif
	for (int i = -WINDOW_RADIUS; i <= WINDOW_RADIUS; i++)
    {
		// Extend padding by one since we don't want to include a sample at the
		// very edge, which will be 0.
        float window = WindowCosine(i / float(WINDOW_RADIUS + 1));
        float2 uvWithOffset = float2(uv.x + i * pixelWidth, uv.y);

        float sinc =
			Sinc(i / AM_DECODE_HIGHPASS_WAVELENGTH) /
			AM_DECODE_HIGHPASS_WAVELENGTH;

        float encodedSample = tex2D(sp, uvWithOffset).x;

    	decoded += encodedSample * sinc * window;
        windowWeight += window;
    }

    return decoded * sin(phaseAM) * 4.0;
}

float3 DecodeNTSC(
	sampler sp,
	float2 uv,
	float pixelWidth,
	float2 rng,
	bool alternatePhase)
{
    float seed = rng.y;

    float rowNoiseIntensity = RandomFloat01(seed);
    rowNoiseIntensity = pow(abs(rowNoiseIntensity), 500.0) * 1.0;

    float horizOffsetNoise = RandomFloat01(seed) * 2.0 - 1.0;
    horizOffsetNoise *= rowNoiseIntensity * 0.1 * NOISE_STRENGTH;

    float phaseNoise = RandomFloat01(seed) * 2.0 - 1.0;
    phaseNoise *= rowNoiseIntensity * 0.5 * PI * NOISE_STRENGTH;

    float frequencyNoise = RandomFloat01(seed) * 2.0 - 1.0;
    frequencyNoise *= rowNoiseIntensity * 0.1 * PI * NOISE_STRENGTH;

    float alt = (alternatePhase) ? PHASE_ALTERNATION : 0.0;

    float3 yiq = 0.0;
    float windowWeight = 0.0;

	#if (__RENDERER__ != 0x10000)
		[unroll]
	#endif
	for (int i = -WINDOW_RADIUS; i <= WINDOW_RADIUS; i++)
    {
		// Extend padding by one since we don't want to include a sample at the
		// very edge, which will be 0.
        float window = WindowCosine(i / float(WINDOW_RADIUS + 1));

        float2 uvWithOffset = float2(uv.x + i * pixelWidth, uv.y);
    	float phase =
			uvWithOffset.x * PI /
			((COLORBURST_WAVELENGTH_DECODER + frequencyNoise) * pixelWidth) +
			phaseNoise + alt;

		float s, c;
		sincos(phase, s, c);

		float3 sincYiq = float3(
			Sinc(i / DECODE_LOWPASS_WAVELENGTH) / DECODE_LOWPASS_WAVELENGTH,
			s,
			c);

        float encodedSample = tex2D(sp, uvWithOffset).x;

		yiq += encodedSample * sincYiq * window;
        windowWeight += window;
    }

    yiq.yz *= SATURATION / windowWeight;

    return max(0.0, mul(MatrixYIQToRGB, yiq));
}

float4 BufferAPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 fragCoord = uv * ReShade::ScreenSize;

	// TODO: Improve RNG.
	// Initialize a random number state based on frag coord and frame.
    // float rngState =
	// 	fragCoord.x * 1973 + fragCoord.y * 9277 + FrameCount * 26699;

	// float rngStateRow = fragCoord.y * 9277 + FrameCount * 26699;

	float rngState = fragCoord.x * 1973 + fragCoord.y * 9277 + FrameCount * 26699 + 1000;
	float rngStateRow = fragCoord.y * 9277 + FrameCount * 26699 + 1000;
	
	if (FrameCount > 70000){
		rngState = fragCoord.x * 1973 + fragCoord.y * 9277 + (Timer * 0.001) * 26699 + 1000;
		rngStateRow = fragCoord.y * 9277 + (Timer * 0.001) * 26699 + 1000;
	}

    float encoded = Encode(
		ReShade::BackBuffer,
		uv,
		BUFFER_RCP_WIDTH * NTSC_SCALE,
		(FrameCount + int(fragCoord.y)) % 2 == 0);

    float snowNoise = RandomFloat01(rngState) - 0.5;

    float sineNoise =
		sin(uv.x * 200.0 + uv.y * -50.0 + frac((Timer * 0.001) * (Timer * 0.001)) * PI * 2.0) *
		0.065;

	float saltPepperNoise = RandomFloat01(rngState) * 2.0 - 1.0;
    saltPepperNoise =
		sign(saltPepperNoise) * pow(abs(saltPepperNoise), 200.0) * 10.0;

    float rowNoise = RandomFloat01(rngStateRow) * 2.0 - 1.0;
    rowNoise *= 0.1;

    float rowSaltPepper = RandomFloat01(rngStateRow) * 2.0 - 1.0;
    rowSaltPepper = sign(rowSaltPepper) * pow(abs(rowSaltPepper), 200.0) * 1.0;

    encoded +=
		(snowNoise + saltPepperNoise + sineNoise + rowNoise + rowSaltPepper) *
		NOISE_STRENGTH;

    return float4(encoded.xxx, 1.0);
}

float4 BufferBPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float2 fragCoord = uv * ReShade::ScreenSize;
    float2 pixelSize = ReShade::PixelSize;

    float value = DecodeAM(s0ntsc, uv, pixelSize.x);

	return float4(value.xxx, 1.0);
}

float4 BufferCPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 fragCoord = uv * ReShade::ScreenSize;

    float rngStateRow = fragCoord.y * 9277 + FrameCount * 26699 + 1000;
    float rngStateCol = fragCoord.x * 1973 + FrameCount * 26699 + 1000;
	
	if (FrameCount > 70000){
		rngStateRow = fragCoord.y * 9277 + (Timer * 0.001) * 26699 + 1000;
		rngStateCol = fragCoord.x * 1973 + (Timer * 0.001) * 26699 + 1000;
	}

	float3 color = DecodeNTSC(
		s1ntsc,
		uv,
		BUFFER_RCP_WIDTH * NTSC_SCALE,
		float2(rngStateCol, rngStateRow),
		(FrameCount + int(fragCoord.y)) % 2 == 0);

    return float4(color, 1.0);
}

technique CustomizableNTSCFilterWithAM
{
	pass BufferA
	{
		VertexShader = PostProcessVS;
		PixelShader = BufferAPS;
		RenderTarget = target0_ntsc_custom;
	}
	pass BufferB
	{
		VertexShader = PostProcessVS;
		PixelShader = BufferBPS;
		RenderTarget = target1_ntsc_custom;
	}
	pass BufferC
	{
		VertexShader = PostProcessVS;
		PixelShader = BufferCPS;
	}
}