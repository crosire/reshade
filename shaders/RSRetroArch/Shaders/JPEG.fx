#include "ReShade.fxh"

uniform float fCompressionStrentgh <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 6.0;
	ui_step = 0.001;
	ui_label = "Compression Strength [JPEG]";
	ui_tooltip = "How compressed the image is.";
> = 1.0;

texture JPEG0_tex {
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA32F;
};

sampler sJPEG0 {
	Texture = JPEG0_tex;
};

#define pi 2.0*asin(1.0)
#define a(x) (x!=0.?1.:1./sqrt(2.))

float3 toYCbCr(float3 rgb)
{
    float3 RGB2Y =  float3( 0.299, 0.587, 0.114);
    float3 RGB2Cb = float3(-0.169,-0.331, 0.500);
    float3 RGB2Cr = float3(0.500,-0.419,-0.081);
	float3 newmat = float3(dot(rgb, RGB2Y), dot(rgb, RGB2Cb), dot(rgb, RGB2Cr));
	float3 addit = float3(0.0,0.5,0.5);
    return newmat+addit;
}

float3 pre( float2 coord ){
    return floor(256.0*(toYCbCr(tex2D(ReShade::BackBuffer, coord/ReShade::ScreenSize.xy).xyz) - .5));
}

float3 DCT8x8( float2 coord, float2 uv ) {
    float3 res = float3(0,0,0);
    for(float x = 0.; x < 8.; x++){
    	for(float y = 0.; y < 8.; y++){
            res += pre(coord + float2(x,y)) *
                cos((2.*x+1.)*uv.x*pi/16.) *
                cos((2.*y+1.)*uv.y*pi/16.);
    	}
    }
    return res * .25 * a(uv.x) * a(uv.y);
}

void PS_JPEG1(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 col : COLOR0)
{
    col.w = 0.;
	float2 texcoord = txcoord * ReShade::ScreenSize;
    float2 uv = floor(texcoord-8.0*floor(texcoord/8.0));
	int CombmA, mA[64] = {  16,  11,  10,  16,  24,  40,  51,  61,
                12,  12,  14,  19,  26,  58,  60,  55,
                14,  13,  16,  24,  40,  57,  69,  56,
                14,  17,  22,  29,  51,  87,  80,  62,
                18,  22,  37,  56,  68, 109, 103,  77,
                24,  35,  55,  64,  81, 104, 113,  92,
                49,  64,  78,  87, 103, 121, 120, 101,
                72,  92,  95,  98, 112, 100, 103,  99
              };
	//uv range needs to be set to range of array. from lowest number to max value.
    float q = (1.0+fCompressionStrentgh*20.0)*float(CombmA = mA[int(uv.x+uv.y*8.)]);
    col.xyz = (floor(.5+DCT8x8(8.*floor(texcoord/8.0),uv)/q))*q;
}


float3 toRGB(float3 ybr) {
    return mul( float3x3(
        1., 0.00,     1.402,
        1.,-0.344136,-0.714136,
        1., 1.772,    0.00), ybr-float3(0,.5,.5));
}

float3 inp(float2 coord){
    return tex2Dfetch(sJPEG0, float4(int2(coord.xy),0,0)).xyz;
}

float3 IDCT8x8(float2 coord, float2 xy ) {
    float3 res = float3(0.0,0.0,0.0);
    for(float u = 0.; u < 8.0; u++){
    	for(float v = 0.; v < 8.0; v++){
            res += inp(coord + float2(u,v)) *
                a(u) * a(v) *
                cos((2.*xy.x+1.)*u*pi/16.) *
                cos((2.*xy.y+1.)*v*pi/16.);
    	}
    }
    return res * .25;
}


void PS_JPEG2(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 col : COLOR0)
{
    col.w = 0.;
	float2 texcoord = txcoord * ReShade::ScreenSize;
    float2 uv = floor(texcoord-8.0*floor(texcoord/8.0));
    col.xyz = toRGB(IDCT8x8(8.*floor(texcoord/8.),uv)/256.+.5);
	//col.xyz = tex2D(ReShade::BackBuffer, texcoord/ReShade::ScreenSize.xy).xyz;
}

technique JPEGCompression {
    pass JPEG_Base {
        VertexShader=PostProcessVS;
        PixelShader=PS_JPEG1;
		RenderTarget=JPEG0_tex;
    }
	pass JPEG_Final {
        VertexShader=PostProcessVS;
        PixelShader=PS_JPEG2;
    }
}