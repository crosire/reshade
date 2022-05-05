#include "ReShade.fxh"

uniform float EVHS_PixelSize <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "Pixel Size [EasyVHS]";
> = 3.0;

uniform float EVHS_CRTFade <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "CRT Fade [EasyVHS]";
> = 0.0;

uniform float EVHS_Brightness <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Brightness [EasyVHS]";
> = 0.0;

uniform float EVHS_YFrequency <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "Y Frequency [EasyVHS]";
> = 6.0;

uniform float EVHS_IFrequency <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "I Frequency [EasyVHS]";
> = 1.2;

uniform float EVHS_QFrequency <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "Q Frequency [EasyVHS]";
> = 0.6;

uniform float EVHS_distortionStrength <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Distortion Strength [EasyVHS]";
> = 0.5;

uniform float EVHS_fisheyeStrength <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Fisheye Strength [EasyVHS]";
> = 0.5;

uniform float EVHS_stripesStrength <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Scanlines Strength [EasyVHS]";
> = 0.5;

uniform float EVHS_noiseStrength <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Noise Strength [EasyVHS]";
> = 0.5;

uniform float EVHS_vignetteStrength <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 1.0;
	ui_label = "Vignette Strength [EasyVHS]";
> = 0.5;

uniform bool EVHS_VHSScanlines <
	ui_type = "boolean";
	ui_label = "VHS Scanlines [EasyVHS]";
> = false;

uniform float EVHS_blurAmount <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 10.0;
	ui_label = "Blur Amount [EasyVHS]";
> = 1.0;

uniform float EVHS_channelDif <
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_label = "Channel Difference [EasyVHS]";
> = 4.0;

uniform float time < source = "timer"; >;

texture2D EVHS_NoiseTexture <source="evhs_noise.jpg";> { Width=512; Height=512;};
sampler2D EVHS_NoiseTex { Texture=EVHS_NoiseTexture; MinFilter=LINEAR; MagFilter=LINEAR; };

//Constants and useful shit
float fmod(float a, float b) {
	float c = frac(abs(a / b)) * abs(b);
	return a < 0 ? -c : c;
}

float4 PS_EVHSBlur(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 col = tex2D(ReShade::BackBuffer, uv);

	// blur amount transformed into texel space
	float blurH = EVHS_blurAmount / ReShade::ScreenSize.y;
	float blurV = EVHS_blurAmount / ReShade::ScreenSize.x;

	// Kernel
	float2 offsets[8] = 
	{
		float2(blurH, 0),
		float2(-blurH, 0),
		float2(0, blurV),
		float2(0, -blurV),
		float2(blurH, blurV),
		float2(blurH, -blurV),
		float2(-blurH, blurV),
		float2(-blurH, -blurV),
	};

	float4 samples[8];
	float4 samplesRed[8];
	for (int ii = 0; ii < 8; ii++)
	{
		samples[ii] = tex2D(ReShade::BackBuffer, uv + offsets[ii]);
		samplesRed[ii] = tex2D(ReShade::BackBuffer, uv + offsets[ii]+float2(EVHS_channelDif/ReShade::ScreenSize.x, 0.0));

		col.r += samplesRed[ii].r;
		col.gb += samples[ii].gb;
	}

	col /= 9.0;

	return col;
}

float4 PS_EVHSCRT(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float3 col;

	float2 cor;
	
	cor.x =  (uv.x * ReShade::ScreenSize.x)/EVHS_PixelSize;
	cor.y = ((uv.y * ReShade::ScreenSize.y)+EVHS_PixelSize*1.5*fmod(floor(cor.x),2.0))/(EVHS_PixelSize*3.0);

	float2 ico = floor( cor );
	float2 fco = frac( cor );

	float3 pix; 
	pix.x = step(1.5, fmod( ico.x, 3.0 ));
	pix.y = step(1.5, fmod( 1.0 + ico.x, 3.0 ));
	pix.z = step(1.5, fmod( 2.0 + ico.x, 3.0 ));

	float3 ima = tex2D( ReShade::BackBuffer,EVHS_PixelSize*ico*float2(1.0,3.0)/ReShade::ScreenSize.xy ).xyz;

	col = pix*dot( pix, ima );

	col *= step( abs(fco.x-0.5), 0.4 );
	col *= step( abs(fco.y-0.5), 0.4 );
				
	col *= (1.0 + EVHS_Brightness);

	if (col.r==0.0 && col.g==0.0 && col.b==0.0) col = lerp(col, ima, EVHS_CRTFade);

	return float4(col, 1.0);
}

float4 CompositeSample(float2 UV) 
{
	float2 InverseRes = 1.0 / ReShade::ScreenSize.xy;
	float2 InverseP = float2(1.0, 0.0) * InverseRes;
				
	float2 C0 = UV;
	float2 C1 = UV + InverseP * 0.25;
	float2 C2 = UV + InverseP * 0.50;
	float2 C3 = UV + InverseP * 0.75;
	float4 Cx = float4(C0.x, C1.x, C2.x, C3.x);
	float4 Cy = float4(C0.y, C1.y, C2.y, C3.y);

	float3 Texel0 = tex2D(ReShade::BackBuffer, C0).rgb;
	float3 Texel1 = tex2D(ReShade::BackBuffer, C1).rgb;
	float3 Texel2 = tex2D(ReShade::BackBuffer, C2).rgb;
	float3 Texel3 = tex2D(ReShade::BackBuffer, C3).rgb;

	float4 T = float4(0.5, 0.5, 0.5, 0.5) * Cy * float4(ReShade::ScreenSize.x, ReShade::ScreenSize.x, ReShade::ScreenSize.x, ReShade::ScreenSize.x) * 2.0 + float4(0.5, 0.5, 0.5, 0.5) + Cx;

	const float3 YTransform = float3(0.299, 0.587, 0.114);
	const float3 ITransform = float3(0.595716, -0.274453, -0.321263);
	const float3 QTransform = float3(0.211456, -0.522591, 0.311135);

	float Y0 = dot(Texel0, YTransform);
	float Y1 = dot(Texel1, YTransform);
	float Y2 = dot(Texel2, YTransform);
	float Y3 = dot(Texel3, YTransform);
	float4 Y = float4(Y0, Y1, Y2, Y3);

	float I0 = dot(Texel0, ITransform);
	float I1 = dot(Texel1, ITransform);
	float I2 = dot(Texel2, ITransform);
	float I3 = dot(Texel3, ITransform);
	float4 I = float4(I0, I1, I2, I3);

	float Q0 = dot(Texel0, QTransform);
	float Q1 = dot(Texel1, QTransform);
	float Q2 = dot(Texel2, QTransform);
	float Q3 = dot(Texel3, QTransform);
	float4 Q = float4(Q0, Q1, Q2, Q3);

	float Wval = 6.283185307 * 3.59754545 * 52.6;
	float4 W = float4(Wval, Wval, Wval, Wval);
	float4 Encoded = Y + I * cos(T * W) + Q * sin(T * W);
	return (Encoded - float4(-1.1183, -1.1183, -1.1183, -1.1183)) / float4(3.2366, 3.2366, 3.2366, 3.2366);
}

float4 NTSCCodec(float2 UV)
{
	float2 InverseRes = 1.0 / ReShade::ScreenSize.xy;
	float4 YAccum = 0.0;
	float4 IAccum = 0.0;
	float4 QAccum = 0.0;
	float QuadXSize = ReShade::ScreenSize.x * 4.0;
	float TimePerSample = 52.6 / QuadXSize;

	float Fc_y1 = (3.59754545 - 2.0) * TimePerSample;
	float Fc_y2 = (3.59754545 + 2.0) * TimePerSample;
	float Fc_y3 = EVHS_YFrequency * TimePerSample;
	float Fc_i = EVHS_IFrequency * TimePerSample;
	float Fc_q = EVHS_QFrequency * TimePerSample;
	float Pi2Length = 6.283185307 / 82.0;
	float4 NotchOffset = float4(0.0, 1.0, 2.0, 3.0);
	float Wval = 6.283185307 * 3.59754545 * 52.6;
	float4 W = float4(Wval, Wval, Wval, Wval);
	for(float n = -41.0; n < 42.0; n += 4.0)
	{
		float4 n4 = n + NotchOffset;
		float4 CoordX = UV.x + InverseRes.x * n4 * 0.25;
		float4 CoordY = float4(UV.y, UV.y, UV.y, UV.y);
		float2 TexCoord = float2(CoordX.r, CoordY.r);
		float4 C = CompositeSample(TexCoord) * float4(3.2366, 3.2366, 3.2366, 3.2366) + float4(-1.1183, -1.1183, -1.1183, -1.1183);
		float4 WT = W * (CoordX  + float4(0.5, 0.5, 0.5, 0.5) * CoordY * 2.0 * ReShade::ScreenSize.x + float4(0.5, 0.5, 0.5, 0.5));

		float4 SincYIn1 = 6.283185307 * Fc_y1 * n4;
		float4 SincYIn2 = 6.283185307 * Fc_y2 * n4;
		float4 SincYIn3 = 6.283185307 * Fc_y3 * n4;

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
		float4 IdealY = (2.0 * Fc_y1 * SincY1 - 2.0 * Fc_y2 * SincY2) + 2.0 * Fc_y3 * SincY3;
		float4 FilterY = (0.54 + 0.46 * cos(Pi2Length * n4)) * IdealY;		
					
		float4 SincIIn = 6.283185307 * Fc_i * n4;
		float4 SincI = sin(SincIIn) / SincIIn;
		if (SincIIn.x == 0.0) SincI.x = 1.0;
		if (SincIIn.y == 0.0) SincI.y = 1.0;
		if (SincIIn.z == 0.0) SincI.z = 1.0;
		if (SincIIn.w == 0.0) SincI.w = 1.0;
		float4 IdealI = 2.0 * Fc_i * SincI;
		float4 FilterI = (0.54 + 0.46 * cos(Pi2Length * n4)) * IdealI;
					
		float4 SincQIn = 6.283185307 * Fc_q * n4;
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

float4 PS_EVHSNTSC(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 col = tex2D(ReShade::BackBuffer, uv);

	col = NTSCCodec(uv);

	return col;
}

float noise(float2 p)
{
	float4 _Time = float4(time*0.001/20, time*0.001, time*0.001*2, time*0.001*3);
	float2 ss = 65.0*float2(1.0, 2.0*cos(4.0*_Time.y));
	float samplen = tex2D(EVHS_NoiseTex, 65.*float2(1., 2.*cos(4.*_Time.y)) + float2(p.x, p.y*.25)).x;
	samplen *= samplen;
	return samplen;
}

float onOff(float a, float b, float c)
{
	float4 _Time = float4(time*0.001/20, time*0.001, time*0.001*2, time*0.001*3);
	//Scale time for better looking distortion
	float time = _Time * 16;
	return step(c, sin(time + a*cos(time*b)));
}

float ramp(float y, float start, float end)
{
	float inside = step(start,y) - step(end,y);
	float fact = (y-start)/(end-start)*inside;
	return (1.-fact) * inside;
}

float stripes(float2 uv)
{
	float4 _Time = float4(time*0.001/20, time*0.001, time*0.001*2, time*0.001*3);
	//Scale time for faster stripes
	float time = _Time * 24;
	float noi = noise(uv*float2(0.5,1.) + float2(1.,3.));
	return ramp(fmod(uv.y*4. + time/2.+sin(time + sin(time*0.63)),1.),0.5,0.6)*noi;
}

float3 getVideo(float2 uv)
{
	float4 _Time = float4(time*0.001/20, time*0.001, time*0.001*2, time*0.001*3);
	float2 olduv = uv;

	//Scale time for better looking distortion
	float time = _Time * 16;
	float2 look = uv;
	float window = 1./(1.+20.*(look.y-fmod(time/4.,1.))*(look.y-fmod(time/4.,1.)));
	look.x = look.x + sin(look.y*10. + time)/50.*onOff(4.,4.,.3)*(1.+cos(time*80.))*window;
	float vShift = onOff(2.,3.,.9)*(sin(time)*sin(time*20.) + 
													 (0.5 + 0.1*sin(time*200.)*cos(time)));
	look.y = fmod(look.y + vShift, 1.);

	look = lerp(olduv, look, EVHS_distortionStrength);

	float3 video = float3(tex2D(ReShade::BackBuffer, look).xyz);

	return video;
}

float2 screenDistort(float2 uv)
{
	float2 newuv = uv;
	newuv -= float2(.5,.5);
	newuv = newuv*1.2*(1./1.2+2.*newuv.x*newuv.x*newuv.y*newuv.y);
	newuv += float2(.5,.5);

	newuv = lerp(uv, newuv, EVHS_fisheyeStrength);

	return newuv;
}

float4 PS_EVHSTVDist(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 _Time = float4(time*0.001/20, time*0.001, time*0.001*2, time*0.001*3);
	//Apply TV distortion
	float xScanline;
	float yScanline;
	
	yScanline += _Time * 0.1f;
	xScanline -= _Time * 0.1f;

	//VHS scanlines
	if(EVHS_VHSScanlines)
	{
		float dx = 1-abs(distance(uv.y, xScanline));
		float dy = 1-abs(distance(uv.y, yScanline));

		uv.x += dy * 0.02;

		uv.y += step(0.99, dx) * dx;

		if(dx > 0.99)
		uv.y = xScanline;
		uv.y = step(0.99, dy) * (yScanline) + step(dy, 0.99) * uv.y;
	}

	//TV screen distortion
	uv = screenDistort(uv);
	float3 video = getVideo(uv);
	float vigAmt = EVHS_vignetteStrength*(3.+.3*sin(_Time + 5.*cos(_Time*5.)));
	float vignette = (1.-vigAmt*(uv.y-.5)*(uv.y-.5))*(1.-vigAmt*(uv.x-.5)*(uv.x-.5));
	video += EVHS_stripesStrength*stripes(uv);
	video += EVHS_noiseStrength*noise(uv*2.)/2.;
	video *= vignette;
	video *= (12.+fmod(uv.y*30.+_Time,1.))/13.;

	float4 col=float4(video,1.0);

	return col;
}

technique EasyVHS_CRT
{
	pass EVHS_CRT
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_EVHSCRT;
	}
}

technique EasyVHS_Blur
{
	pass EVHS_Blur
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_EVHSBlur;
	}
}

technique EasyVHS_NTSC
{
	pass EVHS_NTSC
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_EVHSNTSC;
	}
}

technique EasyVHS_TVDistortion
{
	pass EVHS_TVDist
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_EVHSTVDist;
	}
}