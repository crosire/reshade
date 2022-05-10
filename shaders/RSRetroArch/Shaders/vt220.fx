#include "ReShade.fxh"

uniform int charX <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_WIDTH;
	ui_step = 1;
	ui_label = "Character Width [VT220 Term]";
> = 80;

uniform int charY <
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1;
	ui_label = "Character Height [VT220 Term]";
> = 24;

uniform float WIDTH <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 5.0;
	ui_step = 0.001;
	ui_label = "Width [VT220 Term]";
> = 0.48;

uniform float HEIGHT <
	ui_type = "drag";
	ui_min = 0.1;
	ui_max = 5.0;
	ui_step = "0.001";
	ui_label = "Height [VT220 Term]";
> = 0.3;

uniform float CURVE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 30.0;
	ui_step = 0.001;
	ui_label = "Curvature [VT220 Term]";
> = 3.0;

uniform float3 BEZEL_COL <
	ui_type = "color";
	ui_label = "Bezel Color [VT220 Term]";
> = float3(0.8, 0.8, 0.6);

uniform float3 PHOSPHOR_COL <
	ui_type = "color";
	ui_label = "Phosphor Color [VT220 Term]";
> = float3(0.2, 1.0, 0.2);

uniform float SHINE <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Brightness [VT220 Term]";
> = 0.66;

uniform float AMBIENT <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 2.0;
	ui_step = 0.001;
	ui_label = "Ambient Light [VT220 Term]";
> = 0.33;

#define NO_OF_LINES ReShade::ScreenSize.y*HEIGHT

uniform float SMOOTH <
	ui_type = "drag";
	ui_min = 0.001;
	ui_max = 0.1;
	ui_step = 0.001;
	ui_label = "Smoothness [VT220 Term]";
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

void PS_VT220(in float4 pos : SV_POSITION, in float2 txcoord : TEXCOORD0, out float4 fColor : COLOR0)
{
    float4 c = float4(0.0, 0.0, 0.0, 0.0);
    
	float2 pixel = txcoord * ReShade::ScreenSize.xy;
    float2 uv = pixel / ReShade::ScreenSize.xy;
	// aspect-ratio correction
	float2 aspect = float2(1.,ReShade::ScreenSize.y/ReShade::ScreenSize.x);
	uv = 0.5 + (uv -0.5)/ aspect.yx;
    
    float r = CURVE;
        
    // Screen Layer
    float2 uvS = crtCurve(uv, r);

    // Screen Content           
    float2 uvC = (uvS - 0.5)* 2.0; // screen content coordinate system
    uvC *= float2(0.5/WIDTH, 0.5/HEIGHT);
    uvC = (uvC / 2.0) + 0.5;
     
	float val = colorFetch(uvC);
                
  	c += float4(PHOSPHOR_COL,0.0) * val * 2.0 *
    	smoothstep(SMOOTH/2.0, -SMOOTH/2.0, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH-1E-3, HEIGHT-1E-3), 1.0E-10));
    
    // Shine
    c += max(0.0, SHINE - distance(uv, float2(0.5, 1.0))) *
        smoothstep(SMOOTH/2.0, -SMOOTH/2.0, roundSquare(uvS-float2(0.5, 0.47), float2(WIDTH, HEIGHT), 0.05));
    	
    // Ambient
    c += max(0.0, AMBIENT - 0.5*distance(uvS, float2(0.5,0.5))) *
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH, HEIGHT), 0.05));
  

    // Enclosure Layer
    float2 uvE = crtCurve(uv, r+0.25);
    
    float4 b = float4(0.0, 0.0, 0.0,0.0);
    for(int i=0; i<12; i++)
    	b += (clamp(float4(BEZEL_COL,0.0) + rand(uvE+float(i))*0.05-0.025, 0., 1.) +
        	rand(uvE+1.0+float(i))*0.25 * cos((uv.x-0.5)*3.1415*1.5))/12.;        
    
    // Inner Border
    c += b/3.*( 1. + smoothstep(0.5*HEIGHT/WIDTH-0.025, 0.5*HEIGHT/WIDTH+0.025, abs(atan2(uvS.x-0.5, uvS.y-0.5))/3.1415) 
         + smoothstep(0.5*HEIGHT/WIDTH+0.025, 0.5*HEIGHT/WIDTH-0.025, abs(atan2(uvS.x-0.5, 0.5-uvS.y))/3.1415) )* 
        smoothstep(-SMOOTH, SMOOTH, roundSquare(uvS-float2(0.5, 0.5), float2(WIDTH, HEIGHT), 0.05)) * 
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05));
    // Shine
  	c += (b - 0.4)* 
        smoothstep(-SMOOTH*2.0, SMOOTH*2.0, roundSquare(uvE-float2(0.5, 0.505), float2(WIDTH, HEIGHT) + 0.05, 0.05)) * 
        smoothstep(SMOOTH*2.0, -SMOOTH*2.0, roundSquare(uvE-float2(0.5, 0.495), float2(WIDTH, HEIGHT) + 0.05, 0.05));

    // Outer Border
    c += b * 
       	smoothstep(-SMOOTH, SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.05, 0.05)) * 
        smoothstep(SMOOTH, -SMOOTH, roundSquare(uvE-float2(0.5, 0.5), float2(WIDTH, HEIGHT) + 0.15, 0.05)); 

    // Shine
    c += (b - 0.4)* 
       	smoothstep(-SMOOTH*2.0, SMOOTH*2.0, roundSquare(uvE-float2(0.5, 0.495), float2(WIDTH, HEIGHT) + 0.15, 0.05)) * 
        smoothstep(SMOOTH*2.0, -SMOOTH*2.0, roundSquare(uvE-float2(0.5, 0.505), float2(WIDTH, HEIGHT) + 0.15, 0.05)); 

    fColor = c;
}

technique VT220Term {
    pass VT220 {
        VertexShader=PostProcessVS;
        PixelShader=PS_VT220;
    }
}