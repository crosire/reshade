//Shader edited originalnicodr

//If you want to have multiple instances of StageDepthPlus you will have to change the following lines in every copy of the shader:
//- Line 48: change "StageDepthPlus", "StageMaskTex",  to anything else (any namespace that you arent using in other shader).
//- Line 521: change the name of the technique
//- Replace all "StageTexPlus", "StageDepthTex" and "StageMaskTex" with "StageTexPlus1", "StageDepthTex1" and "StageMaskTex1" respectivly or a different text.
//- Change the name of "STAGE_TEXTURE_WIDTH" and "STAGE_TEXTURE_HEIGHT" every time they appear for something else.

// You can also change the "Stageplus.png" name file in line 58 for the name of the image you want to use in this instance, but it should be easier yo just edit that preprocessor definition inside the shader.

////Check for updates here: https://github.com/originalnicodr/CorgiFX

// Made by Marot Satil for the GShade ReShade package!
// You can follow me via @MarotSatil on Twitter, but I don't use it all that much.
// Follow @GPOSERS_FFXIV on Twitter and join us on Discord (https://discord.gg/39WpvU2)
// for the latest GShade package updates!
//
// This shader was designed in the same vein as GreenScreenDepth.fx, but instead of applying a
// green screen with adjustable distance, it applies a PNG texture with adjustable opacity.
//
// PNG transparency is fully supported, so you could for example add another moon to the sky
// just as readily as create a "green screen" stage like in real life.
//
// Copyright (c) 2019, Marot Satil
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, the header above it, this list of conditions, and the following disclaimer
//    in this position and unchanged.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, the header above it, this list of conditions, and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

namespace StageDepthPlus
{
	#include "Reshade.fxh"

	#if LAYER_SINGLECHANNEL
	    #define TEXFORMAT R8
	#else
	    #define TEXFORMAT RGBA8
	#endif

	#ifndef StageTexPlus
	#define StageTexPlus "StageImage.png"//Put your image file name here or remplace the original file
	#endif

	#ifndef StageDepthTex
	#define StageDepthTex "StageDepthmap.png"//Put your depthmap image file name here or remplace the original file
	#endif

	#ifndef StageMaskTex
	#define StageMaskTex "StageMask.png"//Put your mask image file name here or remplace the original file
	#endif

	  ////////////
	 /// MENU ///
	////////////

	uniform bool DepthMapY < 
		ui_label = "Use depth map";
	    ui_category = "Controls";
	> = false;

	uniform bool UseMask < 
		ui_label = "Use a mask image";
	    ui_category = "Controls";
	> = false;

	uniform bool FlipH < 
		ui_label = "Flip Horizontal";
	    ui_category = "Controls";
	> = false;

	uniform bool FlipV < 
		ui_label = "Flip Vertical";
	    ui_category = "Controls";
	> = false;

	uniform bool RepeatTextureEnabled < 
		ui_label = "Repeat texture";
	    ui_category = "Controls";
	> = false;

	uniform float Stage_Opacity <
		ui_category = "Controls";
	    ui_type = "slider";
	    ui_label = "Opacity";
	    ui_min = 0.0; ui_max = 1.0;
	    ui_step = 0.002;
	    ui_tooltip = "Set the transparency of the image.";
	> = 1.0;

	uniform float2 Layer_Scale <
		ui_category = "Controls";
	  	ui_type = "slider";
		ui_label = "Scale";
		ui_step = 0.01;
		ui_min = 0.01; ui_max = 5.0;
	> = (1.001,1.001);

	uniform float2 Layer_Pos <
		ui_category = "Controls";
	  	ui_type = "slider";
		ui_label = "Position";
		ui_step = 0.001;
		ui_min = -1.5; ui_max = 1.5;
	> = (0,0);	

	uniform float Axis <
		ui_category = "Controls";
		ui_type = "slider";
		ui_label = "Angle";
		ui_step = 0.1;
		ui_min = -180.0; ui_max = 180.0;
	> = 0.0;

	uniform int BlendM <
		ui_category = "Controls";
		ui_type = "combo";
		ui_label = "Blending Mode";
		ui_tooltip = "Select the blending mode used with the gradient on the screen.";
		ui_items = "Normal\0Multiply\0Screen\0Overlay\0Darken\0Lighten\0Color Dodge\0Color Burn\0Hard Light\0Soft Light\0Difference\0Exclusion\0Hue\0Saturation\0Color\0Luminosity\0Linear burn\0Linear dodge\0Vivid light\0Linearlight\0Pin light\0Hardmix\0Reflect\0Glow\0";
	> = 0;

	uniform float Stage_depth <
		ui_category = "Controls";
		ui_type = "slider";
		ui_min = 0.0; ui_max = 1.0;
		ui_label = "Depth";
	> = 0.97;

	uniform float depth_smoothing <
		ui_category = "Controls";
        ui_type = "slider";
        ui_label = "Depth Smoothing";
        ui_tooltip = "Depth Smoothing";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 0.005;

	uniform bool debug_depth < 
		ui_category = "Controls";
		ui_label = "Debug Depth";
	> = false;

	uniform float3 ProjectorPos <
		ui_text = "Experimental Features\n--------------------------------------------";
		ui_label = "Projector Position";
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = float3(-100,-100,-100); ui_max = float3(100,100,100);
	> = float3(0.5,0.5,0);

	//////////////////////////////////////
	// textures
	//////////////////////////////////////
	#ifndef STAGE_TEXTURE_WIDTH
	#define STAGE_TEXTURE_WIDTH BUFFER_WIDTH
	#endif

	#ifndef STAGE_TEXTURE_HEIGHT
	#define STAGE_TEXTURE_HEIGHT BUFFER_HEIGHT
	#endif

	//If you enter a large value for the texture width or height the game might crash and the shader stop working in that preset. To fix it open the preset.ini file and lower de value manually
	//If for some reason the shader dosnt work for a graphic api coment all these if and decommend the commented line below

	#if (__RENDERER__ == 0x9000)//DX9 4096 x 4096
		#if (STAGE_TEXTURE_HEIGHT > 4096 || STAGE_TEXTURE_WIDTH > 4096)
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH>STAGE_TEXTURE_HEIGHT ? 4096 : int((STAGE_TEXTURE_WIDTH/STAGE_TEXTURE_HEIGHT)*4096); Height = STAGE_TEXTURE_HEIGHT>STAGE_TEXTURE_WIDTH ? 4096 : int((STAGE_TEXTURE_HEIGHT/STAGE_TEXTURE_WIDTH)*4096); Format=TEXFORMAT; };
		#else
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH; Height = STAGE_TEXTURE_HEIGHT; Format=TEXFORMAT;};
		#endif
	#elif (__RENDERER__ >=  0xb000) //DX11 to 16384 x 16384, DX12, VULKAN and OPENGL should also enter here
		#if (STAGE_TEXTURE_HEIGHT > 16384 || STAGE_TEXTURE_WIDTH > 16384)
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH>STAGE_TEXTURE_HEIGHT ? 16384 : int((STAGE_TEXTURE_WIDTH/STAGE_TEXTURE_HEIGHT)*16384); Height = STAGE_TEXTURE_HEIGHT>STAGE_TEXTURE_WIDTH ? 16384 : int((STAGE_TEXTURE_HEIGHT/STAGE_TEXTURE_WIDTH)*16384); Format=TEXFORMAT; };
		#else
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH; Height = STAGE_TEXTURE_HEIGHT; Format=TEXFORMAT;};
		#endif
	#elif (__RENDERER__ >= 0xa000)//DX10 8192 x 8192
		#if (STAGE_TEXTURE_HEIGHT > 8192 || STAGE_TEXTURE_WIDTH > 8192)
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH>STAGE_TEXTURE_HEIGHT ? 8192 : int((STAGE_TEXTURE_WIDTH/STAGE_TEXTURE_HEIGHT)*8192); Height = STAGE_TEXTURE_HEIGHT>STAGE_TEXTURE_WIDTH ? 8192 : int((STAGE_TEXTURE_HEIGHT/STAGE_TEXTURE_WIDTH)*8192); Format=TEXFORMAT; };
		#else
			texture Stageplus_texture <source=StageTexPlus;> { Width = STAGE_TEXTURE_WIDTH; Height = STAGE_TEXTURE_HEIGHT; Format=TEXFORMAT;};
		#endif
	#endif
	
	//texture Stageplus_texture <source=StageTexPlus;> { Width = BUFFER_WIDTH*3; Height = BUFFER_HEIGHT*3; Format=TEXFORMAT; };

	texture Depth_Map_tex <
	    source = StageDepthTex;
	> {
	    Format = RGBA8;
	    Width  = STAGE_TEXTURE_WIDTH;
	    Height = STAGE_TEXTURE_HEIGHT;
	};

	texture Mask_tex <
	    source = StageMaskTex;
	> {
	    Format = RGBA8;
	    Width  = STAGE_TEXTURE_WIDTH;
	    Height = STAGE_TEXTURE_HEIGHT;
	};



	sampler Depth_Map_Sampler_Repeat
	{
	    Texture  = Depth_Map_tex;
		AddressU = REPEAT;
		AddressV = REPEAT;

	};

	sampler Stageplus_sampler_Repeat {
		Texture = Stageplus_texture;
		AddressU = REPEAT;
		AddressV = REPEAT;
	};

	sampler Mask_sampler_Repeat {
		Texture = Mask_tex;
		AddressU = REPEAT;
		AddressV = REPEAT;
	};


	sampler Depth_Map_Sampler	{Texture  = Depth_Map_tex; };
	sampler Stageplus_sampler {Texture = Stageplus_texture; };
	sampler Mask_sampler {Texture = Mask_tex; };




	/*
	#if ((3*BUFFER_WIDTH <= 8192) && (3*BUFFER_WIDTH <= 8192))
	texture Stageplus_texture <source=StageTexPlus;> { Width = BUFFER_WIDTH*3; Height = BUFFER_HEIGHT*3; Format=TEXFORMAT; };
	#else
	#if ((2*BUFFER_WIDTH <= 8192) && (2*BUFFER_WIDTH <= 8192))
	texture Stageplus_texture <source=StageTexPlus;> { Width = BUFFER_WIDTH*2; Height = BUFFER_HEIGHT*2; Format=TEXFORMAT; };
	#else
	texture Stageplus_texture <source=StageTexPlus;> { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=TEXFORMAT; };
	#endif
	#endif
	*/


	//Blending modes functions

	// Screen blending mode
	float3 Screen(float3 LayerA, float3 LayerB)
	{ return 1.0 - (1.0 - LayerA) * (1.0 - LayerB); }

	// Multiply blending mode
	float3 Multiply(float3 LayerA, float3 LayerB)
	{ return LayerA * LayerB; }

	// Darken blending mode
	float3 Darken(float3 LayerA, float3 LayerB)
	{ return min(LayerA,LayerB); }

	// Lighten blending mode
	float3 Lighten(float3 LayerA, float3 LayerB)
	{ return max(LayerA,LayerB); }

	// Color Dodge blending mode (by prod80)
	float3 ColorDodge(float3 LayerA, float3 LayerB)
	{ return LayerB>=1.0f ? LayerB:saturate(LayerA/(1.0f-LayerB));}
		
	// Color Burn blending mode (by prod80)
	float3 ColorBurn(float3 LayerA, float3 LayerB)
	{ return LayerB<=0.0f ? LayerB:saturate(1.0f-((1.0f-LayerA)/LayerB)); }

	// Hard light blending mode
	float3 HardLight(float3 LayerA, float3 LayerB)
	{ return (LayerB.r <= 0.5 && LayerB.g <=0.5 && LayerB.b <= 0.5) ? clamp(Multiply(LayerA,2*LayerB),0,1) : clamp(Multiply(LayerA,2*LayerB-1),0,1);}

	float3 Aux(float3 x)
	{ return (x.r<=0.25 && x.g<=0.25 && x.b<=0.25) ? ((16.0*x-12.0)*x+4)*x : sqrt(x);}

	// Soft light blending mode
	float3 SoftLight(float3 LayerA, float3 LayerB)
	{ return (LayerB.r <= 0.5 && LayerB.g <=0.5 && LayerB.b <= 0.5) ? clamp(LayerA-(1.0-2*LayerB)*LayerA*(1-LayerA),0,1) : clamp(LayerA+(2*LayerB-1.0)*(Aux(LayerA)-LayerA),0,1);}


	// Difference blending mode
	float3 Difference(float3 LayerA, float3 LayerB)
	{ return LayerA-LayerB; }

	// Exclusion blending mode
	float3 Exclusion(float3 LayerA, float3 LayerB)
	{ return LayerA+LayerB-2*LayerA*LayerB; }

	// Overlay blending mode
	float3 Overlay(float3 c, float3 b)
	{ return c<0.5f ? 2.0f*c*b:(1.0f-2.0f*(1.0f-c)*(1.0f-b));}


	float Lum(float3 c){
		return (0.3*c.r+0.59*c.g+0.11*c.b);}

	float min3 (float a, float b, float c){
		return min(a,(min(b,c)));
	}

	float max3 (float a, float b, float c){
		return max(a,(max(b,c)));
	}

	float Sat(float3 c){
		return max3(c.r,c.g,c.b)-min3(c.r,c.g,c.b);}

	float3 ClipColor(float3 c){
		float l=Lum(c);
		float n=min3(c.r,c.g,c.b);
		float x=max3(c.r,c.g,c.b);
		float cr=c.r;
		float cg=c.g;
		float cb=c.b;
		if (n<0){
			cr=l+(((cr-l)*l)/(l-n));
			cg=l+(((cg-l)*l)/(l-n));
			cb=l+(((cb-l)*l)/(l-n));
		}
		if (x>1){
			cr=l+(((cr-l)*(1-l))/(x-l));
			cg=l+(((cg-l)*(1-l))/(x-l));
			cb=l+(((cb-l)*(1-l))/(x-l));
		}
		return float3(cr,cg,cb);
	}

	float3 SetLum (float3 c, float l){
		float d= l-Lum(c);
		return float3(c.r+d,c.g+d,c.b+d);
	}

	float3 SetSat(float3 c, float s){
		float cr=c.r;
		float cg=c.g;
		float cb=c.b;
		if (cr==max3(cr,cg,cb) && cb==min3(cr,cg,cb)){//caso r->max g->mid b->min
			if (cr>cb){
				cg=(((cg-cb)*s)/(cr-cb));
				cr=s;
			}
			else{
				cg=0;
				cr=0;
			}
			cb=0;
		}
		else{
			if (cr==max3(cr,cg,cb) && cg==min3(cr,cg,cb)){//caso r->max b->mid g->min
				if (cr>cg){
					cb=(((cb-cg)*s)/(cr-cg));
					cr=s;
				}
				else{
					cb=0;
					cr=0;
				}
				cg=0;
			}
			else{
				if (cg==max3(cr,cg,cb) && cb==min3(cr,cg,cb)){//caso g->max r->mid b->min
					if (cg>cb){
						cr=(((cr-cb)*s)/(cg-cb));
						cg=s;
					}
					else{
						cr=0;
						cg=0;
					}
					cb=0;
				}
				else{
					if (cg==max3(cr,cg,cb) && cr==min3(cr,cg,cb)){//caso g->max b->mid r->min
						if (cg>cr){
							cb=(((cb-cr)*s)/(cg-cr));
							cg=s;
						}
						else{
							cb=0;
							cg=0;
						}
						cr=0;
					}
					else{
						if (cb==max3(cr,cg,cb) && cg==min3(cr,cg,cb)){//caso b->max r->mid g->min
							if (cb>cg){
								cr=(((cr-cg)*s)/(cb-cg));
								cb=s;
							}
							else{
								cr=0;
								cb=0;
							}
							cg=0;
						}
						else{
							if (cb==max3(cr,cg,cb) && cr==min3(cr,cg,cb)){//caso b->max g->mid r->min
								if (cb>cr){
									cg=(((cg-cr)*s)/(cb-cr));
									cb=s;
								}
								else{
									cg=0;
									cb=0;
								}
								cr=0;
							}
						}
					}
				}
			}
		}
		return float3(cr,cg,cb);
	}

	float3 Hue(float3 b, float3 s){
		return SetLum(SetSat(s,Sat(b)),Lum(b));
	}

	float3 Saturation(float3 b, float3 s){
		return SetLum(SetSat(b,Sat(s)),Lum(b));
	}

	float3 ColorM(float3 b, float3 s){
		return SetLum(s,Lum(b));
	}

	float3 Luminosity(float3 b, float3 s){
		return SetLum(b,Lum(s));
	}

	//Blend functions priveded by prod80

	// Linearburn
	float3 Linearburn(float3 c, float3 b) 	{ return max(c+b-1.0f, 0.0f);}
	// Lineardodge
	float3 Lineardodge(float3 c, float3 b) 	{ return min(c+b, 1.0f);}
	// Vividlight
	float3 Vividlight(float3 c, float3 b) 	{ return b<0.5f ? ColorBurn(c, (2.0f*b)):ColorDodge(c, (2.0f*(b-0.5f)));}
	// Linearlight
	float3 Linearlight(float3 c, float3 b) 	{ return b<0.5f ? Linearburn(c, (2.0f*b)):Lineardodge(c, (2.0f*(b-0.5f)));}
	// Pinlight
	float3 Pinlight(float3 c, float3 b) 	{ return b<0.5f ? Darken(c, (2.0f*b)):Lighten(c, (2.0f*(b-0.5f)));}
	// Hard Mix
	float3 Hardmix(float3 c, float3 b)      { return Vividlight(c,b)<0.5f ? 0.0 : 1.0;}
	// Reflect
	float3 Reflect(float3 c, float3 b)      { return b>=0.999999f ? b:saturate(c*c/(1.0f-b));}
	// Glow
	float3 Glow(float3 c, float3 b)         { return Reflect(b, c);}

	//rotate vector spec
	float2 rotate(float2 v,float2 o, float a){
		float2 v2= v-o;
		v2=float2((cos(a) * v2.x-sin(a)*v2.y),sin(a)*v2.x +cos(a)*v2.y);
		v2=v2+o;
		return v2;
	}
	

	  //////////////
	 /// SHADER ///
	//////////////

	void PS_StageDepth(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float4 color : SV_Target)
	{
		static const float3x3 ProjectionMatrix = float3x3(1, 0, ProjectorPos.x-0.5,
										  0, 1, ProjectorPos.y-0.5,
										  0, 0, ProjectorPos.z
										);
		
		float3 uvtemp3=float3(texcoord,ReShade::GetLinearizedDepth(texcoord));
		float2 uvtemp=mul(ProjectionMatrix,uvtemp3).xy;

		float4 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgba;

		color= backbuffer;//because of the warning

		float depth = 1 - ReShade::GetLinearizedDepth(texcoord).r;
		
		if (FlipH) {uvtemp.x = 1-uvtemp.x;}//horizontal flip
	    if (FlipV) {uvtemp.y = 1-uvtemp.y;} //vertical flip

		float2 Layer_Scalereal= float2 (Layer_Scale.x-0.44,(Layer_Scale.y-0.44)*STAGE_TEXTURE_WIDTH/STAGE_TEXTURE_HEIGHT);

	    float2 Layer_Posreal= float2((FlipH) ? -Layer_Pos.x : Layer_Pos.x, (FlipV) ? Layer_Pos.y:-Layer_Pos.y);

		uvtemp= float2(((uvtemp.x*BUFFER_WIDTH-(BUFFER_WIDTH-BUFFER_HEIGHT)/2)/BUFFER_HEIGHT),uvtemp.y);
		
		uvtemp=(rotate(uvtemp,Layer_Posreal+0.5,radians(Axis))*Layer_Scalereal-((Layer_Posreal+0.5)*Layer_Scalereal-0.5));


		float4 layer  = RepeatTextureEnabled ? tex2D(Stageplus_sampler_Repeat, uvtemp).rgba : tex2D(Stageplus_sampler, uvtemp).rgba;

		
		if (UseMask){
			float4 mask= RepeatTextureEnabled ? tex2D(Mask_sampler_Repeat, uvtemp).rgba :tex2D(Mask_sampler, uvtemp).rgba;
			//If there is a weird result check if (mask.a==1) before applying the mask to the layer.
			layer=float4(layer.rgb, min(layer.a,mask.r));
		}

		float4 precolor   = lerp(backbuffer, layer, layer.a * Stage_Opacity);

		float ImageDepthMap_depth = DepthMapY ? (RepeatTextureEnabled ? tex2D(Depth_Map_Sampler_Repeat,uvtemp).x : tex2D(Depth_Map_Sampler,uvtemp).x) : 0;


		float combined_depth = saturate(Stage_depth + ImageDepthMap_depth);

		float depth_np    = combined_depth - 1;
   		float depth_fp    = combined_depth;
    	float dn          = smoothstep( depth_np - depth_smoothing, depth_np, depth );
    	float df          = 1.0f - smoothstep( depth_fp, depth_fp + depth_smoothing, depth );
    
    	float gradient_depth = ( dn * df );

		float total_alpha = layer.a * Stage_Opacity * gradient_depth;

		if(debug_depth){
			color = total_alpha;
			return;
		}

		if(total_alpha==0) return;

		switch (BlendM){
			case 0:{color = lerp(backbuffer.rgb, precolor.rgb, total_alpha);break;}
			case 1:{color = lerp(backbuffer, float4 (Multiply(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 2:{color = lerp(backbuffer, float4 (Screen(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 3:{color = lerp(backbuffer, float4 (Overlay(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 4:{color = lerp(backbuffer, float4 (Darken(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 5:{color = lerp(backbuffer, float4 (Lighten(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 6:{color = lerp(backbuffer, float4 (ColorDodge(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 7:{color = lerp(backbuffer, float4 (ColorBurn(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 8:{color = lerp(backbuffer, float4 (HardLight(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 9:{color = lerp(backbuffer, float4 (SoftLight(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 10:{color = lerp(backbuffer, float4 (Difference(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 11:{color = lerp(backbuffer, float4 (Exclusion(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 12:{color = lerp(backbuffer, float4 (Hue(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 13:{color = lerp(backbuffer, float4 (Saturation(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 14:{color = lerp(backbuffer, float4 (ColorM(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 15:{color = lerp(backbuffer, float4 (Luminosity(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 16:{color = lerp(backbuffer, float4 (Lineardodge(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 17:{color = lerp(backbuffer, float4 (Linearburn(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 18:{color = lerp(backbuffer, float4 (Vividlight(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 19:{color = lerp(backbuffer, float4 (Linearlight(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 20:{color = lerp(backbuffer, float4 (Pinlight(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 21:{color = lerp(backbuffer, float4 (Hardmix(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 22:{color = lerp(backbuffer, float4 (Reflect(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
			case 23:{color = lerp(backbuffer, float4 (Glow(backbuffer.rgb, precolor.rgb),backbuffer.a), total_alpha);break;}
		}


		color.a = backbuffer.a;

	}

	technique StageDepthPlus
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_StageDepth;
		}
	}
}
