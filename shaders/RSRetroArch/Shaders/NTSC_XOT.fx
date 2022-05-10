#include "ReShade.fxh"

//NTSC-XOT by xot
//Using Signal Modulation from Artifact Colors by Flyguy, with some specific options.
//Works OK, as long as you don't tweak too much.
//Debug mode for the filter included for fiddling purposes.

#ifndef ENABLE_FILTER_KNOBS
	#define ENABLE_FILTER_KNOBS 0 //[0 or 1] Enables tweaking of the Filter Values (For debugging purposes)
#endif

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [NTSC-XOT]";
> = BUFFER_WIDTH;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [NTSC-XOT]";
> = BUFFER_HEIGHT;

//  TV adjustments
uniform float SAT <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.1;
	ui_label = "Saturation [NTSC-XOT]";
> = 8.0;

uniform float HUE <
	ui_type = "drag";
	ui_min = -255.0;
	ui_max = 255.0;
	ui_step = 0.1;
	ui_label = "Hue [NTSC-XOT]";
> = 67.7;

uniform float BRI <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.1;
	ui_label = "Brightness [NTSC-XOT]";
> = 1.0;      //  Brightness (normally 1.0)

//NTSC XOT
#define PI   3.14159265358979323846
#define TAU  6.28318530717958647693
#define MUL(A, B) mul(A, B)
#define mod(x,y) (x-y*floor(x/y))
#define display_size float2(display_sizeX,display_sizeY)

texture XOT0_tex {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};

sampler sXOT0 {
	Texture = XOT0_tex;
};

texture XOT1_tex {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = R16F;
};

sampler sXOT1 {
	Texture = XOT1_tex;
};

static const float3x3 rgb2yiq = float3x3(
	0.299, 0.596, 0.211,
	0.587,-0.274,-0.523,
	0.114,-0.322, 0.312
);

//  Filter parameters
#if (ENABLE_FILTER_KNOBS == 0)
	static const int   N   = 18;       //  Filter Width, was 15
	static const int   M   = (N-1)/2;	//  Filter Middle
	static const float FC  = 0.30;     //  Frequency Cutoff, was 0.25
	static const float SCF = 0.25;     //  Subcarrier Frequency
#else

	uniform int F_COL <
		ui_type = "drag";
		ui_min = 1;
		ui_max = 255;
		ui_step = 1;
		ui_label = "Filter Color [NTSC-XOT]";
	> = 4;

	static const int   N_INT   = 15;       //  Filter Width
	
	uniform int N <
		ui_type = "drag";
		ui_min = 0;
		ui_max = 1024;
		ui_step = 1;
		ui_label = "Filter Width [NTSC-XOT]";
	> = 15;
	
	uniform float FC <
		ui_type = "drag";
		ui_min = 0.0;
		ui_max = 10.0;
		ui_step = 0.01;
		ui_label = "Frequency Cutoff [NTSC-XOT]";
	> = 0.25;
	
	uniform float SCF <
		ui_type = "drag";
		ui_min = 0.0;
		ui_max = 10.0;
		ui_step = 0.01;
		ui_label = "Subcarrier Frequency [NTSC-XOT]";
	> = 0.25;
#endif

static const float3x3 rgb2yiq = float3x3(
	0.299, 0.596, 0.211,
	0.587,-0.274,-0.523,
	0.114,-0.322, 0.312
);

static const float3x3 YIQ2RGB = float3x3(1.000, 1.000, 1.000,
                          0.956,-0.272,-1.106,
                          0.621,-0.647, 1.703);

float3 adjust(float3 YIQ, float H, float S, float B) {
    float3x3 M = float3x3(  B,      0.0,      0.0,
                  0.0, S*cos(H),  -sin(H), 
                  0.0,   sin(H), S*cos(H) );
    return mul(M,YIQ);
}

float sinc(float x) {
    if (x == 0.0) return 1.0;
	return sin(PI*x) / (PI*x);
}

//	Hann windowing function
float hann(float n, float N) {
    return 0.5 * (1.0 - cos((TAU*n)/(N-1.0)));
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

//Complex oscillator, Fo = Oscillator freq., Fs = Sample freq., n = Sample index
float2 Oscillator(float Fo, float Fs, float N) {
	float tau_osc = atan(1.0) * 8.0;
	float phase = (tau_osc * Fo * floor(N)) / Fs;
	return float2(cos(phase), sin(phase));
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
	float Fcol = Fs * (1.0 / 4.0);
	
	#if (ENABLE_FILTER_KNOBS == 1)
		Fcol = Fs* (1.0 / F_COL);
	#endif
	
	float n = floor(uv.x * BUFFER_WIDTH);

	float3 cRGB = tex2D(sXOT0, uv).rgb;
	float3 cYIQ = MUL(rgb2yiq, cRGB);

	float2 cOsc = Oscillator(Fcol, Fs, n);

	float sig = cYIQ.x + dot(cOsc, cYIQ.yz);

	return sig;
}

void PS_NTSCXOT(in float pos: SV_POSITION0, in float2 txcoord: TEXCOORD0, out float4 col : COLOR0)
{
	float2 size = ReShade::ScreenSize.xy;
	float2 uv = txcoord.xy;
	
	#if (ENABLE_FILTER_KNOBS == 1)
		int M = (N-1)/2;
	#endif
    
	    //  Compute sampling weights
		#if (ENABLE_FILTER_KNOBS)
			float weights[N_INT];
		#else
			float weights[N];
		#endif
		
    	float sum = 0.0;
		
    	for (int n=0; n<N; n++) {
        	weights[n] = hann(float(n), float(N)) * sinc(FC * float(n-M));
        	sum += weights[n];
    	}
        
        //  Normalize sampling weights
        for (int n=0; n<N; n++) {
            weights[n] /= sum;
        }
        
        //	Sample composite signal and decode to YIQ
        float3 YIQ = float3(0.0,0.0,0.0);
        for (int n=0; n<N; n++) {
            float2 pos = uv + float2(float(n-M) / size.x, 0.0);
	        float phase = TAU * (SCF * size.x * pos.x);
			YIQ += float3(1.0, cos(phase), sin(phase)) * tex2D(sXOT1, pos).rrr * weights[n];
        }
        
        //  Apply TV adjustments to YIQ signal and convert to RGB
        col = float4(mul(YIQ2RGB,adjust(YIQ, HUE, SAT, BRI)), 1.0);

}

technique NTSCXOT
{
	pass Framebuffer_Fetch {
		VertexShader = PostProcessVS;
		PixelShader = PS_Stock;
		RenderTarget = XOT0_tex;
	}
	pass ArtifactColors_Modulator {
		VertexShader = PostProcessVS;
		PixelShader = PS_Modulator;
		RenderTarget = XOT1_tex;
	}

   pass NTSCXOT {
	 VertexShader = PostProcessVS;
	 PixelShader = PS_NTSCXOT;
   }
}