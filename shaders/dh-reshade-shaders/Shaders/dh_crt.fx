#include "Reshade.fxh"

#define max3(v) max(v.x,max(v.y,v.z))

namespace DH_CRT {

// Uniforms
uniform int iPixelHeight <
    ui_category = "Effect";
	ui_label = "Pixel height";
	ui_type = "slider";
    ui_min = 1;
    ui_max = 16;
    ui_step = 1;
> = 2;

uniform int iScanlineHeight <
    ui_category = "Effect";
	ui_label = "Scanline height";
	ui_type = "slider";
    ui_min = 0;
    ui_max = 16;
    ui_step = 1;
> = 1;

uniform float fScanlineOpacity <
    ui_category = "Effect";
	ui_label = "Scanline opacity";
	ui_type = "slider";
    ui_min = 0;
    ui_max = 1;
    ui_step = 0.1;
> = 0.5;

uniform bool bScanlineGrad <
    ui_category = "Effect";
	ui_label = "Scanline grad";
> = true;

uniform float fScanlineBloom <
    ui_category = "Effect";
	ui_label = "Scanline bloom";
	ui_type = "slider";
    ui_min = 0;
    ui_max = 1;
    ui_step = 0.1;
> = 0.1;

uniform float fScanlineBloomThreshold <
    ui_category = "Effect";
	ui_label = "Scanline bloom threshold";
	ui_type = "slider";
    ui_min = 0;
    ui_max = 1;
    ui_step = 0.1;
> = 0.5;

uniform float fBrightnessBoost <
    ui_category = "Effect";
	ui_label = "Brightness boost";
	ui_type = "slider";
    ui_min = 0;
    ui_max = 1;
    ui_step = 0.1;
> = 0.1;


// Textures

// Functions
	int2 screenSize() { return int2(BUFFER_WIDTH,BUFFER_HEIGHT); }
	
	int getScanlineNumber(float2 coords) {
		int yPx = coords.y*BUFFER_HEIGHT;
		int yCellPx = yPx%iPixelHeight;
		
		if(yCellPx>iScanlineHeight) {
			return -1;
		} else {
			return yCellPx;
		}
	}
	
	float isScanline(float2 coords) {
		int yPx = coords.y*BUFFER_HEIGHT;
		int yCellPx = yPx%iPixelHeight;
		if(yCellPx>=iScanlineHeight) {
			return 0;
		} else if(!bScanlineGrad) {
			return 1;
		} else {
			return 1-float(yCellPx)/iScanlineHeight;
		}
	}


// Pixel shaders

	void PS_result(in float4 position : SV_Position, in float2 coords : TEXCOORD, out float4 outPixel : SV_Target)
	{
		float4 color = tex2D(ReShade::BackBuffer,coords);
		
		float scanlineNumber = getScanlineNumber(coords);
		if(scanlineNumber>0) {

			if(bScanlineGrad) {
				color.rgb *= 1-fScanlineOpacity*scanlineNumber/iScanlineHeight;
			} else {
				color.rgb *= 1-fScanlineOpacity;
			}
			
			if(fScanlineBloom>0) {
				float previousLine = coords.y-ReShade::PixelSize.y*scanlineNumber;
				float nextLine = coords.y+ReShade::PixelSize.y*(iScanlineHeight-scanlineNumber);
				float minX = saturate(coords.x - ReShade::PixelSize.x);
				float maxX = saturate(coords.x + ReShade::PixelSize.x);
				
				float w;
				float sumW = 0;
				float3 sumColor;
				if(previousLine>=0) {
					float2 currentCoords = float2(0,previousLine);
					for(currentCoords.x = minX;currentCoords.x<=maxX;currentCoords.x+=ReShade::PixelSize.x) {
						float3 c = tex2Dlod(ReShade::BackBuffer,float4(currentCoords,0,0)).rgb;
						w = 1-float(scanlineNumber)/iScanlineHeight;
						w *= pow(max3(c),2);
						sumColor += c*w;
						sumW += w;
					}
				}
				if(nextLine<=1) {
					float2 currentCoords = float2(0,nextLine);
					for(currentCoords.x = minX;currentCoords.x<=maxX;currentCoords.x+=ReShade::PixelSize.x) {
						float3 c = tex2Dlod(ReShade::BackBuffer,float4(currentCoords,0,0)).rgb;
						w = 1-float(iScanlineHeight-scanlineNumber)/iScanlineHeight;
						w *= pow(max3(c),2);
						sumColor += c*w;
						sumW += w;
					}
				}
				color.rgb += fScanlineBloom*sumColor/sumW;
				

			}
			
		} else {
			if(fBrightnessBoost>0) {
				color.rgb *= 1+fBrightnessBoost;
			}
		}
		outPixel = saturate(color);
	}

	
// Techniques

	technique DH_CRT <
	>
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_result;
		}
	}

}