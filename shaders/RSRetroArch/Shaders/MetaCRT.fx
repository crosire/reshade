#include "ReShade.fxh"

// Meta CRT by P_Malin, ported to ReShade by Matsilagi

// 2D screen effect from "Meta CRT" -> https://www.shadertoy.com/view/4dlyWX

uniform int ScreenResX <
	ui_type = "drag";
	ui_min = 0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [Meta CRT]";
> = 480;

uniform int ScreenResY <
	ui_type = "drag";
	ui_min = 0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [Meta CRT]";
> = 576;

uniform float fAmbientEmissive <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_label = "Monitor Gamma [MetaCRT]";
> = 0.1;

uniform float fBlackEmissive <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_label = "Slot Gamma [MetaCRT]";
> = 0.02;

uniform float kResolutionScale <
	ui_type = "drag";
	ui_min = 0.01;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Resolution Scale [MetaCRT]";
> = 1.0;

uniform float kBrightness <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 256.0;
	ui_label = "Brightness [MetaCRT]";
> = 1.75;

uniform float fNoiseIntensity <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Noise Intensity [MetaCRT]";
> = 0.0;

uniform bool bScanlineInterference <
	ui_label = "Enable Scanline Interference [MetaCRT]";
> = false;

uniform int fVerticalLines <
	ui_type = "drag";
	ui_min = 0;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1.0;
	ui_label = "Vertical Lines Number [Meta CRT]";
	ui_tooltip = "Used by Scanline Interference. Its recommended to change it to always make it the width value + 3";
> = 483;

uniform bool bBorders <
	ui_label = "Enable Borders [MetaCRT]";
> = true;

uniform bool bZoom <
	ui_label = "Enable Mouse Zoom [MetaCRT]";
> = true;

#define kScreenRsolution int2(ScreenResX, ScreenResY);

uniform float  iGlobalTime < source = "timer"; >;
uniform bool iMouse1 < source = "mousebutton"; keycode = 0; toggle = false; >;
uniform bool iMouse2 < source = "mousebutton"; keycode = 1; toggle = false; >;

uniform float2 iMousePos < source = "mousepoint"; >;

float3 PulseIntegral( float3 x, float s1, float s2 )
{
    // Integral of function where result is 1.0 between s1 and s2 and 0 otherwise        

    // V1
    //if ( x > s2 ) return s2 - s1;
	//else if ( x > s1 ) return x - s1;
	//return 0.0f; 
    
    // V2
    //return clamp( (x - s1), 0.0f, s2 - s1);
    //return t;
    
    return clamp( (x - s1), float3(0.0,0.0,0.0), float3(s2 - s1,s2 - s1,s2 - s1));
}

float PulseIntegral( float x, float s1, float s2 )
{
    // Integral of function where result is 1.0 between s1 and s2 and 0 otherwise        

    // V1
    //if ( x > s2 ) return s2 - s1;
	//else if ( x > s1 ) return x - s1;
	//return 0.0f; 
    
    // V2
    //return clamp( (x - s1), 0.0f, s2 - s1);
    //return t;
    
    return clamp( (x - s1), (0.0f), (s2 - s1));
}

float3 Bayer( float2 vUV, float2 vBlur )
{
    float3 x = float3(vUV.x,vUV.x,vUV.x);
    float3 y = float3(vUV.y,vUV.y,vUV.y);           

    x += float3(0.66, 0.33, 0.0);
    y += 0.5 * step( frac( x * 0.5 ), float3(0.5,0.5,0.5) );
        
    //x -= 0.5f;
    //y -= 0.5f;
    
    x = frac( x );
    y = frac( y );
    
    // cell centered at 0.5
    
    float2 vSize = float2(0.16f, 0.75f);
    
    float2 vMin = 0.5 - vSize * 0.5;
    float2 vMax = 0.5 + vSize * 0.5;
    
    float3 vResult= float3(0.0,0.0,0.0);
    
    float3 vResultX = (PulseIntegral( x + vBlur.x, vMin.x, vMax.x) - PulseIntegral( x - vBlur.x, vMin.x, vMax.x)) / min( vBlur.x, 1.0);
    float3 vResultY = (PulseIntegral(y + vBlur.y, vMin.y, vMax.y) - PulseIntegral(y - vBlur.y, vMin.y, vMax.y))  / min( vBlur.y, 1.0);
    
    vResult = min(vResultX,vResultY)  * 5.0;
        
    //vResult = float3(1.0);
    
    return vResult;
}

float3 GetPixelMatrix( float2 vUV )
{
	if (1){
		float2 dx = ddx( vUV );
		float2 dy = ddy( vUV );
		float dU = length( float2( dx.x, dy.x ) );
		float dV = length( float2( dx.y, dy.y ) );
		if (dU <= 0.0 || dV <= 0.0 ) return float3(1.0,1.0,1.0);
		return Bayer( vUV, float2(dU, dV) * 1.0);
	} else {
		return float3(1.0,1.0,1.0);
	}
}

float Scanline( float y, float fBlur )
{   
    float fResult = sin( y * 10.0 ) * 0.45 + 0.55;
    return lerp( fResult, 1.0f, min( 1.0, fBlur ) );
}


float GetScanline( float2 vUV )
{
	if (1){
		vUV.y *= 0.25;
		float2 dx = ddx( vUV );
		float2 dy = ddy( vUV );
		float dV = length( float2( dx.y, dy.y ) );
		if (dV <= 0.0 ) return 1.0;
		return Scanline( vUV.y, dV * 1.3 );
	} else {
		return 1.0;
	}
}

struct Interference
{
    float noise;
    float scanLineRandom;
};

float InterferenceHash(float p)
{
    float hashScale = 0.1031;

    float3 p3  = frac(float3(p, p, p) * hashScale);
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.x + p3.y) * p3.z);
}


float InterferenceSmoothNoise1D( float x )
{
    float f0 = floor(x);
    float fr = frac(x);

    float h0 = InterferenceHash( f0 );
    float h1 = InterferenceHash( f0 + 1.0 );

    return h1 * fr + h0 * (1.0 - fr);
}


float InterferenceNoise( float2 uv )
{
	float displayVerticalLines = fVerticalLines;
    float scanLine = floor(uv.y * displayVerticalLines); 
    float scanPos = scanLine + uv.x;
	float timeSeed = frac( (iGlobalTime*0.001) * 123.78 );
    
    return InterferenceSmoothNoise1D( scanPos * 234.5 + timeSeed * 12345.6 );
}
    
Interference GetInterference( float2 vUV )
{
    Interference interference;
        
    interference.noise = InterferenceNoise( vUV );
    interference.scanLineRandom = InterferenceHash(vUV.y * 100.0 + frac((iGlobalTime*0.001) * 1234.0) * 12345.0);
    
    return interference;
}
    
float3 SampleScreen( float2 vUV )
{   
    float3 vAmbientEmissive = float3(fAmbientEmissive,fAmbientEmissive,fAmbientEmissive);
    float3 vBlackEmissive = float3(fBlackEmissive,fBlackEmissive,fBlackEmissive);
    float fBrightness = kBrightness;
    float2 vResolution = float2(ScreenResX, ScreenResY);
    vResolution *= kResolutionScale;
    
    float2 vPixelCoord = vUV.xy * vResolution;
    
    float3 vPixelMatrix = GetPixelMatrix( vPixelCoord );
    float fScanline = GetScanline( vPixelCoord );
      
    float2 vTextureUV = vUV.xy;
    //float2 vTextureUV = vPixelCoord;
    vTextureUV = floor(vTextureUV * vResolution * 2.0) / (vResolution * 2.0f);
    
    Interference interference = GetInterference( vTextureUV );

    float noiseIntensity = fNoiseIntensity;
    
	if (bScanlineInterference){
		vTextureUV.x += (interference.scanLineRandom * 2.0f - 1.0f) * 0.025f * noiseIntensity;
	}
    
    float3 vPixelEmissive = tex2D( ReShade::BackBuffer, vTextureUV.xy).rgb;
        
    vPixelEmissive = clamp( vPixelEmissive + (interference.noise - 0.5) * 2.0 * noiseIntensity, 0.0, 1.0 );
    
	float3 vResult = (vPixelEmissive * vPixelEmissive * fBrightness + vBlackEmissive) * vPixelMatrix * fScanline + vAmbientEmissive;
    
    // TODO: feather edge?
    if( any(  vUV.xy >= float2(1.0,1.0) ) || any ( vUV.xy < float2(0.0,0.0) ) )
    {
        return float3(0.0,0.0,0.0);
    }
    
    return vResult / (1.0 + fBrightness + vBlackEmissive);
    
}

float4 PS_MetaCRT( float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float2 texcoord = uv * ReShade::ScreenSize;
	float2 vUV = texcoord.xy / ReShade::ScreenSize;

    float3 vResult = SampleScreen( vUV * 1.0 - 0.0 );
	
	if (bZoom && bBorders){
		if ( (iMouse1) || (iMouse2) )
		{
			vUV = (vUV - 0.5) * (iMousePos.y/ReShade::ScreenSize.y) + 0.5;
		}
	}
	
	if (bBorders){
		vResult = SampleScreen( vUV * 1.1 - 0.05 );
	}
    
    vResult = sqrt( vResult );
	return float4(vResult,1.0);
}

technique MetaCRT {
	pass MetaCRT {
		VertexShader=PostProcessVS;
		PixelShader=PS_MetaCRT;
	}
}
