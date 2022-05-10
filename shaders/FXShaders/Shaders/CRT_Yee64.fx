/*
	Ripped from Sonic Mania
*/

#include "ReShade.fxh"

#ifndef YEE64_USEPOINT
#define YEE64_USEPOINT 1
#endif

uniform int iWidth <
	ui_label = "Width";
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_WIDTH;
	ui_step = 1;
> = BUFFER_WIDTH;

uniform int iHeight <
	ui_label = "Height";
	ui_type = "drag";
	ui_min = 1;
	ui_max = BUFFER_HEIGHT;
	ui_step = 1;
> = BUFFER_HEIGHT;

uniform float fDownScale <
	ui_label = "DownScale";
	ui_type = "drag";
	ui_min = 1.0;
	ui_max = 10.0;
	ui_step = 0.001;
> = 1.2;

#define i2Resolution int2(iWidth, iHeight)

sampler sBackBuffer_Point {
	Texture = ReShade::BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
};

void PS_CRT_Yee64(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD,
	out float4 oC0 : SV_TARGET
) {
	//Declare parameters
	//pixelSize
	static const float4 c0 = ReShade::ScreenSize.xyyy / float4(ReShade::AspectRatio, 1, 1, 1);
	//textureSize
	static const float4 c1 = i2Resolution.xyyy / fDownScale;
	//viewSize
	static const float4 c2 = ReShade::ScreenSize.xyyy;
	//texDiffuse
	#if YEE64_USEPOINT
	#define s0 sBackBuffer_Point
	#else
	#define s0 ReShade::BackBuffer
	#endif
	
	//Declare constants
	static const float4 c3 = float4(-1, 0.5, 1.25, 0);
    static const float4 c4 = float4(0.5, -0.5, 1.5, -3);
    static const float4 c5 = float4(-1, 1, -2, 2);
    static const float4 c6 = float4(-3, -8, 720, 0.166666672);
    static const float4 c7 = float4(-0.333000004, -0.666000009, 0.899999976, 1.20000005);
    static const float4 c8 = float4(1.5, 0.5, 2.5, 0.899999976);

	//Declare registers
	float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9;

	//Code starts here
	float4 v0 = uv.xyyy;
	//dcl_2d s0
	r0.z = c3.w;
	r1.x = 1.0 / c0.x;
	r1.y = 1.0 / c0.y;
	r1.xy = (r1 * c1).xy;
	r1.xy = (r1 * v0).xy;
	r2.x = 1.0 / c1.x;
	r2.y = 1.0 / c1.y;
	r1.zw = (r2.xyxy * c0.xyxy).zw;
	r1.zw = (r1 * r1.xyxy).zw;
	r1.xy = (r1 * c2).xy;
	r2.zw = (r1 * c1.xyxy).zw;
	r2.zw = frac(r2).zw;
	r0.xy = (-r2.zwzw).xy;
	r3.xy = (r1.zwzw * c1 + r0.xzzw).xy;
	r4.yz = (r1.xzww * c1.xxyw + r0.xzyw).yz;
	r3.z = r0.y + r3.y;
	r5 = r3.xzxz + -c4.zyxy;
	r3 = r3.xzxz + c8.xyzy;
	r3 = r2.xyxy * r3;
	r5 = r2.xyxy * r5;
	r6 = tex2D(s0, r5.zw);
	r5 = tex2D(s0, r5.xy);
	r5.xyz = (r5 * c3.zzzz).xyz;
	r7 = r1.zwzw * c1.xyxy + r0.xyxy;
	r0.zw = (r1 * c1.xyxy + - r7).zw;
	r8.x = c3.x;
	r1.zw = (r1 * c1.xyxy + r8.xxxx).zw;
	r1.zw = (r0.xyxy + r1).zw;
	r4.x = r0.x + r4.y;
	r4 = r4.xzxz + c4.xyxz;
	r4 = r2.xyxy * r4;
	r0.xy = (r1.zwzw + c3.yyyy).xy;
	r0.xy = (r2 * r0).xy;
	r8 = tex2D(s0, r0.xy);
	r8.xyz = (r8 * c3.zzzz).xyz;
	r0.xy = (r0.zwzw + -c3.yyyy).xy;
	r9 = -r0.xxxx + c5;
	r9 = r9 * r9;
	r9 = r9 * c4.wwww;
	r0.z = pow(2, r9.x);
	r6.xyz = (r6 * r0.zzzz).xyz;
	r6.xyz = (r6 * c3.zzzz).xyz;
	r0.w = pow(2, r9.z);
	r5.xyz = (r5 * r0.wwww + r6).xyz;
	r0.w = r0.z + r0.w;
	r6 = r7.zwzw + c4.zyxx;
	r7 = r7 + c4.yzzz;
	r7 = r2.xyxy * r7;
	r2 = r2.xyxy * r6;
	r6 = tex2D(s0, r2.zw);
	r2 = tex2D(s0, r2.xy);
	r2.xyz = (r2 * c3.zzzz).xyz;
	r1.zw = (r0.xyxy * r0.xyxy).zw;
	r0.xy = (-r0.yyyy + c5).xy;
	r0.xy = (r0 * r0).xy;
	r0.xy = (r0 * c6.yyyy).xy;
	r1.zw = (r1 * c6.xyxy).zw;
	r1.z = pow(2, r1.z);
	r1.w = pow(2, r1.w);
	r6.xyz = (r6 * r1.zzzz).xyz;
	r5.xyz = (r6 * c3.zzzz + r5).xyz;
	r6 = tex2D(s0, r3.xy);
	r3 = tex2D(s0, r3.zw);
	r3.xyz = (r3 * c3.zzzz).xyz;
	r2.w = pow(2, r9.y);
	r3.w = pow(2, r9.w);
	r6.xyz = (r6 * r2.wwww).xyz;
	r5.xyz = (r6 * c3.zzzz + r5).xyz;
	r3.xyz = (r3 * r3.wwww + r5).xyz;
	r0.w = r0.w + r1.z;
	r0.w = r2.w + r0.w;
	r0.w = r3.w + r0.w;
	r0.w = 1.0 / r0.w;
	r3.xyz = (r0.wwww * r3).xyz;
	r3.xyz = (r1.wwww * r3).xyz;
	r5 = tex2D(s0, r4.xy);
	r4 = tex2D(s0, r4.zw);
	r4.xyz = (r1.zzzz * r4).xyz;
	r4.xyz = (r4 * c3.zzzz).xyz;
	r5.xyz = (r5 * c3.zzzz).xyz;
	r5.xyz = (r1.zzzz * r5).xyz;
	r0.w = r0.z + r1.z;
	r0.w = r2.w + r0.w;
	r0.w = 1.0 / r0.w;
	r5.xyz = (r8 * r0.zzzz + r5).xyz;
	r2.xyz = (r2 * r2.wwww + r5).xyz;
	r2.xyz = (r0.wwww * r2).xyz;
	r0.x = pow(2, r0.x);
	r0.y = pow(2, r0.y);
	r2.xyz = (r2 * r0.xxxx + r3).xyz;
	r3 = tex2D(s0, r7.xy);
	r5 = tex2D(s0, r7.zw);
	r5.xyz = (r2.wwww * r5).xyz;
	r3.xyz = (r0.zzzz * r3).xyz;
	r3.xyz = (r3 * c3.zzzz + r4).xyz;
	r3.xyz = (r5 * c3.zzzz + r3).xyz;
	r0.xzw = (r0.wwww * r3.xyyz).xzw;
	r0.xyz = (r0.xzww * r0.yyyy + r2).xyz;
	r1.zw = frac(r1.xyxy).zw;
	r1.xy = (-r1.zwzw + r1).xy;
	r1.xy = (r1 + c3.yyyy).xy;
	r0.w = (r1.y * -c4.w + r1.x);
	r0.w = r0.w * c6.w;
	r0.w = frac(r0.w);
	r1.xy = (r0.wwww + c7).xy;
	r2.yz = (r1.y >= 0 ? c7.xzww : c7.xwzw).yz;
	r2.x = c8.w;
	r1.xyz = (r1.x >= 0 ? r2 : c7.wzzw).xyz;
	r1.xyz = (r0 * r1).xyz;
	r2.z = c6.z;
	r0.w = r2.z + -c2.y;
	oC0.xyz = (r0.w >= 0 ? r0 : r1).xyz;
	oC0.w = -c3.x;
}

technique CRT_Yee64 {
	pass {
		VertexShader = PostProcessVS;
		PixelShader = PS_CRT_Yee64;
	}
}
