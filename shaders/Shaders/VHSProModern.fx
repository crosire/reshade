/*
	VHS Pro by Vladmir Storm
	
	Ported by Marty McFly and Matsilagi	
*/

#include "ReShade.fxh"

#pragma reshade showstatistics;

uniform float screenLinesNum <
	ui_type = "drag";
	ui_min = "1.0";
	ui_max = "(float)BUFFER_HEIGHT";
	ui_label = "Screen Resolution [VHSPro]";
	ui_tooltip = "Screen Resolution (in lines).\nChange screenLinesRes in Preprocessor Definitions to have the same value as this.";
> = (float)BUFFER_HEIGHT;

uniform bool VHS_Bleed <
	ui_type = "boolean";
	ui_label = "Bleeding [VHSPro]";
	ui_tooltip = "Enables beam screen bleeding (makes the image blurry).";
> = true;

uniform int VHS_BleedMode <
	ui_type = "combo";
	ui_items = "Three Phase\0Old Three Phase\0Two Phase (slow)\0Three-Phase (RetroArch)\0Two Phase (RetroArch)\0";
	ui_label = "Bleeding Mode [VHSPro]";
	ui_tooltip = "Toggles between different bleeding modes.";
> = 0;

uniform float bleedAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "15.0";
	ui_label = "Bleed Stretch [VHSPro]";
	ui_tooltip = "Length of the bleeding.";
> = 1.0;

uniform bool VHS_FishEye <
	ui_type = "boolean";
	ui_label = "Fisheye [VHSPro]";
	ui_tooltip = "Enables a CRT Curvature.";
> = true;

uniform bool VHS_FishEye_Hyperspace <
	ui_type = "bool";
	ui_label = "Fisheye Hyperspace [VHSPro]";
	ui_tooltip = "Changes the curvature to look like some sort of hyperspace warping.";
> = false;

uniform float fisheyeBend <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "50.0";
	ui_label = "Fisheye Bend [VHSPro]";
	ui_tooltip = "Curvature of the CRT.";
> = 2.0;

uniform float cutoffX <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "50.0";
	ui_label = "Fisheye Cutoff X [VHSPro]";
	ui_tooltip = "Cutoff of the Horizontal Borders.";
> = 2.0;

uniform float cutoffY <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "50.0";
	ui_label = "Fisheye Cutoff Y [VHSPro]";
	ui_tooltip = "Cutoff of the Vertical Borders.";
> = 3.0;

uniform float cutoffFadeX <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "50.0";
	ui_label = "Fisheye Cutoff Fade X [VHSPro]";
	ui_tooltip = "Size of the Horizontal gradient cutoff.";
> = 25.0;

uniform float cutoffFadeY <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "50.0";
	ui_label = "Fisheye Cutoff Fade Y [VHSPro]";
	ui_tooltip = "Size of the Vertical gradient cutoff.";
> = 25.0;

uniform bool VHS_Vignette <
	ui_type = "bool";
	ui_label = "Vignette [VHSPro]";
	ui_tooltip = "Enables screen vignetting";
> = false;

uniform float vignetteAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Vignette Amount [VHSPro]";
	ui_tooltip = "Strength of the vignette.";
> = 1.0;

uniform float vignetteSpeed <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Vignette Pulse Speed [VHSPro]";
	ui_tooltip = "Speed of the vignette pulsing. (Setting it to 0 makes it stop pulsing)";
> = 1.0;

uniform float noiseLinesNum <
	ui_type = "drag";
	ui_min = "1.0";
	ui_max = "(float)BUFFER_HEIGHT";
	ui_label = "Vertical Resolution [VHSPro]";
	ui_tooltip = "Noise Resolution (in lines).\nChange noiseLinesRes in Preprocessor Definitions to have the same value as this.";
> = 240.0;

uniform float noiseQuantizeX <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.0";
	ui_label = "Quantize Noise X [VHSPro]";
	ui_tooltip = "Makes the noise longer or shorter.";
> = 0.0;

uniform bool VHS_FilmGrain <
	ui_type = "boolean";
	ui_label = "Film Grain [VHSPro]";
	ui_tooltip = "Enables a Film Grain on the screen.";
> = false;

uniform float filmGrainAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "0.1";
	ui_label = "Film Grain Alpha [VHSPro]";
	ui_tooltip = "Intensity of the Film Grain.";
> = 0.016;

uniform bool VHS_YIQNoise <
	ui_type = "boolean";
	ui_label = "Signal Noise [VHSPro]";
	ui_tooltip = "Adds noise to the YIQ Signal, causing a Pink (or green) noise.";
> = true;

uniform int signalNoiseType <
	ui_type = "combo";
	ui_items = "Type 1\0Type 2\0Type 3\0";
	ui_label = "Signal Noise Type [VHS Pro]";
> = 0;

uniform float signalNoiseAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.0";
	ui_label = "Signal Noise Amount [VHSPro]";
	ui_tooltip = "Amount of the signal noise.";
> = 0.15;

uniform float signalNoisePower <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.0";
	ui_label = "Signal Noise Power [VHSPro]";
	ui_tooltip = "Power of the signal noise. Higher values will make it green, lower values will make it more pink.";
> =  0.83;

uniform bool VHS_LineNoise <
	ui_type = "boolean";
	ui_label = "Line Noise [VHSPro]";
	ui_tooltip = "Enables blinking line noise in the image.";
> = true;

uniform float lineNoiseAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "10.0";
	ui_label = "Line Noise Amount [VHSPro]";
	ui_tooltip = "Intensity of the line noise.";
> = 1.0;

uniform float lineNoiseSpeed <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "10.0";
	ui_label = "Line Noise Speed [VHSPro]";
	ui_tooltip = "Speed of the line noise blinking delay.";
> = 5.0;

uniform bool VHS_TapeNoise <
	ui_type = "boolean";
	ui_label = "Tape Noise [VHSPro]";
	ui_tooltip = "Adds scrolling noise like in old VHS Tapes.";
> = true;

uniform float tapeNoiseTH <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.5";
	ui_label = "Tape Noise Amount [VHSPro]";
	ui_tooltip = "Intensity of Tape Noise in the image.";
> = 0.63;

uniform float tapeNoiseAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.5";
	ui_label = "Tape Noise Alpha [VHSPro]";
	ui_tooltip = "Amount of Tape Noise in the image.";
> = 1.0;

uniform float tapeNoiseSpeed <
	ui_type = "drag";
	ui_min = "-1.5";
	ui_max = "1.5";
	ui_label = "Tape Noise Speed [VHSPro]";
	ui_tooltip = "Scrolling speed of the Tape Noise.";
> = 1.0;

uniform bool VHS_ScanLines <
	ui_type = "boolean";
	ui_label = "Scanlines [VHSPro]";
	ui_tooltip = "Enables TV/CRT Scanlines";
> = false;

uniform float scanLineWidth <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "20.0";
	ui_label = "Scanlines Width [VHSPro]";
	ui_tooltip = "Width of the Scanlines";
> = 10.0;

uniform bool VHS_LinesFloat	<
	ui_type = "boolean";
	ui_label = "Lines Float [VHSPro]";
	ui_tooltip = "Makes the lines of the screen floats up or down. Works best with low Screen Lines resolutions.";
> = false;

uniform float linesFloatSpeed <
	ui_type = "drag";
	ui_min = "-3.0";
	ui_max = "3.0";
	ui_label = "Lines Float Speed [VHSPro]";
	ui_tooltip = "Speed (and direction) of the floating lines.";
> = 1.0;

uniform bool VHS_Stretch <
	ui_type = "boolean";
	ui_label = "Stretch Noise [VHSPro]";
	ui_tooltip = "Enables a stretching noise that scrolls up and down on the Image, simulating magnetic interference of VHS tapes.";
> = true;

uniform bool VHS_Jitter_H <
	ui_type = "boolean";
	ui_label = "Interlacing [VHSPro]";
	ui_tooltip = "Enables Interlacing.";
> = true;

uniform float jitterHAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Interlacing Amount [VHSPro]";
	ui_tooltip = "Strength of the Interlacing.";
> = 0.5;

uniform bool VHS_Jitter_V <
	ui_type = "boolean";
	ui_label = "Jitter [VHSPro]";
	ui_tooltip = "Adds vertical jittering noise.";
> = false;

uniform float jitterVAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "15.0";
	ui_label = "Jitter Amount [VHSPro]";
	ui_tooltip = "Amount of the vertical jittering noise.";
> = 1.0; 

uniform float jitterVSpeed <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Jitter Speed [VHSPro]";
	ui_tooltip = "Speed of the vertical jittering noise.";
> = 1.0;

uniform bool VHS_Twitch_H <
	ui_type = "boolean";
	ui_label = "Horizontal Twitch [VHSPro]";
	ui_tooltip = "Makes the image twitches horizontally in certain timed intervals.";
> = false;

uniform float twitchHFreq <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Horizontal Twitch Frequency [VHSPro]";
	ui_tooltip = "Frequency of time in which the image twitches horizontally.";
> = 1.0;

uniform bool VHS_Twitch_V <
	ui_type = "boolean";
	ui_label = "Vertical Twitch [VHSPro]";
	ui_tooltip = "Makes the image twitches vertically in certain timed intervals.";
> = false;

uniform float twitchVFreq <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "5.0";
	ui_label = "Vertical Twitch Frequency [VHSPro]";
	ui_tooltip = "Frequency of time in which the image twitches vertically.";
> = 1.0;

uniform bool VHS_SignalTweak <
	ui_type = "boolean";
	ui_label = "Signal Tweak [VHSPro]";
	ui_tooltip = "Tweak the values of the YIQ signal.";
> = false;

uniform float signalAdjustY <
	ui_type = "drag";
	ui_min = "-0.25";
	ui_max = "0.25";
	ui_label = "Signal Shift Y [VHSPro]";
	ui_tooltip = "Shifts/Tweaks the Luma part of the signal.";
> = 0.0;
uniform float signalAdjustI <
	ui_type = "drag";
	ui_min = "-0.25";
	ui_max = "0.25";
	ui_label = "Signal Shift I [VHSPro]";
	ui_tooltip = "Shifts/Tweaks the Chroma part of the signal.";
> = 0.0;

uniform float signalAdjustQ <
	ui_type = "drag";
	ui_min = "-0.25";
	ui_max = "0.25";
	ui_label = "Signal Shift Q [VHSPro]";
	ui_tooltip = "Shifts/Tweaks the Chroma part of the signal.";
> = 0.0;

uniform float signalShiftY <
	ui_type = "drag";
	ui_min = "-2.0";
	ui_max = "2.0";
	ui_label = "Signal Adjust Y [VHSPro]";
	ui_tooltip = "Adjusts the Luma part of the signal.";
> = 1.0;

uniform float signalShiftI <
	ui_type = "drag";
	ui_min = "-2.0";
	ui_max = "2.0";
	ui_label = "Signal Adjust I [VHSPro]";
	ui_tooltip = "Adjusts the Chroma part of the signal.";
> = 1.0;

uniform float signalShiftQ <
	ui_type = "drag";
	ui_min = "-2.0";
	ui_max = "2.0";
	ui_label = "Signal Adjust Q [VHSPro]";
	ui_tooltip = "Adjusts the Chroma part of the signal.";
> = 1.0;

uniform float gammaCorection <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "2.0";
	ui_label = "Signal Gamma Correction [VHSPro]";
	ui_tooltip = "Gamma corrects the image.";
> = 1.0;

uniform bool VHS_Feedback <
	ui_type = "boolean";
	ui_label = "Phosphor Trails [VHSPro]";
	ui_tooltip = "Enables phosphor-trails from old CRT monitors.";
> = false;

uniform float feedbackAmount <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "3.0";
	ui_label = "Input Amount [VHSPro]";
	ui_tooltip = "Amount of Phosphor Trails.";
> = 2.0;

uniform float feedbackFade <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.0";
	ui_label = "Phosphor Fade [VHSPro]";
	ui_tooltip = "Fade-time of the phosphor-trails.";
> = 0.82;

uniform float feedbackThresh <
	ui_type = "drag";
	ui_min = "0.0";
	ui_max = "1.0";
	ui_label = "Input Cutoff [VHSPro]";
	ui_tooltip = "Cutoff of the trail.";
> = 0.1;

uniform float3 feedbackColor <
	ui_type = "color";
	ui_label = "Phosphor Trail Color [VHSPro]";
	ui_tooltip = "Color of the trail.";
> = float3(1.0,0.5,0.0);

uniform bool feedbackDebug <
	ui_type = "bool";
	ui_label = "Debug Trail [VHS Pro]";
	ui_tooltip = "Enables the visualization of the phosphor-trails only.";
> = false;

uniform int VHS_Filter <
	ui_type = "drag";
	ui_min ="0.0";
	ui_max ="0.0";
	ui_label = "Linear Filtering [VHSPro]";
	ui_tooltip = "Filters the image linearly, increasing quality.\nDefine VHSLINEARFILTER in Preprocessor Definitions to take effect, this is only here as a reminder.";
> = 0.0;

//textures and samplers
#ifndef screenLinesRes
	#define screenLinesRes (float)BUFFER_HEIGHT	//Screen Resolution (to use in _TapeTex, has to be the same as screenLinesNum)
#endif

#ifndef noiseLinesRes
	#define noiseLinesRes 240	//Vertical Resolution (to use in _TapeTex, has to be the same as noiseLinesNum)
#endif

#ifdef VHSLINEARFILTER
	#define VHSFILTERMODE LINEAR
#else
	#define VHSFILTERMODE POINT
#endif

static const float fisheyeSize = 1.2; 					//Size (DO NOT CHANGE)
static const float filmGrainPower = 1.0;				//Film Grain Power (DO NOT CHANGE)
static const float feedbackAmp = 1.0; 					//Feedback Amp (DO NOT CHANGE)

#define _ScreenParams float2(BUFFER_WIDTH, BUFFER_HEIGHT)
#define PixelSize  	float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
uniform float  Timer < source = "timer"; >;

static const float Pi2 = 6.283185307;

#if !defined(__RESHADE__) || __RESHADE__ > 30000
	static const  int TEXHEIGHT = noiseLinesRes ^ ((BUFFER_HEIGHT ^ noiseLinesRes) & -(BUFFER_HEIGHT < noiseLinesRes)); //min(noiseLinesRes, screenLinesRes);
	static const  int TEXWIDTH = (noiseLinesRes ^ ((BUFFER_HEIGHT ^ noiseLinesRes) & -(BUFFER_HEIGHT < noiseLinesRes)))*BUFFER_WIDTH/BUFFER_HEIGHT;
#else
	#define TEXHEIGHT   int(min(noiseLinesRes, screenLinesRes))
	#define TEXWIDTH 	int(((float)TEXHEIGHT*(float)BUFFER_WIDTH/(float)BUFFER_HEIGHT))
#endif

texture2D VHS_InputTexA    { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_InputTexB    { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_FeedbackTexA { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_FeedbackTexB { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D _TapeTex { Width = TEXWIDTH; Height = TEXHEIGHT; Format = RGBA8; };

sampler2D SamplerColorVHS
{
	Texture = ReShade::BackBufferTex;
	MinFilter = VHSFILTERMODE;
	MagFilter = VHSFILTERMODE;
	MipFilter = VHSFILTERMODE;
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler2D SamplerTape
{
	Texture = _TapeTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler2D VHS_InputA    { Texture = VHS_InputTexA; MinFilter = VHSFILTERMODE;  MagFilter = VHSFILTERMODE;  MipFilter = VHSFILTERMODE; };
sampler2D VHS_InputB    { Texture = VHS_InputTexB; MinFilter = VHSFILTERMODE;  MagFilter = VHSFILTERMODE;  MipFilter = VHSFILTERMODE; };
sampler2D VHS_FeedbackA { Texture = VHS_FeedbackTexA; MinFilter = VHSFILTERMODE;  MagFilter = VHSFILTERMODE;  MipFilter = VHSFILTERMODE; };
sampler2D _FeedbackTex { Texture = VHS_FeedbackTexB; MinFilter = VHSFILTERMODE;  MagFilter = VHSFILTERMODE;  MipFilter = VHSFILTERMODE; };

//functions

#define PI 3.14159265359
static const float3 MOD3 = float3(443.8975, 397.2973, 491.1871);

float fmod(float a, float b) {
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float3 bms(float3 c1, float3 c2){ return 1.0- (1.0-c1)*(1.0-c2); }

//turns sth on and off //a - how often 
float onOff(float a, float b, float c, float t)
{
	return step(c, sin(t + a*cos(t*b)));
}

float hash( float n ){ return frac(sin(n)*43758.5453123); }

float hash12(float2 p){
	float3 p3  = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3, p3.yzx + 19.19);
	return frac(p3.x * p3.z * p3.y);
}

float2 hash22(float2 p) {
	float3 p3 = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3.zxy, p3.yzx+19.19);
	return frac(float2((p3.x + p3.y)*p3.z, (p3.x+p3.z)*p3.y));
}

//random hash				
float4 hash42(float2 p)
{				    
	float4 p4 = frac(float4(p.xyxy) * float4(443.8975,397.2973, 491.1871, 470.7827));
	p4 += dot(p4.wzxy, p4 + 19.19);
	return frac(float4(p4.x * p4.y, p4.x*p4.z, p4.y*p4.w, p4.x*p4.w));
}

float niq( in float3 x ){
	float3 p = floor(x);
	float3 f = frac(x);
	f = f*f*(3.0-2.0*f);
	float n = p.x + p.y*57.0 + 113.0*p.z;
	float res = lerp(lerp(	lerp( hash(n+  0.0), hash(n+  1.0),f.x),
				            lerp( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
				            lerp( lerp( hash(n+113.0), hash(n+114.0),f.x),
				            lerp( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
	return res;
}

float filmGrain(float2 uv, float t, float c )
{ 					
//cheap noise - is ok atm
	float nr = hash12( uv + 0.07*frac( t ) );
	return nr*nr*nr;
}

float2 n4rand_bw( float2 p, float t, float c )
{			    
	t = frac( t );//that's why its sort of twitching 
	float2 nrnd0 = hash22( p + 0.07*t );
	c = 1.0 / (10.0*c); //iMouse.y  / iResolution.y
	nrnd0 = pow(nrnd0, c);				    
	return nrnd0; //TODO try to invert 1-...
}

float scanLines(float2 p, float t)
{
			    		
	//cheap (maybe make an option later)
	// float scanLineWidth = 0.26;
	// float scans = 0.5*(cos((p.y*screenLinesNum+t+.5)*2.0*PI) + 1.0);
	// if(scans>scanLineWidth) scans = 1.; else scans = 0.;			        	

		float t_sl = 0.0;					   	
		//if lines aren't floating -> scanlines also shudn't 
		if (VHS_LinesFloat) {
			t_sl = t*linesFloatSpeed;
		}
			        	
		//expensive but better			        	
		float scans = 0.5*(cos( (p.y*screenLinesNum+t_sl)*2.0*PI) + 1.0);
		scans = pow(scans, scanLineWidth); 
		scans = 1.0 - scans;
		return scans; 
}		

float gcos(float2 uv, float s, float p)
{
	return (cos( uv.y * PI * 2.0 * s + p)+1.0)*0.5;
}	

//mw - maximum width
//wcs = widthChangeSpeed
//lfs = line float speed = .5
//lf phase = line float phase = .0
float2 stretch(float2 uv, float t, float mw, float wcs, float lfs, float lfp){	
   
	float SLN = screenLinesNum; //TODO use only SLN
	//width change
	float tt = t*wcs; //widthChangeSpeed
	float t2 = tt-fmod(tt, 0.5);
					   
	//float dw  = hash42( vec2(0.01, t2) ).x ; //on t and not on y
	float w = gcos(uv, 2.0*(1.0-frac(t2)), PI-t2) * clamp( gcos(uv, frac(t2), t2) , 0.5, 1.0);
	//w = clamp(w,0.,1.);
	w = floor(w*mw)/mw;
	w *= mw;
	//get descreete line number
	float ln = (1.0-frac(t*lfs + lfp)) *screenLinesNum; 
	ln = ln - frac(ln); 
	// float ln = (1.-fmod(t*lfs + lfp, 1.))*SLN; 
	// ln = ln - fmod(ln, 1.); //descreete line number
					   
	//ln = 10.;
	//w = 4.;
					   
	//////stretching part///////
					   
	float oy = 1.0/SLN; //TODO global
	float md = fmod(ln, w); //shift within the wide line 0..width
	float sh2 =  1.0-md/w; // shift 0..1

	// #if VHS_LINESFLOAT_ON
	// 	float sh = fmod(t, 1.);				   	
	//  	uv.y = floor( uv.y *SLN  +sh )/SLN - sh/SLN;
	// #else 
	//  	uv.y = floor( uv.y *SLN  )/SLN;
	//  #endif

	// uv.y = floor( uv.y  *SLN  )/SLN ;
					    
	float slb = SLN / w; //screen lines big        

	//TODO finish
	// #if VHS_LINESFLOAT_ON

		//  if(uv.y<oy*ln && uv.y>oy*(ln-w)) ////if(uv.y>oy*ln && uv.y<oy*(ln+w)) 
		//     uv.y = floor( uv.y*slb +sh2 +sh )/slb - (sh2-1.)/slb - sh/slb;

	//   #else
						   
		if(uv.y<oy*ln && uv.y>oy*(ln-w)) ////if(uv.y>oy*ln && uv.y<oy*(ln+w)) 
			uv.y = floor( uv.y*slb +sh2 )/slb - (sh2-1.0)/slb ;
				      
	// #endif

	return uv;
}

float rnd_rd(float2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt= dot(co.xy ,float2(a,b));
	float sn= fmod(dt,3.14);
	return frac(sin(sn) * c);
}

//DANG WINDOWS
float3 rgb2yiq(float3 c)
{   
	return float3(
	0.2989*c.x + 0.5959*c.y + 0.2115*c.z,
	0.5870*c.x - 0.2744*c.y - 0.5229*c.z,
	0.1140*c.x - 0.3216*c.y + 0.3114*c.z);
};

float3 yiq2rgb(float3 c)
{				
	return float3(
	1.0*c.x +1.0*c.y +1.0*c.z,
	0.956*c.x - 0.2720*c.y - 1.1060*c.z,
	0.6210*c.x - 0.6474*c.y + 1.7046*c.z);
};

//rgb distortion
float3 rgbDistortion(float2 uv,  float magnitude, float t)
{
	magnitude *= 0.0001; // float magnitude = 0.0009;
	float3 offsetX = float3( uv.x, uv.x, uv.x );	
	offsetX.r += rnd_rd(float2(t*0.03,uv.y*0.42)) * 0.001 + sin(rnd_rd(float2(t*0.2, uv.y)))*magnitude;
	offsetX.g += rnd_rd(float2(t*0.004,uv.y*0.002)) * 0.004 + sin(t*9.0)*magnitude;
	// offsetX.b = uv.y + rnd_rd(float2(cos(t*0.01),sin(uv.y)))*magnitude;
	// offsetX.b = uv.y + rand_rd(float2(cos(t*0.01),sin(uv.y)))*magnitude;
					    
	float3 col = float3(0.0, 0.0, 0.0);
	//it cud be optimized / but hm
	col.x = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.r, uv.y) ).rgb ).x;
	col.y = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.g, uv.y) ).rgb ).y;
	col.z = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.b, uv.y) ).rgb ).z;

	col = yiq2rgb(col);
	return col;
}

float rndln(float2 p, float t)
{
	float sample_ln = rnd_rd(float2(1.0,2.0*cos(t))*t*8.0 + p*1.0).x;
	sample_ln *= sample_ln;//*sample;
	return sample_ln;
}

float lineNoise(float2 p, float t)
{				   
	float n = rndln(p* float2(0.5,1.0) + float2(1.0,3.0), t)*20.0;
						
	float freq = abs(sin(t));  //1.
	float c = n*smoothstep(fmod(p.y*4.0 + t/2.0+sin(t + sin(t*0.63)),freq), 0.0,0.95);

	return c;
}


// 3d noise function (iq's)
float n( in float3 x )
{
	float3 p = floor(x);
	float3 f = frac(x);
	f = f*f*(3.0-2.0*f);
	float n = p.x + p.y*57.0 + 113.0*p.z;
	float res = lerp(lerp(lerp( hash(n+0.0), hash(n+1.0),f.x),
	lerp( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
	lerp( lerp( hash(n+113.0), hash(n+114.0),f.x),
	lerp( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
	return res;
}

float tapeNoiseLines(float2 p, float t){

//so atm line noise is depending on hash for int values
//i gotta rewrite to for hash for 0..1 values 
//then i can use normilized p for generating lines

	float y = p.y*_ScreenParams.y;
	float s = t*2.0;
	return  	(niq( float3(y*0.01 +s, 			1.0, 1.0) ) + 0.0)
				*(niq( float3(y*0.011+1000.0+s,	1.0, 1.0) ) + 0.0) 
				*(niq( float3(y*0.51+421.0+s, 	1.0, 1.0) ) + 0.0)   
				;
}


float tapeNoise(float nl, float2 p, float t){

	//TODO custom adjustable density (Probability distribution)
	// but will be expensive (atm its ok)

	//atm its just contrast noise 
				   
	//this generates noise mask
	float nm = 	hash12( frac(p+t*float2(0.234,0.637)) ) 
				// *hash12( frac(p+t*float2(0.123,0.867)) ) 
				// *hash12( frac(p+t*float2(0.441,0.23)) );
									;						
		nm = nm*nm*nm*nm +0.3; //cheap and ok
		//nm += 0.3 ; //just bit brighter or just more to threshold?

		nl*= nm; // put mask
		// nl += 0.3; //Value add .3//

	if(nl<tapeNoiseTH) nl = 0.0; else nl =1.0;  //threshold
	return nl;
}

float2 twitchVertical(float freq, float2 uv, float t){

	float vShift = 0.4*onOff(freq,3.0,0.9, t);
	vShift*=(sin(t)*sin(t*20.0) + (0.5 + 0.1*sin(t*200.0)*cos(t)));
	uv.y = fmod(uv.y + vShift, 1.0); 
	return uv;
}

float2 twitchHorizonal(float freq, float2 uv, float t){
	    				
	float window = 1.0/(1.0+20.0*(uv.y-fmod(t/4.0,1.0))*(uv.y-fmod(t/4.0, 1.0)));
	uv.x += sin(uv.y*10.0 + t)/50.0*onOff(freq,4.0,0.3, t)*(1.0+cos(t*80.0))*window;
	return uv;
}


//all that shit is for postVHS"Pro"_First - end

//all that shit is for postVHS"Pro"_Second

//size 1.2, bend 2.
float2 fishEye(float2 uv, float size, float bend)
{
	if (!VHS_FishEye_Hyperspace){
		uv -= float2(0.5,0.5);
		uv *= size*(1.0/size+bend*uv.x*uv.x*uv.y*uv.y);
		uv += float2(0.5,0.5);
	}
						
			if (VHS_FishEye_Hyperspace){

				//http://paulbourke.net/miscellaneous/lenscorrection/
				float mx = bend/50.0;

				float2 p = (uv*_ScreenParams.xy) /_ScreenParams.x ;
				float prop = _ScreenParams.x / _ScreenParams.y;
				float2 m = float2(0.5, 0.5 / prop);	
				float2 d = p - m;	
				float r = sqrt(dot(d, d));
				float bind;

				float power = ( 2.0 * 3.141592 / (2.0 * sqrt(dot(m, m))) ) *
				(mx - 0.5); //amount of effect

				if (power > 0.0) bind = sqrt(dot(m, m));//stick to corners
				else {if (prop < 1.0) bind = m.x; else bind = m.x;}//stick to borders

				if (power > 0.0) //fisheye
					uv = m + normalize(d) * tan(r * power) * bind / tan( bind * power);
				else if (power < 0.0) //antifisheye
					uv = m + normalize(d) * atan(r * -power * 10.0) * bind / atan(-power * bind * 10.0);
				else uv = p; 

			uv.y *=  prop;
		}

	//adjust size
	// uv -= float2(0.5,0.5);
	// uv *= size;
	// uv += float2(0.5,0.5);

	return uv;
}

//pulse vignette
float vignette(float2 uv, float t)
{
	float vigAmt = 2.5+0.1*sin(t + 5.0*cos(t*5.0));
	float c = (1.0-vigAmt*(uv.y-0.5)*(uv.y-0.5))*(1.0-vigAmt*(uv.x-0.5)*(uv.x-0.5));
	c = pow(c, vignetteAmount); //expensive!
	return c;
}

float3 t2d(float2 p)
{
	float3 col = tex2D (SamplerColorVHS, p ).rgb;
	return rgb2yiq( col );
}

float3 yiqDist(float2 uv, float m, float t)
{	
						m *= 0.0001; // float m = 0.0009;
						float3 offsetX = float3( uv.x, uv.x, uv.x );	

						offsetX.r += rnd_rd(float2(t*0.03, uv.y*0.42)) * 0.001 + sin(rnd_rd(float2(t*0.2, uv.y)))*m;
						offsetX.g += rnd_rd(float2(t*0.004,uv.y*0.002)) * 0.004 + sin(t*9.0)*m;
						// offsetX.b = uv.y + rnd_rd(float2(cos(t*0.01),sin(uv.y)))*m;
						// offsetX.b = uv.y + rand_rd(float2(cos(t*0.01),sin(uv.y)))*m;
					    
					   float3 signal = float3(0.0, 0.0, 0.0);
					   //it cud be optimized / but hm
					   signal.x = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.r, uv.y) ).rgb ).x;
					   signal.y = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.g, uv.y) ).rgb ).y;
					   signal.z = rgb2yiq( tex2D( SamplerColorVHS, float2(offsetX.b, uv.y) ).rgb ).z;

					   // signal = yiq2rgb(col);
						return signal;    
}

#define fixCoord (p - float2( 0.5 * PixelSize.x, .0)) 
#define fetch_offset(offset, one_x) t2d(fixCoord + float2( (offset) * (ONE_X), 0.0));

/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float4 PS_VHS1(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
	float t = Timer.x * 0.001;//_Time.y;							
	float2 p = txcoord.xy;
	
	float SLN = screenLinesNum; //TODO use only SLN
	float SLN_Noise = noiseLinesNum; //TODO only SLN_Noise
	float ONE_X = 0.0;
	float ONE_Y = 0.0;

	//basically if its 0 -> set it to fullscreen
	//TODO calc it before shader / already float done
	SLN = screenLinesNum; //TODO use only SLN
	SLN_Noise = noiseLinesNum; //TODO only SLN_Noise
	if(SLN==0.0) SLN = _ScreenParams.y;

	if(SLN_Noise==0 || SLN_Noise>SLN) SLN_Noise = SLN;									
					
	ONE_X = 1.0/_ScreenParams.x; //assigning works only here 
	ONE_Y = 1.0/_ScreenParams.y; 					
					

	if (VHS_Twitch_V){
		p = twitchVertical(0.5*twitchVFreq, p, t); 
	}

	if (VHS_Twitch_H){
		p = twitchHorizonal(0.1*twitchHFreq, p, t);
	}	

	//make discrete lines /w or wo float 
	if(VHS_LinesFloat){
		float sh = frac(-t*linesFloatSpeed); //shift  // float sh = fmod(t, 1.); //shift
		// if(p.x>0.5)
		p.y = -floor( -p.y * SLN + sh )/SLN + sh/SLN;  //v1.3
		// p.y = floor( p.y * SLN + sh )/SLN - sh/SLN; //v1.2
	} else {
		// if(p.x>0.5)
		p.y = -floor( -p.y * SLN )/SLN;  //v1.3
		// p.y = floor( p.y * SLN )/SLN; //v1.2
	}
					
	if (VHS_Stretch){
		p = stretch(p, t, 15.0, 1.0, 0.5, 0.0);
		p = stretch(p, t, 8.0, 1.2, 0.45, 0.5);
		p = stretch(p, t, 11.0, 0.5, -0.35, 0.25); //up   
	}

	if (VHS_Jitter_H){
		if( fmod( p.y * SLN, 2.0)<1.0) 
		p.x += ONE_X*sin(t*13000.0)*jitterHAmount;
	}
	
	//just init	
	float3 col = float3(0.0,0.0,0.0);
	float3 signal = float3(0.0,0.0,0.0);// rgb2yiq(col);

	//gotta initiate all these things here coz of tape noise distortion

	//[NOISE uv init]
	//if SLN_Noise different from SLN->recalc linefloat 			   	
	float2 pn = p;
	if(SLN!=SLN_Noise){
		if(VHS_LineNoise){
			float sh = frac(t); //shift  // float sh = fmod(t, 1.); //shift
			pn.y = floor( pn.y * SLN_Noise + sh )/SLN_Noise - sh/SLN_Noise;
		} else  {
			pn.y = floor( pn.y * SLN_Noise )/SLN_Noise;
		}				 
	}  	

	//SLN_X is quantization of X. goest from _ScreenParams.x to SLN_X
	float ScreenLinesNumX = SLN_Noise * _ScreenParams.x / _ScreenParams.y;
	float SLN_X = noiseQuantizeX*(_ScreenParams.x - ScreenLinesNumX) + ScreenLinesNumX;
	pn.x = floor( pn.x * SLN_X )/SLN_X;

	float2 pn_ = pn*_ScreenParams.xy;

	//TODO probably it shud be 1.0/SLN_Noise
	float ONEXN = 1.0/SLN_X;
	//[//noise uv init]

	float distShift = 0; // for 2nd part of tape noise

	if (VHS_TapeNoise) {

		//uv distortion part of tapenoise
		int distWidth = 20; 
		float distAmount = 4.0;
		float distThreshold = 0.55;
		distShift = 0; // for 2nd part of tape noise 
		for (int ii = 0; ii < distWidth % 1023; ii++){

			//this is t.n. line value at pn.y and down each pixel
			//TODO i guess ONEXN shud be 1.0/sln noise							
			float tnl = tex2Dlod(SamplerTape, float4(0.0,pn.y-ONEXN*ii, 0.0, 0.0)).y;
			// float tnl = tex2D(SamplerTape, float2(0.0,pn.y-ONEXN*ii)).y;
			// float tnl = tapeNoiseLines(float2(0.0,pn.y-ONEXN*i), t*tapeNoiseSpeed)*tapeNoiseAmount;

			// float fadediff = hash12(float2(pn.x-ONEXN*i,pn.y)); 
			if(tnl>distThreshold) {							
					//TODO get integer part other way
					float sh = sin( 1.0*PI*(float(ii)/float(distWidth))) ; //0..1								
					p.x -= float(int(sh)*distAmount*ONEXN); //displacement
					distShift += sh ; //for 2nd part
					// p.x +=  ONEXN * float(int(((tnl-thth)/thth)*distAmount));
					// col.x = sh;	
				}
			}
		}

	//uv transforms over

	//picture proccess start
	if (VHS_Jitter_V){
		signal = yiqDist(p, jitterVAmount, t*jitterVSpeed);
	} else {
		col = tex2D(SamplerColorVHS, p).rgb;
		// col = float3(p.xy, 0.0);//debug
		signal = rgb2yiq(col);
	}


	if (VHS_LineNoise || VHS_FilmGrain){
		signal.x += tex2D(SamplerTape, pn).z;
	}
					   
	//iq noise from yiq
	if (VHS_YIQNoise){
		if (signalNoiseType == 0) {
						//TODO make cheaper noise 						
						//type 1 (best) w Y mask
						float2 noise = n4rand_bw( pn_,t,1.0-signalNoisePower ) ; 
						signal.y += (noise.x*2.0-1.0)*signalNoiseAmount*signal.x;
						signal.z += (noise.y*2.0-1.0)*signalNoiseAmount*signal.x;
				} else if (signalNoiseType == 1){
						//type 2
						float2 noise = n4rand_bw( pn_,t, 1.0-signalNoisePower ) ; 
						signal.y += (noise.x*2.0-1.0)*signalNoiseAmount;
						signal.z += (noise.y*2.0-1.0)*signalNoiseAmount;
				} else {
						//type 3
						float2 noise = n4rand_bw( pn_,t, 1.0-signalNoisePower )*signalNoiseAmount ; 
						signal.y *= noise.x;
						signal.z *= noise.y;	
						signal.x += (noise.x*2.0-1.0)*0.05;
				}
			}

	//2nd part with noise, tail and yiq col shift
	if (VHS_TapeNoise){

	//here is normilized p (0..1)
	float tn = tex2D(SamplerTape, pn).x;
	signal.x = bms(signal.x, tn*tapeNoiseAmount );  
	// float tn = tapeNoise(pn, t*tapeNoiseSpeed)*tapeNoiseAmount;

	//tape noise tail
	int tailLength=10; //TODO adjustable

	for(int j = 0; j < tailLength % 1023; j++){

		float jj = float(j);
		float2 d = float2(pn.x-ONEXN*jj,pn.y);
		tn = tex2Dlod(SamplerTape, float4(d,0.0,0.0) ).x;
		// tn = tex2D(SamplerTape, d).x;
		// tn = tapeNoise(float2(pn.x-ONEXN*i,pn.y), t*tapeNoiseSpeed)*tapeNoiseAmount;

		float fadediff = 0;

		//for tails length difference
		if(__RENDERER__ == 0x0A100 || __RENDERER__ == 0x0B000) {
			fadediff = tex2Dlod(SamplerTape, float4(d,0.0,0.0)).a; //hash12(d);
		}

		if(__RENDERER__ == 0x09300 || __RENDERER__ >= 0x10000) {
			fadediff = tex2D(SamplerTape, d).a; //hash12(d);
		}

		if( tn > 0.8 ){								
			float nsx =  0.0; //new signal x
			float newlength = float(tailLength)*(1.0-fadediff); //tail lenght diff
			if( jj <= newlength ) nsx = 1.0-( jj/ newlength ); //tail
			signal.x = bms(signal.x, nsx*tapeNoiseAmount);									
		}				
	}

	//tape noise color shift
	if(distShift>0.4){
		// float tnl = tapeNoiseLines(float2(0.0,pn.y), t*tapeNoiseSpeed)*tapeNoiseAmount;
		float tnl = tex2D(SamplerTape, pn).y;//tapeNoiseLines(float2(0.0,pn.y), t*tapeNoiseSpeed)*tapeNoiseAmount;
		signal.y *= 1.0/distShift;//tnl*0.1;//*distShift;//*signal.x;
		signal.z *= 1.0/distShift;//*distShift;//*signal.x;

		}
}

	//back to rgb color space
	//signal has negative values
	col = yiq2rgb(signal);

	//TODO put it into 2nd pass
	if (VHS_ScanLines){
		col *= scanLines(txcoord.xy, t); 						
	}

	//fisheye cutoff / outside fisheye
	//helps to remove noise outside the fisheye
	if (VHS_FishEye){

		float cof_x = cutoffFadeX;
		float cof_y = cutoffFadeY;


		p = txcoord.xy;

		float far;
		float2 hco = float2(ONE_X*cutoffX, ONE_Y*cutoffY); //hard cuttoff x
		float2 sco = float2(ONE_X*cutoffFadeX, ONE_Y*cutoffFadeY); //soft cuttoff x

		//hard cutoff
		if( p.x<=(0.0+hco.x) || p.x>=(1.0-hco.x) || p.y<=(0.0+hco.y) || p.y>=(1.0-hco.y) ){
			col = float3(0.0,0.0,0.0);
		} else { 
			//fades
							
			if( //X
				(p.x>(0.0+hco.x) && p.x<(0.0+(sco.x+hco.x) )) || (p.x>(1.0-(sco.x+hco.x)) && p.x<(1.0-hco.x)) 
			){	
				if(p.x<0.5)	far = (0.0-hco.x+p.x)/(sco.x);									
				else			
				far = (1.0-hco.x-p.x)/(sco.x);
								
				col *= float(far).xxx;
			}; 

				if( //Y
					(p.y>(0.0+hco.y) 			 && p.y<(0.0+(sco.y+hco.y) )) || (p.y>(1.0-(sco.y+hco.y)) && p.y<(1.0-hco.y)) 
				){
					if(p.y<0.5)	far = (0.0-hco.y+p.y)/(sco.y);									
					else
					far = (1.0-hco.y-p.y)/(sco.y);
								
					col *= float(far).xxx;
				}
			}
		}
		
	// col = tex2D(SamplerTape, txcoord.xy).x;

	return float4(col, 1.0); 

}

float4 PS_VHS2(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
	float t = Timer.x * 0.001;//_Time.y;					
	float2 p = txcoord.xy;	
	float SLN = screenLinesNum; //TODO use only SLN

	//basically if its 0 -> set it to fullscreen
	if(SLN==0.0) SLN = _ScreenParams.y;

	 //TODO maybe make it based on num of lines? and height ? 
	//TODO make switch between real pixels and pixelated pixels!
    // ONE_X = 1.0 / screenLinesNum;

	float ONE_X = 1.0 / _ScreenParams.x;  // 1px
	float ONE_Y = 1.0 / _ScreenParams.y;
    ONE_X *= bleedAmount; // longer tails, more bleeding, default 1.				    

	//distortions
	if (VHS_FishEye){
		p = fishEye(p, fisheyeSize, fisheyeBend); // p = fishEye(p, 1.2, 2.0);
		// return float4( frac(p*5.0)/2.5, 1.0, 1.0); //debug
	}

	int bleedLength = 21;

	if((VHS_BleedMode == 0 || VHS_BleedMode == 1 || VHS_BleedMode == 3))
	{
		bleedLength = 25;
	}
					  
	if((VHS_BleedMode == 2 || VHS_BleedMode == 4))
	{
		bleedLength = 32;
	}
	 
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
	
	if (VHS_BleedMode == 0 || VHS_BleedMode == 3){ //Three Phase and Three Phase (RetroArch)
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
	}
	
	else if (VHS_BleedMode == 1) { //Old Three-Phase
		luma_filter[0] = -0.000071070;
		luma_filter[1] = -0.000032816;
		luma_filter[2] = 0.000128784;
		luma_filter[3] = 0.000134711;
		luma_filter[4] = -0.000226705;
		luma_filter[5] = -0.000777988;
		luma_filter[6] = -0.000997809;
		luma_filter[7] = -0.000522802;
		luma_filter[8] = 0.000344691;
		luma_filter[9] = 0.000768930;
		luma_filter[10] = 0.000275591;
		luma_filter[11] = -0.000373434;
		luma_filter[12] = 0.000522796;
		luma_filter[13] = 0.003813817;
		luma_filter[14] = 0.007502825;
		luma_filter[15] = 0.006786001;
		luma_filter[16] = -0.002636726;
		luma_filter[17] = -0.019461182;
		luma_filter[18] = -0.033792479;
		luma_filter[19] = -0.029921972;
		luma_filter[20] = 0.005032552;
		luma_filter[21] = 0.071226466;
		luma_filter[22] = 0.151755921;
		luma_filter[23] = 0.218166470;
		luma_filter[24] = 0.243902439;
			
		chroma_filter[0] = 0.001845562;
		chroma_filter[1] = 0.002381606;
		chroma_filter[2] = 0.003040177;
		chroma_filter[3] = 0.003838976;
		chroma_filter[4] = 0.004795341;
		chroma_filter[5] = 0.005925312;
		chroma_filter[6] = 0.007242534;
		chroma_filter[7] = 0.008757043;
		chroma_filter[8] = 0.010473987;
		chroma_filter[9] = 0.012392365;
		chroma_filter[10] = 0.014503872;
		chroma_filter[11] = 0.016791957;
		chroma_filter[12] = 0.019231195;
		chroma_filter[13] = 0.021787070;
		chroma_filter[14] = 0.024416251;
		chroma_filter[15] = 0.027067414;
		chroma_filter[16] = 0.029682613;
		chroma_filter[17] = 0.032199202;
		chroma_filter[18] = 0.034552198;
		chroma_filter[19] = 0.036677005;
		chroma_filter[20] = 0.038512317;
		chroma_filter[21] = 0.040003044;
		chroma_filter[22] = 0.041103048;
		chroma_filter[23] = 0.041777517;
		chroma_filter[24] = 0.042004791;
	}
	
	else if (VHS_BleedMode == 2) { //Two-Phase
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
	
	else if (VHS_BleedMode == 4) { //Two-Phase (RetroArch)
		luma_filter[0] =  -0.000174844;
		luma_filter[1] =  -0.000205844;
		luma_filter[2] =  -0.000149453;
		luma_filter[3] =  -0.000051693;
		luma_filter[4] = 0.000000000;
		luma_filter[5] =  -0.000066171;
		luma_filter[6] =  -0.000245058;
		luma_filter[7] =  -0.000432928;
		luma_filter[8] =  -0.000472644;
		luma_filter[9] =  -0.000252236;
		luma_filter[10] =  0.000198929;
		luma_filter[11] =  0.000687058;
		luma_filter[12] =  0.000944112;
		luma_filter[13] =  0.000803467;
		luma_filter[14] =  0.000363199;
		luma_filter[15] =  0.000013422;
		luma_filter[16] =  0.000253402;
		luma_filter[17] =  0.001339461;
		luma_filter[18] =  0.002932972;
		luma_filter[19] =  0.003983485;
		luma_filter[20] =  0.003026683;
		luma_filter[21] =  -0.001102056;
		luma_filter[22] =  -0.008373026;
		luma_filter[23] =  -0.016897700;
		luma_filter[24] =  -0.022914480;
		luma_filter[25] =  -0.021642347;
		luma_filter[26] =  -0.008863273;
		luma_filter[27] =  0.017271957;
		luma_filter[28] =  0.054921920;
		luma_filter[29] =  0.098342579;
		luma_filter[30] =  0.139044281;
		luma_filter[31] =  0.168055832;
		luma_filter[32] =  0.178571429;
			
		chroma_filter[0] =   0.001384762;
		chroma_filter[1] =   0.001678312;
		chroma_filter[2] =   0.002021715;
		chroma_filter[3] =   0.002420562;
		chroma_filter[4] =   0.002880460;
		chroma_filter[5] =   0.003406879;
		chroma_filter[6] =   0.004004985;
		chroma_filter[7] =   0.004679445;
		chroma_filter[8] =   0.005434218;
		chroma_filter[9] =   0.006272332;
		chroma_filter[10] =   0.007195654;
		chroma_filter[11] =   0.008204665;
		chroma_filter[12] =   0.009298238;
		chroma_filter[13] =   0.010473450;
		chroma_filter[14] =   0.011725413;
		chroma_filter[15] =   0.013047155;
		chroma_filter[16] =   0.014429548;
		chroma_filter[17] =   0.015861306;
		chroma_filter[18] =   0.017329037;
		chroma_filter[19] =   0.018817382;
		chroma_filter[20] =   0.020309220;
		chroma_filter[21] =   0.021785952;
		chroma_filter[22] =   0.023227857;
		chroma_filter[23] =   0.024614500;
		chroma_filter[24] =   0.025925203;
		chroma_filter[25] =   0.027139546;
		chroma_filter[26] =   0.028237893;
		chroma_filter[27] =   0.029201910;
		chroma_filter[28] =   0.030015081;
		chroma_filter[29] =   0.030663170;
		chroma_filter[30] =   0.031134640;
		chroma_filter[31] =   0.031420995;
		chroma_filter[32] =   0.031517031;
	}


	//Bleeding
	float3 signal = float3(0.0,0.0,0.0);

	if (VHS_Bleed){
						
		float3 norm = 	float3(0.0,0.0,0.0);
		float3 adj = 	float3(0.0,0.0,0.0);
		float maxTexLength = 50.0; //maximum texture length
		
		int taps = bleedLength-4;
		//int taps = bleedLength; //RetroArch

		for (int ii = 0; ii < taps % 1023; ii++){

			float offset = float(ii);
			float3 sums = 	fetch_offset(offset - float(taps), ONE_X) +
							fetch_offset(float(taps) - offset, ONE_X) ;

			adj = float3(luma_filter[ii+3], chroma_filter[ii], chroma_filter[ii]);
			//adj = float3(luma_filter[ii], chroma_filter[ii], chroma_filter[ii]); //RetroArch

			signal += sums * adj;
			norm += adj;
					        
		}

		adj = float3(luma_filter[taps], chroma_filter[taps], chroma_filter[taps]);

		signal += t2d(fixCoord) * adj;
		norm += adj;
		signal = signal / norm;
	} else {
	    //no bleeding
	    signal = t2d(p);
	}

	//[Signal Tweak]
	if (VHS_SignalTweak){
					    
		//adjust
		signal.x += signalAdjustY; 
		signal.y += signalAdjustI; 
		signal.z += signalAdjustQ; 

		//shift
		signal.x *= signalShiftY; 
		signal.y *= signalShiftI; 
		signal.z *= signalShiftQ; 

		// //test YIQ
		// signal.x = 0.5;
		// signal.y = p.x*2.0-1.0;
		// signal.z = p.y*2.0-1.0;

		//signal noise
		// float2 noise = n4rand_bw( p,t, 1.0-signalNoisePower )*signalNoiseAmount ; 
		// signal.y *= noise.x;
		// signal.z *= noise.y;


		//phase - it cant be dont coz of intervals
		// signal.x = frac( (signal.x +1.0)*0.5f )*2.0f - 1.0f ;
		//signal.y = frac( (signal.y +1.0)*0.5f )*2.0f - 1.0f ;
		//signal.z = frac( (signal.z +1.0)*0.5f )*2.0f - 1.0f ;
	}
				    
	float3 rgb = yiq2rgb(signal);					
				   
	if (VHS_SignalTweak){
		if(gammaCorection!=1.0) rgb = pow(rgb, gammaCorection); //float3(gammaCorection).rgb
	}

	//cut trash after fish eye
	// if( p.x<0. || p.x>1. || p.y<0. || p.y>1.){
	// 	rgb *= 0.0;
	// }

	//TODO maybe on yiq channel				    
	if (VHS_Vignette){
		rgb *= vignette(p, t*vignetteSpeed); //TODO params //txcoord.xy
	}

	return float4(rgb, 1.0); 
}

//third pass

float3 bm_screen(float3 a, float3 b){ 	return 1.0- (1.0-a)*(1.0-b); }

void PS_VHS3(float4 vpos     : SV_Position, out float4 feedbackoutput   : SV_Target0,     
            float2 texcoord : TEXCOORD,    out float4 feedback : SV_Target1 )
{
	float2 p = texcoord.xy;
	float one_x = 1.0/_ScreenParams.x;
    float3 fc = tex2D( SamplerColorVHS, texcoord).rgb;    // curent frame
	
    //new feedback
    float3 fl = tex2D( VHS_InputB, texcoord).rgb;    //last frame without feedback
    float  diff = abs(fl.x-fc.x + fl.y-fc.y + fl.z-fc.z)/3.0; //dfference between frames
    if(diff<feedbackThresh) diff = 0.0;
    float3 fbn = fc*diff*feedbackAmount; //feedback new
    // fbn = float3(0.0, 0.0, 0.0);

    //feedback buffer
    float3 fbb = tex2D( _FeedbackTex, texcoord).rgb; 
	fbb = ( //blur old bufffer a bit
			tex2D( _FeedbackTex, texcoord).rgb + 
			tex2D( _FeedbackTex, texcoord + float2(one_x, 0.0)).rgb + 
			tex2D( _FeedbackTex, texcoord - float2(one_x, 0.0)).rgb
		) / 3.0;
	fbb *= feedbackFade;

    //blend
	fc = bm_screen(fc, fbb);
	fbn = bm_screen(fbn, fbb); //feedback
    feedbackoutput = float4(fc, 1.0); 
    feedback = float4(fbn * feedbackColor, 1.0);
}

void PS_VHS_Lastframe(float4 vpos     : SV_Position, out float4 feedbackoutput   : SV_Target0, 
                      float2 texcoord : TEXCOORD,    out float4 feedback : SV_Target1)
{
    feedbackoutput   = tex2D( VHS_InputA,    texcoord);
    feedback = tex2D( VHS_FeedbackA, texcoord);
}

float4 PS_VHS4(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{

	float2 p = texcoord;

	float3 col = tex2D( SamplerColorVHS, texcoord).rgb; //curent frame
	float3 fbb = tex2D( _FeedbackTex, texcoord).rgb; //feedback buffer
	
	if (VHS_Feedback){
		col = bm_screen(col, fbb*feedbackAmp);
		if (feedbackDebug){
			col = fbb;
		}
	}
				   
	return float4(col, 1.0); 
}

float4 PS_VHSTape(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
	float t = Timer.x * 0.001;
	float2 p = txcoord.xy;
	
	p.y=1-p.y;
	
	float2 p_ = p*_ScreenParams;
	
	float ns = 0.0; //signal noise //tex2D(_TapeTex, p);
	float nt = 0.0; //tape noise
	float nl = 0.0; //lines for tape noise
	float ntail = 0.0; //tails values for tape noise 
	
	if (VHS_TapeNoise) {

		//here is normilized p (0..1)
			nl = tapeNoiseLines(p, t*tapeNoiseSpeed)*1.0;//tapeNoiseAmount;
			nt = tapeNoise(nl, p, t*tapeNoiseSpeed)*1.0;//tapeNoiseAmount;
			ntail = hash12(p+ float2(0.01,0.02) );
	}
	
	if (VHS_LineNoise) {
			ns += lineNoise(p_, t*lineNoiseSpeed)*lineNoiseAmount;
			// col += lineNoise(pn_, t*lineNoiseSpeed)*lineNoiseAmount;//i.uvn
	}
			
	//y noise from yiq
	if (VHS_FilmGrain) {
		//cheap n ok atm						
		float bg = filmGrain((p_-0.5*_ScreenParams.xy)*0.5, t, filmGrainPower );
		ns += bg * filmGrainAmount;
	}
		
	return float4(nt,nl,ns,ntail);
}

float4 PS_VHSClear(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
	float3 tex = tex2D(SamplerColorVHS, txcoord);
	return float4(tex, 1.0);  //black
}

/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

technique VHS_Pro
{
	pass VHSPro_First
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_VHS1;
	}
	pass VHSPro_Second
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_VHS2;
	}
	pass VHSPro_Third
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_VHS3;
		RenderTarget0 = VHS_InputTexA;
		RenderTarget1 = VHS_FeedbackTexA;
	}
	pass VHSPro_LastFrame
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_VHS_Lastframe;   
		RenderTarget0 = VHS_InputTexB;
		RenderTarget1 = VHS_FeedbackTexB;
	}
	pass VHSPro_Forth
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_VHS4;
	}
	pass VHSPro_Tape
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_VHSTape;
		RenderTarget = _TapeTex;
	}
	pass VHSPro_Clear
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_VHSClear;
	}
}