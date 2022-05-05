//===================================================================================================================
#include				"Pirate_Lumachroma.fxh"
//===================================================================================================================
uniform float DOF_RADIUS <
	ui_label = "DOF - Radius";
	ui_tooltip = "1.0 = Pixel perfect radius. Values above 1.0 might create artifacts.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 10.0;
	> = 1.0;
uniform float DOF_NEAR_STRENGTH <
	ui_label = "DOF - Near Blur Strength";
	ui_tooltip = "Strength of the blur between the camera and focal point.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 0.5;
uniform float DOF_FAR_STRENGTH <
	ui_label = "DOF - Far Blur Strength";
	ui_tooltip = "Strength of the blur past the focal point.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 1.0;
uniform float DOF_FOCAL_RANGE <
	ui_label = "DOF - Focal Range";
	ui_tooltip = "Along with Focal Curve, this controls how much will be in focus";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.0;
uniform float DOF_FOCAL_CURVE <
	ui_label = "DOF - Focal Curve";
	ui_tooltip = "1.0 = No curve. Values above this put more things in focus, lower values create a macro effect.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 1.0;
uniform float DOF_HYPERFOCAL <
	ui_label = "DOF - Hyperfocal Range";
	ui_tooltip = "When the focus goes past this point, everything in the distance is focused.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.9;
uniform float DOF_BLEND <
	ui_label = "DOF - Blending Curve";
	ui_tooltip = "Controls the blending curve between the DOF texture and original image. Use this to avoid artifacts where the DOF begins";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.3;
uniform float DOF_BOKEH_CONTRAST <
	ui_label = "DOF - Bokeh - Contrast";
	ui_tooltip = "Contrast of bokeh and blurred areas. Use very small values.";
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.0;
	> = 0.04;
uniform float DOF_BOKEH_BIAS <
	ui_label = "DOF - Bokeh - Bias";
	ui_tooltip = "0.0 = No Bokeh, 1.0 = Natural bokeh, 2.0 = Forced bokeh.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 1.0;
uniform float DOF_MANUAL_FOCUS <
	ui_label = "DOF - Manual Focus";
	ui_tooltip = "Only works then manual focus is on.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.5;
uniform float DOF_FOCUS_X <
	ui_label = "DOF - Auto Focus X";
	ui_tooltip = "Horizontal point in the screen to focus. 0.5 = middle";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.5;
uniform float DOF_FOCUS_Y <
	ui_label = "DOF - Auto Focus Y";
	ui_tooltip = "Vertical point in the screen to focus. 0.5 = middle";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.5;
uniform float DOF_FOCUS_SPREAD <
	ui_label = "DOF - Auto Focus Spread";
	ui_tooltip = "Focus takes the average of 5 points, this is how far away they are. Use low values for a precise focus.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 0.5;
	> = 0.05;
uniform float DOF_FOCUS_SPEED <
	ui_label = "DOF - Auto Focus Speed";
	ui_tooltip = "How fast focus changes happen. 1.0 = One second. Values above 1.0 are faster, bellow are slower.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 10.0;
	> = 0.5;
uniform float DOF_SCRATCHES_STRENGTH <
	ui_label = "DOF - Lens Scratches Strength";
	ui_tooltip = "How strong is the scratch effect. Low values are better as this shows up a lot in bright scenes.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.15;
uniform int DOF_DEBUG <
	ui_label = "DOG - Debug - Show Focus";
	ui_tooltip = "Black is in focus, red is blurred.";
	ui_type = "combo";
	ui_items = "No\0Yes\0";
	> = 0;
//===================================================================================================================
#if (DOF_USE_MANUAL_FOCUS == 0)
texture2D	TexF1 {Width = 1; Height = 1; Format = R16F;};
sampler2D	SamplerFocalPoint {Texture = TexF1; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
texture2D	TexF2 {Width = 1; Height = 1; Format = R16F;};
sampler2D	SamplerFCopy {Texture = TexF2; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
#endif

#if DOF_P_WORD_NEAR
texture2D	TexFocus {Width = BUFFER_WIDTH * DOF_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DOF_TEXTURE_QUALITY; Format = R16F;};
sampler2D	SamplerFocus {Texture = TexFocus; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
#else
texture2D	TexFocus {Width = BUFFER_WIDTH * DOF_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DOF_TEXTURE_QUALITY; Format = R8;};
sampler2D	SamplerFocus {Texture = TexFocus; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
#endif

texture2D	TexDOF1 {Width = BUFFER_WIDTH * DOF_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DOF_TEXTURE_QUALITY; Format = RGBA8;};
sampler2D	SamplerDOF1 {Texture = TexDOF1; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
texture2D	TexDOF2 {Width = BUFFER_WIDTH * DOF_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DOF_TEXTURE_QUALITY; Format = RGBA8;};
sampler2D	SamplerDOF2 {Texture = TexDOF2; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

#if DOF_USE_LENS_SCRATCH
texture2D	TexLensScratches <source = DOF_SCRATCH_FILENAME;> { Width = DOF_SCRATCH_WIDTH; Height = DOF_SCRATCH_HEIGHT; Format = RGBA8;};
sampler2D	SamplerLensScratches {Texture = TexLensScratches; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
#endif
//===================================================================================================================
float2 Rotate60(float2 v) {
	#define sin60 0.86602540378f
	float x = v.x * 0.5 - v.y * sin60;
	float y = v.x * sin60 + v.y * 0.5;
	return float2(x, y);
}

float2 Rotate120(float2 v) {
	#define sin120 0.58061118421f
	float x = v.x * -0.5 - v.y * sin120;
	float y = v.x * sin120 + v.y * -0.5;
	return float2(x, y);
}

float2 Rotate(float2 v, float angle) {
	float x = v.x * cos(angle) - v.y * sin(angle);
	float y = v.x * sin(angle) + v.y * cos(angle);
	return float2(x, y);
}

float GetFocus(float d) {
	#if DOF_USE_MANUAL_FOCUS
	float focus = min(DOF_HYPERFOCAL, DOF_MANUAL_FOCUS);
	#else
	float focus = min(DOF_HYPERFOCAL, tex2D(SamplerFocalPoint, 0.5).x);
	#endif
	float res;
	if (d > focus) {
		res = smoothstep(focus, 1.0, d) * DOF_FAR_STRENGTH;
		res = lerp(res, 0.0, focus / DOF_HYPERFOCAL);
	} else if (d < focus) {
		res = smoothstep(focus, 0.0, d) * DOF_NEAR_STRENGTH;
	} else {
		res = 0.0;
	}
	
	res = pow(smoothstep(DOF_FOCAL_RANGE, 1.0, res), DOF_FOCAL_CURVE);
	#if DOF_P_WORD_NEAR
	res *= 1 - (d < focus) * 2;
	#endif
	
	return res;
}
float4 GenDOF(float2 texcoord, float2 v, sampler2D samp) 
{
	float4 origcolor = tex2D(samp, texcoord);
	float4 res = origcolor;
	res.w = LumaChroma(origcolor).w;
	
	#if DOF_P_WORD_NEAR
	float bluramount = abs(tex2D(SamplerFocus, texcoord).r);
	#else
	float bluramount = tex2D(SamplerFocus, texcoord).r;
	if (bluramount == 0) return origcolor;
	res.w *= bluramount;
	#endif
	#if (DOF_USE_MANUAL_FOCUS == 0)
	v = Rotate(v, tex2D(SamplerFocalPoint, 0.5).x * 2.0);
	#endif
	float4 bokeh = res;
	res.rgb *= res.w;
	
	#if DOF_P_WORD_NEAR
	float2 calcv = v * DOF_RADIUS * PixelSize / DOF_TEXTURE_QUALITY;
	float depths[DOF_TAPS * 2];
	[unroll] for(int ii=0; ii < DOF_TAPS; ii++)
	{
		float2 tapcoord = texcoord + calcv * (ii + 1);
		depths[ii * 2] = tex2Dlod(SamplerFocus, float4(tapcoord, 0, 0)).r;
		bluramount = (depths[ii * 2] < 0) ? max(bluramount, -depths[ii * 2]) : bluramount;
		tapcoord = texcoord - calcv * (ii + 1);
		depths[ii * 2 + 1] = tex2Dlod(SamplerFocus, float4(tapcoord, 0, 0)).r;
		bluramount = (depths[ii * 2 + 1] < 0) ? max(bluramount, -depths[ii * 2 + 1]) : bluramount;
	}
	float discradius = bluramount * DOF_RADIUS;
	calcv = v * discradius * PixelSize / DOF_TEXTURE_QUALITY;
	#else
	float discradius = bluramount * DOF_RADIUS;
	if (discradius < PixelSize.x / DOF_TEXTURE_QUALITY) return origcolor;
	float2 calcv = v * discradius * PixelSize / DOF_TEXTURE_QUALITY;
	#endif
	
	for(int i=1; i <= DOF_TAPS; i++)
	{

		// ++++
		float2 tapcoord = texcoord + calcv * i;

		float4 tap = tex2Dlod(samp, float4(tapcoord, 0, 0));
		
		#if DOF_P_WORD_NEAR
		tap.w = abs(depths[(i - 1) * 2]);
		tap.w *= LumaChroma(tap).w;
		#else
		tap.w = tex2Dlod(SamplerFocus, float4(tapcoord, 0, 0)).r * LumaChroma(tap).w;
		#endif

		//bokeh = lerp(bokeh, tap, (tap.w > bokeh.w));
		bokeh = lerp(bokeh, tap, (tap.w > bokeh.w) * tap.w);
		
		res.rgb += tap.rgb * tap.w;
		res.w += tap.w;
		
		// ----
		tapcoord = texcoord - calcv * i;

		tap = tex2Dlod(samp, float4(tapcoord, 0, 0));
		
		#if DOF_P_WORD_NEAR
		tap.w = abs(depths[(i - 1) * 2 + 1]);
		tap.w *= LumaChroma(tap).w;
		#else
		tap.w = tex2Dlod(SamplerFocus, float4(tapcoord, 0, 0)).r * LumaChroma(tap).w;
		#endif
		
		//bokeh = lerp(bokeh, tap, (tap.w > bokeh.w));
		bokeh = lerp(bokeh, tap, (tap.w > bokeh.w) * tap.w);

		res.rgb += tap.rgb * tap.w;
		res.w += tap.w;
		
	}
	
	res.rgb /= res.w;
	#if DOF_P_WORD_NEAR
	res.rgb = (discradius == 0) ? res.rgb : lerp(res.rgb, bokeh.rgb, saturate(bokeh.w * DOF_BOKEH_BIAS));
	#else
	res.rgb = lerp(res.rgb, bokeh.rgb, saturate(bokeh.w * DOF_BOKEH_BIAS));
	#endif
	res.w = 1.0;
	float4 lc = LumaChroma(res);
	lc.w = pow(lc.w, 1.0 + DOF_BOKEH_CONTRAST / 10.0);
	res.rgb = lc.rgb * lc.w;

	return res;
}
//===================================================================================================================
#if (DOF_USE_MANUAL_FOCUS == 0)
float PS_GetFocus (float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float lastfocus = tex2D(SamplerFCopy, 0.5).x;
	float res;
	
	const float2 offset[5]=
	{
		float2(0.0, 0.0),
		float2(0.0, -1.0),
		float2(0.0, 1.0),
		float2(1.0, 0.0),
		float2(-1.0, 0.0)
	};
	for(int i=0; i < 5; i++)
	{
		res += tex2D(SamplerND, float2(DOF_FOCUS_X, DOF_FOCUS_Y) + offset[i] * DOF_FOCUS_SPREAD).w;
	}
	res /= 5;
	res = lerp(lastfocus, res, DOF_FOCUS_SPEED * Frametime / 1000.0);
	return res;
}
float PS_CopyFocus (float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return tex2D(SamplerFocalPoint, 0.5).x;
}
#endif
float PS_GenFocus (float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GetFocus(tex2D(SamplerND, texcoord).w);
}
float4 PS_DOF1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GenDOF(texcoord, float2(1.0, 0.0), SamplerColor);
}
float4 PS_DOF2(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GenDOF(texcoord, Rotate60(float2(1.0, 0.0)), SamplerDOF1);
}
float4 PS_DOF3(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GenDOF(texcoord, Rotate120(float2(1.0, 0.0)), SamplerDOF2);
}
float4 PS_DOFCombine(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	#if DOF_P_WORD_NEAR
	float4 res = tex2D(SamplerDOF1, texcoord);
	float bluramount = abs(tex2D(SamplerFocus, texcoord).r);
	#if DOF_USE_LENS_SCRATCH
	res.rgb = lerp(res.rgb, BlendColorDodge(res.rgb, tex2D(SamplerLensScratches, texcoord).rgb), bluramount * LumaChroma(res).w * DOF_SCRATCHES_STRENGTH);
	#endif
	if (DOF_DEBUG) res.rgb = abs(tex2D(SamplerFocus, texcoord).r);
	return res;
	#else
	float bluramount = tex2D(SamplerFocus, texcoord).r;
	float4 orig = tex2D(SamplerColor, texcoord);
	
	float4 res;
	if (bluramount == 0.0) {
		res = orig;
	} else {
		res = lerp(orig, tex2D(SamplerDOF1, texcoord), smoothstep(0.0, DOF_BLEND, bluramount));
		#if DOF_USE_LENS_SCRATCH
		res.rgb = lerp(res.rgb, BlendColorDodge(res.rgb, tex2D(SamplerLensScratches, texcoord).rgb), bluramount * LumaChroma(res).w * DOF_SCRATCHES_STRENGTH);
		#endif
	}
	if (DOF_DEBUG) res = tex2D(SamplerFocus, texcoord);
	return res;
	#endif
}
//===================================================================================================================
technique Pirate_DOF
{
	#if (DOF_USE_MANUAL_FOCUS == 0)
	pass GetFocus
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_GetFocus;
		RenderTarget = TexF1;
	}
	pass CopyFocus
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_CopyFocus;
		RenderTarget = TexF2;
	}
	#endif
	pass FocalRange
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_GenFocus;
		RenderTarget = TexFocus;
	}
	pass DOF1
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_DOF1;
		RenderTarget = TexDOF1;
	}
	pass DOF2
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_DOF2;
		RenderTarget = TexDOF2;
	}
	pass DOF3
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_DOF3;
		RenderTarget = TexDOF1;
	}
	pass Combine
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_DOFCombine;
	}
}