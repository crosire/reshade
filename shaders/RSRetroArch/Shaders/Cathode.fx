#include "ReShade.fxh"

//Cathode by nimitz (twitter: @stormoid)
//2017 nimitz All rights reserved

/*
	CRT simulation shadowmask style, I also have a trinitron version
	optimized for 4X scaling on a ~100ppi display.

	The "Scanlines" seen in the simulated picture are only a side effect of the phoshor placement
	and decay, instead of being artificially added on at the last step. This has the advantage

	I have done some testing and it performs especially well with "hard" input such a faked
	(dither based) transparency and faked specular highlights as seen in the bigger sprite.
	A version tweaked and made for 4k displays could look pretty close to the real thing.
*/

uniform float resSize <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 20.0;
	ui_step = 1.0;
	ui_label = "Resolution Size [Cathode]";
	ui_tooltip = "Size of the scanlines.";
> = 4.0;

#define mod(x,y) (x-y*floor(x/y))

//Phosphor decay
float decay(in float d)
{
    return lerp(exp2(-d*d*2.5-.3),0.05/(d*d*d*0.45+0.055),.65)*0.99;
}

//Phosphor shape
float sqd(in float2 a, in float2 b)
{
    a -= b;
    a *= float2(1.25,1.8)*.905;
    float d = max(abs(a.x), abs(a.y));
    d = lerp(d, length(a*float2(1.05, 1.))*0.85, .3);
    return d;
}

float3 phosphors(in float2 p, sampler2D tex, float2 frgCoord)
{   
    float3 col = float3(0.0,0.0,0.0);
    p -= 0.25;
    p.y += mod(frgCoord.x,2.)<1.?.03:-0.03;
    p.y += mod(frgCoord.x,4.)<2.?.02:-0.02;
    
	//5x5 kernel (this means a given fragment can be affected by a pixel 4 game pixels away)
    for(int i=-2;i<=2;i++)
    for(int j=-2;j<=2;j++)
    {
        float2 tap = floor(p) + 0.5 + float2(i,j);
		float3 rez = tex2D(tex, frgCoord.xy).rgb; //nearest neighbor
        
		//center points
        float rd = sqd(tap, p + float2(0.0,0.2));//distance to red dot
		const float xoff = .25;
        float gd = sqd(tap, p + float2(xoff,.0));//distance to green dot
        float bd = sqd(tap, p + float2(-xoff,.0));//distance to blue dot
		
        rez = pow(rez,float3(1.18,1.18,1.18))*1.08;
        rez.r *= decay(rd);
        rez.g *= decay(gd);
        rez.b *= decay(bd);
		
        col += rez;
    }
    return col;
}

void PS_Cathode(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 frgcol : COLOR0)
{
	float2 uv = txcoord.xy * ReShade::ScreenSize.xy;
	float2 p = uv.xy/ReShade::ScreenSize.xy;
    float2 f = uv.xy;
    
    float3 col = phosphors(uv.xy/resSize, ReShade::BackBuffer, txcoord.xy);
	frgcol = float4(col, 1.0);
}

technique Cathode {
	pass Cathode
	{	
		VertexShader = PostProcessVS;
		PixelShader = PS_Cathode;
	}
}