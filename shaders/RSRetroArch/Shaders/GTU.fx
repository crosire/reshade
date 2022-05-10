#include "ReShade.fxh"

////////////////////////////////////////////////////////
// GTU version 0.50
// Author: aliaspider - aliaspider@gmail.com
// License: GPLv3
// Ported to ReShade by Matsilagi
////////////////////////////////////////////////////////

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width (GTU)";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height (GTU)";
> = 240.0;

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width (GTU)";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.0;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height (GTU)";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.0;

uniform bool compositeConnection <
	ui_label = "Enables Composite Connection (GTU)";
> = 0;

uniform bool noScanlines <
	ui_label = "Disables Scanlines (GTU)";
> = 0;

uniform float signalResolution <
	ui_type = "drag";
	ui_min = 16.0; ui_max = 1024.0;
	ui_tooltip = "Signal Resolution Y (GTU)";
	ui_step = 16.0;
> = 256.0;

uniform float signalResolutionI <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 350.0;
	ui_tooltip = "Signal Resolution I (GTU)";
	ui_step = 2.0;
> = 83.0;

uniform float signalResolutionQ <
	ui_type = "drag";
	ui_min = 1.0; ui_max = 350.0;
	ui_tooltip = "Signal Resolution Q (GTU)";
	ui_step = 2.0;
> = 25.0;

uniform float tvVerticalResolution <
	ui_type = "drag";
	ui_min = 20.0; ui_max = 1000.0;
	ui_tooltip = "TV Vertical Resolution (GTU)";
	ui_step = 10.0;
> = 250.0;

uniform float blackLevel <
	ui_type = "drag";
	ui_min = -0.30; ui_max = 0.30;
	ui_tooltip = "Black Level (GTU)";
	ui_step = 0.01;
> = 0.07;

uniform float contrast <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_tooltip = "Constrast (GTU)";
	ui_step = 0.1;
> = 1.0;

texture target0_gtu
{
    Width = BUFFER_WIDTH;
    Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};
sampler s0 { Texture = target0_gtu; };

#define _tex2D(sp, uv) tex2Dlod(sp, float4(uv, 0.0, 0.0))

#define texture_size float2(texture_sizeX, texture_sizeY)
#define video_size float2(video_sizeX, video_sizeY)

#define RGB_to_YIQ   transpose(float3x3( 0.299 , 0.595716 , 0.211456 , 0.587 , -0.274453 , -0.522591 , 0.114 , -0.321263 , 0.311135 ))
#define pi        3.14159265358

//PASS 2
#define YIQ_to_RGB   transpose(float3x3( 1.0 , 1.0  , 1.0 , 0.9563 , -0.2721 , -1.1070 , 0.6210 , -0.6474 , 1.7046 ))
#define a(x) abs(x)
#define d(x,b) (pi*b*min(a(x)+0.5,1.0/b))
#define e(x,b) (pi*b*min(max(a(x)-0.5,-1.0/b),1.0/b))
#define STU(x,b) ((d(x,b)+sin(d(x,b))-e(x,b)-sin(e(x,b)))/(2.0*pi))
#define X(i) (offset-(i))
#define GETC (_tex2D(s0, float2(tex.x - X/texture_size.x,tex.y)).rgb)

#define VAL_composite float3((c.x*STU(X,(signalResolution/video_size.x))),(c.y*STU(X,(signalResolutionI/video_size.x))),(c.z*STU(X,(signalResolutionQ/video_size.x))))
#define VAL (c*STU(X,(signalResolution/video_size.x)))

#define PROCESS(i) X=X(i);c=GETC;tempColor+=VAL;
#define PROCESS_composite(i) X=X(i);c=GETC;tempColor+=VAL_composite;

//PASS 3
#define normalGauss(x) ((exp(-(x)*(x)*0.5))/sqrt(2.0*pi))

float normalGaussIntegral(float x)
{
	float a1 = 0.4361836;
	float a2 = -0.1201676;
	float a3 = 0.9372980;
	float p = 0.3326700;
	float t = 1.0 / (1.0 + p*abs(x));
	return (0.5-normalGauss(x) * (t*(a1 + t*(a2 + a3*t))))*sign(x);
}

float3 scanlines( float x , float3 c){
	float temp=sqrt(2*pi)*(tvVerticalResolution/texture_sizeY);

	float rrr=0.5*(texture_sizeY/ReShade::ScreenSize.y);
	float x1=(x+rrr)*temp;
	float x2=(x-rrr)*temp;
	c.r=(c.r*(normalGaussIntegral(x1)-normalGaussIntegral(x2)));
	c.g=(c.g*(normalGaussIntegral(x1)-normalGaussIntegral(x2)));
	c.b=(c.b*(normalGaussIntegral(x1)-normalGaussIntegral(x2)));
	c*=(ReShade::ScreenSize.y/texture_sizeY);
	return c;
}

#define Y(j) (offset.y-(j))

#define SOURCE(j) float2(tex.x,tex.y - Y(j)/texture_size.y)
#define C(j) (_tex2D(ReShade::BackBuffer, SOURCE(j)).xyz)

#define VAL_1(j) (C(j)*STU(Y(j),(tvVerticalResolution/video_size.y)))

#define VAL_scanlines(j) (scanlines(Y(j),C(j)))

float4 PS_GTU1(float4 vpos : SV_Position, float2 tex : TexCoord) : SV_Target
{
	float4 c=tex2D(ReShade::BackBuffer, tex);
	if(compositeConnection){
		c.rgb=mul(RGB_to_YIQ,c.rgb);
	}
	return c;
}

float4 PS_GTU2(float4 vpos : SV_Position, float2 tex : TexCoord) : SV_Target
{
	// return tex2D(s0, tex);
	float offset   = frac((tex.x * texture_size.x) - 0.5);
	float3   tempColor = float3(0.0,0.0,0.0);
	float    X;
	float3   c;
	float range;
	if (compositeConnection){
		range=ceil(0.5+video_size.x/min(min(signalResolution,signalResolutionI),signalResolutionQ));
	} else {
		range=ceil(0.5+video_size.x/signalResolution);
	}

	float i;
	if(compositeConnection){
		//[loop]
		for (i=-range;i<range+2.0;i++){
			PROCESS_composite(i)
		}
	} else {
		//[loop]
		for (i=-range;i<range+2.0;i++){
			PROCESS(i)
		}
	}
	if(compositeConnection) {
		tempColor=clamp(mul(YIQ_to_RGB,tempColor),0.0,1.0);
	} else {
		tempColor=clamp(tempColor,0.0,1.0);
	}

	// tempColor=clamp(tempColor,0.0,1.0);
	return float4(tempColor,1.0);
}

float4 PS_GTU3(float4 vpos : SV_Position, float2 tex : TexCoord) : SV_Target
{
	float2   offset   = frac((tex.xy * texture_size) - 0.5);
	float3   tempColor = float3(0.0,0.0,0.0);

	float range=ceil(0.5+video_size.y/tvVerticalResolution);

	float i;

	if (noScanlines){
		//[loop]
		for (i=-range;i<range+2.0;i++){
			tempColor+=VAL_1(i);
		}
	} else {
		//[loop]
		for (i=-range;i<range+2.0;i++){
			tempColor+=VAL_scanlines(i);
		}
	}

	tempColor-=float3(blackLevel,blackLevel,blackLevel);
	tempColor*=(contrast/float3(1.0-blackLevel,1.0-blackLevel,1.0-blackLevel));
	return float4(tempColor, 1.0);
}

technique GTUV50 {
	pass GTU1
	{	
		RenderTarget = target0_gtu;
		
		VertexShader = PostProcessVS;
		PixelShader = PS_GTU1;
	}
	
	pass p2
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_GTU2;
	}
	
	pass p3
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_GTU3;
	}
}