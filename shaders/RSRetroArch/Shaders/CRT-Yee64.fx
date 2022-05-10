/*
	Ripped from Sonic Mania
	CRT-Soft
*/

#include "ReShade.fxh"

void PS_CRT_Yee64(
	float4 pos : SV_POSITION,
	float2 uv : TEXCOORD,
	out float4 oC0 : SV_TARGET
) {
	//Declare parameters
	//Window Size
	int2 i2Resolution = int2(424,240);
	float fDownScale = 0.005;
	//pixelSize
	float4 c0 = ReShade::ScreenSize.xyyy / float4(ReShade::AspectRatio, 0.5, 1, 1);
	//textureSize
	float4 c1 = i2Resolution.xyyy / fDownScale;
	//viewSize
	float4 c2 = ReShade::ScreenSize.xyyy / float4(ReShade::AspectRatio, 1, 1, 1);
	//texDiffuse
	#define s0 ReShade::BackBuffer
	
	//Declare constants
	static const float4 c3 = float4(-1, 0.5, 1.25, 0);
    static const float4 c4 = float4(0.5, -0.5, 1.5, -3);
    static const float4 c5 = float4(-1, 1, -2, 2);
    static const float4 c6 = float4(-3, -8, 720, 0.166666672);
    static const float4 c7 = float4(-0.333000004, -0.666000009, 0.899999976, 1.20000005);
    static const float4 c8 = float4(1.5, 0.5, 2.5, 0.899999976);

	//Declare registers
	float4 r_r0, r_r1, r_r2, r_r3, r_r4, r_r5, r_r6, r_r7, r_r8, r_r9;

	//Code starts here
	float4 v0 = uv.xyyy;
	//dcl_2d s0
	r_r0.z = c3.w;
	r_r1.x = 1.0 / c0.x;
	r_r1.y = 1.0 / c0.y;
	r_r1.xy = (r_r1 * c1).xy;
	r_r1.xy = (r_r1 * v0).xy;
	r_r2.x = 1.0 / c1.x;
	r_r2.y = 1.0 / c1.y;
	r_r1.zw = (r_r2.xyxy * c0.xyxy).zw;
	r_r1.zw = (r_r1 * r_r1.xyxy).zw;
	r_r1.xy = (r_r1 * c2).xy;
	r_r2.zw = (r_r1 * c1.xyxy).zw;
	r_r2.zw = frac(r_r2).zw;
	r_r0.xy = (-r_r2.zwzw).xy;
	r_r3.xy = (r_r1.zwzw * c1 + r_r0.xzzw).xy;
	r_r4.yz = (r_r1.xzww * c1.xxyw + r_r0.xzyw).yz;
	r_r3.z = r_r0.y + r_r3.y;
	r_r5 = r_r3.xzxz + -c4.zyxy;
	r_r3 = r_r3.xzxz + c8.xyzy;
	r_r3 = r_r2.xyxy * r_r3;
	r_r5 = r_r2.xyxy * r_r5;
	r_r6 = tex2D(s0, r_r5.zw);
	r_r5 = tex2D(s0, r_r5.xy);
	r_r5.xyz = (r_r5 * c3.zzzz).xyz;
	r_r7 = r_r1.zwzw * c1.xyxy + r_r0.xyxy;
	r_r0.zw = (r_r1 * c1.xyxy + - r_r7).zw;
	r_r8.x = c3.x;
	r_r1.zw = (r_r1 * c1.xyxy + r_r8.xxxx).zw;
	r_r1.zw = (r_r0.xyxy + r_r1).zw;
	r_r4.x = r_r0.x + r_r4.y;
	r_r4 = r_r4.xzxz + c4.xyxz;
	r_r4 = r_r2.xyxy * r_r4;
	r_r0.xy = (r_r1.zwzw + c3.yyyy).xy;
	r_r0.xy = (r_r2 * r_r0).xy;
	r_r8 = tex2D(s0, r_r0.xy);
	r_r8.xyz = (r_r8 * c3.zzzz).xyz;
	r_r0.xy = (r_r0.zwzw + -c3.yyyy).xy;
	r_r9 = -r_r0.xxxx + c5;
	r_r9 = r_r9 * r_r9;
	r_r9 = r_r9 * c4.wwww;
	r_r0.z = pow(2, r_r9.x);
	r_r6.xyz = (r_r6 * r_r0.zzzz).xyz;
	r_r6.xyz = (r_r6 * c3.zzzz).xyz;
	r_r0.w = pow(2, r_r9.z);
	r_r5.xyz = (r_r5 * r_r0.wwww + r_r6).xyz;
	r_r0.w = r_r0.z + r_r0.w;
	r_r6 = r_r7.zwzw + c4.zyxx;
	r_r7 = r_r7 + c4.yzzz;
	r_r7 = r_r2.xyxy * r_r7;
	r_r2 = r_r2.xyxy * r_r6;
	r_r6 = tex2D(s0, r_r2.zw);
	r_r2 = tex2D(s0, r_r2.xy);
	r_r2.xyz = (r_r2 * c3.zzzz).xyz;
	r_r1.zw = (r_r0.xyxy * r_r0.xyxy).zw;
	r_r0.xy = (-r_r0.yyyy + c5).xy;
	r_r0.xy = (r_r0 * r_r0).xy;
	r_r0.xy = (r_r0 * c6.yyyy).xy;
	r_r1.zw = (r_r1 * c6.xyxy).zw;
	r_r1.z = pow(2, r_r1.z);
	r_r1.w = pow(2, r_r1.w);
	r_r6.xyz = (r_r6 * r_r1.zzzz).xyz;
	r_r5.xyz = (r_r6 * c3.zzzz + r_r5).xyz;
	r_r6 = tex2D(s0, r_r3.xy);
	r_r3 = tex2D(s0, r_r3.zw);
	r_r3.xyz = (r_r3 * c3.zzzz).xyz;
	r_r2.w = pow(2, r_r9.y);
	r_r3.w = pow(2, r_r9.w);
	r_r6.xyz = (r_r6 * r_r2.wwww).xyz;
	r_r5.xyz = (r_r6 * c3.zzzz + r_r5).xyz;
	r_r3.xyz = (r_r3 * r_r3.wwww + r_r5).xyz;
	r_r0.w = r_r0.w + r_r1.z;
	r_r0.w = r_r2.w + r_r0.w;
	r_r0.w = r_r3.w + r_r0.w;
	r_r0.w = 1.0 / r_r0.w;
	r_r3.xyz = (r_r0.wwww * r_r3).xyz;
	r_r3.xyz = (r_r1.wwww * r_r3).xyz;
	r_r5 = tex2D(s0, r_r4.xy);
	r_r4 = tex2D(s0, r_r4.zw);
	r_r4.xyz = (r_r1.zzzz * r_r4).xyz;
	r_r4.xyz = (r_r4 * c3.zzzz).xyz;
	r_r5.xyz = (r_r5 * c3.zzzz).xyz;
	r_r5.xyz = (r_r1.zzzz * r_r5).xyz;
	r_r0.w = r_r0.z + r_r1.z;
	r_r0.w = r_r2.w + r_r0.w;
	r_r0.w = 1.0 / r_r0.w;
	r_r5.xyz = (r_r8 * r_r0.zzzz + r_r5).xyz;
	r_r2.xyz = (r_r2 * r_r2.wwww + r_r5).xyz;
	r_r2.xyz = (r_r0.wwww * r_r2).xyz;
	r_r0.x = pow(2, r_r0.x);
	r_r0.y = pow(2, r_r0.y);
	r_r2.xyz = (r_r2 * r_r0.xxxx + r_r3).xyz;
	r_r3 = tex2D(s0, r_r7.xy);
	r_r5 = tex2D(s0, r_r7.zw);
	r_r5.xyz = (r_r2.wwww * r_r5).xyz;
	r_r3.xyz = (r_r0.zzzz * r_r3).xyz;
	r_r3.xyz = (r_r3 * c3.zzzz + r_r4).xyz;
	r_r3.xyz = (r_r5 * c3.zzzz + r_r3).xyz;
	r_r0.xzw = (r_r0.wwww * r_r3.xyyz).xzw;
	r_r0.xyz = (r_r0.xzww * r_r0.yyyy + r_r2).xyz;
	r_r1.zw = frac(r_r1.xyxy).zw;
	r_r1.xy = (-r_r1.zwzw + r_r1).xy;
	r_r1.xy = (r_r1 + c3.yyyy).xy;
	r_r0.w = (r_r1.y * -c4.w + r_r1.x);
	r_r0.w = r_r0.w * c6.w;
	r_r0.w = frac(r_r0.w);
	r_r1.xy = (r_r0.wwww + c7).xy;
	r_r2.yz = (r_r1.y >= 0 ? c7.xzww : c7.xwzw).yz;
	r_r2.x = c8.w;
	r_r1.xyz = (r_r1.x >= 0 ? r_r2 : c7.wzzw).xyz;
	r_r1.xyz = (r_r0 * r_r1).xyz;
	r_r2.z = c6.z;
	r_r0.w = r_r2.z + -c2.y;
	oC0.xyz = (r_r0.w >= 0 ? r_r0 : r_r1).xyz;
	oC0.w = -c3.x;
}

technique CRT_Yee64 {
	pass {
		VertexShader = PostProcessVS;
		PixelShader = PS_CRT_Yee64;
	}
}
