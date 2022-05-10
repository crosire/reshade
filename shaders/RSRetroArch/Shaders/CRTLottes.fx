#include "ReShade.fxh"

//
// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER
//
//   by Timothy Lottes
//
// This is more along the style of a really good CGA arcade monitor.
// With RGB inputs instead of NTSC.
// The shadow mask example has the mask rotated 90 degrees for less chromatic aberration.
//
// Left it unoptimized to show the theory behind the algorithm.
//
// It is an example what I personally would want as a display option for pixel art games.
// Please take and use, change, or whatever.
//

uniform float resX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CRT-Lottes]";
> = 320.0;

uniform float resY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CRT-Lottes]";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [CRT-Lottes]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [CRT-Lottes]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform float hardScan <
	ui_type = "drag";
	ui_min = -20.0; ui_max = 0.0;
	ui_step = 1.0;
	ui_tooltip = "Hard Scan [CRT-Lottes]";
> = -8.0;

uniform float hardPix <
	ui_type = "drag";
	ui_min = -20.0; ui_max = 0.0;
	ui_step = 1.0;
	ui_tooltip = "Hard Pixel [CRT-Lottes]";
> = -3.0;

uniform float warpX <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.125;
	ui_step = 0.01;
	ui_tooltip = "Warp X [CRT-Lottes]";
> = 0.031;

uniform float warpY <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.125;
	ui_step = 0.01;
	ui_tooltip = "Warp Y [CRT-Lottes]";
> = 0.041;

uniform float maskDark <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_step = 0.1;
	ui_tooltip = "Mask Dark [CRT-Lottes]";
> = 0.5;

uniform float maskLight <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_step = 0.1;
	ui_tooltip = "Mask Light [CRT-Lottes]";
> = 1.5;

uniform float scaleInLinearGamma <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_step = 1.0;
	ui_tooltip = "Linear Gamma Scale [CRT-Lottes]";
> = 1.0;

uniform int shadowMask <
	ui_type = "drag";
	ui_min = 0; ui_max = 4;
	ui_step = 1.0;
	ui_tooltip = "Shadow Mask Type [CRT-Lottes]";
> = 3;

uniform float brightBoost <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_step = 0.05;
	ui_tooltip = "Brightness [CRT-Lottes]";
> = 1.0;

uniform float hardBloomPix <
	ui_type = "drag";
	ui_min = -2.0; ui_max = -0.5;
	ui_step = 0.1;
	ui_tooltip = "Bloom X Soft [CRT-Lottes]";
> = -1.5;

uniform float hardBloomScan <
	ui_type = "drag";
	ui_min = -4.0; ui_max = -1.0;
	ui_step = 0.1;
	ui_tooltip = "Bloom Y Soft [CRT-Lottes]";
> = -2.0;

uniform float bloomAmt <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_step = 0.05;
	ui_tooltip = "Bloom Amount [CRT-Lottes]";
> = 0.15;

uniform float shape <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 10.0;
	ui_step = 0.05;
	ui_tooltip = "Filter Kernel Shape [CRT-Lottes]";
> = 2.0;

uniform bool doBloom <
	ui_tooltip = "Bloom [CRT-Lottes]";
> = 1;

#define TextureSize float2(resX, resY)
#define warp float2(warpX,warpY)
#define video_size float2(video_sizeX, video_sizeY)

sampler2D CRTBackBuffer
{
	Texture = ReShade::BackBufferTex;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float ToLinear1(float c)
{
   //if (scaleInLinearGamma==0){return c;}
   //return (c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);
   return scaleInLinearGamma==0 ? c : (c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);
}

float3 ToLinear(float3 c)
{
   //if (scaleInLinearGamma==0){return c;}
      //return float3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));
   return scaleInLinearGamma==0 ? c : float3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));
}

float ToSrgb1(float c)
{
   //if (scaleInLinearGamma==0){return c;}
   //return (c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);
   return scaleInLinearGamma==0 ? c : (c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);
}

float3 ToSrgb(float3 c)
{
   //if (scaleInLinearGamma==0){return c;}
   //return float3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));
   return scaleInLinearGamma==0 ? c : float3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));
}

float3 Fetch(float2 pos,float2 off){
  float2 res = TextureSize.xy;
  pos=(floor(pos*res.xy+off)+float2(0.5,0.5))/res.xy;
  if(max(abs(pos.x-0.5),abs(pos.y-0.5))>0.5)return float3(0.0,0.0,0.0);
  return ToLinear(brightBoost * tex2D(CRTBackBuffer,pos.xy).rgb);
}

float2 Dist(float2 pos){
	float2 res = TextureSize.xy;
	
	pos=pos*res; 
	return -((pos-floor(pos))-float2(0.5,0.5));
}

float Gaus(float pos,float scale){return exp2(scale*pow(abs(pos),shape));}

float3 Horz3(float2 pos,float off){
  float3 b=Fetch(pos,float2(-1.0,off));
  float3 c=Fetch(pos,float2( 0.0,off));
  float3 d=Fetch(pos,float2( 1.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  // Return filtered sample.
  return (b*wb+c*wc+d*wd)/(wb+wc+wd);}
  
float3 Horz5(float2 pos,float off){
  float3 a=Fetch(pos,float2(-2.0,off));
  float3 b=Fetch(pos,float2(-1.0,off));
  float3 c=Fetch(pos,float2( 0.0,off));
  float3 d=Fetch(pos,float2( 1.0,off));
  float3 e=Fetch(pos,float2( 2.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wa=Gaus(dst-2.0,scale);
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  float we=Gaus(dst+2.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);}
  
float3 Horz7(float2 pos,float off){
  float3 a=Fetch(pos,float2(-3.0,off));
  float3 b=Fetch(pos,float2(-2.0,off));
  float3 c=Fetch(pos,float2(-1.0,off));
  float3 d=Fetch(pos,float2( 0.0,off));
  float3 e=Fetch(pos,float2( 1.0,off));
  float3 f=Fetch(pos,float2( 2.0,off));
  float3 g=Fetch(pos,float2( 3.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardBloomPix;
  float wa=Gaus(dst-3.0,scale);
  float wb=Gaus(dst-2.0,scale);
  float wc=Gaus(dst-1.0,scale);
  float wd=Gaus(dst+0.0,scale);
  float we=Gaus(dst+1.0,scale);
  float wf=Gaus(dst+2.0,scale);
  float wg=Gaus(dst+3.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we+f*wf+g*wg)/(wa+wb+wc+wd+we+wf+wg);}
  
float Scan(float2 pos,float off){
  float dst=Dist(pos).y;
  return Gaus(dst+off,hardScan);}
  
float BloomScan(float2 pos,float off){
  float dst=Dist(pos).y;
  return Gaus(dst+off,hardBloomScan);}
  
float3 Tri(float2 pos){
  float3 a=Horz3(pos,-1.0);
  float3 b=Horz5(pos, 0.0);
  float3 c=Horz3(pos, 1.0);
  float wa=Scan(pos,-1.0);
  float wb=Scan(pos, 0.0);
  float wc=Scan(pos, 1.0);
  return a*wa+b*wb+c*wc;}
  
float3 Bloom(float2 pos){
  float3 a=Horz5(pos,-2.0);
  float3 b=Horz7(pos,-1.0);
  float3 c=Horz7(pos, 0.0);
  float3 d=Horz7(pos, 1.0);
  float3 e=Horz5(pos, 2.0);
  float wa=BloomScan(pos,-2.0);
  float wb=BloomScan(pos,-1.0);
  float wc=BloomScan(pos, 0.0);
  float wd=BloomScan(pos, 1.0);
  float we=BloomScan(pos, 2.0);
  return a*wa+b*wb+c*wc+d*wd+e*we;}
  
float2 Warp(float2 pos){
  pos=pos*2.0-1.0;    
  pos*=float2(1.0+(pos.y*pos.y)*warp.x,1.0+(pos.x*pos.x)*warp.y);
  return pos*0.5+0.5;}
  
float3 Mask(float2 pos){
  float3 mask=float3(maskDark,maskDark,maskDark);

  // Very compressed TV style shadow mask.
  if (shadowMask == 1) {
    float sline=maskLight;
    float odd=0.0;
    if(frac(pos.x/6.0)<0.5)odd=1.0;
    if(frac((pos.y+odd)/2.0)<0.5)sline=maskDark;  
    pos.x=frac(pos.x/3.0);
   
    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
    mask*=sline;  
  } 

  // Aperture-grille.
  else if (shadowMask == 2) {
    pos.x=frac(pos.x/3.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  } 

  // Stretched VGA style shadow mask (same as prior shaders).
  else if (shadowMask == 3) {
    pos.x+=pos.y*3.0;
    pos.x=frac(pos.x/6.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  }

  // VGA style shadow mask.
  else if (shadowMask == 4) {
    pos.xy=floor(pos.xy*float2(1.0,0.5));
    pos.x+=pos.y*3.0;
    pos.x=frac(pos.x/6.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  }

  return mask;}
  
float4 PS_CRTLottes(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {

  float2 ppos=Warp(uv.xy*(TextureSize/video_size.xy)*( video_size.xy/TextureSize));
  float3 outColor = Tri(ppos);

  outColor.rgb += doBloom ? (Bloom(ppos)*bloomAmt) : outColor.rgb;
  
  outColor.rgb *= shadowMask ? (Mask(floor(uv.xy*(TextureSize/video_size.xy)*ReShade::ScreenSize.xy)+float2(0.5,0.5))) : outColor.rgb;
	
  return float4(ToSrgb(outColor.rgb),1.0);
}

technique LottesCRT {
	pass CRTLottes {
		VertexShader=PostProcessVS;
		PixelShader=PS_CRTLottes;
	}
}