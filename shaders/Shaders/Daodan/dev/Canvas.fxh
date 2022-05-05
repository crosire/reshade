/*******************************************************
	ReShade Header: Canvas.fxh
	https://github.com/Daodan317081/reshade-shaders
*******************************************************/

#include "ReShade.fxh"

/*******************************************************
	Only used in this header
*******************************************************/
#define CANVAS_TEXTURE_NAME(ref) tex##ref
#define CANVAS_SAMPLER_NAME(ref) s##ref
#define CANVAS_DRAW_SHADER_NAME(ref) ref##draw
#define CANVAS_OVERLAY_SHADER_NAME(ref) ref##overlay

/*******************************************************
	Functions for drawing,
    not necessary to use them directly
*******************************************************/
namespace Canvas {
    float3 OverlaySampler(float3 image, sampler overlay, float2 texcoord, int2 offset, float opacity) {
        float3 retVal;
        float3 col = image;
        float fac = 0.0;

        float2 screencoord = float2(BUFFER_WIDTH, BUFFER_HEIGHT) * texcoord;
        float2 overlay_size = (float2)tex2Dsize(overlay, 0);
        offset.x = clamp(offset.x, 0, BUFFER_WIDTH - overlay_size.x);
        offset.y = clamp(offset.y, 0, BUFFER_HEIGHT - overlay_size.y);
        float2 border_min = (float2)offset;
        float2 border_max = border_min + overlay_size;

        if( screencoord.x <= border_max.x &&
            screencoord.y <= border_max.y &&
            screencoord.x >= border_min.x &&
            screencoord.y >= border_min.y   ) {
                fac = opacity;
                float2 coord_overlay = (screencoord - border_min) / overlay_size;
                col = tex2D(overlay, coord_overlay).rgb;
        }

        return lerp(image, col, fac);
    }
    // aastep()-function taken from page 3 of this tutorial:
    // http://webstaff.itn.liu.se/~stegu/webglshadertutorial/shadertutorial.html
    float aastep(float threshold, float value)
    {
        float afwidth = length(float2(ddx(value), ddy(value)));
        return smoothstep(threshold - afwidth, threshold + afwidth, value);
    }
    float3 DrawCurve(float3 texcolor, float3 pointcolor, float2 pointcoord, float2 texcoord, float threshold) {
        return lerp(pointcolor, texcolor, aastep(threshold, length(texcoord - pointcoord)));
    }
    float3 DrawBox(float3 texcolor, float3 color, int2 pos, int2 size, float2 texcoord, sampler s) {
        int2 texSize = tex2Dsize(s, 0);
        int2 pixelcoord = texcoord * texSize;
        //Clamp values
        size.y = clamp(size.y, 0, texSize.y);
        size.x = clamp(size.x, 0, texSize.x);
        pos.x = clamp(pos.x, 0, texSize.x - size.x);
        pos.y = clamp(pos.y, 0, texSize.y - size.y);

        if( pixelcoord.x >= pos.x &&
            pixelcoord.x <= pos.x + size.x &&
            pixelcoord.y <= pos.y + size.y &&
            pixelcoord.y >= pos.y ) {
                texcolor = color;
        }
        return texcolor;
    }
    float3 DrawScale(float3 texcolor, float3 color_begin, float3 color_end, int2 pos, int2 size, float value, float3 color_marker, float2 texcoord, sampler s, float threshold) {
        int2 texSize = tex2Dsize(s, 0);
        
        //Clamp values
        size.x = clamp(size.x, 0, texSize.x);
        size.y = clamp(size.y, 0, texSize.y);
        pos.x = clamp(pos.x, 0, texSize.x - size.x);
        pos.y = clamp(pos.y, 0, texSize.y - size.y);
        
        float2 scalePosFloat = (float2)pos / (float2)size;
        int2 pixelcoord = texcoord * texSize;
        float2 sizeFactor = (float2)size / (float2)texSize;

        if( pixelcoord.x >= pos.x &&
            pixelcoord.x <= pos.x + size.x &&
            pixelcoord.y <= pos.y + size.y &&
            pixelcoord.y >= pos.y ) {
            if(size.y >= size.x) {
                //Vertical scale
                texcolor = lerp(color_begin, color_end, texcoord.y / sizeFactor.y - scalePosFloat.y);
                texcolor = DrawCurve(texcolor, color_marker, float2(texcoord.x, (value + scalePosFloat.y) * sizeFactor.y), texcoord, threshold);
            }
            else {
                //Horizontal scale
                texcolor = lerp(color_begin, color_end, texcoord.x / sizeFactor.x - scalePosFloat.x);
                texcolor = DrawCurve(texcolor, color_marker, float2((value + scalePosFloat.x) * sizeFactor.x, texcoord.y), texcoord, threshold);
            }
        }
        return texcolor;
    }
}

/*******************************************************
	Setting up the canvas:
    - Add uniforms for position and opacity
    - Add texture and sampler for canvas
    - Add function to show the canvas
*******************************************************/
#define CANVAS_SETUP(ref, width, height) \
    uniform int2 ref##Position < \
        ui_type = "drag"; \
        ui_category = #ref; \
        ui_label = "Position"; \
        ui_min = 0; ui_max = BUFFER_WIDTH; \
        ui_step = 1; \
    > = int2(0, 0); \
    uniform float ref##Opacity < \
        ui_type = "drag"; \
        ui_category = #ref; \
        ui_label = "Opacity"; \
        ui_min = 0.0; ui_max = 1.0; \
        ui_step = 0.01; \
    > = 1.0; \
    texture2D CANVAS_TEXTURE_NAME(ref) { Width = width; Height = height; Format = RGBA8; }; \
    sampler2D CANVAS_SAMPLER_NAME(ref) { Texture = CANVAS_TEXTURE_NAME(ref); }; \
    float3 CANVAS_OVERLAY_SHADER_NAME(ref)(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target { \
        float3 backbuffer = tex2D(ReShade::BackBuffer, texcoord).rgb; \
        return Canvas::OverlaySampler(backbuffer, CANVAS_SAMPLER_NAME(ref), texcoord, ref##Position, ref##Opacity); \
    }  

/*******************************************************
	For drawing
*******************************************************/
#define CANVAS_DRAW_BEGIN(ref, background)                                              float3 CANVAS_DRAW_SHADER_NAME(ref)(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target { float3 ref = background;
#define CANVAS_DRAW_BACKGROUND(ref, background)                                         ref = background;
#define CANVAS_DRAW_CURVE_XY(ref, color, func)                                          ref = Canvas::DrawCurve(ref, color, float2(texcoord.x, func), float2(texcoord.x, /*flip coordinate*/ 1.0 - texcoord.y), 0.002);
#define CANVAS_DRAW_CURVE_YX(ref, color, func)                                          ref = Canvas::DrawCurve(ref, color, float2(func, texcoord.y), float2(texcoord.x, texcoord.y), 0.002);
#define CANVAS_DRAW_SCALE(ref, color_begin, color_end, pos, size, value, color_marker)  ref = Canvas::DrawScale(ref, color_begin, color_end, pos, size, value, color_marker, float2(texcoord.x, /*flip coordinate*/ 1.0 - texcoord.y), CANVAS_SAMPLER_NAME(ref), 0.002);
#define CANVAS_DRAW_BOX(ref, color, pos, size)                                          ref = Canvas::DrawBox(ref, color, pos, size, float2(texcoord.x, 1.0 - texcoord.y), CANVAS_SAMPLER_NAME(ref));
#define CANVAS_DRAW_END(ref)                                                            return ref; }

/*******************************************************
	Add technique to show canvas
*******************************************************/
#define CANVAS_TECHNIQUE(ref) \
    technique ref { \
        pass { \
            VertexShader = PostProcessVS; \
            PixelShader = CANVAS_DRAW_SHADER_NAME(ref); \
            RenderTarget0 = CANVAS_TEXTURE_NAME(ref); \
        } \
        pass { \
            VertexShader = PostProcessVS; \
            PixelShader = CANVAS_OVERLAY_SHADER_NAME(ref); \
        } \
    }

/*******************************************************
	Example Shader
*******************************************************/
/*
#include "ReShade.fxh"
#include "Canvas.fxh"

uniform float SineFreq<
    ui_type = "drag";
    ui_label = "Sine Frequency";
    ui_min = 0.0; ui_max = 20.0;
    ui_step = 0.001;
> = 2.0;

uniform float SineAmplitude<
    ui_type = "drag";
    ui_label = "Sine Amplitude";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.001;
> = 0.5;

uniform float2 Chirp<
    ui_type = "drag";
    ui_label = "Chirp";
    ui_min = 0.0; ui_max = 100.0;
    ui_step = 0.1;
> = float2(0.0, 2.0);

uniform float Value<
    ui_type = "drag";
    ui_label = "Scale Value";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = 0.001;
> = 0.25;

uniform int4 BoxPosSize<
    ui_type = "drag";
    ui_label = "Box Pos / Size";
    ui_min = 0; ui_max = BUFFER_WIDTH;
    ui_step = 1;
> = int4(20, 20, 300, 150);

//Set up canvas
CANVAS_SETUP(TestCanvas, BUFFER_WIDTH/2, BUFFER_HEIGHT/2)
//Start drawing
CANVAS_DRAW_BEGIN(TestCanvas, tex2D(ReShade::BackBuffer, texcoord).rgb)
    //Draw lines in the middle
    CANVAS_DRAW_CURVE_XY(TestCanvas, texcoord.x, 0.5)
    CANVAS_DRAW_CURVE_YX(TestCanvas, texcoord.y, 0.5)
    //Draw scales (whether the marker runs horizontal or vertical is based on the size of the scale (aspect ratio))
    CANVAS_DRAW_SCALE(TestCanvas, 0.7.rrr, 0.3.rrr, int2(0, 11), int2(10, BUFFER_HEIGHT/2 - 10), Value, float3(0.0, 1.0, 0.0))
    CANVAS_DRAW_SCALE(TestCanvas, 0.0.rrr, 1.0.rrr, int2(11, 0), int2(BUFFER_WIDTH/2 - 10, 10), Value, float3(1.0, 0.0, 1.0))
    //Draw a sine
    CANVAS_DRAW_CURVE_XY(TestCanvas, float3(1.0, 0.0, 0.0), SineAmplitude * sin(SineFreq * 2 * 3.14 * texcoord.x) * 0.5 + 0.5)
    //Draw Chirp
    float k = (Chirp.y - Chirp.x);
    CANVAS_DRAW_CURVE_XY(TestCanvas, float3(1.0, 1.0, 0.0), sin(2 * 3.14 * (Chirp.x * texcoord.x + (k / 2.0) * texcoord.x * texcoord.x)) * 0.5 + 0.5)
    //Draw a box
    CANVAS_DRAW_BOX(TestCanvas, float3(0.0, 1.0, 1.0), BoxPosSize.xy, BoxPosSize.zw)
//end
CANVAS_DRAW_END(TestCanvas)

//Pixelshader that does nothing
float3 CanvasPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target {
    return tex2D(ReShade::BackBuffer, texcoord).rgb;
}

technique Canvas {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = CanvasPS;
    }
}

//Add technique for showing the canvas
CANVAS_TECHNIQUE(TestCanvas)
*/