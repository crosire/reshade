/*
	Light Depth of Field by luluco250
	
	Poisson disk blur by spite ported from: https://github.com/spite/Wagner/blob/master/fragment-shaders/poisson-disc-blur-fs.glsl
	
	Why "light"?
	Because I wanted a DoF shader that didn't tank my GPU and didn't take rocket science to configure.
	Also with delayed auto focus like in ENB.
	
	License: https://creativecommons.org/licenses/by-sa/4.0/
	CC BY-SA 4.0
	
	You are free to:

	Share — copy and redistribute the material in any medium or format
		
	Adapt — remix, transform, and build upon the material
	for any purpose, even commercially.

	The licensor cannot revoke these freedoms as long as you follow the license terms.
		
	Under the following terms:

	Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. 
	You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.

	ShareAlike — If you remix, transform, or build upon the material, 
	you must distribute your contributions under the same license as the original.

	No additional restrictions — You may not apply legal terms or technological measures 
	that legally restrict others from doing anything the license permits.
*/

#include "ReShade.fxh"

//user variables//////////////////////////////////////////////////////////////////////////////////

#include "ReShadeUI.fxh"

uniform float fLightDoF_Width < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Bokeh Width [Light DoF]";
	ui_min = 1.0;
	ui_max = 25.0;
> = 5.0;

uniform float fLightDoF_Amount < __UNIFORM_SLIDER_FLOAT1
	ui_label = "DoF Amount [Light DoF]";
	ui_min = 0.0;
	ui_max = 10.0;
> = 10.0;

uniform bool bLightDoF_UseCA <
	ui_label = "Use Chromatic Aberration [Light DoF]";
	ui_tooltip = "Use color channel shifting.";
> = false;

uniform float2 f2LightDoF_CA < __UNIFORM_SLIDER_FLOAT2
	ui_label = "Chromatic Aberration [Light DoF]";
	ui_tooltip = "Shifts color channels.\nFirst value controls far CA, second controls near CA.";
	ui_min = 0.0;
	ui_max = 1.0;
> = float2(0.0, 1.0);

uniform bool bLightDoF_AutoFocus <
	ui_label = "Use Auto Focus [Light DoF]";
> = true;

uniform float fLightDoF_AutoFocusSpeed <
	ui_label = "Auto Focus Speed [Light DoF]";
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 1.0;
> = 0.1;

uniform bool bLightDoF_UseMouseFocus <
	ui_label = "Use Mouse for Auto Focus Center [Light DoF]";
	ui_tooltip = "Use the mouse position as the auto focus center";
> = false;

uniform float2 f2Bokeh_AutoFocusCenter < __UNIFORM_SLIDER_FLOAT2
	ui_label = "Auto Focus Center [Light DoF]";
	ui_tooltip = "Target for auto focus.\nFirst value is horizontal: Left<->Right\nSecond value is vertical: Up<->Down";
	ui_min = 0.0;
	ui_max = 1.0;
> = float2(0.5, 0.5);

uniform float fLightDoF_ManualFocus < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Manual Focus [Light DoF]";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.0;

//system variables////////////////////////////////////////////////////////////////////////////////

//internal variable that holds the current mouse position
uniform float2 f2LightDoF_MouseCoord <source="mousepoint";>;

//textures////////////////////////////////////////////////////////////////////////////////////////

/*
	For those curious...
	
	Two textures are needed in order to delay focus.
	I just lerp between them by a speed.
*/

//texture for saving the current frame's focus
texture tFocus { Format=R16F; };
//texture for saving the last frame's focus
texture tLastFocus { Format=R16F; };

//samplers////////////////////////////////////////////////////////////////////////////////////////

//sampler for the current frame's focus
sampler sFocus { Texture=tFocus; };
//sampler for the last frame's focus
sampler sLastFocus { Texture=tLastFocus; };

//functions///////////////////////////////////////////////////////////////////////////////////////

//interpret the focus textures and separate far/near focuses
float getFocus(float2 coord, bool farOrNear) {
	float depth = ReShade::GetLinearizedDepth(coord);
	
	depth -= bLightDoF_AutoFocus ? tex2D(sFocus, 0).x : fLightDoF_ManualFocus;
	
	if (farOrNear) {
		depth = saturate(-depth * fLightDoF_Amount);
	}
	else {
		depth = saturate(depth * fLightDoF_Amount);
	}
	
	return depth;
}

//helper function for poisson-disk blur
float2 rot2D(float2 pos, float angle) {
	float2 source = float2(sin(angle), cos(angle));
	return float2(dot(pos, float2(source.y, -source.x)), dot(pos, source));
}

//poisson-disk blur
float3 poisson(sampler sp, float2 uv, float farOrNear, float CA) {
	float2 poisson[12];
	poisson[0]  = float2(-.326,-.406);
	poisson[1]  = float2(-.840,-.074);
	poisson[2]  = float2(-.696, .457);
	poisson[3]  = float2(-.203, .621);
	poisson[4]  = float2( .962,-.195);
	poisson[5]  = float2( .473,-.480);
	poisson[6]  = float2( .519, .767);
	poisson[7]  = float2( .185,-.893);
	poisson[8]  = float2( .507, .064);
	poisson[9]  = float2( .896, .412);
	poisson[10] = float2(-.322,-.933);
	poisson[11] = float2(-.792,-.598);
	
	float3 col = 0;
	float random = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
	float4 basis = float4(rot2D(float2(1, 0), random), rot2D(float2(0, 1), random));
	[unroll]
	for (int i = 0; i < 12; ++i) {
		float2 offset = poisson[i];
		offset = float2(dot(offset, basis.xz), dot(offset, basis.yw));
		
		if (bLightDoF_UseCA) {
			float2 rCoord = uv + offset * ReShade::PixelSize * fLightDoF_Width * (1.0 + CA);
			float2 gCoord = uv + offset * ReShade::PixelSize * fLightDoF_Width * (1.0 + CA * 0.5);
			float2 bCoord = uv + offset * ReShade::PixelSize * fLightDoF_Width;
			
			rCoord = lerp(uv, rCoord, getFocus(rCoord, farOrNear));
			gCoord = lerp(uv, gCoord, getFocus(gCoord, farOrNear));
			bCoord = lerp(uv, bCoord, getFocus(bCoord, farOrNear));
			
			col += 	float3(
						tex2Dlod(sp, float4(rCoord, 0, 0)).r,
						tex2Dlod(sp, float4(gCoord, 0, 0)).g,
						tex2Dlod(sp, float4(bCoord, 0, 0)).b
					);
		}
		else {
			float2 coord = uv + offset * ReShade::PixelSize * fLightDoF_Width;
			coord = lerp(uv, coord, getFocus(coord, farOrNear));
			col += tex2Dlod(sp, float4(coord, 0, 0)).rgb;
		}
		
	}
	return col * 0.083;
}

//shaders/////////////////////////////////////////////////////////////////////////////////////////

//far blur shader
float3 Far(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
	return poisson(ReShade::BackBuffer, uv, false, f2LightDoF_CA.x);
}

//near blur shader
float3 Near(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
	return poisson(ReShade::BackBuffer, uv, true, f2LightDoF_CA.y);
}

//shader to get the focus, kinda like center of confusion but less complicated
float GetFocus(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
	float2 linearMouse = f2LightDoF_MouseCoord * ReShade::PixelSize; //linearize the mouse position
	float2 focus = bLightDoF_UseMouseFocus ? linearMouse : f2Bokeh_AutoFocusCenter;
	return lerp(tex2D(sLastFocus, 0).x, ReShade::GetLinearizedDepth(focus), fLightDoF_AutoFocusSpeed);
}

//shader for saving this frame's focus to lerp with the next one's
float SaveFocus(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
	return tex2D(sFocus, 0).x;
}


//techniques//////////////////////////////////////////////////////////////////////////////////////

//this technique is dedicated for auto focus, so you don't need it if you're not using auto-focus :)
technique LightDoF_AutoFocus {
	pass GetFocus {
		VertexShader=PostProcessVS;
		PixelShader=GetFocus;
		RenderTarget=tFocus;
	}
	pass SaveFocus {
		VertexShader=PostProcessVS;
		PixelShader=SaveFocus;
		RenderTarget=tLastFocus;
	}
}

//technique for far blur
technique LightDoF_Far {
	pass Far {
		VertexShader=PostProcessVS;
		PixelShader=Far;
	}
}

//technique for near blur
technique LightDoF_Near {
	pass Near {
		VertexShader=PostProcessVS;
		PixelShader=Near;
	}
}

