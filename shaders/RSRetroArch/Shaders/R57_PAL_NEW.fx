#include "ReShade.fxh"

//RETROARCH

// NES PAL composite signal simulation for RetroArch
// shader by r57shell
// thanks to feos & HardWareMan & NewRisingSun

// LICENSE: PUBLIC DOMAIN

uniform float video_sizeX <
	ui_type = "drag";
	ui_min = 1.00;
	ui_max = BUFFER_WIDTH;
	ui_label = "Frame Width [PAL-R57-NEW]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 320.00;

uniform float video_sizeY <
	ui_type = "drag";
	ui_min = 1.00;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Frame Height [PAL-R57-NEW]";
	ui_tooltip = "This should be sized according to the video data in the texture (If you're using emulators, set this to the Emu video frame size, otherwise, keep it the same as Screen Width/Height or Fullscreen res.)";
> = 240.00;

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.00;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [PAL-R57-NEW]";
> = 320.00;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.00;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [PAL-R57-NEW]";
> = 240.00;

float fmod(float a, float b)
{
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

#define video_size float2(video_sizeX, video_sizeY)
#define texture_size float2(texture_sizeX, texture_sizeY)

// Quality considerations

// there are three main options:
// USE_RAW (R), USE_DELAY_LINE (D), USE_COLORIMETRY (C)
// here is table of quality in decreasing order:
// RDC, RD, RC, DC, D, C

// TWEAKS start

static const float3x3 RGB_to_XYZ = float3x3(
	0.4306190, 0.3415419, 0.1783091,
	0.2220379, 0.7066384, 0.0713236,
	0.0201853, 0.1295504, 0.9390944
);

static const float3x3 XYZ_to_sRGB = float3x3(
	 3.2406, -1.5372, -0.4986,
	-0.9689,  1.8758,  0.0415,
	 0.0557, -0.2040,  1.0570
);

// TWEAKS end

uniform float Gamma <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_step = 0.003125;
	ui_label = "PAL Gamma [PAL-R57-NEW]";
> = 2.5;

uniform float Brightness <
	ui_type = "drag";
	ui_min = -1.0;
	ui_max = 2.0;
	ui_step = 0.003125;
	ui_label = "PAL Brightness [PAL-R57-NEW]";
> = 0.00;

uniform float Contrast <
	ui_type = "drag";
	ui_min = -1.00;
	ui_max = 2.00;
	ui_step = 0.003125;
	ui_label = "PAL Contrast [PAL-R57-NEW]";
> = 1.00;

uniform float Saturation <
	ui_type = "drag";
	ui_min = -1.00;
	ui_max = 2.0;
	ui_step = 0.003125;
	ui_label = "PAL Saturation [PAL-R57-NEW]";
> = 1.00;

uniform float HueShift <
	ui_type = "drag";
	ui_min = -6.0;
	ui_max = 6.0;
	ui_step = 0.0015625;
	ui_label = "PAL Hue Shift [PAL-R57-NEW]";
> = -2.50;

uniform float HueRotation <
	ui_type = "drag";
	ui_min = -5.0;
	ui_max = 5.0;
	ui_step = 0.0015625;
	ui_label = "PAL Hue Rotation [PAL-R57-NEW]";
> = 2.00;

uniform int Ywidth <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 32;
	ui_step = 1;
	ui_label = "PAL Y Width [PAL-R57-NEW]";
> = 12;

uniform int Uwidth <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 32;
	ui_step = 1;
	ui_label = "PAL U Width [PAL-R57-NEW]";
> = 23;
uniform int Vwidth <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 32;
	ui_step = 1;
	ui_label = "PAL V Width [PAL-R57-NEW]";
> = 23;

uniform int TV_Pixels <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 2400;
	ui_step = 1;
	ui_label = "PAL TV Pixels [PAL-R57-NEW]";
> = 200;

uniform int SizeX <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 4096;
	ui_step = 1;
	ui_label = "Active Width [PAL-R57-NEW]";
> = 256;

uniform int SizeY <
	ui_type = "drag";
	ui_min = 1;
	ui_max = 4096;
	ui_step = 1;
	ui_label = "Active Height [PAL-R57-NEW]";
> = 240;

uniform float dark_scanline <
	ui_type = "drag";
	ui_min = 0.00;
	ui_max = 1.00;
	ui_step = 0.0025;
	ui_label = "PAL Scanline [PAL-R57-NEW]";
> = 0.05;

uniform float Phase_Y <
	ui_type = "drag";
	ui_min = 0.00;
	ui_max = 12.0;
	ui_step = 0.0025;
	ui_label = "PAL Phase Y [PAL-R57-NEW]";
> = 2.00;

uniform float Phase_One <
	ui_type = "drag";
	ui_min = 0.00;
	ui_max = 12.0;
	ui_step = 0.0025;
	ui_label = "PAL Phase One [PAL-R57-NEW]";
> = 0.00;

uniform float Phase_Two <
	ui_type = "drag";
	ui_min = 0.00;
	ui_max = 12.0;
	ui_step = 0.0025;
	ui_label = "PAL Phase Two [PAL-R57-NEW]";
> = 8.0;

static const float Mwidth = 24;

static const int Ywidth_static = 1;
static const int Uwidth_static = 1;
static const int Vwidth_static = 1;

static const float Contrast_static = 1.;
static const float Saturation_static = 1.;

static const float YUV_u = 0.492;
static const float YUV_v = 0.877;

static const float3x3 RGB_to_YUV = float3x3(
	float3( 0.299, 0.587, 0.114), //Y
	float3(-0.299,-0.587, 0.886)*YUV_u, //B-Y
	float3( 0.701,-0.587,-0.114)*YUV_v); //R-Y

static const float DeltaV = 1.;

static const float comb_line = 1.0;

static const float RGB_y = Contrast_static/Ywidth_static/DeltaV;
static const float RGB_u = comb_line*Contrast_static*Saturation_static/YUV_u/Uwidth_static/DeltaV;
static const float RGB_v = comb_line*Contrast_static*Saturation_static/YUV_v/Vwidth_static/DeltaV;

static const float3x3 YUV_to_RGB = float3x3(
	float3(1., 1., 1.)*RGB_y,
	float3(0., -0.114/0.587, 1.)*RGB_u,
	float3(1., -0.299/0.587, 0.)*RGB_v
);

static const float pi = 3.1415926535897932384626433832795;

float sinn(float x)
{
	return sin(/*fmod(x,24)*/x*(pi*2./24.));
}

float coss(float x)
{
	return cos(/*fmod(x,24)*/x*(pi*2./24.));
}

float3 monitor(sampler2D tex, float2 p)
{
	const float2 size = float2(SizeX,SizeY);
	// align vertical coord to center of texel
	float2 uv = float2(p.x+p.x*(Ywidth/8.)/size.x,(floor(p.y*texture_size.y)+0.5)/texture_size.y);
	float2 sh = (video_size/texture_size/size)*float2(14./10.,-1.0);
	float2 pc = uv*texture_size/video_size*size*float2(10.,1.);
	float alpha = dot(floor(float2(pc.x,pc.y)),float2(2.,Phase_Y*2.));
	alpha += Phase_One*2.;

	// 1/size.x of screen in uv coords = video_size.x/texture_size.x/size.x;
	// then 1/10*size.x of screen:
	float ustep = video_size.x/texture_size.x/size.x/10.;

	float border = video_size.x/texture_size.x;
	float ss = 2.0;

	#define PAL_SWITCH(A) A > 1.
	if (PAL_SWITCH(fmod(uv.y*texture_size.y/video_size.y*size.y,2.0)))
	{
		// cos(pi-alpha) = -cos(alpha)
		// sin(pi-alpha) = sin(alpha)
		// pi - alpha
		alpha = -alpha+12012.0;
		ss = -2.0;
	}

	float ysum = 0., usum = 0., vsum = 0.;
	for (int i=0; i<Mwidth; ++i)
	{
		float4 res = tex2D(tex, uv);
		
		float3 yuv = mul(RGB_to_YUV, res.xyz);
		const float a1 = alpha+(HueShift+2.5)*2.0-yuv.x*ss*HueRotation;
		float sig = yuv.x+dot(yuv.yz,sign(float2(sinn(a1),coss(a1))));
		
		float4 res1 = tex2D(tex, uv+sh);
		float3 yuv1 = mul(RGB_to_YUV, res1.xyz);
		const float a2 = (HueShift+2.5)*2.0+12012.0-alpha+yuv.x*ss*HueRotation;
		float sig1 = yuv1.x+dot(yuv1.yz,sign(float2(sinn(a2),coss(a2))));
		
		if (i < Ywidth)
			ysum += sig;

		if (i < Uwidth)
			usum += (sig+sig1)*sinn(alpha);
		if (i < Vwidth)
			vsum += (sig-sig1)*coss(alpha);
		alpha -= ss;
		uv.x -= ustep;
	}

	ysum *= Contrast/Ywidth;
	usum *= Contrast*Saturation/Uwidth;
	vsum *= Contrast*Saturation/Vwidth;

	float3 rgb = mul(float3(ysum+Brightness*Ywidth_static,usum,vsum), YUV_to_RGB);

	float3 rgb1 = saturate(rgb);
	rgb = pow(rgb1, Gamma);

	// size of pixel screen in texture coords:
	//float output_pixel_size = video_size.x/(output_size.x*texture_size.x);

	// correctness check
	//if (fmod(p.x*output_pixel_size,2.0) < 1.0)
	//	rgb = float3(0.,0.,0.);

	float3 xyz1 = mul(RGB_to_XYZ,rgb);
	float3 srgb = saturate(mul(XYZ_to_sRGB,xyz1));
	float3 a1 = 12.92*srgb;
	float3 a2 = 1.055*pow(srgb,1/2.4)-0.055;
	float3 ssrgb = (srgb<float3(0.0031308,0.0031308,0.0031308)?a1:a2);
	return ssrgb;
}

float4 PS_R57PAL_NEW(float4 vpos : SV_Position, float2 tex : TexCoord) : SV_Target
{
	return float4(monitor(ReShade::BackBuffer, tex), 1.0);
}

technique R57PALNEW {
	pass R57PALNEW {
		VertexShader=PostProcessVS;
		PixelShader=PS_R57PAL_NEW;
	}
}