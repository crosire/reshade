#include "ReShade.fxh"

uniform float _Glitch <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Glitch [Glitch2]";
	ui_tooltip = "Glitch Intensity";
> = 1.0;

uniform float Timer < source = "timer"; >;

float hash( float n )
{
	return frac( sin(n) * 43812.175489);
}

float noise( float2 p ) 
{
	float2 pi = floor( p );
	float2 pf = frac( p );
	float n = pi.x + 59.0 * pi.y;
	pf = pf * pf * (3.0 - 2.0 * pf);
	return lerp( 
	lerp( hash( n ), hash( n + 1.0 ), pf.x ),
	lerp( hash( n + 59.0 ), hash( n + 1.0 + 59.0 ), pf.x ),
	pf.y );
}

float noise( float3 p ) 
{

	float3 pi = floor( p );
	float3 pf = frac( p );
	float n = pi.x + 59.0 * pi.y + 256.0 * pi.z;
	pf.x = pf.x * pf.x * (3.0 - 2.0 * pf.x);
	pf.y = pf.y * pf.y * (3.0 - 2.0 * pf.y);
	pf.z = sin( pf.z );

	float v1 = 	lerp(
	lerp( hash( n ), hash( n + 1.0 ), pf.x ),
	lerp( hash( n + 59.0 ), hash( n + 1.0 + 59.0 ), pf.x ),
	pf.y );

	float v2 = 	lerp(
	lerp( hash( n + 256.0 ), hash( n + 1.0 + 256.0 ), pf.x ),
	lerp( hash( n + 59.0 + 256.0 ), hash( n + 1.0 + 59.0 + 256.0 ), pf.x ),
	pf.y );

	return lerp( v1, v2, pf.z );
}

float4 PS_Glitch2(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target {

	float2 uv = texcoord.xy;
	uv -= 0.5;
	
	float _TimeX = Timer*0.001;
	
	_TimeX = _TimeX / 30.0;
	_TimeX = 0.5 + 0.5 * sin( _TimeX * 6.238 );
	_TimeX = tex2D( ReShade::BackBuffer, float2(0.5,0.5) ).x; 

	if( _TimeX < 0.2 ) uv *= 1.0;
	else if( _TimeX < 0.4 )
	{
	uv.x += 100.55;
	uv *= 0.00005;
	}
	else if( _TimeX < 0.6 )
	{
	uv *= 0.00045;
	}
	else if( _TimeX < 0.8 ) 
	{
	uv *= 500000.0;
	}
	else if( _TimeX < 1.0 ) 
	{
	uv *= 0.000045;
	}


	float fft = tex2D( ReShade::BackBuffer, float2(uv.x,0.25) ).x; 
	float ftf = tex2D( ReShade::BackBuffer, float2(uv.x,0.15) ).x; 
	float fty = tex2D( ReShade::BackBuffer, float2(uv.x,0.35) ).x; 
	uv *= 200.0 * sin( log( fft ) * 10.0 );

	if( sin( fty ) < 0.5 ) uv.x += sin( fty ) * sin( cos( _TimeX ) + uv.y * 40005.0 ) ;
	uv *= sin( _TimeX * 179.0 );

	float3 p;
	p.x = uv.x;
	p.y = uv.y;
	p.z = sin( 0.0 * _TimeX * ftf );

	float no = noise(p);

	float3 col = float3( 
	hash( no * 6.238  * cos( _TimeX ) ), 
	hash( no * 6.2384 + 0.4 * cos( _TimeX * 2.25 ) ), 
	hash( no * 6.2384 + 0.8 * cos( _TimeX * 0.8468 ) ) );

	float b = dot( uv, uv );
	b *= 10000.0;
	b = b * b;
	float3 res = tex2D(ReShade::BackBuffer, texcoord.xy).rgb;
	col.rgb = lerp(res, col.rgb * res, _Glitch);

	return  float4(col,1.0);

}

//techniques///////////////////////////////////////////////////////////////////////////////////////////

technique Glitch2 {
	pass Glitch2 {
		VertexShader=PostProcessVS;
		PixelShader=PS_Glitch2;
	}
}