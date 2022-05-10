#include "ReShade.fxh"

uniform float texture_sizeX <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_WIDTH;
	ui_label = "Screen Width [MAME PostProcess]";
> = 320.0;

uniform float texture_sizeY <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = BUFFER_HEIGHT;
	ui_label = "Screen Height [MAME PostProcess]";
> = 240.0;

uniform bool scanlines <
	ui_type = "boolean";
	ui_label = "Scanline Toggle [MAME PostProcess]"; 
> = true;

uniform float scandark <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_step = 0.05;
	ui_label = "Scanline Intensity [MAME PostProcess]";
> = 0.175;

uniform bool deconverge <
	ui_type = "boolean";
	ui_label = "Deconvergence/Convolution [MAME PostProcess]";
> = true;

uniform bool pincushion <
	ui_type = "boolean";
	ui_label = "Bezel Toggle [MAME PostProcess]";
> = false;

uniform bool hertzroll <
	ui_type = "boolean";
	ui_label = "Refresh Roll Toggle [MAME PostProcess]";
> = true;

//Useful Constants
static const float4 Zero = float4(0.0,0.0,0.0,0.0);
static const float4 Half = float4(0.5,0.5,0.5,0.5);
static const float4 One = float4(1.0,1.0,1.0,1.0);
static const float4 Two = float4(2.0,2.0,2.0,2.0);
static const float3 Gray = float3(0.3, 0.59, 0.11);
static const float Pi = 3.1415926535;
static const float Pi2 = 6.283185307;

// NTSC static constants
static const float4 A = float4(0.5,0.5,0.5,0.5);
static const float4 A2 = float4(1.0,1.0,1.0,1.0);
static const float4 B = float4(0.5,0.5,0.5,0.5);
static const float P = 1.0;
static const float CCFrequency = 3.59754545;
static const float NotchUpperFrequency = 3.59754545 + 2.0;
static const float NotchLowerFrequency = 3.59754545 - 2.0;
static const float YFrequency = 6.0;
static const float IFrequency = 1.2;
static const float QFrequency = 0.6;
static const float NotchHalfWidth = 2.0;
static const float ScanTime = 52.6;
static const float Pi2ScanTime = 6.283185307 * 52.6;
static const float MaxC = 2.1183;
static const float4 YTransform = float4(0.299, 0.587, 0.114, 0.0);
static const float4 ITransform = float4(0.595716, -0.274453, -0.321263, 0.0);
static const float4 QTransform = float4(0.211456, -0.522591, 0.311135, 0.0);
static const float3 YIQ2R = float3(1.0, 0.956, 0.621);
static const float3 YIQ2G = float3(1.0, -0.272, -0.647);
static const float3 YIQ2B = float3(1.0, -1.106, 1.703);
static const float4 MinC = float4(-1.1183,-1.1183,-1.1183,-1.1183);
static const float4 CRange = float4(3.2366,3.2366,3.2366,3.2366);
static const float4 InvCRange = float4(1.0/3.2366,1.0/3.2366,1.0/3.2366,1.0/3.2366);
static const float Pi2Length = Pi2 / 63.0;
static const float4 NotchOffset = float4(0.0, 1.0, 2.0, 3.0);
static const float4 W = float4(Pi2 * CCFrequency * ScanTime,Pi2 * CCFrequency * ScanTime,Pi2 * CCFrequency * ScanTime,Pi2 * CCFrequency * ScanTime);

// Color Convolution Constants
static const float3 RedMatrix = float3(1.0, 0.0, 0.0);
static const float3 GrnMatrix = float3(0.0, 1.0, 0.0);
static const float3 BluMatrix = float3(0.0, 0.0, 1.0);
static const float3 DCOffset = float3(0.0, 0.0, 0.0);
static const float3 ColorScale = float3(0.95, 0.95, 0.95);
static const float Saturation = 1.4;

// Deconverge Constants
static const float3 ConvergeX = float3(-0.4,  0.0, 0.2);
static const float3 ConvergeY = float3( 0.0, -0.4, 0.2);
static const float3 RadialConvergeX = float3(1.0, 1.0, 1.0);
static const float3 RadialConvergeY = float3(1.0, 1.0, 1.0);

// Scanline/Pincushion Constants
static const float PincushionAmount = 0.015;
static const float CurvatureAmount = 0.015;
//const float ScanlineAmount = 0.175; <- move to parameter
static const float ScanlineScale = 1.0;
static const float ScanlineHeight = 1.0;
static const float ScanlineBrightScale = 1.0;
static const float ScanlineBrightOffset = 0.0;
static const float ScanlineOffset = 0.0;
static const float3 Floor = float3(0.05, 0.05, 0.05);

// 60Hz Bar Constants
static const float SixtyHertzRate = (60.0 / 59.97 - 1.0); // Difference between NTSC and line frequency
static const float SixtyHertzScale = 0.1;

#define texture_size float2(texture_sizeX, texture_sizeY)
#define texture_size_pixel float2(1.0/texture_sizeX, 1.0/texture_sizeY)
uniform int FCount < source = "framecount"; >;

float4 CompositeSample(float2 UV, float2 InverseRes) {
	float2 InverseP = float2(P, 0.0) * InverseRes;
	
	// UVs for four linearly-interpolated samples spaced 0.25 texels apart
	float2 C0 = UV;
	float2 C1 = UV + InverseP * 0.25;
	float2 C2 = UV + InverseP * 0.50;
	float2 C3 = UV + InverseP * 0.75;
	float4 Cx = float4(C0.x, C1.x, C2.x, C3.x);
	float4 Cy = float4(C0.y, C1.y, C2.y, C3.y);

	float4 Texel0 = tex2D(ReShade::BackBuffer, C0);
	float4 Texel1 = tex2D(ReShade::BackBuffer, C1);
	float4 Texel2 = tex2D(ReShade::BackBuffer, C2);
	float4 Texel3 = tex2D(ReShade::BackBuffer, C3);
	
	float Frequency = CCFrequency;
	//Frequency = Frequency;// Uncomment for bad color sync + (sin(UV.y * 2.0 - 1.0) / CCFrequency) * 0.001;

	// Calculated the expected time of the sample.
	float4 T = A2 * Cy * float4(texture_size.y,texture_size.y,texture_size.y,texture_size.y) + B + Cx;
	float4 W = float4(Pi2ScanTime * Frequency,Pi2ScanTime * Frequency,Pi2ScanTime * Frequency,Pi2ScanTime * Frequency);
	float4 TW = T * W;
	float4 Y = float4(dot(Texel0, YTransform), dot(Texel1, YTransform), dot(Texel2, YTransform), dot(Texel3, YTransform));
	float4 I = float4(dot(Texel0, ITransform), dot(Texel1, ITransform), dot(Texel2, ITransform), dot(Texel3, ITransform));
	float4 Q = float4(dot(Texel0, QTransform), dot(Texel1, QTransform), dot(Texel2, QTransform), dot(Texel3, QTransform));

	float4 Encoded = Y + I * cos(TW) + Q * sin(TW);
	return (Encoded - MinC) * InvCRange;
}

float4 NTSCCodec(float2 UV, float2 InverseRes)
{
	float4 YAccum = Zero;
	float4 IAccum = Zero;
	float4 QAccum = Zero;
	float QuadXSize = texture_size.x * 4.0;
	float TimePerSample = ScanTime / QuadXSize;
	
	// Frequency cutoffs for the individual portions of the signal that we extract.
	// Y1 and Y2 are the positive and negative frequency limits of the notch filter on Y.
	// Y3 is the center of the frequency response of the Y filter.
	// I is the center of the frequency response of the I filter.
	// Q is the center of the frequency response of the Q filter.
	float Fc_y1 = NotchLowerFrequency * TimePerSample;
	float Fc_y2 = NotchUpperFrequency * TimePerSample;
	float Fc_y3 = YFrequency * TimePerSample;
	float Fc_i = IFrequency * TimePerSample;
	float Fc_q = QFrequency * TimePerSample;
	float Pi2Fc_y1 = Fc_y1 * Pi2;
	float Pi2Fc_y2 = Fc_y2 * Pi2;
	float Pi2Fc_y3 = Fc_y3 * Pi2;
	float Pi2Fc_i = Fc_i * Pi2;
	float Pi2Fc_q = Fc_q * Pi2;
	float Fc_y1_2 = Fc_y1 * 2.0;
	float Fc_y2_2 = Fc_y2 * 2.0;
	float Fc_y3_2 = Fc_y3 * 2.0;
	float Fc_i_2 = Fc_i * 2.0;
	float Fc_q_2 = Fc_q * 2.0;
	float4 CoordY = float4(UV.y,UV.y,UV.y,UV.y);
	
	// 83 composite samples wide, 4 composite pixels per texel
	for(float n = -31.0; n < 32.0; n += 4.0)
	{
		float4 n4 = n + NotchOffset;
		float4 CoordX = UV.x + InverseRes.x * n4 * 0.25;
		float2 TexCoord = float2(CoordX.x, CoordY.x);
		float4 C = CompositeSample(TexCoord, InverseRes) * CRange + MinC;
		float4 WT = W * (CoordX  + A2 * CoordY * texture_size.y + B);
		float4 Cosine = 0.54 + 0.46 * cos(Pi2Length * n4);

		float4 SincYIn1 = Pi2Fc_y1 * n4;
		float4 SincYIn2 = Pi2Fc_y2 * n4;
		float4 SincYIn3 = Pi2Fc_y3 * n4;
		float4 SincY1 = sin(SincYIn1) / SincYIn1;
		float4 SincY2 = sin(SincYIn2) / SincYIn2;
		float4 SincY3 = sin(SincYIn3) / SincYIn3;
		
		// These zero-checks could be made more efficient if WebGL supported mix(float4, float4, bfloat4)
		// Unfortunately, the universe hates us
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
		float4 IdealY = (Fc_y1_2 * SincY1 - Fc_y2_2 * SincY2) + Fc_y3_2 * SincY3;
		float4 FilterY = Cosine * IdealY;		
		
		float4 SincIIn = Pi2Fc_i * n4;
		float4 SincI = sin(SincIIn) / SincIIn;
		if (SincIIn.x == 0.0) SincI.x = 1.0;
		if (SincIIn.y == 0.0) SincI.y = 1.0;
		if (SincIIn.z == 0.0) SincI.z = 1.0;
		if (SincIIn.w == 0.0) SincI.w = 1.0;
		float4 IdealI = Fc_i_2 * SincI;
		float4 FilterI = Cosine * IdealI;
		
		float4 SincQIn = Pi2Fc_q * n4;
		float4 SincQ = sin(SincQIn) / SincQIn;
		if (SincQIn.x == 0.0) SincQ.x = 1.0;
		if (SincQIn.y == 0.0) SincQ.y = 1.0;
		if (SincQIn.z == 0.0) SincQ.z = 1.0;
		if (SincQIn.w == 0.0) SincQ.w = 1.0;
		float4 IdealQ = Fc_q_2 * SincQ;
		float4 FilterQ = Cosine * IdealQ;
		
		YAccum += C * FilterY;
		IAccum += C * cos(WT) * FilterI;
		QAccum += C * sin(WT) * FilterQ;
	}
	
	float Y = dot(YAccum, One);
	float I = dot(IAccum, One) * 2.0;
	float Q = dot(QAccum, One) * 2.0;
	
	float3 YIQ = float3(Y, I, Q);
	float3 OutRGB = float3(dot(YIQ, YIQ2R), dot(YIQ, YIQ2G), dot(YIQ, YIQ2B));
	
	return float4(OutRGB, 1.0);
}

float4 ColorConvolution(float2 UV, float2 InverseRes)
{
	float3 InPixel = tex2D(ReShade::BackBuffer, UV).rgb;
	
	// Color Matrix
	float RedValue = dot(InPixel, RedMatrix);
	float GrnValue = dot(InPixel, GrnMatrix);
	float BluValue = dot(InPixel, BluMatrix);
	float3 OutColor = float3(RedValue, GrnValue, BluValue);
	
	// DC Offset & Scale
	OutColor = (OutColor * ColorScale) + DCOffset;
	
	// Saturation
	float Luma = dot(OutColor, Gray);
	float3 Chroma = OutColor - Luma;
	OutColor = (Chroma * Saturation) + Luma;
	
	return float4(OutColor, 1.0);
}

float4 Deconverge(float2 UV)
{
	float2 InverseRes = 1.0 / texture_size.xy;
	float2 InverseSrcRes = 1.0 / ReShade::ScreenSize.xy;

	float3 CoordX = UV.x * RadialConvergeX;
	float3 CoordY = UV.y * RadialConvergeY;

	CoordX += ConvergeX * InverseRes.x - (RadialConvergeX - 1.0) * 0.5;
	CoordY += ConvergeY * InverseRes.y - (RadialConvergeY - 1.0) * 0.5;

	float RedValue = ColorConvolution(float2(CoordX.x, CoordY.x), InverseSrcRes).r;
	float GrnValue = ColorConvolution(float2(CoordX.y, CoordY.y), InverseSrcRes).g;
	float BluValue = ColorConvolution(float2(CoordX.z, CoordY.z), InverseSrcRes).b;

	if (deconverge > 0.5) return float4(RedValue, GrnValue, BluValue, 1.0);
	else return float4(tex2D(ReShade::BackBuffer, UV));
}

float4 ScanlinePincushion(float2 UV)
{
	float4 InTexel = Deconverge(UV);
	
	float2 PinUnitCoord = UV * Two.xy - One.xy;
	float PincushionR2 = pow(length(PinUnitCoord), 2.0);
	float2 PincushionCurve = PinUnitCoord * PincushionAmount * PincushionR2;
	float2 BaseCoord = UV;
	float2 ScanCoord = UV;
	
	BaseCoord *= One.xy - PincushionAmount * 0.2; // Warning: Magic constant
	BaseCoord += PincushionAmount * 0.1;
	BaseCoord += PincushionCurve;
	
	ScanCoord *= One.xy - PincushionAmount * 0.2; // Warning: Magic constant
	ScanCoord += PincushionAmount * 0.1;
	ScanCoord += PincushionCurve;
	
	float2 CurfloatlipUnitCoord = UV * Two.xy - One.xy;
	float CurvatureClipR2 = pow(length(CurfloatlipUnitCoord), 2.0);
	float2 CurvatureClipCurve = CurfloatlipUnitCoord * CurvatureAmount * CurvatureClipR2;
	float2 ScreenClipCoord = UV;
	ScreenClipCoord -= Half.xy;
	ScreenClipCoord *= One.xy - CurvatureAmount * 0.2; // Warning: Magic constant
	ScreenClipCoord += Half.xy;
	ScreenClipCoord += CurvatureClipCurve;

	if (pincushion > 0.5){
		// -- Alpha Clipping --
		if (BaseCoord.x < 0.0) return float4(0.0, 0.0, 0.0, 1.0);
		if (BaseCoord.y < 0.0) return float4(0.0, 0.0, 0.0, 1.0);
		if (BaseCoord.x > 1.0) return float4(0.0, 0.0, 0.0, 1.0);
		if (BaseCoord.y > 1.0) return float4(0.0, 0.0, 0.0, 1.0);
	}
	
	// -- Scanline Silerpation --
	float InnerSine = ScanCoord.y * texture_size.y * ScanlineScale;
	float ScanBrightMod = sin(InnerSine * Pi + ScanlineOffset * texture_size.y);
	float ScanBrightness = lerp(1.0, (pow(ScanBrightMod * ScanBrightMod, ScanlineHeight) * ScanlineBrightScale + 1.0) * 0.5, scandark);
	float3 ScanlineTexel = InTexel.rgb * ScanBrightness;
	
	// -- Color Compression (increasing the floor of the signal without affecting the ceiling) --
	ScanlineTexel = Floor + (One.xyz - Floor) * ScanlineTexel;
	if (scanlines > 0.5)	return float4(ScanlineTexel, 1.0);
	else return float4(InTexel);
}

float4 SixtyHertz(float2 UV)
{
	float4 InPixel = ScanlinePincushion(UV);
	float Milliseconds = float(FCount) * 15.0;
	float TimeStep = frac(Milliseconds * SixtyHertzRate);
	float BarPosition = 1.0 - frac(UV.y + TimeStep) * SixtyHertzScale;
	float4 OutPixel = InPixel * BarPosition;
	if (hertzroll > 0.5) return OutPixel;
	else return InPixel;
}

float4 PS_MAMENTSC( float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
   return float4(NTSCCodec(uv, texture_size_pixel.xy));
}

float4 PS_MAMEPostProc( float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 OutPixel = SixtyHertz(uv.xy);
	return OutPixel;
}

technique MAMEPostProcess {
    pass MAMENTSC {
		VertexShader = PostProcessVS;
		PixelShader = PS_MAMENTSC;
	}
	
    pass MAMEPostProcess {
		VertexShader = PostProcessVS;
		PixelShader = PS_MAMEPostProc;
	}
}	