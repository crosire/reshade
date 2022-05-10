#include "ReShade.fxh"

uniform float display_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_step = 1.0;
	ui_label = "Screen Width [LCD-Grid]";
> = 320.0;

uniform float display_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1.0;
	ui_label = "Screen Height [LCD-Grid]";
> = 240.0;

uniform float GRID_STRENGTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "LCD Grid Strength [LCD-Grid]";
> = 0.05;

uniform float gamma <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 5.0;
	ui_step = 0.1;
	ui_label = "LCD Gamma [LCD-Grid]";
> = 2.2;

#define display_size float2(display_sizeX,display_sizeY)
#define TX2D(c) pow(tex2D(ReShade::BackBuffer, (c)), float4(gamma,gamma,gamma,gamma))
#define round(x) floor( (x) + 0.5 )
#define mod(x,y) ((x)-(y)*floor((x)/(y)))

float intsmear_func(float z)
{
  float z2 = z*z;
  float z4 = z2*z2;
  float z8 = z4*z4;
  return z - 2.0/3.0*z*z2 - 1.0/5.0*z*z4 + 4.0/7.0*z*z2*z4 - 1.0/9.0*z*z8
    - 2.0/11.0*z*z2*z8 + 1.0/13.0*z*z4*z8;
}

float intsmear(float x, float dx)
{
  const float d = 1.5;
  float zl = clamp((x-dx)/d,-1.0,1.0);
  float zh = clamp((x+dx)/d,-1.0,1.0);
  return d * ( intsmear_func(zh) - intsmear_func(zl) )/(2.0*dx);
}

float4 PS_LCDGrid(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  float4 frgCol;
  float2 SourceSizeZW = 1.0 / display_size.xy;
  float2 texelSize = SourceSizeZW;
  float2 subtexelSize = texelSize / float2(3.0,1.0);
  float2 range;
  range = display_size.xy / (ReShade::ScreenSize.xy * display_size.xy);
  
  float left   = texcoord.x - texelSize.x*0.5;
  float top    = texcoord.y + range.y;
  float right  = texcoord.x + texelSize.x*0.5;
  float bottom = texcoord.y - range.y;
  
  float4 lcol, rcol;
  float subpix = mod(texcoord.x/subtexelSize.x+1.5,3.0);
  float rsubpix = range.x/subtexelSize.x;
  lcol = float4(intsmear(subpix+1.0,rsubpix),intsmear(subpix    ,rsubpix),
	      intsmear(subpix-1.0,rsubpix),0.0);
  rcol = float4(intsmear(subpix-2.0,rsubpix),intsmear(subpix-3.0,rsubpix),
	      intsmear(subpix-4.0,rsubpix),0.0);
			  
  float4 topLeftColor     = TX2D((floor(float2(left, top)     / texelSize) + 0.5) * texelSize) * lcol;
  float4 bottomRightColor = TX2D((floor(float2(right, bottom) / texelSize) + 0.5) * texelSize) * rcol;
  float4 bottomLeftColor  = TX2D((floor(float2(left, bottom)  / texelSize) + 0.5) * texelSize) * lcol;
  float4 topRightColor    = TX2D((floor(float2(right, top)    / texelSize) + 0.5) * texelSize) * rcol;
  
  float2 border = round(texcoord.xy/subtexelSize);
  float2 bordert = clamp((border+float2(0.0,+GRID_STRENGTH)) * subtexelSize,
		       float2(left, bottom), float2(right, top));
  float2 borderb = clamp((border+float2(0.0,-GRID_STRENGTH)) * subtexelSize,
		       float2(left, bottom), float2(right, top));
  float totalArea = 2.0 * range.y;  

  float4 averageColor;
  averageColor  = ((top - bordert.y)    / totalArea) * topLeftColor;
  averageColor += ((borderb.y - bottom) / totalArea) * bottomRightColor;
  averageColor += ((borderb.y - bottom) / totalArea) * bottomLeftColor;
  averageColor += ((top - bordert.y)    / totalArea) * topRightColor;
  
  frgCol = pow(averageColor,float4(1.0/gamma,1.0/gamma,1.0/gamma,1.0/gamma));
  return frgCol;
}

technique LCDGrid
{
   pass LCDGrid {
	 VertexShader = PostProcessVS;
	 PixelShader = PS_LCDGrid;
   }
}