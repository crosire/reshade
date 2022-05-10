#include "ReShade.fxh"

#ifndef MAIN_BLOOM_ITERATIONS_DX9
	#define MAIN_BLOOM_ITERATIONS_DX9 10
#endif

#ifndef REFLECTION_BLUR_ITERATIONS_DX9
	#define REFLECTION_BLUR_ITERATIONS_DX9 10
#endif

uniform int charX <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_WIDTH;
	ui_step = 1;
	ui_label = "Character Width [VT220 Term Night]";
> = 80;

uniform int charY <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1;
	ui_label = "Character Height [VT220 Term Night]";
> = 24;

uniform int MAIN_BLOOM_ITERATIONS <
	ui_type = "drag";
	ui_min = 2;
	ui_max = 255;
	ui_step = 1;
	ui_label = "Bloom Iterations [VT220 Term Night]";
> = 10;

uniform float MAIN_BLOOM_SIZE <
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Bloom Size [VT220 Term Night]";
> = 0.01;

uniform int REFLECTION_BLUR_ITERATIONS <
	ui_type = "drag";
	ui_min = 2;
	ui_max = 255;
	ui_step = 1;
	ui_label = "Reflection Blur Iterations [VT220 Term Night]";
> = 10;

uniform float REFLECTION_BLUR_SIZE<
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 1.0;
	ui_step = 0.001;
	ui_label = "Reflection Blur Size [VT220 Term Night]";
> = 0.05;

uniform float WIDTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Width [VT220 Term Night]";
> = 0.48;

uniform float HEIGHT <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = "0.001";
	ui_label = "Height [VT220 Term Night]";
> = 0.3;

uniform float CURVE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 30.0;
	ui_step = 0.001;
	ui_label = "Curvature [VT220 Term Night]";
> = 3.0;

uniform float3 BEZEL_COL <
	ui_type = "color";
	ui_label = "Bezel Color [VT220 Term Night]";
> = float3(0.8, 0.8, 0.6);

uniform float3 PHOSPHOR_COL <
	ui_type = "color";
	ui_label = "Phosphor Color [VT220 Term Night]";
> = float3(0.2, 1.0, 0.2);

uniform float SHINE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Brightness [VT220 Term Night]";
> = 0.66;

uniform float AMBIENT <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Ambient Light [VT220 Term Night]";
> = 0.33;

#define NO_OF_LINES ReShade::ScreenSize.y*HEIGHT

uniform float SMOOTH <
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 0.1;
	ui_step = 0.001;
	ui_label = "Smoothness [VT220 Term Night]";
> = 0.004;

// using normal floattors of a sphere with radius r
float2 crtCurve(float2 uv, float r) 
{
        uv = (uv - 0.5) * 2.0;// uv is now -1 to 1
    	uv = r*uv/sqrt(r*r -dot(uv, uv));
        uv = (uv / 2.0) + 0.5;// back to 0-1 coords
        return uv;
}

float roundSquare(float2 p, float2 b, float r)
{
    return length(max(abs(p)-b,0.0))-r;
}

float rand(float2 co){
    return frac(sin(dot(co.xy ,float2(12.9898,78.233))) * 43758.5453);
}

// Calculate normal to distance function and move along
// normal with distance to get point of reflection
float2 borderReflect(float2 p, float r)
{
    float eps = 0.0001;
    float2 epsx = float2(eps,0.0);
    float2 epsy = float2(0.0,eps);
    float2 b = (1.+float2(r,r))* 0.5;
    r /= 3.0;
    
    p -= 0.5;
    float2 normal = float2(roundSquare(p-epsx,b,r)-roundSquare(p+epsx,b,r),
                       roundSquare(p-epsy,b,r)-roundSquare(p+epsy,b,r))/eps;
    float d = roundSquare(p, b, r);
    p += 0.5;
    return p + d*normal;
}

float colorFetch(float2 uv)
{
    if(uv.x < 0.0 || uv.x > 1.0 ||  uv.y < 0.0 || uv.y > 1.0) return 0.0;
    
	float scln = 0.5 - 0.5*cos(uv.y*3.14*NO_OF_LINES); // scanlines
	uv *= float2(charX, charY); // 80 by 24 characters
    uv = ceil(uv);
    uv /= float2(charX, charY);
	
	float3 color = tex2D(ReShade::BackBuffer, uv);
	float col = (color.x+color.y+color.z).xxx/3.0;
	return scln*floor(3.0*(0.5+0.499*sin(color)))/3.0; // vt220 has 2 intensitiy levels
}

void PS_VT220_Night(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 fColor : COLOR0)
{
    float4 c = float4(0.0, 0.0, 0.0, 0.0);
	
	int Bloom_Loop = MAIN_BLOOM_ITERATIONS;
	int Reflection_Loop = REFLECTION_BLUR_ITERATIONS;
	
	if (__RENDERER__ == 0x09300 ){
		Bloom_Loop = MAIN_BLOOM_ITERATIONS_DX9;
		Reflection_Loop = REFLECTION_BLUR_ITERATIONS_DX9;
	}
    
	float2 pixel = txcoord * ReShade::ScreenSize.xy;
    float2 uv = pixel / ReShade::ScreenSize.xy;
	// aspect-ratio correction
	float2 aspect = float2(1.,ReShade::ScreenSize.y/ReShade::ScreenSize.x);
	uv = 0.5 + (uv -0.5)/ aspect.yx;
    
    float r = CURVE;
        
    // Screen Layer
    float2 uvS = crtCurve(uv, r);
	float dx;
	float dy;

    // Screen Content           
    float2 uvC = (uvS - 0.5)* 2.0; // screen content coordinate system
    uvC *= float2(0.5/WIDTH, 0.5/HEIGHT);
    uvC = (uvC / 2.0) + 0.5;
	
	//Simple Bloom
	float B = float(MAIN_BLOOM_ITERATIONS*MAIN_BLOOM_ITERATIONS);
    for(int i=0; i<Bloom_Loop; i++)
    {
        dx = float(i-MAIN_BLOOM_ITERATIONS/2)*MAIN_BLOOM_SIZE;
        for(int j=0; j<Bloom_Loop; j++)
        {
            dy = float(j-MAIN_BLOOM_ITERATIONS/2)*MAIN_BLOOM_SIZE;
            c += float4(PHOSPHOR_COL,0.0) * colorFetch(uvC + float2(dx,dy))/B;
        }
    }
	
	float val = colorFetch(uvC);
	
	c += float4(PHOSPHOR_COL,0.0) * val; 
    
    // Shine
    c += max(0.0, SHINE - distance(uv, float2(0.5, 1.0))) *
        smoothstep(SMOOTH/2.0, -SMOOTH/2.0, roundSquare(uvS-float2(0.5, 0.47), float2(WIDTH, HEIGHT), 0.05));
    	
    // Ambient
    c += max(0.0, AMBIENT - 0.3*distance(uvS, float2(0.5,0.5))) *
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH, HEIGHT), 0.05));
  

    // Enclosure Layer
    float2 uvE = crtCurve(uv, r+0.25);
	float2 uvR = float2(0.0,0.0);
    
    // Inner Border
    for( int k=0; k<Reflection_Loop; k++)
    {
    	uvR = borderReflect(uvC + (float2(rand(uvC+float(k)), rand(uvC+float(k)+0.1))-0.5)*REFLECTION_BLUR_SIZE, 0.05) ;
    	c += (float4(PHOSPHOR_COL,0.0) - float4(BEZEL_COL,0.0)*AMBIENT) * colorFetch(uvR) / float(REFLECTION_BLUR_ITERATIONS) * 
	        smoothstep(-SMOOTH, SMOOTH, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH, HEIGHT), 0.05)) * 
			smoothstep(SMOOTH, -SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05));
    }
               
  	c += float4(BEZEL_COL,0.0) * AMBIENT * 0.7 *
        smoothstep(-SMOOTH, SMOOTH, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH, HEIGHT), 0.05)) * 
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05));

    // Corner
  	c -= float4(BEZEL_COL,0.0)* 
        smoothstep(-SMOOTH*2.0, SMOOTH*10.0, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05)) * 
        smoothstep(SMOOTH*2.0, -SMOOTH*2.0, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05));

    // Outer Border
    c += float4(BEZEL_COL,0.0) * AMBIENT *
       	smoothstep(-SMOOTH, SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05)) * 
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.15, 0.05));

    fColor = c;
}

technique VT220Term_Night {
    pass VT220_Night {
        VertexShader=PostProcessVS;
        PixelShader=PS_VT220_Night;
    }
}