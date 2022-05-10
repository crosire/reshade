#include "ReShade.fxh"

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [CoolRetroTerm]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [CoolRetroTerm]";
> = 240.0;

uniform float contrast <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Contrast [CoolRetroTerm]";
> = 0.85;
	
uniform float brightness <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Brightness [CoolRetroTerm]";
> = 0.5;

static const float3 terminalColor = float3(0.0, 0.0, 0.0);

uniform float3 terminalBackColor <
	ui_type = "color";
	ui_label = "Terminal Color [CoolRetroTerm]";
> = float3(0.04, 0.8, 0.4);

uniform float staticNoise <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Static Noise [CoolRetroTerm]";
> = 0.1;

uniform float screenCurvature <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Screen Curvature [CoolRetroTerm]";
> = 0.1;
	
uniform float glowingLine <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Glowing Line [CoolRetroTerm]";
> = 0.2;
	
uniform float saturationColor <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Saturation [CoolRetroTerm]";
> = 0.0;
	
uniform float jitter <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Jitter [CoolRetroTerm]";
> = 0.18;
	
uniform float horizontalSync <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.01;
	ui_label = "Horizontal Sync [CoolRetroTerm]";
> = 0.16;
	
uniform float flickering <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Flickering [CoolRetroTerm]";
> = 0.1;
	
uniform float rgbShift <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "RGB Shift [CoolRetroTerm]";
> = 0.0;

uniform int rasterization <
	ui_type = "combo";
	ui_items = "Default\0Scanlines\0Pixels\0";
	ui_label = "Rasterization [CoolRetroTerm]";
> = 0;

uniform float time < source = "timer"; >;
#define PI 3.14159265359
#define absSinAvg 0.63661828335466886
#define virtual_resolution float2(texture_sizeX, texture_sizeY)
	
texture tNoise <source="allNoise512.png";> { Width=512; Height=512; Format = RGBA8;};
texture2D qtSourceTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
sampler2D qtSourceSampler
{
	Texture = qtSourceTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};
sampler2D noiseSource
{
	Texture = tNoise;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = REPEAT;
	AddressV = REPEAT;
};

float randomPass(float2 coords){ return frac(smoothstep(-120.0, 0.0, coords.y - (virtual_resolution.y + 120.0) * frac(time * 0.00015)));}		

float rgb2grey(float3 v){
	return dot(v, float3(0.21, 0.72, 0.04));
}

float getScanlineIntensity(float2 coords){
	
	float val = 0.0;
	float result = 1.0;
	
	float width = floor(ReShade::ScreenSize.x / (0.0 * virtual_resolution.x));
	float height = floor(ReShade::ScreenSize.y / 0.0);
	
	float2 rasterizationSmooth = float2(clamp(2.0 * virtual_resolution.x / (width * ReShade::PixelSize.x), 0.0, 1.0),clamp(2.0 * virtual_resolution.y / (height * ReShade::PixelSize.y), 0.0, 1.0));
	
	if(rasterization != 0){
		val = abs(sin(coords.y * virtual_resolution.y * PI));
		result *= lerp(val,absSinAvg, rasterizationSmooth.y);
	}
		
	if (rasterization == 2){
		val = abs(sin(coords.x * virtual_resolution.x * PI));
		result *= lerp(val,absSinAvg, rasterizationSmooth.x);
	}
	
   return result;
}

float4 PS_Stock( float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float3 origColor = tex2D(ReShade::BackBuffer,texcoord);
	float3 blur_color = tex2D(ReShade::BackBuffer,texcoord);
	float3 color = min(origColor + blur_color, max(origColor, blur_color));
	
	float rgb_noise = 0.0;
	float rcolor = 0.0;
	float bcolor = 0.0;
	float greyscale_color = rgb2grey(origColor) + color;
	float rgbShiftFS = rgbShift * 0.2;
	
	if (rgbShift != 0){
		rgb_noise = abs(tex2D(noiseSource, float2(frac(time/(1024.0 * 256.0)), frac(time/(1024.0*1024.0)))).a - 0.5);
		rcolor = tex2D(ReShade::BackBuffer, texcoord + float2(0.1, 0.0) * rgbShiftFS * rgb_noise).r;
		bcolor = tex2D(ReShade::BackBuffer, texcoord - float2(0.1, 0.0) * rgbShiftFS * rgb_noise).b;
		color.r = rcolor;
		color.b = bcolor;
		greyscale_color = 0.33 * (rcolor + bcolor);
	}
	
	return float4(color, rgb2grey(color - origColor));
}

float4 PS_CoolRetroTerm (float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{


	float2 texcoord = uv;
	float saturatedColor = lerp(float3(1.0,1.0,1.0),terminalColor,(saturationColor+2.2)*0.5);
	float3 fontColor = lerp(saturatedColor,terminalBackColor,0.7 + saturate((contrast *0.3)));
	float3 backgroundColor = lerp(terminalBackColor,saturatedColor,0.7+ saturate((contrast * 0.3)));
	float jitterFS = jitter * 0.007;
	float glowingLineFS = glowingLine * 0.2;
	float horizontalSyncFS = horizontalSync * 0.5;
	float screen_brightness = brightness * 1.5 + 0.5;
  	float dispX = ReShade::ScreenSize.x;
	float dispY = ReShade::ScreenSize.y;
	float2 scaleNoiseSize = float2((ReShade::ScreenSize.x) / 512, (ReShade::ScreenSize.y) / 512);
	
	float2 cc = float2(0.5,0.5) - texcoord;
	float distance = length(cc);
	
	
	//Fallback 1
	float2 initialCoords = float2(frac(time/(1024.0*2.0)), frac(time/(1024.0*1024.0)));
	float4 initialNoiseTexel = tex2D(noiseSource, texcoord);
	
	//Fallback 2
	float brightnessFS = 1.0 + (initialNoiseTexel.g - 0.5) * flickering;
	
	//Fallback 3
	float randval = horizontalSyncFS - initialNoiseTexel.r;
	float distortionFreq = lerp(4.0, 40.0, initialNoiseTexel.g);
	float distortionScale = max(0.0, randval) * randval * horizontalSyncFS;
	
	
	if (flickering != 0.0 || horizontalSync != 0.0){
		initialCoords = float2(frac(time/(1024.0*2.0)), frac(time/(1024.0*1024.0)));
		initialNoiseTexel = tex2D(noiseSource, initialCoords);
	}
			
	if (flickering != 0.0){
		brightnessFS = 1.0 + (initialNoiseTexel.g - 0.5) * flickering;
	}
	
	if (horizontalSync != 0.0){
		randval = horizontalSyncFS - initialNoiseTexel.r;
		distortionScale = max(0.0, randval) * randval * horizontalSyncFS;
		distortionFreq = lerp(4.0, 40.0, initialNoiseTexel.g);
	}
	
	float noise = staticNoise;
	float distortion = 0.0;
	float2 staticCoords = texcoord;
		
	float2 hco = float2(ReShade::PixelSize.x, ReShade::PixelSize.y); //hard cuttoff x
	
	if (screenCurvature != 0) {
		distortion = dot(cc, cc) * screenCurvature;
		staticCoords = (texcoord - cc * (1.0 + distortion) * distortion);
	} else {
		staticCoords = texcoord;
	}
	
	float2 coords = staticCoords;
	float dst = 0;

    if (horizontalSync !=0){
		dst = sin((coords.y + time * 0.001) * distortionFreq);
		coords.x += dst * distortionScale;
		if (staticNoise){
			noise += distortionScale * 7.0;
		}
	}
	
	float4 noiseTexel = tex2D(noiseSource, scaleNoiseSize * coords + float2(frac(time / 51.0), frac(time / 237.0)));
	float2 txt_coords = coords;
	
	float2 offset = float2(0.0,0.0);
	
	if (jitter != 0) {
		offset = float2(noiseTexel.b, noiseTexel.a) - float2(0.5,0.5);
		txt_coords = coords + offset * jitterFS;
	} else {
		offset = coords;
		txt_coords = coords;
	}
	
	float color = 0.0;
	float noiseVal = noiseTexel.a;
  
	if (staticNoise != 0){
		color += noiseVal * noise * (1.0 - distance * 1.3);
	}

	if (glowingLine != 0){
		color += randomPass(coords * virtual_resolution) * glowingLineFS;
	}
	
	float3 txt_color = tex2D(qtSourceSampler, txt_coords).rgb;
	
	float greyscale_color = rgb2grey(txt_color) + color;
	
	float3 mixedColor = float3(0.0,0.0,0.0);
	float3 finalBackColor = float3(0.0,0.0,0.0);
	float3 finalColor = float3(0.0,0.0,0.0);
	
	finalColor = lerp(backgroundColor.rgb,fontColor.rgb,greyscale_color);
	
	finalColor *= getScanlineIntensity(coords);
	
	finalColor *= smoothstep(-dispX, 0.0, staticCoords.x) - smoothstep(1.0, 1.0 + dispX, staticCoords.x);
	finalColor *= smoothstep(-dispY, 0.0, staticCoords.y) - smoothstep(1.0, 1.0 + dispY, staticCoords.y);
	
	if (flickering != 0){
		finalColor *= brightnessFS;
	}
	
	if( staticCoords.x<=(0.0+hco.x) || staticCoords.x>=(1.0-hco.x) || staticCoords.y<=(0.0+hco.y) || 		staticCoords.y>=(1.0-hco.y) ){
		finalColor = float3(0.0,0.0,0.0);
	}
	
	return float4(finalColor * screen_brightness,1);
}

technique CoolRetroTerm
{
   pass Stock {
	 VertexShader = PostProcessVS;
	 PixelShader = PS_Stock;
	 RenderTarget = qtSourceTex;
   }

   pass CoolRetroTerm
   {
     VertexShader = PostProcessVS;
     PixelShader  = PS_CoolRetroTerm;
   }  
}