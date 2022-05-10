#include "ReShade.fxh"

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width (NTSC)";
> = 320.0;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height (NTSC)";
> = 240.0;

uniform bool bUseTwoPhase <
	ui_label = "Use Two-Phase (NTSC)";
> = false;

uniform bool bUseSVideo <
	ui_label = "Use S-Video (NTSC)";
	ui_tooltip = "Uses S-Video Cables instead of Composite.";
> = false;

uniform float NTSC_DISPLAY_GAMMA <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.1;
	ui_label = "NTSC Display Gamma (NTSC)";
> = 2.1;

uniform float NTSC_CRT_GAMMA <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.1;
	ui_label = "NTSC CRT Gamma (NTSC)";
> = 2.5;

#define PI 3.14159265

#define CHROMA_MOD_FREQ (PI / 3.0)
#define CHROMA_MOD_FREQ_TWO (4.0 * PI / 15.0)

#define SATURATION 1.0
#define BRIGHTNESS 1.0
#define ARTIFACTING 1.0
#define FRINGING 1.0

#define ARTIFACTING_SV 0.0
#define FRINGING_SV 0.0

#define display_size int2(display_sizeX,display_sizeY)

static const float3x3 mix_mat = float3x3(
      BRIGHTNESS, ARTIFACTING, ARTIFACTING,
      FRINGING, 2.0 * SATURATION, 0.0,
      FRINGING, 0.0, 2.0 * SATURATION 
);

static const float3x3 mix_mat_sv = float3x3(
      BRIGHTNESS, ARTIFACTING_SV, ARTIFACTING_SV,
      FRINGING_SV, 2.0 * SATURATION, 0.0,
      FRINGING_SV, 0.0, 2.0 * SATURATION 
);

static const float3x3 yiq2rgb_mat = float3x3(
   1.0, 0.956, 0.6210,
   1.0, -0.2720, -0.6474,
   1.0, -1.1060, 1.7046);

float3 yiq2rgb(float3 yiq)
{
   return mul(yiq2rgb_mat, yiq);
}

static const float3x3 yiq_mat = float3x3(
      0.2989, 0.5870, 0.1140,
      0.5959, -0.2744, -0.3216,
      0.2115, -0.5229, 0.3114
);

float3 rgb2yiq(float3 col)
{
   return mul(yiq_mat, col);
};

float fmod(float a, float b) {
    float c = frac(abs(a / b)) * abs(b);
    return (a < 0) ? -c : c;
};

texture target0_ntsc
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};
sampler s0 { Texture = target0_ntsc; };

uniform int framecount < source = "framecount"; >;

float4 NTSCStartPS( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	return tex2D(s0,texcoord);
}

float4 NTSCEncodePS( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float3 col = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float3 yiq = rgb2yiq(col);
	
	float2 pix_no = texcoord * display_size * float2( 4.0, 1.0 );
	float2 one = 1.0 / ReShade::ScreenSize;
	
	float chroma_phase = 0.6667 * PI * (fmod(pix_no.y, 3.0) + framecount);
	float mod_phase = chroma_phase + pix_no.x * CHROMA_MOD_FREQ;
	
	if (bUseTwoPhase){
		chroma_phase = PI * (fmod(pix_no.y, 2.0) + framecount);
		mod_phase = chroma_phase + pix_no.x * CHROMA_MOD_FREQ_TWO;
	}

	float i_mod = cos(mod_phase);
	float q_mod = sin(mod_phase);
	
	if(bUseSVideo){
		yiq.yz *= float2(i_mod, q_mod); // Modulate.
		yiq = mul(mix_mat_sv,yiq); // Cross-talk.
		yiq.yz *= float2(i_mod, q_mod); // Demodulate.
	} else {
		yiq.yz *= float2(i_mod, q_mod); // Modulate.
		yiq = mul(mix_mat,yiq); // Cross-talk.
		yiq.yz *= float2(i_mod, q_mod); // Demodulate.
	}
	
	return float4(yiq, 1.0);
}

float3 t2d(float2 texcoord)
{
	return tex2D(s0,texcoord).rgb;
}

#define fixCoord (texcoord - float2( 0.5 * one_x, 0.0)) 
#define fetch_offset(offset, one_x) t2d(fixCoord + float2( (offset) * (one_x), 0.0))

float4 NTSCDecodePS( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	int TAPS = 24;
	
	float luma_filter[33] = {
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000
	};
	
	float chroma_filter[33] = {
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000,
		0.000000000
	};
	
	luma_filter[0] =	-0.000012020;
	luma_filter[1] =	-0.000022146;
	luma_filter[2] =	-0.000013155;
	luma_filter[3] =	-0.000012020;
	luma_filter[4] =	-0.000049979;
	luma_filter[5] =	-0.000113940;
	luma_filter[6] =	-0.000122150;
	luma_filter[7] =	-0.000005612;
	luma_filter[8] =	0.000170516;
	luma_filter[9] =	0.000237199;
	luma_filter[10] =	0.000169640;
	luma_filter[11] =	0.000285688;
	luma_filter[12] =	0.000984574;
	luma_filter[13] =	0.002018683;
	luma_filter[14] =	0.002002275;
	luma_filter[15] =	-0.000909882;
	luma_filter[16] =	-0.007049081;
	luma_filter[17] =	-0.013222860;
	luma_filter[18] =	-0.012606931;
	luma_filter[19] =	0.002460860;
	luma_filter[20] =	0.035868225;
	luma_filter[21] =	0.084016453;
	luma_filter[22] =	0.135563500;
	luma_filter[23] =	0.175261268;
	luma_filter[24] =	0.190176552;

	chroma_filter[0] =	-0.000118847;
	chroma_filter[1] =	-0.000271306;
	chroma_filter[2] =	-0.000502642;
	chroma_filter[3] =	-0.000930833;
	chroma_filter[4] =	-0.001451013;
	chroma_filter[5] =	-0.002064744;
	chroma_filter[6] =	-0.002700432;
	chroma_filter[7] =	-0.003241276;
	chroma_filter[8] =	-0.003524948;
	chroma_filter[9] =	-0.003350284;
	chroma_filter[10] =	-0.002491729;
	chroma_filter[11] =	-0.000721149;
	chroma_filter[12] =	0.002164659;
	chroma_filter[13] =	0.006313635;
	chroma_filter[14] =	0.011789103;
	chroma_filter[15] =	0.018545660;
	chroma_filter[16] =	0.026414396;
	chroma_filter[17] =	0.035100710;
	chroma_filter[18] =	0.044196567;
	chroma_filter[19] =	0.053207202;
	chroma_filter[20] =	0.061590275;
	chroma_filter[21] =	0.068803602;
	chroma_filter[22] =	0.074356193;
	chroma_filter[23] =	0.077856564;
	chroma_filter[24] =	0.079052396;
	
	if (bUseTwoPhase){
		
		TAPS = 32;
	
		luma_filter[0] =	-0.000205844;
		luma_filter[1] =	-0.000149453;
		luma_filter[2] =	-0.000051693;
		luma_filter[3] =	0.000000000;
		luma_filter[4] =	-0.000066171;
		luma_filter[5] =	-0.000245058;
		luma_filter[6] =	-0.000432928;
		luma_filter[7] =	-0.000472644;
		luma_filter[8] =	-0.000252236;
		luma_filter[9] =	0.000198929;
		luma_filter[10] =	0.000687058;
		luma_filter[11] =	0.000944112;
		luma_filter[12] =	0.000803467;
		luma_filter[13] =	0.000363199;
		luma_filter[14] =	0.000013422;
		luma_filter[15] =	0.000253402;
		luma_filter[16] =	0.001339461;
		luma_filter[17] =	0.002932972;
		luma_filter[18] =	0.003983485;
		luma_filter[19] =	0.003026683;
		luma_filter[20] =	-0.001102056;
		luma_filter[21] =	-0.008373026;
		luma_filter[22] =	-0.016897700;
		luma_filter[23] =	-0.022914480;
		luma_filter[24] =	-0.021642347;
		luma_filter[25] =	-0.008863273;
		luma_filter[26] =	0.017271957;
		luma_filter[27] =	0.054921920;
		luma_filter[28] =	0.098342579;
		luma_filter[29] =	0.139044281;
		luma_filter[30] =	0.168055832;
		luma_filter[31] =	0.178571429;
			
		chroma_filter[0] =	0.001384762;
		chroma_filter[1] =	0.001678312;
		chroma_filter[2] =	0.002021715;
		chroma_filter[3] =	0.002420562;
		chroma_filter[4] =	0.002880460;
		chroma_filter[5] =	0.003406879;
		chroma_filter[6] =	0.004004985;
		chroma_filter[7] =	0.004679445; 
		chroma_filter[8] =	0.005434218;
		chroma_filter[9] =	0.006272332; 
		chroma_filter[10] =	0.007195654; 
		chroma_filter[11] =	0.008204665; 
		chroma_filter[12] =	0.009298238; 
		chroma_filter[13] =	0.010473450;
		chroma_filter[14] =	0.011725413; 
		chroma_filter[15] =	0.013047155;
		chroma_filter[16] =	0.014429548;
		chroma_filter[17] =	0.015861306; 
		chroma_filter[18] =	0.017329037; 
		chroma_filter[19] =	0.018817382;
		chroma_filter[20] =	0.020309220; 
		chroma_filter[21] =	0.021785952;
		chroma_filter[22] =	0.023227857;
		chroma_filter[23] =	0.024614500; 
		chroma_filter[24] =	0.025925203;
		chroma_filter[25] =	0.027139546; 
		chroma_filter[26] =	0.028237893;
		chroma_filter[27] =	0.029201910; 
		chroma_filter[28] =	0.030015081; 
		chroma_filter[29] =	0.030663170;
		chroma_filter[30] =	0.031134640;
		chroma_filter[31] =	0.031420995; 
		chroma_filter[32] =	0.031517031;
	}
	
	float one_x = 1.0 / ReShade::ScreenSize.x;
	one_x *= 0.5;
	
	float3 signal = float3(0.0,0.0,0.0);
	for (int i = 0; i < TAPS; i++)
	{
	   float offset = float(i);

	   float3 sums = fetch_offset(offset - float(TAPS), one_x) +
		  fetch_offset(float(TAPS) - offset, one_x);

	   signal += sums * float3(luma_filter[i], chroma_filter[i], chroma_filter[i]);
	}
	signal += tex2D(s0, texcoord).xyz *
	   float3(luma_filter[TAPS], chroma_filter[TAPS], chroma_filter[TAPS]);
	   
   float3 rgb = yiq2rgb(signal);
   return float4(rgb, 1.0);
}

float4 NTSCGaussPS( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 pix_no = texcoord * display_size * float2( 4.0, 1.0 );
	float2 one = 1.0 / ReShade::ScreenSize;

   #define TEX(off) pow(tex2D(ReShade::BackBuffer, texcoord + float2(0.0, (off) * one.y)).rgb, float3(NTSC_CRT_GAMMA,NTSC_CRT_GAMMA,NTSC_CRT_GAMMA))

   float3 frame0 = TEX(-2.0);
   float3 frame1 = TEX(-1.0);
   float3 frame2 = TEX(0.0);
   float3 frame3 = TEX(1.0);
   float3 frame4 = TEX(2.0);

   float offset_dist = frac(pix_no.y) - 0.5;
   float dist0 =  2.0 + offset_dist;
   float dist1 =  1.0 + offset_dist;
   float dist2 =  0.0 + offset_dist;
   float dist3 = -1.0 + offset_dist;
   float dist4 = -2.0 + offset_dist;

   //return just scanline = frame0 or frame2 to use the gamma controls.
   float3 scanline = frame2;
   
   return float4(pow(1.00 * scanline, float3(1.0 / NTSC_DISPLAY_GAMMA, 1.0 / NTSC_DISPLAY_GAMMA, 1.0 / NTSC_DISPLAY_GAMMA)), 1.0);
}

float4 NTSCFinalPS( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	return tex2D(ReShade::BackBuffer,texcoord);
}


technique NTSC_RetroArch_Gamma
{	
	// first pass: encode s-video signal
	pass p1
	{	
		RenderTarget = target0_ntsc;
		
		VertexShader = PostProcessVS;
		PixelShader = NTSCEncodePS;
	}
	
	// second pass: decode composite signal
	pass p2
	{
		VertexShader = PostProcessVS;
		PixelShader = NTSCDecodePS;
	}
	// third pass: adds gaussian with scanlines
	pass p3
	{
		VertexShader = PostProcessVS;
		PixelShader = NTSCGaussPS;
	}
	// final pass: decode composite signal
	pass p4
	{
		VertexShader = PostProcessVS;
		PixelShader = NTSCFinalPS;
	}
}