/*
	Adapted from
	http://filmicworlds.com/blog/filmic-tonemapping-with-piecewise-power-curves/

	Work-in-progress, still not correct.
*/

//#region Preprocessor

#include "ReShade.fxh"
#include "ReShadeUI.fxh"

#ifndef PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS
#define PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS 0
#endif

//#endregion

//#region Constants

static const float EPSILON = 1e-3;

//#endregion

//#region Uniforms

#if PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS

uniform float2 Point0
<
	__UNIFORM_SLIDER_FLOAT2

	ui_min = 0.0;
	ui_max = 1.0;
> = float2(0.25, 0.25);

uniform float2 Point1
<
	__UNIFORM_SLIDER_FLOAT2

	ui_min = 0.0;
	ui_max = 1.0;
> = float2(0.75, 0.75);

uniform float Whitepoint
<
	__UNIFORM_SLIDER_FLOAT1

	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float2 Overshoot
<
	__UNIFORM_SLIDER_FLOAT2

	ui_min = 0.0;
	ui_max = 1.0;
> = float2(0.0, 0.0);

#else

uniform int FixWhitepoint
<
	__UNIFORM_COMBO_INT1

	ui_label = "Fix Whitepoint";
	ui_tooltip = "Default: Divide by curve(white)";
	ui_items = "Off\0Divide by curve(white)\0Multiply by curve_inv(white)\0";
> = 1;

uniform float ToeStrength
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Toe Strength";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.0;

uniform float ToeLength
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Toe Length";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.5;

uniform float ShoulderStrength
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Shoulder Strength";
	ui_min = 0.0;
	ui_max = 3.0;
> = 0.0;

uniform float ShoulderLength
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Shoulder Length";
	ui_min = EPSILON;
	ui_max = 1.0;
> = 0.5;

uniform float ShoulderAngle
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Shoulder Angle";
	ui_min = 0.0;
	ui_max = 1.0;
> = 0.0;

#endif

uniform float Gamma
<
	__UNIFORM_SLIDER_FLOAT1

	ui_label = "Gamma";
	ui_min = 1.0;
	ui_max = 5.0;
> = 1.0;

//#endregion

//#region Textures

sampler BackBuffer
{
	Texture = ReShade::BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	//SRGBTexture = true;
};

/*texture PiecewiseFilmicTonemap_HDRBackBufferTex <pooled = true;>
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGB10A2;
};
sampler HDRBackBuffer
{
	Texture = PiecewiseFilmicTonemap_HDRBackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	//SRGBTexture = true;
};*/

//#endregion

//#region Data Types

//#region CurveParamsDirect
struct CurveParamsDirect
{
	float2 p0, p1;
	float w;
	float2 overshoot;
	float gamma;
};

CurveParamsDirect CurveParamsDirect_new()
{
	CurveParamsDirect self;
	self.p0 = 0.25;
	self.p1 = 0.75;
	self.w = 1.0;
	self.gamma = 1.0;
	self.overshoot = 0.0;
	return self;
}

CurveParamsDirect CurveParamsDirect_new(
	float2 p0, float2 p1, float w, float2 overshoot, float gamma)
{
	CurveParamsDirect self;
	self.p0 = p0;
	self.p1 = p1;
	self.w = w;
	self.gamma = gamma;
	self.overshoot = overshoot;
	return self;
}
//#endregion

//#region CurveSegment
struct CurveSegment
{
	float2 offset, scale;
	float ln_a, b;
};

CurveSegment CurveSegment_new()
{
	CurveSegment self;
	self.offset = 0.0;
	self.scale = 1.0;
	self.ln_a = 0.0;
	self.b = 1.0;
	return self;
}

CurveSegment CurveSegment_new(float2 offset, float2 scale, float ln_a, float b)
{
	CurveSegment self;
	self.offset = offset;
	self.scale = scale;
	self.ln_a = ln_a;
	self.b = b;
	return self;
}

float CurveSegment_eval(CurveSegment self, float x)
{
	float2 p0 = float2((x - self.offset.x) * self.scale.x, 0.0);

	if (p0.x > 0.0)
		p0.y = exp(self.ln_a + self.b * log(p0.x));
	
	return p0.y * self.scale.y + self.offset.y;
}

float CurveSegment_eval_inv(CurveSegment self, float y)
{
	float2 p0 = float2(0.0, (y - self.offset.y) / self.scale.y);

	if (p0.y > 0.0)
		p0.x = exp((log(p0.y) - self.ln_a) / self.b);
	
	return p0.x / self.scale.x + self.offset.x;
}
//#endregion

//#region FullCurve
struct FullCurve
{
	float w, inv_w;
	float2 p0, p1;

	// CurveSegment[3]
	float2 offset[3], scale[3];
	float ln_a[3], b[3];
};

CurveSegment FullCurve_get_segment(FullCurve self, int i)
{
	return CurveSegment_new(
		self.offset[i], self.scale[i], self.ln_a[i], self.b[i]);
}

void FullCurve_set_segment(inout FullCurve self, int i, CurveSegment seg)
{
	self.offset[i] = seg.offset;
	self.scale[i] = seg.scale;
	self.ln_a[i] = seg.ln_a;
	self.b[i] = seg.b;
}

FullCurve FullCurve_new()
{
	FullCurve self;
	self.w = 1.0;
	self.inv_w = 1.0;
	self.p0 = 0.25;
	self.p1 = 0.75;

	[unroll]
	for (int i = 0; i < 3; ++i)
		FullCurve_set_segment(self, i, CurveSegment_new());

	return self;
}

float FullCurve_eval(FullCurve self, float x)
{
	float norm_x = x * self.inv_w;
	int index = (norm_x < self.p0.x) ? 0 : ((norm_x < self.p1.x) ? 1 : 2);
	CurveSegment segment = FullCurve_get_segment(self, index);
	return CurveSegment_eval(segment, norm_x);
}

float FullCurve_eval_inv(FullCurve self, float y)
{
	int index = (y < self.p0.y) ? 0 : ((y < self.p1.y) ? 1 : 2);
	CurveSegment segment = FullCurve_get_segment(self, index);
	
	float norm_x = CurveSegment_eval_inv(segment, y);
	return norm_x * self.w;
}
//#endregion

//#endregion

//#region Functions

// Returns float2(ln_a, b)
float2 solve_a_b(float2 p0, float m)
{
	float b = (m * p0.x) / p0.y;
	float ln_a = log(p0.y) - b * log(p0.x);
	return float2(ln_a, b);
}

// Returns float2(m, b)
float2 as_slope_intercept(float2 p0, float2 p1)
{
	float2 d = p1 - p0;
	float m = (d.x == 0.0) ? 1.0 : d.y / d.x;
	float b = p0.y - p0.x * m;
	return float2(m, b);
}

float eval_derivative_linear_gamma(float m, float b, float g, float x)
{
	return g * m * pow(abs(m * x + b), g - 1.0);
}

FullCurve create_curve(CurveParamsDirect params)
{
	FullCurve curve = FullCurve_new();
	curve.w = params.w;
	curve.inv_w = rcp(params.w);

	//params.w = 1.0;
	params.p0.x /= params.w;
	params.p1.x /= params.w;
	params.overshoot.x /= params.w;

	float toe_m = 0.0;
	float shoulder_m = 0.0;
	float endpoint_m = 0.0;
	{
		float2 m_b = as_slope_intercept(params.p0, params.p1);
		float g = params.gamma;

		CurveSegment mid_segment = CurveSegment_new(
			float2(-(m_b.y / m_b.x), 0.0), 1.0, g * log(m_b.x), g);
		
		FullCurve_set_segment(curve, 1, mid_segment);

		toe_m = eval_derivative_linear_gamma(m_b.x, m_b.y, g, params.p0.x);
		shoulder_m = eval_derivative_linear_gamma(m_b.x, m_b.y, g, params.p1.x);

		params.p0.y = max(EPSILON, pow(abs(params.p0.y), params.gamma));
		params.p1.y = max(EPSILON, pow(abs(params.p1.y), params.gamma));

		params.overshoot.y = pow(abs(1.0 + params.overshoot.y), params.gamma) - 1.0;
	}

	curve.p0 = params.p0;
	curve.p1 = params.p1;

	{
		CurveSegment toe_segment = CurveSegment_new();
		toe_segment.offset = 0.0;
		toe_segment.scale = 1.0;

		float2 a_b = solve_a_b(params.p0, toe_m);
		toe_segment.ln_a = a_b.x;
		toe_segment.b = a_b.y;
		FullCurve_set_segment(curve, 0, toe_segment);
	}

	{
		CurveSegment shoulder_segment = CurveSegment_new();

		float2 p0 = (1.0 + params.overshoot) - params.p1;

		float2 a_b = solve_a_b(p0, shoulder_m);

		shoulder_segment.offset = 1.0 + params.overshoot;

		shoulder_segment.scale = -1.0;
		shoulder_segment.ln_a = a_b.x;
		shoulder_segment.b = a_b.y;

		FullCurve_set_segment(curve, 2, shoulder_segment);
	}

	{
		float scale = CurveSegment_eval(FullCurve_get_segment(curve, 2), 1.0);
		float inv_scale = rcp(scale);

		curve.offset[0].y *= inv_scale;
		curve.scale[0].y *= inv_scale;

		curve.offset[1].y *= inv_scale;
		curve.scale[1].y *= inv_scale;

		curve.offset[2].y *= inv_scale;
		curve.scale[2].y *= inv_scale;
	}

	return curve;
}

#if !PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS

CurveParamsDirect calc_direct_params_from_user()
{
	CurveParamsDirect params = CurveParamsDirect_new();

	float perceptual_gamma = 2.2;
	float toe_length = pow(abs(ToeLength), perceptual_gamma);

	params.p0.x = toe_length * 0.5;
	params.p0.y = (1.0 - ToeStrength) * params.p0.x;

	float remaining_y = 1.0 - params.p0.y;

	float initial_w  = params.p0.x + remaining_y;

	float y1_offset = (1.0 - ShoulderLength) * remaining_y;
	params.p1 = params.p0 + y1_offset;

	float extra_w = exp2(ShoulderStrength) - 1.0;

	params.w = initial_w + extra_w;

	params.gamma = Gamma;

	params.overshoot = float2(
		(params.w * 2.0) * ShoulderAngle * ShoulderStrength,
		0.5 * ShoulderAngle * ShoulderStrength);

	return params;
}

#endif

float3 apply_filmic_curve(float3 color)
{
	#if PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS

	CurveParamsDirect params = CurveParamsDirect_new(
		Point0, Point1, Whitepoint, Overshoot, Gamma);

	#else

	CurveParamsDirect params = calc_direct_params_from_user();

	#endif

	FullCurve curve = create_curve(params);

	color = float3(
		FullCurve_eval(curve, color.r),
		FullCurve_eval(curve, color.g),
		FullCurve_eval(curve, color.b));
	
	#if !PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS
	switch (FixWhitepoint)
	{
		case 1:
			color /= FullCurve_eval(curve, 1.0);
			break;
		case 2:
			color *= FullCurve_eval_inv(curve, 1.0);
			break;
	}
	#endif

	return color;
}

float3 inv_reinhard(float3 color, float inv_max) 
{
	return (color / max(1.0 - color, inv_max));
}

//#endregion

//#region Shaders

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float4 color = tex2D(BackBuffer, uv);
	//float inv_max_hdr = rcp(ShoulderLength);
	//color.rgb = inv_reinhard(color.rgb, inv_max_hdr);

	color.rgb = apply_filmic_curve(color.rgb);

	return color;
}

/*float4 ToLDRPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	return tex2D(HDRBackBuffer, uv);
}*/

float4 DebugPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	#if PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS

	CurveParamsDirect params = CurveParamsDirect_new(
		Point0, Point1, Whitepoint, Overshoot, Gamma);

	#else

	CurveParamsDirect params = calc_direct_params_from_user();

	#endif

	FullCurve curve = create_curve(params);

	float4 color = tex2D(ReShade::BackBuffer, uv);
	float y = 1.0 - uv.y;

	y *= 2.0;

	float2 p0 = step(y, params.p0);
	p0.x *= step(uv.x, 0.01);
	p0.y *= step(0.01, uv.x) * step(uv.x, 0.02);
	color.rgb = lerp(color.rgb, float3(1.0, 0.0, 0.0), p0.x);
	color.rgb = lerp(color.rgb, float3(0.0, 1.0, 0.0), p0.y);

	float2 p1 = step(y, params.p1);
	p1.x *= step(0.02, uv.x) * step(uv.x, 0.03);
	p1.y *= step(0.03, uv.x) * step(uv.x, 0.04);
	color.rgb = lerp(color.rgb, float3(1.0, 0.0, 0.5), p1.x);
	color.rgb = lerp(color.rgb, float3(0.0, 1.0, 0.5), p1.y);

	float w = step(y, params.w);
	w *= step(0.04, uv.x) * step(uv.x, 0.05);
	color.rgb = lerp(color.rgb, 1.0, w);

	float2 overshoot = step(y, params.overshoot);
	overshoot.x *= step(0.05, uv.x) * step(uv.x, 0.06);
	overshoot.y *= step(0.06, uv.x) * step(uv.x, 0.07);
	color.rgb = lerp(color.rgb, float3(1.0, 0.0, 1.0), overshoot.x);
	color.rgb = lerp(color.rgb, float3(0.0, 1.0, 1.0), overshoot.y);

	float gamma = step(y, (params.gamma - 1.0) / 4.0);
	gamma *= step(0.07, uv.x) * step(uv.x, 0.08);
	color.rgb  = lerp(color.rgb, float3(1.0, 1.0, 0.0), gamma);

	float ruler = y % 0.025;
	ruler = step(0.024, ruler);
	ruler *= step(uv.x, 0.1);
	color.rgb = lerp(color.rgb, 1.0 - color.rgb, ruler);

	float x = FullCurve_eval(curve, uv.x);

	#if !PIECEWISE_FILMIC_TONEMAP_USE_DIRECT_PARAMS
	switch (FixWhitepoint)
	{
		case 1:
			x /= FullCurve_eval(curve, 1.0);
			break;
		case 2:
			x *= FullCurve_eval_inv(curve, 1.0);
			break;
	}
	#endif
	
	y *= 0.5;

	float2 ps = ReShade::PixelSize * 4.0;
	float line_ = step(x, y + ps.y) * step(y - ps.y, x);

	color.rgb = lerp(color.rgb, 1.0 - color.rgb, line_);

	return color;
}

//#endregion

//#region Technique

technique PiecewiseFilmicTonemap
{
	pass Main
	{
		VertexShader = PostProcessVS;
		PixelShader = MainPS;
		//RenderTarget = PiecewiseFilmicTonemap_HDRBackBufferTex;
		SRGBWriteEnable = true;
	}
	/*pass ToLDR
	{
		VertexShader = PostProcessVS;
		PixelShader = ToLDRPS;
	}*/
}

technique PiecewiseFilmicTonemap_Debug
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = DebugPS;
	}
}

//#endregion