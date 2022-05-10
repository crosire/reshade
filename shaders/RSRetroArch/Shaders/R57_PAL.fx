#include "ReShade.fxh"

// NES CRT simulation
// by r57shell
// thanks to feos & HardWareMan
// Ported to ReShade by Matsilagi

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [PAL-R57]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [PAL-R57]";
> = 240.0;

#define texture_size float2(texture_sizeX, texture_sizeY)
 
static const float pi = 3.142;

#define mod(x,y) (x-y*floor(x/y))
 
float3 monitor(sampler text, float2 p)
{
        float2 size = texture_size;
        float2 pos = floor(p*size);
        float2 uv = floor(pos)/size;
        float4 res = tex2D(text, uv);
        float3 yuv = mul(float3x3(
                0.2126, 0.7152, 0.0722,
                -0.09991, -0.33609, 0.436,
                0.615, -0.55861, -0.05639), res.xyz);
        float alpha = (floor(p.x*size.x*4.)/2.0)*pi;
        float2 sincv = float2(cos(alpha), sin(alpha));
        if (mod(pos.y + 5.,4.) < 2.)
                sincv.x = -sincv.x;
        if (mod(pos.y, 4.) >= 2.)
                sincv.y = -sincv.y;
        float mc = 1.+dot(sincv, yuv.zy)/yuv.x;
 
        /*float3 rgb = float3(
                yuv.x + 1.28033 * yuv.z,
                yuv.x - 0.21482 * yuv.y - 0.38059 * yuv.z,
                yuv.x + 2.12798 * yuv.y);*/
    return res.xyz*mc;
}
 
// pos (left corner, sample size)
float4 monitor_sample(sampler text, float2 p, float2 samp)
{
        // linear interpolation was...
        // now other thing.
        // http://imgur.com/m8Z8trV
        // AT LAST IT WORKS!!!!
        // going to check in retroarch...
        float2 size = texture_size;
        float2 next = float2(.25,1.)/size;
        float2 f = frac(float2(4.,1.)*size*p);
        samp *= float2(4.,1.)*size;
        float2 l;
        float2 r;
        if (f.x+samp.x < 1.)
        {
                l.x = f.x+samp.x;
                r.x = 0.;
        }
        else
        {
                l.x = 1.-f.x;
                r.x = min(1.,f.x+samp.x-1.);
        }
        if (f.y+samp.y < 1.)
        {
                l.y = f.y+samp.y;
                r.y = 0.;
        }
        else
        {
                l.y = 1.-f.y;
                r.y = min(1.,f.y+samp.y-1.);
        }
        float3 top = lerp(monitor(text, p), monitor(text, p+float2(next.x,0.)), r.x/(l.x+r.x));
        float3 bottom = lerp(monitor(text, p+float2(0.,next.y)), monitor(text, p+next), r.x/(l.x+r.x));
        return float4(lerp(top,bottom, r.y/(l.y+r.y)),1.0);
}
 
float4 PS_PALR57(float4 pos : SV_Position, float2 coords : TEXCOORD0) : SV_Target
{
        return monitor_sample(ReShade::BackBuffer, coords, 1./ReShade::ScreenSize);
}

technique PAL_R57
{
	pass PAL_R57_1
	{
		//RenderTarget = iChannel0_Tex;
		VertexShader = PostProcessVS;
		PixelShader = PS_PALR57;
	}
}