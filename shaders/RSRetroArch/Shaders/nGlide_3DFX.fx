#include "ReShade.fxh"

uniform float f3DFX_Gamma <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 10.0;
	ui_step = 0.01;
	ui_label = "Gamma [3DFX Dither]";
> = 1.3;

uniform bool b3DFX_Filter <
	ui_label = "Enable RAMDAC Video filter [3DFX Dither]";
> = true;
			  
static const int4x4 dithmat = int4x4(int4(0,12,3,15),
									 int4(8,4,11,7),
									 int4(2,14,1,13),
									 int4(10,6,9,5)
									);
									
int4 Dither(float2 inpos, float4 incolor)
{
	// DITHER 4x4
	int3 outcolor = int3(incolor.rgb * 255.0);

	#if (__RENDERER__ == 0x09300)
		outcolor.r = floor(outcolor.r * 2) + floor(outcolor.r / 128) - floor(outcolor.r / 16);
		outcolor.g = floor(outcolor.g * 4) + floor(outcolor.g / 64) - floor(outcolor.g / 16);
		outcolor.b = floor(outcolor.b * 2) + floor(outcolor.b / 128) - floor(outcolor.b / 16);
	#else
		outcolor.r = (outcolor.r << 1) + (outcolor.r >> 7) - (outcolor.r >> 4);
		outcolor.g = (outcolor.g << 2) + (outcolor.g >> 6) - (outcolor.g >> 4);
		outcolor.b = (outcolor.b << 1) + (outcolor.b >> 7) - (outcolor.b >> 4);
	#endif

	int2 dithpos = int2(inpos * tex2Dsize(ReShade::BackBuffer,0));

	outcolor += dithmat[dithpos.x % 4][dithpos.y % 4]; //outcolor += dithmat[dithpos.x & 3][dithpos.y & 3];

	#if (__RENDERER__ == 0x09300)
		outcolor.r = floor(outcolor.r / 16) * 8;
		outcolor.g = floor(outcolor.g / 16) * 4;
		outcolor.b = floor(outcolor.r / 16) * 8;
	#else
		outcolor.r = ((outcolor.r >> 4) << 3);
		outcolor.g = ((outcolor.g >> 4) << 2);
		outcolor.b = ((outcolor.b >> 4) << 3);
	#endif

	return int4(clamp(outcolor, 0, 255), 0);
}


void PS_n3DFX(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD0, out float4 col : COLOR0)
{
	// DITHER 4x4
	float2 texelsize = 1.0/tex2Dsize(ReShade::BackBuffer,0);
	float4 color_out;

	float4 tex_lmf = tex2Dlod(ReShade::BackBuffer, float4(uv - float2(texelsize.x*2,0.0),0.0,0.0));
	float4 tex_lnf = tex2Dlod(ReShade::BackBuffer, float4(uv - float2(texelsize.x,0.0),0.0,0.0));
	float4 tex_ctf = tex2Dlod(ReShade::BackBuffer, float4(uv,0.0,0.0));
	float4 tex_rnf = tex2Dlod(ReShade::BackBuffer, float4(uv + float2(texelsize.x,0.0),0.0,0.0));

	int4 tex_lm = Dither(uv - float2(texelsize.x*2, 0), tex_lmf);
	int4 tex_ln = Dither(uv - float2(texelsize.x, 0), tex_lnf);
	int4 tex_ct = Dither(uv, tex_ctf);
	int4 tex_rn = Dither(uv + float2(texelsize.x, 0), tex_rnf);

	// VIDEO FILTER 4x1
	if (b3DFX_Filter != 0)
	{
		int4 thresh = int4(0x18, 0x0C, 0x18, 0);
		int4 dif_rn = tex_rn - tex_ct;
		int4 dif_lm = tex_lm - tex_ct;
		int4 dif_ln = tex_ln - tex_ct;

		tex_ct *= 4;

		if(dif_rn.r >= -thresh.r && dif_rn.r <= thresh.r) tex_ct.r += dif_rn.r;
		if(dif_rn.g >= -thresh.g && dif_rn.g <= thresh.g) tex_ct.g += dif_rn.g;
		if(dif_rn.b >= -thresh.b && dif_rn.b <= thresh.b) tex_ct.b += dif_rn.b;

		if(dif_lm.r >= -thresh.r && dif_lm.r <= thresh.r) tex_ct.r += dif_lm.r;
		if(dif_lm.g >= -thresh.g && dif_lm.g <= thresh.g) tex_ct.g += dif_lm.g;
		if(dif_lm.b >= -thresh.b && dif_lm.b <= thresh.b) tex_ct.b += dif_lm.b;

		if(dif_ln.r >= -thresh.r && dif_ln.r <= thresh.r) tex_ct.r += dif_ln.r;
		if(dif_ln.g >= -thresh.g && dif_ln.g <= thresh.g) tex_ct.g += dif_ln.g;
		if(dif_ln.b >= -thresh.b && dif_ln.b <= thresh.b) tex_ct.b += dif_ln.b;

		tex_ct /= 4;
	}

	tex_ctf.rgb = float3(tex_ct.rgb / 255.0);
	tex_ctf.a = 1.0;

	color_out = tex_ctf;

	// GAMMA CORRECTION
	float gamma = f3DFX_Gamma;
	color_out.rgb = pow(color_out.rgb, 1.0/gamma.xxx);
	col = color_out;
}

technique nGlide_3DFX
{
	pass nGlide_3DFX
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_n3DFX;
	}
}