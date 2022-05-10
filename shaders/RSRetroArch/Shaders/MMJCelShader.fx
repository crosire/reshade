#include "ReShade.fxh"

/*
MMJ's Cel Shader - v1.03
----------------------------------------------------------------
-- 180403 --
This is a port of my old shader from around 2006 for Pete's OGL2 
plugin for ePSXe. It started out as a shader based on the 
"CComic" shader by Maruke. I liked his concept, but I was 
looking for something a little different in the output. 

Since the last release, I've seen some test screenshots from MAME 
using a port of my original shader and have also seen another 
port to get it working with the PCSX2 emulator. Having recently 
seen some Kingdom Hearts II and Soul Calibur 3 YouTube videos with 
my ported shader inspired me to revisit it and get it working in 
RetroArch.

As for this version (1.03), I've made a few small modifications 
(such as to remove the OGL2Param references, which were specific 
to Pete's plugin) and I added some RetroArch Parameter support, 
so some values can now be changed in real time.

Keep in mind, that this was originally developed for PS1, using
various 3D games as a test. In general, it will look better in 
games with less detailed textures, as "busy" textures will lead 
to more outlining / messy appearance. Increasing "Outline 
Brightness" can help mitigate this some by lessening the 
"strength" of the outlines.

Also (in regards to PS1 - I haven't really tested other systems 
too much yet), 1x internal resolution will look terrible. 2x 
will also probably be fairly blurry/messy-looking. For best 
results, you should probably stick to 4x or higher internal 
resolution with this shader.

Parameters:
-----------
White Level Cutoff = Anything above this luminance value will be 
    forced to pure white.

Black Level Cutoff = Anything below this luminance value will be 
    forced to pure black.

Shading Levels = Determines how many color "slices" there should 
    be (not counting black/white cutoffs, which are always 
    applied).

Saturation Modifier = Increase or decrease color saturation. 
    Default value boosts saturation a little for a more 
    cartoonish look. Set to 0.00 for grayscale.

Outline Brightness = Adjusts darkness of the outlines. At a 
    setting of 1, outlines should be disabled.

Shader Strength = Adjusts the weight of the color banding 
    portion of the shader from 0% (0.00) to 100% (1.00). At a 
    setting of 0.00, you can turn off the color banding effect 
    altogether, but still keep outlines enabled.
-----------
MMJuno
*/

uniform float WhtCutoff <
	ui_type = "drag";
	ui_min = 0.50;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "White Level Cutoff [MMJCelShader]";
> = 0.97;

uniform float BlkCutoff <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 0.5;
	ui_step = 0.01;
	ui_label = "Black Level Cutoff [MMJCelShader]";
> = 0.03;

uniform float ShdLevels <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 16.0;
	ui_step = 1.0;
	ui_label = "Shading Levels [MMJCelShader]";
> = 9.0;

uniform float SatModify <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.01;
	ui_label = "Saturation Modifier [MMJCelShader]";
> = 1.15;

uniform float OtlModify <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Outline Brightness [MMJCelShader]";
> = 0.20;

uniform float ShdWeight <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Shader Strength [MMJCelShader]";
> = 0.50;

sampler RetroArchSRGB { Texture = ReShade::BackBufferTex; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; SRGBTexture = true;};

#define mod(x,y) (x-y*floor(x/y))

float3 RGB2HCV(in float3 RGB)
{
	RGB = saturate(RGB);
	float Epsilon = 1e-10;
    	// Based on work by Sam Hocevar and Emil Persson
    	float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0/3.0) : float4(RGB.gb, 0.0, -1.0/3.0);
    	float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
    	float C = Q.x - min(Q.w, Q.y);
    	float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
    	return float3(H, C, Q.x);
}

float3 RGB2HSL(in float3 RGB)
{
    	float3 HCV = RGB2HCV(RGB);
    	float L = HCV.z - HCV.y * 0.5;
    	float S = HCV.y / (1.0000001 - abs(L * 2 - 1));
    	return float3(HCV.x, S, L);
}

float3 HSL2RGB(in float3 HSL)
{
	HSL = saturate(HSL);
	//HSL.z *= 0.99;
    	float3 RGB = saturate(float3(abs(HSL.x * 6.0 - 3.0) - 1.0,2.0 - abs(HSL.x * 6.0 - 2.0),2.0 - abs(HSL.x * 6.0 - 4.0)));
    	float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
    	return (RGB - 0.5) * C + HSL.z;
}

float3 colorAdjust(float3 cRGB) 
{
    float3 cHSL = RGB2HSL(cRGB);

    float cr = 1.0 / ShdLevels;

    // brightness modifier
    float BrtModify = mod(cHSL[2], cr); 

    if     (cHSL[2] > WhtCutoff) { cHSL[1]  = 1.0; cHSL[2] = 1.0; }
    else if(cHSL[2] > BlkCutoff) { cHSL[1] *= SatModify; cHSL[2] += (cHSL[2] * cr - BrtModify); }
    else                         { cHSL[1]  = 0.0; cHSL[2] = 0.0; }
    cRGB = 1.2 * HSL2RGB(cHSL);

    return cRGB;
}

void PS_MMJCel(in float4 pos : SV_POSITION, in float2 texcoord : TEXCOORD0, out float4 fragColor : COLOR0)
{

	float2 offset = float2(0.0,0.0);
	float2 offset_inv = float2(0.0,0.0);
	float2 TEX0 = texcoord.xy;
	float2 TEX1 = float2(0.0,0.0);
	float2 TEX1_INV = float2(0.0,0.0);
	float2 TEX2 = float2(0.0,0.0);
	float2 TEX2_INV = float2(0.0,0.0);
	float2 TEX3 = float2(0.0,0.0);
	float2 TEX3_INV = float2(0.0,0.0);
	
	offset = -(float2(1.0 / ReShade::ScreenSize.x, 0.0)); //XY
	offset_inv = float2(1.0 / ReShade::ScreenSize.x,0.0); //ZW
    TEX1 = TEX0 + offset;
	TEX1_INV = TEX0 + offset_inv;
	
	offset = -(float2(0.0,(1.0 / ReShade::ScreenSize.y))); //XY
	offset_inv = float2(0.0, (1.0 / ReShade::ScreenSize.y)); //ZW
    
    TEX2 = TEX0.xy + offset;
	TEX2_INV = TEX0.xy + offset_inv;
    TEX3 = TEX1.xy + offset;
	TEX3_INV = TEX1.xy + offset_inv;
	
    float3 c0 = tex2D(RetroArchSRGB, TEX3.xy).rgb;
    float3 c1 = tex2D(RetroArchSRGB, TEX2.xy).rgb;
    float3 c2 = tex2D(RetroArchSRGB, float2(TEX3_INV.x,TEX3.y)).rgb;
    float3 c3 = tex2D(RetroArchSRGB, TEX1.xy).rgb;
    float3 c4 = tex2D(RetroArchSRGB, TEX0.xy).rgb;
    float3 c5 = tex2D(RetroArchSRGB, TEX1_INV.xy).rgb;
    float3 c6 = tex2D(RetroArchSRGB, float2(TEX3.x,TEX3_INV.y)).rgb;
    float3 c7 = tex2D(RetroArchSRGB, TEX2_INV).rgb;
    float3 c8 = tex2D(RetroArchSRGB, TEX3_INV).rgb;
    float3 c9 = ((c0 + c2 + c6 + c8) * 0.15 + (c1 + c3 + c5 + c7) * 0.25 + c4) / 2.6;

    float3 o = float3(1.0,1.0,1.0); 
	float3 h = float3(0.05,0.05,0.05); 
	float3 hz = h; 
	float k = 0.005; 
    float kz = 0.007; 
	float i = 0.0;

    float3 cz = (c9 + h) / (dot(o, c9) + k);
	float3 col = float3(0.0,0.0,0.0);

    hz = (cz - ((c0 + h) / (dot(o, c0) + k))); i  = kz / (dot(hz, hz) + kz);
    hz = (cz - ((c1 + h) / (dot(o, c1) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c2 + h) / (dot(o, c2) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c3 + h) / (dot(o, c3) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c5 + h) / (dot(o, c5) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c6 + h) / (dot(o, c6) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c7 + h) / (dot(o, c7) + k))); i += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c8 + h) / (dot(o, c8) + k))); i += kz / (dot(hz, hz) + kz);

    i /= 8.0; 
	i = pow(i, 0.75);

    if(i < OtlModify) { i = OtlModify; }
    c9 = min(o, min(c9, c9 + dot(o, c9)));
	col = lerp(c4 * i, colorAdjust(c9 * i), ShdWeight);
    fragColor = float4(col,1.0);
} 

technique MMJCelShader {
    pass MMJCelShader {
        VertexShader=PostProcessVS;
        PixelShader=PS_MMJCel;
		SRGBWriteEnable = true;
    }
}