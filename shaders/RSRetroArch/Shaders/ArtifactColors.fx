/*
Adapted for RetroArch from Flyguy's "Apple II-Like Artifact Colors" from shadertoy:
https://www.shadertoy.com/view/llyGzR

"Colors created through NTSC artifacting on 4-bit patterns, similar to the Apple II's lo-res mode."

"Ported to ReShade by Matsilagi and luluco250"
*/

#define MUL(A, B) mul(A, B)

#include "ReShade.fxh"

//Macros/////////////////////////////////////////////////////////////////////////////////////////

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [ArtifactColors]";
> = BUFFER_WIDTH;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [ArtifactColors]";
> = BUFFER_HEIGHT;

uniform float HUE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Hue [ArtifactColors]";
> = 0.0;

uniform float SATURATION <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Saturation [ArtifactColors]";
> = 30.0;

uniform float BRIGHTNESS <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_label = "Brightness [ArtifactColors]";
	ui_step = 0.01;
> = 1.0;

uniform float F_COL <
	ui_type = "drag";
	ui_min = 4.0;
	ui_max = 8.0;
	ui_step = 1.0;
	ui_label = "F Col [ArtifactColors]";
	ui_tooltip = "Realtime changes not available in DX9.\nUse F_COL_DX9 to mod that in DX9.\n(Preprocessor Definition)";
> = 4.0;

uniform float F_COL_BW <
	ui_type = "drag";
	ui_min = 10.0;
	ui_max = 200.0;
	ui_step = 1.0;
	ui_label = "F Col BW [ArtifactColors]";
	ui_tooltip = "Realtime changes not available in DX9.\nUse F_COL_BW_DX9 to mod that in DX9.\n(Preprocessor Definition)";
> = 50.0;

uniform float F_LUMA_LP <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 12.0;
	ui_step = 1.0;
	ui_label = "F Luma LP [ArtifactColors]";
	ui_tooltip = "Realtime changes not available in DX9.\nUse F_LUMA_LP_DX9 to mod that in DX9.\n(Preprocessor Definition)";
> = 6.0;

uniform int Viewmode <
	ui_type = "combo";
	ui_label = "View Mode [ArtifactColors]";
	ui_items = "Composite\0RGB\0Luma\0Chroma\0Signal\0Split\0";
> = 0;

//DirectX 9 fixes HERE WE GOOOOOO fuck...
#ifndef F_COL_DX9
	#define F_COL_DX9 4.0
#endif

#ifndef F_LUMA_LP_DX9
	#define F_LUMA_LP_DX9 6.0
#endif

#ifndef F_COL_BW_DX9
	#define F_COL_BW_DX9 50.0
#endif

#define FIR_SIZE 29

#define rrend __RENDERER__

#define display_size float2(display_sizeX,display_sizeY)

//Statics////////////////////////////////////////////////////////////////////////////////////////

#define pi atan(1.0) * 4.0

#define tau atan(1.0) * 8.0

static const float3x3 rgb2yiq = float3x3(
	0.299, 0.596, 0.211,
	0.587,-0.274,-0.523,
	0.114,-0.322, 0.312
);
static const float3x3 yiq2rgb = float3x3(
	1.000, 1.000, 1.000,
	0.956,-0.272,-1.106,
	0.621,-0.647, 1.703
);

//Uniforms///////////////////////////////////////////////////////////////////////////////////////
//Textures///////////////////////////////////////////////////////////////////////////////////////

texture ArtifactChannel0_tex {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};

texture tArtifactColors_Modulator {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = R16F;
};
sampler sArtifactColors_Modulator {
	Texture = tArtifactColors_Modulator;
};

texture tArtifactColors_Demodulator {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA16F;
};

sampler sArtifactColors_Demodulator {
	Texture = tArtifactColors_Demodulator;
};

sampler ArtifactChannel0 {
	Texture = ArtifactChannel0_tex;
};

//Functions//////////////////////////////////////////////////////////////////////////////////////

//Angle -> 2D rotation matrix
float2x2 rotate(float a) {
	return float2x2(
		 cos(a), sin(a),
		-sin(a), cos(a)
	);
}

//Non-normalized texture sampling
float4 sample2D(sampler sp, float2 res, float2 uv) {
	return tex2D(sp, uv / res);
}

//Complex multiply
float2 cmul(float2 a, float2 b) {
	return float2((a.x * b.x) - (a.y * b.y), (a.x * b.y) + (a.y * b.y));
}

float sinc(float x) { //reshade warns about division by zero here, even though there should be none
	//return (x == 0.0) ? 1.0 : sin(x * pi) / max(x * pi, 0.001); //so we'll use max()
	return (x == 0.0) ? 1.0 : sin(x * pi) / (x * pi);
}

//https://en.wikipedia.org/wiki/Window_function
float WindowBlackman(float a, int N, int i) {
    float a0 = (1.0 - a) / 2.0;
    float a1 = 0.5;
    float a2 = a / 2.0;
    
    float wnd = a0;
    wnd -= a1 * cos(2.0 * pi * (float(i) / float(N - 1)));
    wnd += a2 * cos(4.0 * pi * (float(i) / float(N - 1)));
    
    return wnd;
}

//FIR lowpass filter 
//Fc = Cutoff freq., Fs = Sample freq., N = # of taps, i = Tap index
float Lowpass(float Fc, float Fs, int N, int i) {
	float wc = (Fc / Fs);
    
    float wnd = WindowBlackman(0.16, N, i);
    
    return 2.0 * wc * wnd * sinc(2.0 * wc * float(i - N / 2));
}

//FIR bandpass filter 
//Fa/Fb = Low/High cutoff freq., Fs = Sample freq., N = # of taps, i = Tap index
float Bandpass(float Fa, float Fb, float Fs, int N, int i) {
    float wa = (Fa / Fs);
    float wb = (Fb / Fs);
    
    float wnd = WindowBlackman(0.16, N, i);
    
    return 2.0 * (wb - wa) * wnd * (sinc(2.0 * wb * float(i - N / 2)) - sinc(2.0 * wa * float(i - N / 2)));
}

//Complex oscillator, Fo = Oscillator freq., Fs = Sample freq., n = Sample index
float2 Oscillator(float Fo, float Fs, float N) {
	float phase = (tau * Fo * floor(N)) / Fs;
	return float2(cos(phase), sin(phase));
}

float2 p_sh(float2 px){

	float2 xx = float2(320.0, 240.0); //output screen res				
	float2 ar = float2(1.0,1.0); //final aspect ratio (calculated)
	
	xx = display_size;
	
	ar = float(1.0).xx;
	
	return 0.5 * float2(
		step(0.0, 1.0 - ar.y) * (1.0 - 1.0 / ar.y), 
		step(1.0, ar.y) * (1.0 - ar.y));
}

//Shaders////////////////////////////////////////////////////////////////////////////////////////

float4 PS_Stock(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{	
	float2 px = texcoord.xy*float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT);
	float2 xx = float2(320.0,240.0);
	float2 ar = float2(1.0,1.0); //final aspect ratio (calculated)
	
	xx = ReShade::ScreenSize.xy;
	ar = float(1.0).xx;
	
	xx = display_size;
		
	px = texcoord;
	px -= 0.5;
	px *= ar;
	px += 0.5;
		
	if(ar.y<1.0){px -= 0.5; px = px*(1.0/ar.y); px += 0.5; }
		
	px = floor(px*xx)/xx;
	px +=  p_sh(px);
	
	return tex2D(ReShade::BackBuffer, px).rgba;
}		

float PS_Modulator(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float Fs = BUFFER_WIDTH;
	float Fcol = Fs * (1.0 / F_COL);
	float n = floor(uv.x * BUFFER_WIDTH);

	float3 cRGB = tex2D(ArtifactChannel0, uv).rgb;
	float3 cYIQ = MUL(rgb2yiq, cRGB);

	float2 cOsc = Oscillator(Fcol, Fs, n);

	float sig = cYIQ.x + dot(cOsc, cYIQ.yz);

	return sig;
}

float4 PS_Demodulator(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float2 _uv = uv * ReShade::ScreenSize;

	float Fs = BUFFER_WIDTH;
	float Fcol = Fs * (1.0 / F_COL_DX9);
	float Fcolbw = Fs * (1.0 / F_COL_BW_DX9);
	float Flumlp = Fs * (1.0 / F_LUMA_LP_DX9);
	float n = floor(_uv.x);
	
	if (rrend != 0x09300){
		Fcol = Fs * (1.0 / F_COL);
		Fcolbw = Fs * (1.0 / F_COL_BW);
		Flumlp = Fs * (1.0 / F_LUMA_LP);
	}

	float y_sig = 0.0;
	float iq_sig = 0.0;

	float2 cOsc = Oscillator(Fcol, Fs, n);

	n += float(FIR_SIZE) / 2.0;

	//Separate luma(Y) & chroma(IQ) signals
	[unroll]
	for (int i = 0; i < FIR_SIZE; ++i) {
		int tpidx = FIR_SIZE - i - 1;
		float lp = Lowpass(Flumlp, Fs, FIR_SIZE, tpidx);
		float bp = Bandpass(Fcol - Fcolbw, Fcol + Fcolbw, Fs, FIR_SIZE, tpidx);

		y_sig += sample2D(sArtifactColors_Modulator, ReShade::ScreenSize, float2(n - float(i), _uv.y)).x * lp;
		iq_sig += sample2D(sArtifactColors_Modulator, ReShade::ScreenSize, float2(n - float(i), _uv.y)).x * bp;
	}

	//Shift IQ signal down from Fcol to DC
	float2 iq_sig_mix = cmul(float2(iq_sig, 0.0), cOsc);

	return float4(y_sig, iq_sig_mix, 0.0);
}

float4 PS_Final(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD
) : SV_TARGET {
	float Fs = BUFFER_WIDTH;
	float Fcol = Fs * (1.0 / F_COL);
	float Flumlp = Fs * (1.0 / F_LUMA_LP);
	float n = floor(uv.x * BUFFER_WIDTH);
	
	if (rrend != 0x09300){
		Fcol = Fs * (1.0 / F_COL_DX9);
		Flumlp = Fs * (1.0 / F_LUMA_LP_DX9);
	}

	float2 _uv = uv * ReShade::ScreenSize;

	float luma = sample2D(sArtifactColors_Demodulator, ReShade::ScreenSize, _uv).x;
	float2 chroma = 0.0;

	//Filtering out unwanted high frequency content from the chroma(IQ) signal
	[unroll]
	for (int i = 0; i < FIR_SIZE; ++i) {
		int tpidx = FIR_SIZE - i -1;
		float lp = Lowpass(Flumlp, Fs, FIR_SIZE, tpidx);
		chroma += sample2D(sArtifactColors_Demodulator, ReShade::ScreenSize, _uv - float2(i - FIR_SIZE / 2, 0)).yz * lp;
	}
		

	//chroma *= rotate(tau * HUE);
	chroma = MUL(chroma, rotate(tau * HUE));

	float3 color = MUL(yiq2rgb, float3(BRIGHTNESS * luma, chroma * SATURATION));
	
	if (Viewmode == 0) {
		return float4(color, 0.0);
	} else if (Viewmode == 1) {
		return tex2D(sArtifactColors_Modulator, uv).xxxx;
	} else if (Viewmode == 2){
		return float4(luma.xxx, 0.0);
	} else if (Viewmode == 3){
		return float4(40.0 * chroma + 0.5, 0.0, 0.0);
	} else if (Viewmode == 4){
		return 0.5 * tex2D(sArtifactColors_Demodulator, uv).xxxx + 0.25;
	} else if (Viewmode == 5){
		if (uv.x < 0.5)
			return tex2D(sArtifactColors_Modulator, uv).xxxx;
		else
			return float4(color, 0.0);
	} else {
		return 0;
	}
}

//Technique//////////////////////////////////////////////////////////////////////////////////////

technique ArtifactColors {
	pass Resize {
		VertexShader = PostProcessVS;
		PixelShader = PS_Stock;
		RenderTarget = ArtifactChannel0_tex;
	}
	pass Modulator {
		VertexShader = PostProcessVS;
		PixelShader = PS_Modulator;
		RenderTarget = tArtifactColors_Modulator;
	}
	pass Demodulator {
		VertexShader = PostProcessVS;
		PixelShader = PS_Demodulator;
		RenderTarget = tArtifactColors_Demodulator;
	}
	pass Final {
		VertexShader = PostProcessVS;
		PixelShader = PS_Final;
	}
}
