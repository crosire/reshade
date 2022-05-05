/*
Real Grain PS v0.3.0 (c) 2018 Jacob Maximilian Fober

This work is licensed under the Creative Commons 
Attribution-ShareAlike 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by-sa/4.0/.
*/


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform float2 Intensity < __UNIFORM_SLIDER_FLOAT2
	ui_label = "Noise intensity";
	ui_tooltip = "First Value - Macro Noise\n"
		"Second Value - Micro Noise";
	ui_min = 0.0; ui_max = 1.0; ui_step = 0.002;
> = float2(0.316, 0.08);

uniform int2 Framerate <
	ui_label = "Noise framerate";
	ui_tooltip = "Zero will match in-game framerate\n"
		"\n"
		"First Value - Macro Noise\n"
		"Second Value - Micro Noise";
	ui_type = "drag";
	ui_min = 0; ui_max = 120; ui_step = 1;
> = int2(6, 12);

uniform int Coefficient < __UNIFORM_RADIO_INT1
	ui_tooltip = "For digital video signal (HDMI, DVI, Display Port) use BT.709,\n"
	"for analog (like VGA, S-Video) use BT.601";
	#if __RESHADE__ < 40000
		ui_label = "YUV coefficients";
		ui_items = "BT.709 (digital signal)\0BT.601 (analog signal))\0";
	#else
		ui_items = "BT.709 - digital\0BT.601 - analog\0";
	#endif
> = 0;


	  //////////////
	 /// SHADER ///
	//////////////

uniform float Timer < source = "timer"; >;
uniform int FrameCount < source = "framecount"; >;


// Overlay blending mode
float Overlay(float LayerA, float LayerB)
{
	float MinA = min(LayerA, 0.5);
	float MinB = min(LayerB, 0.5);
	float MaxA = max(LayerA, 0.5);
	float MaxB = max(LayerB, 0.5);
	return 2 * (MinA * MinB + MaxA + MaxB - MaxA * MaxB) - 1.5;
}

// Noise generator
float SimpleNoise(float p)
{
	return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

#include "ReShade.fxh"

#if !defined(__RESHADE__) || __RESHADE__ < 40000
	static const float GoldenAB = sqrt(5) * 0.5 + 0.5;
	static const float GoldenABh = sqrt(5) * 0.25 + 0.25;
#else
	static const float GoldenAB = 1.6180339887498948482045868343656;
	static const float GoldenABh = 0.80901699437494742410229341718282;
#endif

#if !defined(ResolutionX) || !defined(ResolutionY)
	texture FilmGrainTex { Width = BUFFER_WIDTH * 0.7; Height = BUFFER_HEIGHT * 0.7; Format = R8; };
#else
	texture FilmGrainTex { Width = ResolutionX * 0.7; Height = ResolutionY * 0.7; Format = R8; };
#endif
sampler FilmGrain { Texture = FilmGrainTex; };

float Seed(float2 Coordinates, float Frames)
{
	// Calculate seed change
	float Seed = Frames == 0 ? FrameCount : floor(Timer * 0.001 * Frames);
	// Protect from enormous numbers
	Seed = frac(Seed * 0.0001) * 10000;
	return Seed * Coordinates.x * Coordinates.y;
}

void GrainLowRes(float4 vois : SV_Position, float2 TexCoord : TEXCOORD, out float Grain : SV_Target)
{
	Grain = saturate(SimpleNoise(Seed(TexCoord, Framerate.x)) * GoldenABh);
	Grain = lerp(0.5, Grain, 0.5);
}

// Shader pass
void RealGrainPS(float4 vois : SV_Position, float2 TexCoord : TEXCOORD, out float3 Image : SV_Target)
{
	// Choose luma coefficient, if True BT.709 Luma, else BT.601 Luma
	const float3 LumaCoefficient = (Coefficient == 0) ?
		float3( 0.2126,  0.7152,  0.0722)
		: float3( 0.299,  0.587,  0.114);
	// Sample image
	Image = tex2D(ReShade::BackBuffer, TexCoord).rgb;
	// Mask out bright pixels  gamma: (sqrt(5)+1)/2
	float Mask = pow(1 - dot(Image.rgb, LumaCoefficient), GoldenAB);
	// Generate noise *  (sqrt(5) + 1) / 4  (to remain brightness)
	float MicroNoise = SimpleNoise(Seed(TexCoord, Framerate.y));
	float MacroNoise = tex2D(FilmGrain, TexCoord).r;

	MicroNoise = 10 * (max(MicroNoise, 0.908 + 0.044) + min(MicroNoise, 1 - 0.908) - 1 - 0.044);
	MicroNoise = saturate(MicroNoise + 0.5);

	MacroNoise = lerp(0.5, MacroNoise, Intensity.x);

	float Noise = Overlay(MicroNoise, MacroNoise);
	Noise = lerp(0.5, Noise, Intensity.y);

	Image = tex2D(ReShade::BackBuffer, TexCoord).rgb;
	
	// Blend noise with image
	Image.rgb = float3(
		Overlay(Image.r, Noise),
		Overlay(Image.g, Noise),
		Overlay(Image.b, Noise)
	);
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique RealGrain < ui_label = "Real Grain"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = GrainLowRes;
		RenderTarget = FilmGrainTex;
	}
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = RealGrainPS;
	}
}
