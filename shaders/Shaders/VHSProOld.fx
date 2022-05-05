/*
	VHS Pro (Version 1.1) by Vladmir Storm
	
	Ported by Marty McFly and Matsilagi	
*/

#include "ReShade.fxh";

uniform float screenLinesNum <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = (float)BUFFER_HEIGHT;
	ui_label = "Screen Lines Resolution [VHS Pro 1.1]";
	ui_tooltip = "Changes the Screen Resolution";
> = 240.0;

uniform int VHS_BleedMode <
	ui_type = "combo";
	ui_items = "Three Phase\0Old Three Phase\0Two Phase (slow)\0";
	ui_label = "Bleed Mode [VHS Pro 1.1]";
	ui_tooltip = "Changes the screen bleeding mode.";
> = 2;

uniform float bleedAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Bleed Amount [VHS Pro 1.1]";
	ui_tooltip = "Changes the stretching of the screen bleeding.";
> = 1.0;

uniform bool VHS_FishEye <
	ui_type = "bool";
	ui_label = "Enable FishEye [VHS Pro 1.1]";
	ui_tooltip = "Enables Fish Eye curvature, simulating a CRT Screen.";
> = true;

uniform float fisheyeSize <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 50.0;
	ui_label = "Fish Eye Size [VHS Pro 1.1]";
	ui_tooltip = "Changes the size of the fisheye.";
> = 1.2;

uniform float fisheyeBend <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 50.0;
	ui_label = "Fish Eye Bend [VHS Pro 1.1]";
	ui_tooltip = "Changes the bending of the fisheye.";
> = 1.0;

uniform bool VHS_Vignette <
	ui_type = "bool";
	ui_label = "Vignette [VHS Pro 1.1]";
	ui_tooltip = "Enables vignetting on the screen.";
> = false;

uniform float vignetteAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_label = "Vignette Amount [VHS Pro 1.1]";
	ui_tooltip = "Changes the amount of vignetting.";
> = 0.36;

uniform float vignetteSpeed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_label = "Vignette Speed [VHS Pro 1.1]";
	ui_tooltip = "Changes the blinking speed of the Vignette.";
> = 1.0;

uniform float noiseLinesNum <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = (float)BUFFER_HEIGHT;
	ui_label = "Noise Lines Resolution [VHS Pro 1.1]";
	ui_tooltip = "Changes the Noise resolution";
> = 240.0;

uniform float noiseQuantizeX <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Noise Quantization [VHS Pro 1.1]";
	ui_tooltip = "Changes the Noise Quantization";
> = 1.0;

uniform bool VHS_FilmGrain <
	ui_type = "bool";
	ui_label = "Film Grain [VHS Pro 1.1]";
	ui_tooltip = "Enables FilmGrain";
> = false;

uniform float filmGrainAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 20.0;
	ui_label = "Film Grain Amount [VHS Pro 1.1]";
	ui_tooltip = "Alpha of FilmGrain";
> = 16.0;

uniform bool VHS_TapeNoise <
	ui_type = "bool";
	ui_label = "Tape Noise [VHS Pro 1.1]";
	ui_tooltip = "Enables Scrolling Tape Noise";
> = true;

uniform float tapeNoiseTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.5;
	ui_label = "Tape Noise Alpha [VHS Pro 1.1]";
	ui_tooltip = "Alpha of Tape Noise";
> = 1.05;

uniform float tapeNoiseAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.5;
	ui_label = "Tape Noise Amount [VHS Pro 1.1]";
	ui_tooltip = "Amount of Tape Noise";
> = 1.0;

uniform float tapeNoiseSpeed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.5;
	ui_label = "Tape Noise Speed [VHS Pro 1.1]";
	ui_tooltip = "Speed of the Tape Noise";
> = 1.0;

uniform bool VHS_LineNoise <
	ui_type = "bool";
	ui_label = "Line Noise [VHS Pro 1.1]";
	ui_tooltip = "Enables Line Noise";
> = true;

uniform float lineNoiseAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Line Noise Amount [VHS Pro 1.1]";
	ui_tooltip = "Amount of Line Noise";
> = 1.0;

uniform float lineNoiseSpeed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "Line Noise Speed [VHS Pro 1.1]";
	ui_tooltip = "Speed of Line Noise";
> = 5.0;

uniform bool VHS_ScanLines <
	ui_type = "bool";
	ui_label = "Scan Lines [VHS Pro 1.1]";
> = true;

uniform float scanLineWidth <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 20.0;
	ui_label = "Scan Lines Width [VHS Pro 1.1]";
	ui_tooltip = "Changes the Width of the Scan Lines";
> = 3.0;

uniform bool VHS_LinesFloat <
	ui_type = "bool";
	ui_label = "Floating Lines [VHS Pro 1.1]";
> = false;

uniform bool VHS_Stretch <
	ui_type = "bool";
	ui_label = "Streching Noise [VHS Pro 1.1]";
	ui_tooltip = "Enables Stretching Noise.";
> = true;

uniform bool VHS_Twitch_H <
	ui_type = "bool";
	ui_label = "Horizontal Twitch [VHS Pro 1.1]";
	ui_tooltip = "Enables Horizontal Image twitching.";
> = false;

uniform float twitchHFreq <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Horizontal Twitching Frequency [VHS Pro 1.1]";
	ui_tooltip = "Frequency that the screen twitches. (In seconds)";
> = 1.0;

uniform bool VHS_Twitch_V <
	ui_type = "bool";
	ui_label = "Vertical Twitch [VHS Pro 1.1]";
	ui_tooltip = "Enables Vertical Image twitching.";
> = false;

uniform float twitchVFreq < 
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_label = "Vertical Twitching Frequency [VHS Pro 1.1]";
	ui_tooltip = "Frequency that the screen twitches. (In seconds)";
> = 1.0;

uniform bool VHS_Jitter_H <
	ui_type = "bool";
	ui_label = "Interlacing [VHS Pro 1.1]";
	ui_tooltip = "Enables Interlacing.";
> = true;

uniform float jitterHAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 15.0;
	ui_label = "Interlacing Amount [VHS Pro 1.1]";
	ui_tooltip = "Amount of the interlacing effect.";
> = 0.5;

uniform bool VHS_Jitter_V <
	ui_type = "bool";
	ui_label = "Jittering [VHS Pro 1.1]";
	ui_tooltip = "Enables Jittering.";
> = true;

uniform float jitterVAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 15.0;
	ui_label = "Jittering Amount [VHS Pro 1.1]";
	ui_tooltip = "Amount of the Jittering effect.";
> = 1.0;

uniform float jitterVSpeed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_label = "Jittering Speed [VHS Pro 1.1]";
	ui_tooltip = "Speed of the Jittering effect.";
> = 1.0;

uniform bool VHS_SignalTweak <
	ui_type = "bool";
	ui_label = "Signal Tweak [VHS Pro 1.1]";
	ui_tooltip = "Enables tweaking of the YIQ Signal.";
> = true;

uniform float signalAdjustY <
	ui_type = "drag";
	ui_min = -0.25;
	ui_max = 0.25;
	ui_label = "Signal Shift Y [VHS Pro 1.1]";
	ui_tooltip = "Shifts the Luma part of the Signal.";
> = 0.064;

uniform float signalAdjustI <
	ui_type = "drag";
	ui_min = -0.25;
	ui_max = 0.25;
	ui_label = "Signal Shift I [VHS Pro 1.1]";
	ui_tooltip = "Shifts the Chroma part of the Signal.";
> = 0.0;

uniform float signalAdjustQ <
	ui_type = "drag";
	ui_min = -0.25;
	ui_max = 0.25;
	ui_label = "Signal Shift Q [VHS Pro 1.1]";
	ui_tooltip = "Shifts the Chroma part of the Signal.";
> = 0.0;

uniform float signalShiftY <
	ui_type = "drag";
	ui_min = -2.0;
	ui_max = 2.0;
	ui_label = "Signal Adjust Y [VHS Pro 1.1]";
	ui_tooltip = "Adjust the Luma part of the Signal.";
> = 1.0;

uniform float signalShiftI <
	ui_type = "drag";
	ui_min = -2.0;
	ui_max = 2.0;
	ui_label = "Signal Adjust I [VHS Pro 1.1]";
	ui_tooltip = "Adjusts the Chroma part of the Signal";
> = 1.0;

uniform float signalShiftQ <
	ui_type = "drag";
	ui_min = -2.0;
	ui_max = 2.0;
	ui_label = "Signal Adjust Q [VHS Pro 1.1]";
	ui_tooltip = "Adjusts the Chroma part of the Signal.";
> = 1.0;

uniform float gammaCorection <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_label = "Gamma Adjust [VHS Pro 1.1]";
	ui_tooltip = "Adjusts the Gamma of the image.";
> = 0.97;


uniform bool VHS_Feedback <
	ui_type = "bool";
	ui_label = "Phosphor Trail [VHS Pro 1.1]";
	ui_tooltip = "Enables red phosphor trails.";
> = true;

uniform float feedbackAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_label = "Input Amount [VHS Pro 1.1]";
	ui_tooltip = "Amount of Feedback";
> = 1.85;

uniform float feedbackFade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Input Fade [VHS Pro 1.1]";
	ui_tooltip = "Feedback Fade time.";
> = 0.89;

uniform float feedbackThresh <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Input Cutoff [VHS Pro 1.1]";
	ui_tooltip = "Feedback Cutoff.";
> = 0.23;

uniform bool feedbackDebug <
	ui_type = "bool";
	ui_label = "Debug Trail [VHS Pro 1.1]";
	ui_tooltip = "Enables the visualization of the phosphor-trails only.";
> = false;

/////////////////////////TEXTURES / INTERNAL PARAMETERS/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////TEXTURES / INTERNAL PARAMETERS/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PI 3.14159265359

#ifdef VHSLINEARFILTER
	#define VHSFILTERMODE LINEAR
#else
	#define VHSFILTERMODE POINT
#endif

static const float feedbackAmp = 1.0;

uniform float  Timer < source = "timer"; >;

texture2D VHS_InputTexA    { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_InputTexB    { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_FeedbackTexA { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture2D VHS_FeedbackTexB { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

sampler2D VHS_InputA    { Texture = VHS_InputTexA;    };
sampler2D VHS_InputB    { Texture = VHS_InputTexB;    };
sampler2D VHS_FeedbackA { Texture = VHS_FeedbackTexA; };
sampler2D VHS_FeedbackB { Texture = VHS_FeedbackTexB; };

sampler2D SamplerColorVHS
{
	Texture = ReShade::BackBufferTex;
	MinFilter = VHSFILTERMODE;
	MagFilter = VHSFILTERMODE;
	MipFilter = VHSFILTERMODE;
	AddressU = Clamp;
	AddressV = Clamp;
};

/////////////////////////FUNCTIONS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////FUNCTIONS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//all that shit is for postVHS"Pro"_First


float3 bms(float3 c1, float3 c2)
{ 
	return 1.0- (1.0-c1)*(1.0-c2); 
}

float fmod(float a, float b)
{
  float c = frac(abs(a/b))*abs(b);
  return (a < 0) ? -c : c;   /* if ( a < 0 ) c = 0-c */
}

//turns sth on and off //a - how often 
float onOff(float a, float b, float c, float t)
{
	return step(c, sin(t + a*cos(t*b)));
}

float filmGrain(float2 uv, float t)
{  						
	float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * (t * 10.0);
	x = fmod((fmod(x, 13.0) + 1.0 ) * (fmod(x, 123.0) + 1.0 ), 0.01)-0.005;					   
	return x;
}

float scanLines(float2 p, float t)
{
	//cheap
	// float scanLineWidth = 0.26;
	// float scans = 0.5*(cos((p.y*screenLinesNum+t+.5)*2.0*PI) + 1.0);
	// if(scans>scanLineWidth) scans = 1.; else scans = 0.;			        	
			        	
	//expensive but better			        	
	p.y=1-p.y;
	float scans = 0.5*(cos( (p.y*screenLinesNum+t)*2.0*PI) + 1.0);
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
float2 streatch(float2 uv, float t, float mw, float wcs, float lfs, float lfp){	
					    
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
					   
	 //////streatching part///////
					   
	 float oy = 1.0/screenLinesNum; //TODO global
	float md = fmod(ln, w); //shift within the wide line 0..width
	float sh2 =  1.0-md/w; // shift 0..1

	// #if VHS_LINESFLOAT_ON
	// 	float sh = (t % 1.);				   	
	//  	uv.y = floor( uv.y *SLN  +sh )/SLN - sh/SLN;
	// #else 
	//  	uv.y = floor( uv.y *SLN  )/SLN;
	//  #endif

	// uv.y = floor( uv.y  *SLN  )/SLN ;
					    
	float slb = screenLinesNum / w; //screen lines big        

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
	col.x = rgb2yiq( tex2D(  SamplerColorVHS, float2(offsetX.r, uv.y) ).rgb ).x;
	col.y = rgb2yiq( tex2D(  SamplerColorVHS, float2(offsetX.g, uv.y) ).rgb ).y;
	col.z = rgb2yiq( tex2D(  SamplerColorVHS, float2(offsetX.b, uv.y) ).rgb ).z;

	col = yiq2rgb(col);
	return col;
}

float rnd_tn(float2 p, float t)
{
	float sample2 = rnd_rd(float2(1.0,2.0*cos(t))*t*8.0 + p*1.0).x;
	sample2 *= sample2;
	return sample2;
}

float lineNoise(float2 p, float t)
{
					   
	float n = rnd_tn(p* float2(0.5,1.0) + float2(1.0,3.0), t)*20.0;
						
	float freq = abs(sin(t));  //1.
	float c = n*smoothstep(fmod(p.y*4.0 + t/2.0+sin(t + sin(t*0.63)),freq), 0.0,0.95);
	return c;
}

//random hash				
float4 hash42(float2 p)
{				    
	float4 p4 = frac(float4(p.xyxy) * float4(443.8975,397.2973, 491.1871, 470.7827));
	p4 += dot(p4.wzxy, p4 + 19.19);
	return frac(float4(p4.x * p4.y, p4.x*p4.z, p4.y*p4.w, p4.x*p4.w));
}


float hash( float n ){ return frac(sin(n)*43758.5453123); }


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

//my tape noise
float tapeNoise(float2 p, float t){

	float tapeNoiseVA = 0.3; 
	float tapeNoiseGC = 1.0;
	float y = p.y;

	if (__RENDERER__ == 0x09300 || __RENDERER__ == 0x0A100 || __RENDERER__ == 0x0B000){
		y = 1-p.y;
	}
	
	float s = t*2.0;
					    
	float v = (n( float3(y*0.01 +s, 1.0, 1.0) ) + 0.0)
		*(n( float3(y*0.011+1000.0+s, 1.0, 1.0) ) + 0.0) 
		*(n( float3(y*0.51+421.0+s, 	1.0, 1.0) ) + 0.0);
					   
	//v*= n( float3( (fragCoord.xy + float2(s,0.))*100.,1.0) );
	float nm = hash42( float2(p.x +t*0.01, p.y) ).x +0.3 ; //noise mask
	v*= nm;

	//TODO pow is expensive ->gotta check
	v += tapeNoiseVA; //Value add .3//
	v = pow(v, tapeNoiseGC); //gamma correction
	if(v<tapeNoiseTH) v = 0.0;  //threshold // if(v<.7) v = 0.;  
	// //remap to 0..1
	// float a = tapeNoiseVA;
	// float b = 1+tapeNoiseVA;
	// v *= clamp( (nm-a)/b , 0., 1.);

	return v;
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
	uv -= float2(0.5,0.5);
	uv *= size*(1.0/size+bend*uv.x*uv.x*uv.y*uv.y);
	uv += float2(0.5,0.5);
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
	float3 col = tex2D ( SamplerColorVHS, p ).rgb;
	return rgb2yiq( col );
}

#define fixCoord (p - float2( 0.5 * ReShade::PixelSize.x, .0)) 
#define fetch_offset(offset, one_x) t2d(fixCoord + float2( (offset) * (ONE_X), 0.0));

/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////PIXEL SHADERS//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


float4 PS_VHS1(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{  
	float t = Timer.x * 0.001;
	float2 p = txcoord.xy;
	
	if (__RENDERER__ == 0x09300 || __RENDERER__ == 0x0A100 || __RENDERER__ == 0x0B000){
		p.y=1-p.y;
	}

	if (VHS_Twitch_V){
		p = twitchVertical(0.5*twitchVFreq, p, t); //twitch / shift
	}

	if (VHS_Twitch_H) {
		p = twitchHorizonal(0.1*twitchHFreq, p, t);
	}

	if (VHS_LinesFloat){
		float sh = frac(t); //shift  // float sh = fmod(t, 1.); //shift
		p.y = floor( p.y * screenLinesNum + sh )/screenLinesNum - sh/screenLinesNum;
	} else {
		p.y = floor( p.y * screenLinesNum )/screenLinesNum;
	}

	if (VHS_Stretch) {
		p = streatch(p, t, 15.0, 1.0, 0.5, 0.0);
		p = streatch(p, t, 8.0, 1.2, 0.45, 0.5);
		p = streatch(p, t, 11.0, 0.5, -0.35, 0.25); //up   
	}

	if (VHS_Jitter_H) {
		if( fmod( p.y * screenLinesNum, 2.0)<1.0) p.x += ReShade::PixelSize.x*sin(t*13000.0)*jitterHAmount;
	}

	float3 col = float3(0.0,0.0,0.0);


		if( p.x>(0.0+ReShade::PixelSize.x*2.0)  && p.x<(1.0-ReShade::PixelSize.x*2.0) && p.y>(0.0+ReShade::PixelSize.x*3.0) && p.y<(1.0-ReShade::PixelSize.x*2.0) )
		{
		p.y=1-p.y;
			if (VHS_Jitter_V) {	
				col = rgbDistortion(p, jitterVAmount, t*jitterVSpeed);
			} else {
				col = tex2D( SamplerColorVHS, p).rgb;
			}

			float2 pn = p;
			if(screenLinesNum!=noiseLinesNum)
			{
				 if (VHS_LinesFloat) {
					float sh = frac(t); //shift  // float sh = fmod(t, 1.); //shift
					pn.y = floor( pn.y * noiseLinesNum + sh )/noiseLinesNum - sh/noiseLinesNum;
				 } else {
					pn.y = floor( pn.y * noiseLinesNum )/noiseLinesNum;
				}			 
			}  

			float ScreenLinesNumX = noiseLinesNum * (float)BUFFER_WIDTH / (float)BUFFER_HEIGHT;
			float SLN_X = noiseQuantizeX*((float)BUFFER_WIDTH - ScreenLinesNumX) + ScreenLinesNumX;
			pn.x = floor( pn.x * SLN_X )/SLN_X;

			float2 pn_ = pn*float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT);
	
			if (VHS_FilmGrain) {						
				float bg = filmGrain((pn_-0.5*float2((float)BUFFER_WIDTH,(float)BUFFER_HEIGHT))*0.5, t );
				col = bms(col, float3(bg,bg,bg)*filmGrainAmount); 
			}

			if (VHS_TapeNoise) {
				col = bms(col, tapeNoise(pn_, t*tapeNoiseSpeed)*tapeNoiseAmount);  
			}
						
			if (VHS_LineNoise) {
				col += lineNoise(pn_, t*lineNoiseSpeed)*lineNoiseAmount;//i.uvn
			}

			if (VHS_ScanLines) {
				float t_sl = t;				   	

				if (VHS_LinesFloat) {
				 //No idea why its empty
				} else {
					t_sl = 0.0;
				}
			col *= scanLines(txcoord.xy, t_sl);	
		}
	}
	return float4(col, 1.0);
}


float4 PS_VHS2(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{  

	float t = Timer.x * 0.001;
	float2 p = txcoord.xy;

	if (VHS_FishEye) {
		p = fishEye(p, fisheyeSize, fisheyeBend); // p = fishEye(p, 1.2, 2.0);
	}

	float ONE_X = ReShade::PixelSize.x * bleedAmount;

	int bleedLength = 25;

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
	
	if (VHS_BleedMode == 0){ //Three Phase and Three Phase (RetroArch)
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

	float3 signal = float3(0.0,0.0,0.0);
	float3 norm = 	float3(0.0,0.0,0.0);
	float3 adj = 	float3(0.0,0.0,0.0);
	float maxTexLength = 50.0; //maximum texture length

	int taps = bleedLength-4;
	for (int ii = 0; ii < taps; ii++)
	{
		float offset = float(ii);
		float3 sums = 	fetch_offset(offset - float(taps), ONE_X) +
		fetch_offset(float(taps) - offset, ONE_X) ;

		adj = float3(luma_filter[ii+3], chroma_filter[ii], chroma_filter[ii]);

		signal += sums * adj;
		norm += adj;
				        
	}

	adj = float3(luma_filter[taps], chroma_filter[taps], chroma_filter[taps]);

	signal += t2d(fixCoord) * adj;
	norm += adj;
	signal = signal / norm;

	//[Signal Tweak]
	if (VHS_SignalTweak) {
					    
		//adjust
		signal.x += signalAdjustY; 
		signal.y += signalAdjustI; 
		signal.z += signalAdjustQ; 

		//shift
		signal.x *= signalShiftY; 
		signal.y *= signalShiftI; 
		signal.z *= signalShiftQ; 

	}
	
	float3 rgb = yiq2rgb(signal);

	if (VHS_SignalTweak) {
		if(gammaCorection!=1.0) rgb = pow(rgb, gammaCorection); //half3(gammaCorection).rgb
	}

 	if (VHS_Vignette){
		rgb *= vignette(p, t*vignetteSpeed); //TODO params //i.uvn
	}


	return float4(rgb, 1.0);
}

//third pass

float3 bm_screen(float3 a, float3 b){ 	return 1.0- (1.0-a)*(1.0-b); }

void PS_VHS3(float4 vpos     : SV_Position, out float4 feedbackoutput   : SV_Target0,     
            float2 texcoord : TEXCOORD,    out float4 feedback : SV_Target1 )
{
    float3 color     = tex2D(  SamplerColorVHS, texcoord).rgb;    // curent frame

	float3 feedbackColor = float3(0.0,0.0,0.0);
	feedbackColor = float3(1.0,0.5,0.0);
	
    //new feedback
    float3 lastframe = tex2D( VHS_InputB, texcoord).rgb;    //last frame without feedback
    float  diff      = dot(abs(lastframe - color), 0.3333); //dfference between frames
    diff             = (diff < feedbackThresh)? 0.0: diff;
    float3 fbn       = color * diff * feedbackAmount;       //feedback new
    // fbn = float3(0.0, 0.0, 0.0);

    //feedback buffer
    float3 fbb = tex2D( VHS_FeedbackB, texcoord).rgb; 
    fbb       += tex2D( VHS_FeedbackB, texcoord + float2(ReShade::PixelSize.x, 0.0)).rgb + 
                 tex2D( VHS_FeedbackB, texcoord - float2(ReShade::PixelSize.x, 0.0)).rgb;
    fbb       /= 3.0;
    fbb        = bm_screen(fbn, fbb * feedbackFade) * feedbackColor;    //feedback

    //blend
    color    = bm_screen(color, fbb * feedbackAmp);
//    color = color + fbb * feedbackAmp;
    feedbackoutput   = float4( color, 1.0); 
    feedback = float4( fbb,   1.0);
}

void PS_VHS_Lastframe(float4 vpos     : SV_Position, out float4 feedbackoutput   : SV_Target0, 
                      float2 texcoord : TEXCOORD,    out float4 feedback : SV_Target1)
{
    feedbackoutput   = tex2D( VHS_InputA,    texcoord);
    feedback = tex2D( VHS_FeedbackA, texcoord);
}

float4 PS_VHS4(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	if (VHS_Feedback) {
		if (feedbackDebug){
			return tex2D( VHS_FeedbackB, texcoord); } else 
			{ return tex2D( VHS_InputB, texcoord); }
		} else { return tex2D( SamplerColorVHS, texcoord); }
}

float4 PS_VHSClear(float4 pos : SV_Position, float2 txcoord : TEXCOORD0) : SV_Target
{
	float3 tex = tex2D(SamplerColorVHS, txcoord);
	return float4(tex, 1.0);  //black
}

/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////TECHNIQUES/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

technique VHS_Pro_Old
{
	pass HVS1
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_VHS1;
	}
	pass HVS2
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_VHS2;
	}
    pass HVS3
    {
        VertexShader  = PostProcessVS;
        PixelShader   = PS_VHS3;
        RenderTarget0 = VHS_InputTexA;
        RenderTarget1 = VHS_FeedbackTexA;
    }
    pass PS_VHS_Lastframe
    {
        VertexShader  = PostProcessVS;
        PixelShader   = PS_VHS_Lastframe;   
        RenderTarget0 = VHS_InputTexB;
        RenderTarget1 = VHS_FeedbackTexB;
    }
    pass HVS4
    {
        VertexShader  = PostProcessVS;
        PixelShader   = PS_VHS4;
    }
	pass HVSClear
	{
		VertexShader  = PostProcessVS;
		PixelShader   = PS_VHSClear;
	}
}