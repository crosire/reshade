// Amateur port to ReShade v3.x by XIIICaesar.
//
// Ported by IDDQD.
// Original code from Shadertoy
//
// VHS Tape Noise: Vladmir Storm (https://www.shadertoy.com/view/MlfSWr)
// VCR Distortion: ryk (https://www.shadertoy.com/view/ldjGzV)
// VHS Distortion: drmelon (https://www.shadertoy.com/view/4dBGzK)
// Dirty old CRT: Klowner (https://www.shadertoy.com/view/MsXGD4)
// NTSC Codec: UltraMoogleMan (https://www.shadertoy.com/view/ldXGRf)
//
// Posted by Matsilagi and further optimized for ReShade by crosire, MartyMcFly and Ganossa
// http://reshade.me/forum/shader-presentation/1258-vhs-shader
//
// Do not distribute without giving credit to the original author(s).
 
#ifndef sNoiseMode_TextureName
	#define sNoiseMode_TextureName "VHS_N1.jpg"
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 
uniform bool bVHSDistortGammaFix <
    ui_tooltip = "Fixes the darkening of the NTSC Shader";
> = false;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 
#include "ReShade.fxh"
 
texture texnoise2  < source = sNoiseMode_TextureName;  > {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8;};
 
sampler2D SamNoise
{
    Texture = texnoise2;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU = Wrap;
    AddressV = Wrap;
};
 
// Useful Constants
static const float Pi = 3.1415926535;
static const float Pi2 = 6.283185307;
 
// NTSC Constants
static const float4 A = 0.5;
static const float4 B = 0.5;
static const float P = 1.0;
static const float CCFrequency = 3.59754545;
static const float YFrequency = 6.0;
static const float IFrequency = 1.2;
static const float QFrequency = 0.6;
static const float NotchHalfWidth = 2.0;
static const float ScanTime = 52.6;
static const float MaxC = 2.1183;
static const float4 MinC = -1.1183;
static const float4 CRange = 3.2366;
 
uniform float Timer < source = "timer"; >;
 
#define t Timer.x*0.001
#define t2 Timer*0.001
 
#define mod(x,y) (x-y*floor(x/y)) 

float4 CompositeSample(float2 texcoord)
 {
    float2 InverseRes = 1.0 / ReShade::ScreenSize.xy;
    float2 InverseP = float2(P, 0.0) * InverseRes;
   
    // UVs for four linearly-interpolated samples spaced 0.25 texels apart
    float2 C0 = texcoord;
    float2 C1 = texcoord + ReShade::PixelSize.x * 0.25;
    float2 C2 = texcoord + ReShade::PixelSize.x * 0.50;
    float2 C3 = texcoord + ReShade::PixelSize.x * 0.75;
    float4 Cx = float4(C0.x, C1.x, C2.x, C3.x);
    float4 Cy = float4(C0.y, C1.y, C2.y, C3.y);
 
    float3 Texel0 = tex2D(ReShade::BackBuffer, C0).rgb;
    float3 Texel1 = tex2D(ReShade::BackBuffer, C1).rgb;
    float3 Texel2 = tex2D(ReShade::BackBuffer, C2).rgb;
    float3 Texel3 = tex2D(ReShade::BackBuffer, C3).rgb;
   
    // Calculated the expected time of the sample.
    float4 T = Cy * ReShade::ScreenSize.x + 0.5 + Cx;
 
    const float3 YTransform = float3(0.299, 0.587, 0.114);
    const float3 ITransform = float3(0.595716, -0.274453, -0.321263);
    const float3 QTransform = float3(0.211456, -0.522591, 0.311135);
 
    float Y0 = dot(Texel0.xyz, YTransform);
    float Y1 = dot(Texel1.xyz, YTransform);
    float Y2 = dot(Texel2.xyz, YTransform);
    float Y3 = dot(Texel3.xyz, YTransform);
    float4 Y = float4(Y0, Y1, Y2, Y3);
 
    float I0 = dot(Texel0.xyz, ITransform);
    float I1 = dot(Texel1.xyz, ITransform);
    float I2 = dot(Texel2.xyz, ITransform);
    float I3 = dot(Texel3.xyz, ITransform);
    float4 I = float4(I0, I1, I2, I3);
 
    float Q0 = dot(Texel0.xyz, QTransform);
    float Q1 = dot(Texel1.xyz, QTransform);
    float Q2 = dot(Texel2.xyz, QTransform);
    float Q3 = dot(Texel3.xyz, QTransform);
    float4 Q = float4(Q0, Q1, Q2, Q3); 
 
    float4 W = 6.283185307 * 3.59754545 * 52.6;
    float4 Encoded = Y + I * cos(T * W) + Q * sin(T * W);
    return (Encoded + 1.1183) / 3.2366;
}
 
float4 NTSCCodec(float2 texcoord)
{
    float4 YAccum = 0.0;
    float4 IAccum = 0.0;
    float4 QAccum = 0.0;
    float QuadXSize = ReShade::ScreenSize.x * 4.0;
    float TimePerSample = ScanTime / QuadXSize;
   
    // Frequency cutoffs for the individual portions of the signal that we extract.
    // Y1 and Y2 are the positive and negative frequency limits of the notch filter on Y.
    //
    float Fc_y1 = (CCFrequency - NotchHalfWidth) * TimePerSample;
    float Fc_y2 = (CCFrequency + NotchHalfWidth) * TimePerSample;
    float Fc_y3 = YFrequency * TimePerSample;
    float Fc_i = IFrequency * TimePerSample;
    float Fc_q = QFrequency * TimePerSample;
    float Pi2Length = Pi2 / 82.0;
    float4 NotchOffset = float4(0.0, 1.0, 2.0, 3.0);
    float4 W = Pi2 * CCFrequency * ScanTime;
    for(float n = -41.0; n < 42.0; n += 4.0)
    {
        float4 n4 = n + NotchOffset  + 0.00001;
        float4 CoordX = texcoord.x + ReShade::PixelSize.x * n4 * 0.25;
        float4 CoordY = texcoord.y;
        float2 TexCoord = float2(CoordX.r, CoordY.r);
        float4 C = CompositeSample(TexCoord) * CRange + MinC;
        float4 WT = W * (CoordX  + A * CoordY * 2.0 * ReShade::ScreenSize.x + B);
 
        float4 SincYIn1 = Pi2 * Fc_y1 * n4;
        float4 SincYIn2 = Pi2 * Fc_y2 * n4;
        float4 SincYIn3 = Pi2 * Fc_y3 * n4;
 
        float4 SincY1 = sin(SincYIn1) / SincYIn1;
        float4 SincY2 = sin(SincYIn2) / SincYIn2;
        float4 SincY3 = sin(SincYIn3) / SincYIn3;
        if(SincYIn1.x == 0.0) SincY1.x = 1.0;
        if(SincYIn1.y == 0.0) SincY1.y = 1.0;
        if(SincYIn1.z == 0.0) SincY1.z = 1.0;
        if(SincYIn1.w == 0.0) SincY1.w = 1.0;
        if(SincYIn2.x == 0.0) SincY2.x = 1.0;
        if(SincYIn2.y == 0.0) SincY2.y = 1.0;
        if(SincYIn2.z == 0.0) SincY2.z = 1.0;
        if(SincYIn2.w == 0.0) SincY2.w = 1.0;
        if(SincYIn3.x == 0.0) SincY3.x = 1.0;
        if(SincYIn3.y == 0.0) SincY3.y = 1.0;
        if(SincYIn3.z == 0.0) SincY3.z = 1.0;
        if(SincYIn3.w == 0.0) SincY3.w = 1.0;
        //float4 IdealY = (2.0 * Fc_y1 * SincY1 - 2.0 * Fc_y2 * SincY2) + 2.0 * Fc_y3 * SincY3;
        float4 IdealY = (2.0 * Fc_y1 * SincY1 - 2.0 * Fc_y2 * SincY2) + 2.0 * Fc_y3 * SincY3;
        float4 FilterY = (0.54 + 0.46 * cos(Pi2Length * n4)) * IdealY;     
       
        float4 SincIIn = Pi2 * Fc_i * n4;
        float4 SincI = sin(SincIIn) / SincIIn;
        if (SincIIn.x == 0.0) SincI.x = 1.0;
        if (SincIIn.y == 0.0) SincI.y = 1.0;
        if (SincIIn.z == 0.0) SincI.z = 1.0;
        if (SincIIn.w == 0.0) SincI.w = 1.0;
        float4 IdealI = 2.0 * Fc_i * SincI;
        float4 FilterI = (0.54 + 0.46 * cos(Pi2Length * n4)) * IdealI;
       
        float4 SincQIn = Pi2 * Fc_q * n4;
        float4 SincQ = sin(SincQIn) / SincQIn;
        if (SincQIn.x == 0.0) SincQ.x = 1.0;
        if (SincQIn.y == 0.0) SincQ.y = 1.0;
        if (SincQIn.z == 0.0) SincQ.z = 1.0;
        if (SincQIn.w == 0.0) SincQ.w = 1.0;
        float4 IdealQ = 2.0 * Fc_q * SincQ;
        float4 FilterQ = (0.54 + 0.46 * cos(Pi2Length * n4)) * IdealQ;
       
        YAccum = YAccum + C * FilterY;
        IAccum = IAccum + C * cos(WT) * FilterI;
        QAccum = QAccum + C * sin(WT) * FilterQ;
    }
   
    float Y = YAccum.r + YAccum.g + YAccum.b + YAccum.a;
    float I = (IAccum.r + IAccum.g + IAccum.b + IAccum.a) * 2.0;
    float Q = (QAccum.r + QAccum.g + QAccum.b + QAccum.a) * 2.0;
   
    float3 YIQ = float3(Y, I, Q);
 
    float3 OutRGB = float3(dot(YIQ, float3(1.0, 0.956, 0.621)), dot(YIQ, float3(1.0, -0.272, -0.647)), dot(YIQ, float3(1.0, -1.106, 1.703)));      
   
    return float4(OutRGB, 1.0);
}
 
 
void PS_VHS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 GetOut : SV_Target)
{  
    float4 origcolor;
   
    origcolor=tex2D(ReShade::BackBuffer, texcoord);
   
    origcolor = NTSCCodec(texcoord);
 
    origcolor = pow(origcolor,0.5);
   
    GetOut = origcolor;
}
 
//random hash
float4 hash42(float2 p){
   
    float4 p4 = frac(float4(p.xyxy) * float4(443.8975,397.2973, 491.1871, 470.7827));
    p4.x += dot(p4.wzxy, p4+19.19);
    return frac(float4(p4.x * p4.y, p4.x*p4.z, p4.y*p4.w, p4.x*p4.w));
}
 
 
float hash( float n ){
    return frac(sin(n)*43758.5453123);
}
 
// 3d noise function (iq's)
float n( in float3 x ){
    float3 p = floor(x);
    float3 f = frac(x);
    f = f*f*(3.0-2.0*f);
    float n = p.x + p.y*57.0 + 113.0*p.z;
    float res = lerp(lerp(lerp( hash(n+  0.0), hash(n+  1.0),f.x),
                        lerp( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
                    lerp(lerp( hash(n+113.0), hash(n+114.0),f.x),
                        lerp( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
    return res;
}
 
//tape noise
float nn(float2 p){
 
 
    float y = p.y;
   
    float v = (n( float3(y*0.01 +t*2.0,             1., 1.0) ) + .0)
             *(n( float3(y*.011+1000.0+t*2.0,   1., 1.0) ) + .0)
             *(n( float3(y*.51+421.0+t*2.0,     1., 1.0) ) + .0)  
        ;
    v*= hash42(   float2(p.x +t*0.01, p.y) ).x +.3 ;
 
    return v;
}
 
void PS_VHS3(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 OutGet : SV_Target)
{
    float4 uv = 0.0;
    uv.xy = texcoord.xy;
    uv.w = 0.0;
    float4 origcolor2= tex2Dlod(ReShade::BackBuffer, uv);
   
    float linesN = 240; //fields per seconds
    float one_y = ReShade::ScreenSize.y / linesN; //field line
    uv.xy = floor(uv.xy*ReShade::ScreenSize.xy/one_y)*one_y;
 
    float col = nn(-uv.xy);
 
    origcolor2 += dot(col, float4(0.233,0.233,0.233,0.233));
   
    OutGet = origcolor2;
}
 
float noise(float2 p)
{
    float sampless = tex2D(SamNoise,float2(1.,2.*cos(t2))*t2*8. + p*1.).x;
    sampless *= sampless;
    return sampless;
}
 
float onOff(float a, float b, float c)
{
    return step(c, sin(t2 + a*cos(t2*b)));
}
 
float ramp(float y, float start, float end)
{
    float inside = step(start,y) - step(end,y);
    float fact = (y-start)/(end-start)*inside;
    return (1.-fact) * inside;
   
}
 
float stripes(float2 uv)
{
    float noi = noise(uv*float2(0.5,1.) + float2(1.,3.));
    return ramp(mod(uv.y*4.0 + t2/2.+sin(t2 + sin(t2*0.63)),2.0),0.5,0.6)*noi;
}
 
float3 getVideo(float2 uv)
{
    float2 look = uv;
    float window = 1.0/(1.0+20.0*(look.y-(mod(t2/4.0,2.2))*(look.y-(mod(t2/4.0,2.0))))); //this was broken
    look.x = look.x + sin(look.y*10. + t2)/500.*onOff(4.,4.,.3)*(1.+cos(t2*80.))*window;
    float vShift = 5.4*onOff(2.,3.,.9)*(sin(t2)*sin(t2*20.) + (0.5 + 0.1*sin(t2*200.)*cos(t2)));
    look.y = (mod(look.y + vShift,2.0)); //this too
    float3 video = tex2D(ReShade::BackBuffer,look).rgb;
    return video;
}
 
float2 screenDistort(float2 uv)
{
    uv -= float2(.5,.5);
    uv = uv*1.2*(1./1.2+2.*uv.x*uv.x*uv.y*uv.y);
    uv += float2(.5,.5);
    return uv;
}
 
void PS_VHS4(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 OutGet2 : SV_Target)
{
    float4 origcolor3=tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
    texcoord = screenDistort(texcoord);
    float3 video = getVideo(texcoord);
    float vigAmt = 3.+.3*sin(t2 + 5.*cos(t2*5.));
    float vignette = (1.-vigAmt*(texcoord.y-0.5)*(texcoord.y-0.5))*(1.0-vigAmt*(texcoord.x-0.5)*(texcoord.x-0.5));
   
    //video += stripes(texcoord);
    video += noise(texcoord*2.)/2.;
    video *= vignette;
 
    origcolor3.xyz = video;
   
    OutGet2 = origcolor3;
}
 
float rand(float2 co)
{
        float a = 12.9898;
        float b = 78.233;
        float c = 43758.5453;
        float dst= dot(co.xy ,float2(a,b));
        float snm= mod(dst,3.14); //this was broken aswell
        return frac(sin(snm) * c);
}
 
void PS_VHS5(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 OutGet3 : SV_Target)
{
    float4 origcolor4=tex2D(ReShade::BackBuffer, texcoord);
    float magnitude = 0.0009;
   
    // Set up offset
    float2 offsetRedUV = texcoord;
    offsetRedUV.x = texcoord.x + rand(float2(t2*0.03,texcoord.y*0.42)) * 0.001;
    offsetRedUV.x += sin(rand(float2(t2*0.2, texcoord.y)))*magnitude;
   
    float2 offsetGreenUV = texcoord;
    offsetGreenUV.x = texcoord.x + rand(float2(t2*0.004,texcoord.y*0.002)) * 0.004;
    offsetGreenUV.x += sin(t2*9.0)*magnitude;
   
    float2 offsetBlueUV = texcoord;
    offsetBlueUV.x = texcoord.y;
    offsetBlueUV.x += rand(float2(cos(t2.x*0.01),sin(texcoord.y)));
   
    // Load Texture
    float r = tex2D(ReShade::BackBuffer, offsetRedUV).r;
    float g = tex2D(ReShade::BackBuffer, offsetGreenUV).g;
    float b = tex2D(ReShade::BackBuffer, texcoord).b;
   
    if (bVHSDistortGammaFix)
	{
    origcolor4 += float4(r,g,b,0);
   
    OutGet3 = origcolor4;
    }
    else
    {
    OutGet3 = float4(r,g,b,1.0);
    }
}
 
float scanline(float2 uv) {
    return sin(ReShade::ScreenSize.y * uv.y * 0.7 - t2 * 10.0);
}
 
float slowscan(float2 uv) {
    return sin(ReShade::ScreenSize.y * uv.y * 0.02 + t2 * 6.0);
}
 
float2 colorShift(float2 uv) {
    return float2(
        uv.x,
        uv.y + sin(t2)*0.02
    );
}
 
float noise2(float2 uv) {
    return clamp(tex2D(SamNoise, uv.xy + t2*6.0).r +
        tex2D(SamNoise, uv.xy - t2*4.0).g, 0.96, 1.0);
}
 
// from https://www.shadertoy.com/view/4sf3Dr
// Thanks, Jasper
float2 crt(float2 coord, float bend)
{
    // put in symmetrical coords
    coord = (coord - 0.5) * 2.0;
 
    coord *= 0.5;  
   
    // deform coords
    coord.x *= 1.0 + pow((abs(coord.y) / bend), 2.0);
    coord.y *= 1.0 + pow((abs(coord.x) / bend), 2.0);
 
    // transform back to 0.0 - 1.0 space
    coord  = (coord / 1.0) + 0.5;
 
    return coord;
}
 
float2 colorshift(float2 uv, float amount, float rand) {
   
    return float2(
        uv.x,
        uv.y + amount * rand  
    );
}
 
float2 scandistort(float2 uv) {
    float scan1 = clamp(cos(uv.y * 2.0 + t2), 0.0, 1.0);
    float scan2 = clamp(cos(uv.y * 2.0 + t2 + 4.0) * 10.0, 0.0, 1.0) ;
    float amount = scan1 * scan2 * uv.x;
   
    uv.x -= 0.05 * lerp(tex2D(SamNoise, float2(uv.x, amount)).r * amount, amount, 0.9);
 
    return uv;
     
}
 
float vignette(float2 uv) {
    uv = (uv - 0.5) * 0.98;
    return clamp(pow(abs(cos(uv.x * 3.1415)), 1.2) * pow(abs(cos(uv.y * 3.1415)), 1.2) * 50.0, 0.0, 1.0);
}
 
void PS_VHS6(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 OutGet3 : SV_Target)
{
    float4 uv = 0.0;
    uv.xy = texcoord.xy;
    uv.w = 0.0;
    float2 sdUv = scandistort(uv.xy);
    float2 crtUv = crt(sdUv, 2.0);
   
    float4 color = float4(0, 0, 0, 0);
   
    //float rand_r = sin(t2 * 3.0 + sin(t2)) * sin(t2 * 0.2);
    //float rand_g = clamp(sin(t2 * 1.52 * uv.y + sin(t2)) * sin(t2* 1.2), 0.0, 1.0);
    float4 randss = tex2D(SamNoise, float2(t2 * 0.01, t2 * 0.02));
   
    color.r = tex2D(ReShade::BackBuffer, crt(colorshift(sdUv, 0.025, randss.r), 2.0)).r;
    color.g = tex2D(ReShade::BackBuffer, crt(colorshift(sdUv, 0.01, randss.g), 2.0)).g;
    color.b = tex2D(ReShade::BackBuffer, crt(colorshift(sdUv, 0.024, randss.b), 2.0)).b;   
       
    float4 scanlineColor = scanline(crtUv);
    float4 slowscanColor = slowscan(crtUv);
   
    OutGet3 = lerp(color, lerp(scanlineColor, slowscanColor, 0.5), 0.05) *
        vignette(texcoord) *
        noise2(texcoord);  
    //fragColor = float4(vignette(uv));
    //float2 scan_dist = scandistort(uv);
    //fragColor = float4(scan_dist.x, scan_dist.y,0.0, 1.0);
}
 
technique NTSCFilter
{
	pass NTSCFilter { VertexShader = PostProcessVS; PixelShader = PS_VHS; }
}

technique TapeNoise
{
	pass TapeNoise { VertexShader = PostProcessVS; PixelShader = PS_VHS3; }
}

technique VCRDistort
{
	pass VCRDistort { VertexShader = PostProcessVS; PixelShader = PS_VHS4; }
}

technique VHSDistort
{
	pass VHSDistort { VertexShader = PostProcessVS; PixelShader = PS_VHS5; }
}

technique DirtyCRT
{
	pass DirtyCRT { VertexShader = PostProcessVS; PixelShader = PS_VHS6; }
}