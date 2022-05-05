#include "ReShade.fxh"

uniform float _Value <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Value [OilPaintHQ]";
> = 2.0;

float4 PS_OilPaintHQ(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

	float2 src_size = float2 (_Value/ ReShade::ScreenSize.x, _Value / ReShade::ScreenSize.y);
	float2 uv = texcoord.xy;
	float n = 25.;

	float3 m0 = 0.0;  float3 m2 = 0.0; 
	float3 s0 = 0.0;  float3 s2 = 0.0; 
	float3 c;

	c = tex2D(ReShade::BackBuffer, uv + float2(-4.,-4.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(0,0) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-3.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(1,0) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-2.,-2.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(2,0) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-1.,-4.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(3,0) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,0-4.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(4.,0) * src_size).rgb; m2 += c; s2 += c * c;

	c = tex2D(ReShade::BackBuffer, uv + float2(-4.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,1.) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-3.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(1.,1.) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-2.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(2.,1.) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-1.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(3.,1.) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(4.,1) * src_size).rgb; m2 += c; s2 += c * c;

	c = tex2D(ReShade::BackBuffer, uv + float2(-4.,-2.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(0,2) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-3.,-2.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(1,2) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-2.,-3.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(2,2) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-1.,-2.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(3,2) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,-2.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(4,2) * src_size).rgb; m2 += c; s2 += c * c;

	c = tex2D(ReShade::BackBuffer, uv + float2(-4.,-1.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(0,3) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-3.,-1.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(1,3) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-2.,-1.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(2,3) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-1.,-1.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(3,3) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,-1.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(4,3) * src_size).rgb; m2 += c; s2 += c * c;	 

	c = tex2D(ReShade::BackBuffer, uv + float2(-4.,0.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(0,4) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-3.,0.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(1,4) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-2.,0.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(2,4) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(-1.,0.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(3,4) * src_size).rgb; m2 += c; s2 += c * c;
	c = tex2D(ReShade::BackBuffer, uv + float2(0.,0.) * src_size).rgb; m0 += c; s0 += c * c;	
	c = tex2D(ReShade::BackBuffer, uv + float2(4,4) * src_size).rgb; m2 += c; s2 += c * c;

	float4 ccolor = 0.;

	float min_sigma2 = 1e+2;
	m0 /= n;
	s0 = abs(s0 / n - m0 * m0);


	float sigma2 = s0.r + s0.g + s0.b;
	if (sigma2 < min_sigma2) {
	min_sigma2 = sigma2;
	ccolor = float4(m0, 1.0);
	}


	m2 /= n;
	s2 = abs(s2 / n - m2 * m2);

	sigma2 = s2.r + s2.g + s2.b;
	if (sigma2 < min_sigma2) {
	min_sigma2 = sigma2;
	ccolor = float4(m2, 1.0);
	}

	return ccolor;

}


//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique OilPaintHQ {
	pass OilPaintHQ {
		VertexShader=PostProcessVS;
		PixelShader=PS_OilPaintHQ;
	}
}