/*******************************************************
	ReShade Header: Tools
	https://github.com/Daodan317081/reshade-shaders
*******************************************************/

#include "ReShade.fxh"

#define BLACK	float3(0.0, 0.0, 0.0)
#define WHITE	float3(1.0, 1.0, 1.0)
#define GREY50  float3(0.5, 0.5, 0.5)
#define RED 	float3(1.0, 0.0, 0.0)
#define ORANGE 	float3(1.0, 0.5, 0.0)
#define GREEN 	float3(0.0, 1.0, 0.0)
#define BLUE 	float3(0.0, 0.0, 1.0)
#define CYAN 	float3(0.0, 1.0, 1.0)
#define MAGENTA float3(1.0, 0.0, 1.0)
#define YELLOW 	float3(1.0, 1.0, 0.0)

#define LumaCoeff float3(0.2126, 0.7151, 0.0721)
#define YIQ_I_RANGE float2(-0.5957, 0.5957)
#define YIQ_Q_RANGE float2(-0.5226, 0.5226)
#define FLOAT_RANGE float2(0.0, 1.0)

#define MAX2(v) max(v.x, v.y)
#define MIN2(v) min(v.x, v.y)
#define MAX3(v) max(v.x, max(v.y, v.z))
#define MIN3(v) min(v.x, min(v.y, v.z))
#define MAX4(v) max(v.x, max(v.y, max(v.z, v.w)))
#define MIN4(v) min(v.x, min(v.y, min(v.z, v.w)))

/*
uniform int iUILayerMode <
	ui_type = "combo";
	ui_label = "Layer Mode";
	ui_items = "LAYER_MODE_NORMAL\0LAYER_MODE_MULTIPLY\0LAYER_MODE_DIVIDE\0LAYER_MODE_SCREEN\0LAYER_MODE_OVERLAY\0LAYER_MODE_DODGE\0LAYER_MODE_BURN\0LAYER_MODE_HARDLIGHT\0LAYER_MODE_SOFTLIGHT\0LAYER_MODE_GRAINEXTRACT\0LAYER_MODE_GRAINMERGE\0LAYER_MODE_DIFFERENCE\0LAYER_MODE_ADDITION\0LAYER_MODE_SUBTRACT\0LAYER_MODE_DARKENONLY\0LAYER_MODE_LIGHTENONLY\0LAYER_MODE_VIVIDLIGHT\0";
> = 0;
*/
#define LAYER_MODE_NORMAL			0
#define LAYER_MODE_MULTIPLY		    1
#define LAYER_MODE_DIVIDE			2
#define LAYER_MODE_SCREEN			3
#define LAYER_MODE_OVERLAY		    4
#define LAYER_MODE_DODGE			5
#define LAYER_MODE_BURN			    6
#define LAYER_MODE_HARDLIGHT		7
#define LAYER_MODE_SOFTLIGHT		8
#define LAYER_MODE_GRAINEXTRACT 	9
#define LAYER_MODE_GRAINMERGE		10
#define LAYER_MODE_DIFFERENCE		11
#define LAYER_MODE_ADDITION		    12
#define LAYER_MODE_SUBTRACT		    13
#define LAYER_MODE_DARKENONLY		14
#define LAYER_MODE_LIGHTENONLY	    15
#define LAYER_MODE_VIVIDLIGHT		16

/*
uniform int iUIEdgeType <
	ui_type = "combo";
	ui_label = "Edge detection kernel";
	ui_items = "CONV_PREWITT\0CONV_PREWITT_FULL\0CONV_SOBEL\0CONV_SOBEL_FULL\0CONV_SCHARR\0CONV_SCHARR_FULL\0";
> = 0;
*/
#define CONV_PREWITT 0
#define CONV_PREWITT_FULL 1
#define CONV_SOBEL 2
#define CONV_SOBEL_FULL 3
#define CONV_SCHARR 4
#define CONV_SCHARR_FULL 5

/*
uniform int iUIEdgeMergeMethod <
	ui_type = "combo";
	ui_label = "Edge merge method";
	ui_items = "CONV_MUL\0CONV_DOT\0CONV_X\0CONV_Y\0CONV_ADD\0CONV_MAX\0";
> = 0;
*/
#define CONV_MUL 0
#define CONV_DOT 1
#define CONV_X	 2
#define CONV_Y   3
#define CONV_ADD 4
#define CONV_MAX 5

struct sctpoint {
    float3 color;
    float2 coord;
};


float3 ConvReturn(float3 X, float3 Y, int MulDotXYAddMax) {
        float3 ret = float3(1.0, 0.0, 1.0);

        if(MulDotXYAddMax == CONV_MUL)
            ret = X * Y;
        else if(MulDotXYAddMax == CONV_DOT)
            ret = dot(X,Y);
        else if(MulDotXYAddMax == CONV_X)
            ret = X;
        else if(MulDotXYAddMax == CONV_Y)
            ret = Y;
        else if(MulDotXYAddMax == CONV_ADD)
            ret = X + Y;
        else if(MulDotXYAddMax == CONV_MAX)
            ret = max(X, Y);
        return ret;
}


namespace Tools {

    namespace Types {
        
        sctpoint Point(float3 color, float2 coord) {
            sctpoint p;
            p.color = color;
            p.coord = coord;
            return p;
        }

    }

    namespace Color {

        float3 RGBtoYIQ(float3 color) {
            static const float3x3 YIQ = float3x3( 	0.299, 0.587, 0.144,
                                                    0.596, -0.274, -0.322,
                                                    0.211, -0.523, 0.312  );
            return mul(YIQ, color);
        }

        float3 YIQtoRGB(float3 yiq) {
            static const float3x3 RGB = float3x3( 	1.0, 0.956, 0.621,
                                                    1.0, -0.272, -0.647,
                                                    1.0, -1.106, 1.703  );
            return saturate(mul(RGB, yiq));
        }

        float4 RGBtoCMYK(float3 color) {
            float3 CMY;
            float K;
            K = 1.0 - max(color.r, max(color.g, color.b));
            CMY = (1.0 - color - K) / (1.0 - K);
            return float4(CMY, K);
        }

        float3 CMYKtoRGB(float4 cmyk) {
            return (1.0.xxx - cmyk.xyz) * (1.0 - cmyk.w);
        }

        //These RGB/HSV conversion functions are based on the blogpost from:
        //http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
        float3 RGBtoHSV(float3 c) {
            float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
            float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);

            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }

        float3 HSVtoRGB(float3 c) {
            float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
        }

        float GetSaturation(float3 color) {
            float maxVal = max(color.r, max(color.g, color.b));
            float minVal = min(color.r, min(color.g, color.b));         
            return maxVal - minVal;
        }

        //https://docs.gimp.org/en/gimp-concepts-layer-modes.html
        //https://en.wikipedia.org/wiki/Blend_modes
        float3 LayerMerge(float3 mask, float3 image, int mode) {
            float3 E = float3(1.0, 0.0, 1.0);
        
            if(mode == LAYER_MODE_NORMAL)
                E = mask;
            else if(mode == LAYER_MODE_MULTIPLY)
                E = image * mask;
            else if(mode == LAYER_MODE_DIVIDE)
                E = image / (mask + 0.00001);
            else if(mode == LAYER_MODE_SCREEN || mode == LAYER_MODE_SOFTLIGHT) {
                E = 1.0 - (1.0 - image) * (1.0 - mask);
                if(mode == LAYER_MODE_SOFTLIGHT)
                    E = image * ((1.0 - image) * mask + E);
            }
            else if(mode == LAYER_MODE_OVERLAY)
                E = lerp(2*image*mask, 1.0 - 2.0 * (1.0 - image) * (1.0 - mask), max(image.r, max(image.g, image.b)) < 0.5 ? 0.0 : 1.0 );
            else if(mode == LAYER_MODE_DODGE)	
                E =  image / (1.00001 - mask);
            else if(mode == LAYER_MODE_BURN)
                E = 1.0 - (1.0 - image) / (mask + 0.00001);
            else if(mode == LAYER_MODE_HARDLIGHT)
                E = lerp(
                            2*image*mask,
                            1.0 - 2.0 * (1.0 - image) * (1.0 - mask),
                            max(image.r, max(image.g, image.b)) > 0.5 ? 0.0 : 1.0
                        );
            else if(mode == LAYER_MODE_GRAINEXTRACT)
                E = image - mask + 0.5;
            else if(mode == LAYER_MODE_GRAINMERGE)
                E = image + mask - 0.5;
            else if(mode == LAYER_MODE_DIFFERENCE)
                E = abs(image - mask);
            else if(mode == LAYER_MODE_ADDITION)
                E = image + mask;
            else if(mode == LAYER_MODE_SUBTRACT)
                E = image - mask;
            else if(mode == LAYER_MODE_DARKENONLY)
                E = min(image, mask);
            else if(mode == LAYER_MODE_LIGHTENONLY)
                E = max(image, mask);
            else if(mode == LAYER_MODE_VIVIDLIGHT)
                E = lerp(
                            max(1.0 - ((1.0 - image) / ((2.0 * mask) + 1e-9)), 0.0),
                            min(image / (2 * (1.0 - mask) + 1e-9), 1.0),
                            max(mask.r, max(mask.g, mask.b)) <= 0.5 ? 0.0 : 1.0
                        );

            return saturate(E);
        }

    }

    namespace Convolution {

        float3 ThreeByThree(sampler s, int2 vpos, float kernel[9], float divisor) {
            float3 acc;

            [unroll]
            for(int m = 0; m < 3; m++) {
                [unroll]
                for(int n = 0; n < 3; n++) {
                    acc += kernel[n + (m*3)] * tex2Dfetch(s, int4( (vpos.x - 1 + n), (vpos.y - 1 + m), 0, 0)).rgb;
                }
            }

            return acc / divisor;
        }

        float3 ConvReturn(float3 X, float3 Y, int MulDotXYAddMax) {
            float3 ret = float3(1.0, 0.0, 1.0);

            if(MulDotXYAddMax == CONV_MUL)
                ret = X * Y;
            else if(MulDotXYAddMax == CONV_DOT)
                ret = dot(X,Y);
            else if(MulDotXYAddMax == CONV_X)
                ret = X;
            else if(MulDotXYAddMax == CONV_Y)
                ret = Y;
            else if(MulDotXYAddMax == CONV_ADD)
                ret = X + Y;
            else if(MulDotXYAddMax == CONV_MAX)
                ret = max(X, Y);
            return saturate(ret);
        }

        float3 Edges(sampler s, int2 vpos, int kernel, int type) {
            static const float Prewitt_X[9] = { -1.0,  0.0, 1.0,
                                                -1.0,  0.0, 1.0,
                                                -1.0,  0.0, 1.0	 };

            static const float Prewitt_Y[9] = { 1.0,  1.0,  1.0,
                                                0.0,  0.0,  0.0,
                                                -1.0, -1.0, -1.0  };

            static const float Prewitt_X_M[9] = { 1.0,  0.0, -1.0,
                                                1.0,  0.0, -1.0,
                                                1.0,  0.0, -1.0	 };

            static const float Prewitt_Y_M[9] = { -1.0,  -1.0,  -1.0,
                                                0.0,  0.0,  0.0,
                                                1.0, 1.0, 1.0  };

            static const float Sobel_X[9] = { 	1.0,  0.0, -1.0,
                                                2.0,  0.0, -2.0,
                                                1.0,  0.0, -1.0	 };

            static const float Sobel_Y[9] = { 	1.0,  2.0,  1.0,
                                                0.0,  0.0,  0.0,
                                            -1.0, -2.0, -1.0	 };

            static const float Sobel_X_M[9] = { 	-1.0,  0.0, 1.0,
                                                    -2.0,  0.0, 2.0,
                                                    -1.0,  0.0, 1.0	 };

            static const float Sobel_Y_M[9] = {   -1.0, -2.0, -1.0,
                                                0.0,  0.0,  0.0,
                                                1.0,  2.0,  1.0	 };

            static const float Scharr_X[9] = { 	 3.0,  0.0,  -3.0,
                                                10.0,  0.0, -10.0,
                                                3.0,  0.0,  -3.0  };

            static const float Scharr_Y[9] = { 	3.0,  10.0,   3.0,
                                                0.0,   0.0,   0.0,
                                                -3.0, -10.0,  -3.0  };

            static const float Scharr_X_M[9] = { 	 -3.0,  0.0,  3.0,
                                                -10.0,  0.0, 10.0,
                                                -3.0,  0.0,  3.0  };

            static const float Scharr_Y_M[9] = { 	-3.0,  -10.0,   -3.0,
                                                0.0,   0.0,   0.0,
                                                3.0, 10.0,  3.0  };
            
            float3 retValX, retValXM;
            float3 retValY, retValYM;

            if(kernel == CONV_PREWITT) {
                retValX = Convolution::ThreeByThree(s, vpos, Prewitt_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Prewitt_Y, 1.0);
            }
            if(kernel == CONV_PREWITT_FULL) {
                retValX = Convolution::ThreeByThree(s, vpos, Prewitt_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Prewitt_Y, 1.0);
                retValXM = Convolution::ThreeByThree(s, vpos, Prewitt_X_M, 1.0);
                retValYM = Convolution::ThreeByThree(s, vpos, Prewitt_Y_M, 1.0);
                retValX = max(retValX, retValXM);
                retValY = max(retValY, retValYM);
            }
            if(kernel == CONV_SOBEL) {
                retValX = Convolution::ThreeByThree(s, vpos, Sobel_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Sobel_Y, 1.0);
            }
            if(kernel == CONV_SOBEL_FULL) {
                retValX = Convolution::ThreeByThree(s, vpos, Sobel_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Sobel_Y, 1.0);
                retValXM = Convolution::ThreeByThree(s, vpos, Sobel_X_M, 1.0);
                retValYM = Convolution::ThreeByThree(s, vpos, Sobel_Y_M, 1.0);
                retValX = max(retValX, retValXM);
                retValY = max(retValY, retValYM);
            }
            if(kernel == CONV_SCHARR) {
                retValX = Convolution::ThreeByThree(s, vpos, Scharr_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Scharr_Y, 1.0);
            }
            if(kernel == CONV_SCHARR_FULL) {
                retValX = Convolution::ThreeByThree(s, vpos, Scharr_X, 1.0);
                retValY = Convolution::ThreeByThree(s, vpos, Scharr_Y, 1.0);
                retValXM = Convolution::ThreeByThree(s, vpos, Scharr_X_M, 1.0);
                retValYM = Convolution::ThreeByThree(s, vpos, Scharr_Y_M, 1.0);
                retValX = max(retValX, retValXM);
                retValY = max(retValY, retValYM);
            }

            return Convolution::ConvReturn(retValX, retValY, type);
        }

        float3 SimpleBlur(sampler s, int2 vpos, int size) {
            float3 acc;

            size = clamp(size, 3, 14);
            [unroll]
            for(int m = 0; m < size; m++) {
                [unroll]
                for(int n = 0; n < size; n++) {
                    acc += tex2Dfetch(s, int4( (vpos.x - size / 3 + n), (vpos.y - size / 3 + m), 0, 0)).rgb;
                }
            }

            return acc / (size * size);
        }
    }

    namespace Draw {

        float aastep(float threshold, float value)
        {
            float afwidth = length(float2(ddx(value), ddy(value)));
            return smoothstep(threshold - afwidth, threshold + afwidth, value);
        }

        float3 Point2(float3 texcolor, float3 pointcolor, float2 pointcoord, float2 texcoord, float power) {
            return lerp(texcolor, pointcolor, saturate(exp(-power * length(texcoord - pointcoord))));
        }
        
        float3 PointEXP(float3 texcolor, sctpoint p, float2 texcoord, float power) {
            return lerp(texcolor, p.color, saturate(exp(-power * length(texcoord - p.coord))));
        }

       float3 PointAASTEP(float3 texcolor, sctpoint p, float2 texcoord, float power) {
            return lerp(p.color, texcolor, aastep(power, length(texcoord - p.coord)));
        }

        float3 OverlaySampler(float3 image, sampler overlay, float scale, float2 texcoord, int2 offset, float opacity) {
            float3 retVal;
            float3 col = image;
            float fac = 0.0;

            float2 coord_pix = float2(BUFFER_WIDTH, BUFFER_HEIGHT) * texcoord;
            float2 overlay_size = (float2)tex2Dsize(overlay, 0) * scale;
            float2 border_min = (float2)offset;
            float2 border_max = border_min + overlay_size + 1;

            if( coord_pix.x <= border_max.x &&
                coord_pix.y <= border_max.y &&
                coord_pix.x >= border_min.x &&
                coord_pix.y >= border_min.y   ) {
                    fac = opacity;
                    float2 coord_overlay = (coord_pix - border_min) / overlay_size;
                    col = tex2D(overlay, coord_overlay).rgb;
                }

            return lerp(image, col, fac);
        }

    }

    namespace Functions {
        
        float Map(float value, float2 span_old, float2 span_new) {
            float span_old_diff = abs(span_old.y - span_old.x) < 1e-6 ? 1e-6 : span_old.y - span_old.x;
            return lerp(span_new.x, span_new.y, (clamp(value, span_old.x, span_old.y)-span_old.x)/(span_old_diff));
        }

        float Level(float value, float black, float white) {
            value = clamp(value, black, white);
            return Map(value, float2(black, white), FLOAT_RANGE);
        }

        float Posterize(float x, int numLevels, float continuity, float slope, int type) {
            float stepheight = 1.0 / numLevels;
            float stepnum = floor(x * numLevels);
            float frc = frac(x * numLevels);
            float step1 = floor(frc) * stepheight;
            float step2;

            if(type == 1)
                step2 = smoothstep(0.0, 1.0, frc) * stepheight;
            else if(type == 2)
                step2 = (1.0 / (1.0 + exp(-slope*(frc - 0.5)))) * stepheight;
            else
                step2 = frc * stepheight;

            return lerp(step1, step2, continuity) + stepheight * stepnum;
        }

        float DiffEdges(sampler s, float2 texcoord)
        {
            float valC = dot(tex2D(s, texcoord).rgb, LumaCoeff);
            float valN = dot(tex2D(s, texcoord + float2(0.0, -ReShade::PixelSize.y)).rgb, LumaCoeff);
            float valNE = dot(tex2D(s, texcoord + float2(ReShade::PixelSize.x, -ReShade::PixelSize.y)).rgb, LumaCoeff);
            float valE = dot(tex2D(s, texcoord + float2(ReShade::PixelSize.x, 0.0)).rgb, LumaCoeff);
            float valSE = dot(tex2D(s, texcoord + float2(ReShade::PixelSize.x, ReShade::PixelSize.y)).rgb, LumaCoeff);
            float valS = dot(tex2D(s, texcoord + float2(0.0, ReShade::PixelSize.y)).rgb, LumaCoeff);
            float valSW = dot(tex2D(s, texcoord + float2(-ReShade::PixelSize.x, ReShade::PixelSize.y)).rgb, LumaCoeff);
            float valW = dot(tex2D(s, texcoord + float2(-ReShade::PixelSize.x, 0.0)).rgb, LumaCoeff);
            float valNW = dot(tex2D(s, texcoord + float2(-ReShade::PixelSize.x, -ReShade::PixelSize.y)).rgb, LumaCoeff);

            float diffNS = abs(valN - valS);
            float diffWE = abs(valW - valE);
            float diffNWSE = abs(valNW - valSE);
            float diffSWNE = abs(valSW - valNE);
            return saturate((diffNS + diffWE + diffNWSE + diffSWNE) * (1.0 - valC));
        }

        float GetDepthBufferOutlines(float2 texcoord, int fading)
        {
            float depthC =  ReShade::GetLinearizedDepth(texcoord);
            float depthN =  ReShade::GetLinearizedDepth(texcoord + float2(0.0, -ReShade::PixelSize.y));
            float depthNE = ReShade::GetLinearizedDepth(texcoord + float2(ReShade::PixelSize.x, -ReShade::PixelSize.y));
            float depthE =  ReShade::GetLinearizedDepth(texcoord + float2(ReShade::PixelSize.x, 0.0));
            float depthSE = ReShade::GetLinearizedDepth(texcoord + float2(ReShade::PixelSize.x, ReShade::PixelSize.y));
            float depthS =  ReShade::GetLinearizedDepth(texcoord + float2(0.0, ReShade::PixelSize.y));
            float depthSW = ReShade::GetLinearizedDepth(texcoord + float2(-ReShade::PixelSize.x, ReShade::PixelSize.y));
            float depthW =  ReShade::GetLinearizedDepth(texcoord + float2(-ReShade::PixelSize.x, 0.0));
            float depthNW = ReShade::GetLinearizedDepth(texcoord + float2(-ReShade::PixelSize.x, -ReShade::PixelSize.y));
            float diffNS = abs(depthN - depthS);
            float diffWE = abs(depthW - depthE);
            float diffNWSE = abs(depthNW - depthSE);
            float diffSWNE = abs(depthSW - depthNE);
            float outline = (diffNS + diffWE + diffNWSE + diffSWNE);

            if(fading == 1)
                outline *= (1.0 - depthC);
            else if(fading == 2)
                outline *= depthC;

            return outline;
        }
    }
}