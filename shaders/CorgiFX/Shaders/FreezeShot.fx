//Check for updates here: https://github.com/originalnicodr/CorgiFX

#include "ReShade.fxh"

uniform bool FlipH < 
	ui_label = "Flip Horizontal";
    ui_category = "Controls";
> = false;

uniform bool FlipV < 
	ui_label = "Flip Vertical";
    ui_category = "Controls";
> = false;

uniform bool Freeze < 
    ui_category = "Controls";
> = false;

uniform float Shot_depth_far <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Shot depth far";
> = 0;

uniform float Shot_depth_close <
	ui_type = "slider";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "Shot depth close";
> = 1;

uniform float Stage_Opacity <
    ui_type = "slider";
    ui_label = "Opacity";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.002;
    ui_tooltip = "Set the transparency of the image.";
> = 1.0;

uniform float2 Layer_Scale <
  	ui_type = "slider";
	ui_label = "Scale";
	ui_step = 0.01;
	ui_min = 0.01; ui_max = 5.0;
> = (1.5,1.5);

uniform float2 Layer_Pos <
  	ui_type = "slider";
	ui_label = "Position";
	ui_step = 0.001;
	ui_min = -1.5; ui_max = 1.5;
> = (0,0);	

uniform float Axis <
	ui_type = "slider";
	ui_label = "Angle";
	ui_step = 0.1;
	ui_min = -180.0; ui_max = 180.0;
> = 0.0;

uniform int BlendM <
	ui_type = "combo";
	ui_label = "Blending Mode";
	ui_tooltip = "Select the blending mode used with the gradient on the screen.";
	ui_items = "Normal\0Multiply\0Screen\0Overlay\0Darken\0Lighten\0Color Dodge\0Color Burn\0Hard Light\0Soft Light\0Difference\0Exclusion\0Hue\0Saturation\0Color\0Luminosity\0Linear burn\0Linear dodge\0Vivid light\0Linearlight\0Pin light\0Hardmix\0Reflect\0Glow\0";
> = 0;

uniform float Stage_depth <
	ui_type = "slider";
	ui_min = -1.0; ui_max = 1.0;
	ui_label = "Layer Depth";
	ui_tooltip = "If you want to use the depth from the frozen layer decrease this value";
> = 0.97;

uniform bool BlackBackground < 
	ui_label = "Black Background";
	ui_tooltip = "If you are using this shader for hotsampling purposes turning this on might make the process easier";
> = false;


texture FreezeTexold		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=RGBA8;};
texture FreezeTexnew		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=RGBA8;};
sampler FreezeSamplernew		{ Texture = FreezeTexold; };
sampler FreezeSamplerold		{ Texture = FreezeTexnew; };


texture DepthTexold		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=RGBA8;};
texture DepthTexnew		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format=RGBA8;};
sampler DepthSamplernew		{ Texture = DepthTexold; };
sampler DepthSamplerold		{ Texture = DepthTexnew; };

void GrabFrame(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab the color selected
    float depth = 1 - ReShade::GetLinearizedDepth(UvCoord).r;
	if (!Freeze){
        Image= tex2D(ReShade::BackBuffer, UvCoord).rgba;
        Image.a= (depth>=Shot_depth_far && depth<=Shot_depth_close) ? 1 : 0;
        }
    else{Image= tex2D(FreezeSamplerold, UvCoord).rgba;}
}

void SaveFrame(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab the color selected 
	Image = tex2D(FreezeSamplernew, UvCoord).rgba;
}

void GrabDepth(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab the color selected
    float depth = 1 - ReShade::GetLinearizedDepth(UvCoord).r;
	if (!Freeze){
        Image= float3(depth,depth,depth);
        Image.a= (depth>=Shot_depth_far && depth<=Shot_depth_close) ? 1 : 0;
        }
    else{Image= tex2D(DepthSamplerold, UvCoord).rgba;}
}

void SaveDepth(float4 vpos : SV_Position, float2 UvCoord : TEXCOORD, out float4 Image : SV_Target)
{
	// Grab the color selected 
	Image = tex2D(DepthSamplernew, UvCoord).rgba;
}



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

// Color Dodge blending mode
float3 ColorDodge(float3 LayerA, float3 LayerB)
{ return (LayerB.r < 1 && LayerB.g < 1 && LayerB.b < 1) ? min(1.0,LayerA/(1.0-LayerB)) : 1.0;}

// Color Burn blending mode
float3 ColorBurn(float3 LayerA, float3 LayerB)
{ return (LayerB.r > 0 && LayerB.g > 0 && LayerB.b > 0) ? 1.0-min(1.0,(1.0-LayerA)/LayerB) : 0;}

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
float3 Overlay(float3 LayerA, float3 LayerB)
{ return HardLight(LayerB,LayerA); }


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


float2 rotate(float2 v,float2 o, float a){
	float2 v2= v-o;
	v2=float2((cos(a) * v2.x-sin(a)*v2.y),sin(a)*v2.x +cos(a)*v2.y);
	v2=v2+o;
	return v2;
}

//float3 Freezef(float4 position : SV_Position, float2 texcoord : TexCoord, out float4 color : SV_Target)
void Freezef(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0, out float3 color : SV_Target)
{
    float2 Layer_Scalereal= float2 (Layer_Scale.x-0.44,(Layer_Scale.y-0.44)*BUFFER_WIDTH/BUFFER_HEIGHT);
    float2 Layer_Posreal= float2((FlipH) ? -Layer_Pos.x : Layer_Pos.x, (FlipV) ? Layer_Pos.y:-Layer_Pos.y);
	float4 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgba;
	float depth = 1 - ReShade::GetLinearizedDepth(texcoord).r;
    float2 uvtemp= texcoord;
    if (FlipH) {uvtemp.x = 1-uvtemp.x;}//horizontal flip
    if (FlipV) {uvtemp.y = 1-uvtemp.y;} //vertical flip
	uvtemp=float2(((uvtemp.x*BUFFER_WIDTH-(BUFFER_WIDTH-BUFFER_HEIGHT)/2)/BUFFER_HEIGHT),uvtemp.y);
    uvtemp=(rotate(uvtemp,Layer_Posreal+0.5,radians(Axis))*Layer_Scalereal-((Layer_Posreal+0.5)*Layer_Scalereal-0.5));
	float4 layer     = tex2D(FreezeSamplernew, uvtemp).rgba;

	//layer.a=BlackBackground ? 1 : layer.a;

	float4 precolor   = lerp(backbuffer, layer, Stage_Opacity);
    //float4 color;
	color=backbuffer.rgb;

	float ImageDepthMap_depth = tex2D(DepthSamplernew,uvtemp).x;

	if( depth < saturate(ImageDepthMap_depth+Stage_depth))
	{   
        if (uvtemp.x>0 && uvtemp.x>0 && uvtemp.y>0 && uvtemp.y>0 && uvtemp.x<1 && uvtemp.x<1 && uvtemp.y<1 && uvtemp.y<1){
		    switch (BlendM){
		    	case 0:{color = lerp(backbuffer.rgb, precolor.rgb,layer.a*Stage_Opacity);break;}
		    	case 1:{color = lerp(backbuffer.rgb, Multiply(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 2:{color = lerp(backbuffer.rgb, Screen(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
                case 3:{color = lerp(backbuffer.rgb, Overlay(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 4:{color = lerp(backbuffer.rgb, Darken(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 5:{color = lerp(backbuffer.rgb, Lighten(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 6:{color = lerp(backbuffer.rgb, ColorDodge(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 7:{color = lerp(backbuffer.rgb, ColorBurn(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 8:{color = lerp(backbuffer.rgb, HardLight(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 9:{color = lerp(backbuffer.rgb, SoftLight(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 10:{color = lerp(backbuffer.rgb, Difference(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 11:{color = lerp(backbuffer.rgb, Exclusion(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 12:{color = lerp(backbuffer.rgb, Hue(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 13:{color = lerp(backbuffer.rgb, Saturation(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 14:{color = lerp(backbuffer.rgb, ColorM(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    	case 15:{color = lerp(backbuffer.rgb, Luminosity(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 16:{color = lerp(backbuffer.rgb, Linearburn(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 17:{color = lerp(backbuffer.rgb, Lineardodge(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 18:{color = lerp(backbuffer.rgb, Vividlight(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 19:{color = lerp(backbuffer.rgb, Linearlight(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 20:{color = lerp(backbuffer.rgb, Pinlight(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 21:{color = lerp(backbuffer.rgb, Hardmix(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 22:{color = lerp(backbuffer.rgb, Reflect(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
				case 23:{color = lerp(backbuffer.rgb, Glow(backbuffer.rgb, precolor.rgb), layer.a*Stage_Opacity);break;}
		    }
        }
		else {
			if (BlackBackground) color= float3(0,0,0);
		}
	}
	//color.a = backbuffer.a;
    //return color;
}

/*float3 Freezef(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target{
    //if (FlipH) {texcoord.x = 1-texcoord.x;}//horizontal flip
    //if (FlipV) {texcoord.y = 1-texcoord.y;} //vertical flip
    float2 framecoord=texcoord;
    const float2 overlayPos = Layer_Pos * (1.0 - Layer_Scale) * BUFFER_SCREEN_SIZE;
    //const float2 overlayPos = rotate(Layer_Pos * (1.0 - Layer_Scale) * BUFFER_SCREEN_SIZE,float2(0.5,0.5), radians(Axis));

    if(all(vpos.xy >= overlayPos) && all(vpos.xy < overlayPos + BUFFER_SCREEN_SIZE * Layer_Scale))
    {
        framecoord = frac((framecoord - overlayPos / BUFFER_SCREEN_SIZE) / Layer_Scale);
    }
    framecoord=rotate(framecoord,Layer_Pos,radians(Axis));
    float3 fragment = tex2D(ReShade::BackBuffer, texcoord).rgb;
    return tex2D(FreezeSamplernew, framecoord).rgb;
}*/

technique FreezeShot
{
	pass Grab{
		VertexShader = PostProcessVS;
		PixelShader = GrabFrame;
		RenderTarget = FreezeTexold;
	}
    pass Save{
		VertexShader = PostProcessVS;
		PixelShader = SaveFrame;
		RenderTarget = FreezeTexnew;
	}
	pass GrabDepth{
		VertexShader = PostProcessVS;
		PixelShader = GrabDepth;
		RenderTarget = DepthTexold;
	}
    pass SaveDepth{
		VertexShader = PostProcessVS;
		PixelShader = SaveDepth;
		RenderTarget = DepthTexnew;
	}

    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = Freezef;
    }
}