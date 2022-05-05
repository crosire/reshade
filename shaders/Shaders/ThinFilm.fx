/*
ThinFilm PS (c) 2019 Jacob Maximilian Fober

This work is licensed under the Creative Commons 
Attribution 4.0 International License. 
To view a copy of this license, visit 
http://creativecommons.org/licenses/by/4.0/ 

For inquiries please contact jakub.m.fober@pm.me
*/

/*
This shader generates thin film interference based from view and normal angle.
The irridecensce formula was sourced from:
http://www.gamedev.net/page/resources/_/technical/graphics-programming-and-theory/thin-film-interference-for-computer-graphics-r2962
by Bacterius
*/

/*
Depth Map sampler is from ReShade.fxh by Crosire.
Normal Map generator is from DisplayDepth.fx by CeeJay.
Soft light blending mode is from pegtop.net
*/

// version 1.0.3


	  ////////////
	 /// MENU ///
	////////////

#include "ReShadeUI.fxh"

uniform int FOV < __UNIFORM_SLIDER_INT1
	ui_label = "Field of View (horizontal)";
	ui_min = 1; ui_max = 170;
	ui_category = "Depth settings";
	ui_category_closed = true;
> = 90;

uniform float FarPlane <
	ui_label = "Far Plane adjustment";
	ui_tooltip = "Adjust Normal Map strength";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1000.0; ui_step = 0.2;
	ui_category = "Depth settings";
> = 1000.0;

uniform bool Skip4Background <
	ui_label = "Show background";
	ui_tooltip = "Mask reflection for:\nDEPTH = 1.0";
	ui_category = "Depth settings";
> = true;

// If not using thin film noise texture
uniform int thick <
	ui_label = "Film thickness";
	ui_tooltip = "Thickness of the film, in nm";
	ui_type = "drag";
	ui_min = 0; ui_max = 1000; ui_step = 1;
	ui_category = "Irridescence settings";
> = 350;
/*
uniform int thicknessMin <
	ui_label = "Film thickness min";
	ui_tooltip = "Minimum thickness of the film, in nm";
	ui_type = "drag";
	ui_min = 0; ui_max = 1000; ui_step = 1;
	ui_category = "Irridescence settings";
> = 250;

uniform int thicknessMax <
	ui_label = "Film thickness max";
	ui_tooltip = "Minimum thickness of the film, in nm";
	ui_type = "drag";
	ui_min = 0; ui_max = 1000; ui_step = 1;
	ui_category = "Irridescence settings";
> = 400;

uniform float thickness < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Thickness noise";
	ui_tooltip = "Noise that determines the thickness of the film as fraction between the min and max";
	ui_min = 0.0; ui_max = 1.0;
	ui_category = "Irridescence settings";
> = 0.5;
*/

uniform float nmedium < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Refractive index of air";
	ui_tooltip = "Refractive index of the outer medium (typically air)";
	ui_min = 1.0; ui_max = 5.0;
	ui_category = "Irridescence settings";
> = 1.0;

uniform float nfilm < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Refractive index of thin film layer";
	ui_tooltip = "Refractive index of the thin film itself";
	ui_min = 1.0; ui_max = 5.0;
	ui_category = "Irridescence settings";
> = 1.5;

uniform float ninternal < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Refractive index of the lower material";
	ui_tooltip = "Refractive index of the material below the film";
	ui_min = 1.0; ui_max = 5.0;
	ui_category = "Irridescence settings";
> = 3.5;

uniform int BlendingMode <
	ui_label = "Mix mode";
	ui_type = "combo";
	ui_items = "Normal mode\0Multiply\0Screen\0Overlay\0Soft Light\0";
	ui_category = "Blending options";
> = 2;

uniform bool LumaBlending <
	ui_label = "Luminance based mixing";
	ui_category = "Blending options";
> = true;

uniform float BlendingAmount < __UNIFORM_SLIDER_FLOAT1
	ui_label = "Blend";
	ui_tooltip = "Blend with background";
	ui_min = 0.0; ui_max = 1.0;
	ui_category = "Blending options";
> = 1.0;

#include "ReShade.fxh"


	  //////////////////////
	 /// BLENDING MODES ///
	//////////////////////

float3 Multiply(float3 A, float3 B){ return A*B; }

float3 Screen(float3 A, float3 B){ return A+B-A*B; } // By JMF

float3 Overlay(float3 A, float3 B)
{
	// Algorithm by JMF
	float3 HL[4] = { max(A, 0.5), max(B, 0.5), min(A, 0.5), min(B, 0.5) };
	return ( HL[0] + HL[1] - HL[0]*HL[1] + HL[2]*HL[3] )*2.0 - 1.5;
}

float3 SoftLight(float3 A, float3 B)
{
	float3 A2 = pow(A, 2.0);
	return 2.0*A*B+A2-2.0*A2*B;
}

// RGB to YUV709 luma
float LumaMask(float3 Color)
{
	static const float3 Luma709 = float3(0.2126, 0.7152, 0.0722);
	return dot(Luma709, Color);
}


	  //////////////
	 /// SHADER ///
	//////////////

// Get depth function from ReShade.fxh with custom Far Plane
float GetDepth(float2 TexCoord)
{
	#if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
		TexCoord.y = 1.0 - TexCoord.y;
	#endif
	float Depth = tex2Dlod( ReShade::DepthBuffer, float4(TexCoord, 0, 0) ).x;
	
	#if RESHADE_DEPTH_INPUT_IS_LOGARITHMIC
		const float C = 0.01;
		Depth = (exp(Depth * log(C + 1.0)) - 1.0) / C;
	#endif
	#if RESHADE_DEPTH_INPUT_IS_REVERSED
		Depth = 1.0 - Depth;
	#endif

	const float N = 1.0;
	Depth /= FarPlane - Depth * (FarPlane - N);

	return Depth;
}

// Normal map (OpenGL oriented) generator from DisplayDepth.fx
float3 NormalVector(float2 texcoord)
{
	float3 offset = float3(ReShade::PixelSize.xy, 0.0);
	float2 posCenter = texcoord.xy;
	float2 posNorth  = posCenter - offset.zy;
	float2 posEast   = posCenter + offset.xz;

	float3 vertCenter = float3(posCenter - 0.5, 1.0) * GetDepth(posCenter);
	float3 vertNorth  = float3(posNorth - 0.5,  1.0) * GetDepth(posNorth);
	float3 vertEast   = float3(posEast - 0.5,   1.0) * GetDepth(posEast);

	return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5;
}

/* Amplitude reflection coefficient (s-polarized) */
float rs(float n1, float n2, float cosI, float cosT)
{ return (n1 * cosI - n2 * cosT) / (n1 * cosI + n2 * cosT); }

/* Amplitude reflection coefficient (p-polarized) */
float rp(float n1, float n2, float cosI, float cosT)
{ return (n2 * cosI - n1 * cosT) / (n1 * cosT + n2 * cosI); }

/* Amplitude transmission coefficient (s-polarized) */
float ts(float n1, float n2, float cosI, float cosT)
{ return 2.0 * n1 * cosI / (n1 * cosI + n2 * cosT); }

/* Amplitude transmission coefficient (p-polarized) */
float tp(float n1, float n2, float cosI, float cosT)
{ return 2.0 * n1 * cosI / (n1 * cosT + n2 * cosI); }

// cosI is the cosine of the incident angle, that is, cos0 = dot(view angle, normal)
// lambda is the wavelength of the incident light (e.g. lambda = 510 for green, 650 for red, 475 for blue)
// From http://www.gamedev.net/page/resources/_/technical/graphics-programming-and-theory/thin-film-interference-for-computer-graphics-r2962
float thinFilmReflectance(float cos0, float lambda, float thickness, float n0, float n1, float n2) {
	const float PI = radians(180.0);

	// compute the phase change term (constant)
	float d10 = (n1 > n0) ? 0.0 : PI;
	float d12 = (n1 > n2) ? 0.0 : PI;
	float delta = d10 + d12;

	// now, compute cos1, the cosine of the reflected angle
	float sin1 = pow(n0 / n1, 2.0) * (1.0 - pow(cos0, 2.0));
	if(sin1 > 1.0) return 1.0; // total internal reflection
	float cos1 = sqrt(1.0 - sin1);

	// compute cos2, the cosine of the final transmitted angle, i.e. cos(theta_2)
	// we need this angle for the Fresnel terms at the bottom interface
	float sin2 = pow(n0 / n2, 2.0) * (1.0 - pow(cos0, 2.0));
	if(sin2 > 1.0) return 1.0; // total internal reflection
	float cos2 = sqrt(1.0 - sin2);

	// get the reflection transmission amplitude Fresnel coefficients
	float alpha_s = rs(n1, n0, cos1, cos0) * rs(n1, n2, cos1, cos2); // rho_10 * rho_12 (s-polarized)
	float alpha_p = rp(n1, n0, cos1, cos0) * rp(n1, n2, cos1, cos2); // rho_10 * rho_12 (p-polarized)

	float beta_s = ts(n0, n1, cos0, cos1) * ts(n1, n2, cos1, cos2); // tau_01 * tau_12 (s-polarized)
	float beta_p = tp(n0, n1, cos0, cos1) * tp(n1, n2, cos1, cos2); // tau_01 * tau_12 (p-polarized)

	// compute the phase term (phi)
	float phi = (2.0 * PI / lambda) * (2.0 * n1 * thickness * cos1) + delta;

	// finally, evaluate the transmitted intensity for the two possible polarizations
	float ts = pow(beta_s, 2.0) / (pow(alpha_s, 2.0) - 2.0 * alpha_s * cos(phi) + 1.0);
	float tp = pow(beta_p, 2.0) / (pow(alpha_p, 2.0) - 2.0 * alpha_p * cos(phi) + 1.0);

	// we need to take into account conservation of energy for transmission
	float beamRatio = (n2 * cos2) / (n0 * cos0);

	// calculate the average transmitted intensity (if you know the polarization distribution of your
	// light source, you should specify it here. if you don't, a 50%/50% average is generally used)
	float t = beamRatio * (ts + tp) * 0.5;

	// and finally, derive the reflected intensity
	return 1.0 - t;
}

// Calculate dot product of view angle and normal
float GetReflectionCosine(float2 TexCoord)
{
	// Get aspect ratio
	float Aspect = ReShade::AspectRatio;

	// Sample normal pass
	float3 Normal = NormalVector(TexCoord);
	// Center normal coordinates
	Normal.xy = Normal.xy * 2.0 - 1.0;

	// Get ray vector from camera to the visible geometry
	float3 CameraRay;
	CameraRay.xy = TexCoord * 2.0 - 1.0;
	CameraRay.y /= Aspect; // Correct aspect ratio
	CameraRay.z = 1.0 / tan(radians(FOV*0.5)); // Scale frustum Z position from FOV

	// Get dot product of view angle and normal
	return abs( dot(CameraRay, Normal) );
}

float3 ThinFilmPS(float4 vois : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	if(Skip4Background) if( GetDepth(texcoord)==1.0 ) return tex2D(ReShade::BackBuffer, texcoord).rgb;

	float3 Display = tex2D(ReShade::BackBuffer, texcoord).rgb;
	float3 Irridescence;

	float cos0 = GetReflectionCosine(texcoord);
//	float thick = thicknessMin*(1.0-thickness) + thicknessMax*thickness;

	Irridescence.r = thinFilmReflectance(cos0, 650.0, thick, nmedium, nfilm, ninternal); // Red wavelength
	Irridescence.g = thinFilmReflectance(cos0, 510.0, thick, nmedium, nfilm, ninternal); // Green wavelength
	Irridescence.b = thinFilmReflectance(cos0, 475.0, thick, nmedium, nfilm, ninternal); // Blue wavelength

	Irridescence = saturate(Irridescence);

	if(BlendingAmount == 1.0 && !bool(BlendingMode) && !LumaBlending) return Irridescence;

	switch(BlendingMode)
	{
		case 1:{ Irridescence = Multiply(Irridescence, Display); break; } // Multiply
		case 2:{ Irridescence = Screen(Irridescence, Display); break; } // Screen
		case 3:{ Irridescence = Overlay(Irridescence, Display); break; } // Overlay
		case 4:{ Irridescence = SoftLight(Irridescence, Display); break; } // Soft Light
	}

	if(BlendingAmount == 1.0 && !LumaBlending) return Irridescence;
	else if(LumaBlending) return lerp(Display, Irridescence, LumaMask(Display) * BlendingAmount);
	return lerp(Display, Irridescence, BlendingAmount);
}


	  //////////////
	 /// OUTPUT ///
	//////////////

technique ThinFilm < ui_label = "Thin-film (iridescence)";
ui_tooltip = "Apply thin-film interference (iridescence) 'holographic' effect..."; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ThinFilmPS;
	}
}
