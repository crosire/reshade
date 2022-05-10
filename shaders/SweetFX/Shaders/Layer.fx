/*------------------.
| :: Description :: |
'-------------------/

    Layer (version 0.2)

    Author: CeeJay.dk
    License: MIT

    About:
    Blends an image with the game.
    The idea is to give users with graphics skills the ability to create effects using a layer just like in an image editor.
    Maybe they could use this to create custom CRT effects, custom vignettes, logos, custom hud elements, toggable help screens and crafting tables or something I haven't thought of.

    Ideas for future improvement:
    * More blend modes
    * Tiling control
    * A default Layer texture with something useful in it

    History:
    (*) Feature (+) Improvement (x) Bugfix (-) Information (!) Compatibility

    Version 0.2 by seri14 & Marot Satil
    * Added the ability to scale and move the layer around on XY axis
*/

#include "ReShade.fxh"

#ifndef LAYER_SOURCE
#define LAYER_SOURCE "Layer.png"
#endif
#ifndef LAYER_SIZE_X
#define LAYER_SIZE_X 1280
#endif
#ifndef LAYER_SIZE_Y
#define LAYER_SIZE_Y 720
#endif

#if LAYER_SINGLECHANNEL
    #define TEXFORMAT R8
#else
    #define TEXFORMAT RGBA8
#endif

#include "ReShadeUI.fxh"

uniform float2 Layer_Pos < __UNIFORM_DRAG_FLOAT2
    ui_label = "Layer Position";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = (1.0 / 200.0);
> = float2(0.5, 0.5);

uniform float Layer_Scale < __UNIFORM_DRAG_FLOAT1
    ui_label = "Layer Scale";
    ui_min = (1.0 / 100.0); ui_max = 4.0;
    ui_step = (1.0 / 250.0);
> = 1.0;

uniform float Layer_Blend < __UNIFORM_COLOR_FLOAT1
    ui_label = "Layer Blend";
    ui_tooltip = "How much to blend layer with the original image.";
    ui_min = 0.0; ui_max = 1.0;
    ui_step = (1.0 / 255.0); // for slider and drag
> = 1.0;

texture Layer_Tex <
    source = LAYER_SOURCE;
> {
    Format = TEXFORMAT;
    Width  = LAYER_SIZE_X;
    Height = LAYER_SIZE_Y;
};

sampler Layer_Sampler
{
    Texture  = Layer_Tex;
    AddressU = BORDER;
    AddressV = BORDER;
};

void PS_Layer(float4 pos : SV_Position, float2 texCoord : TEXCOORD, out float4 passColor : SV_Target)
{
    const float4 backColor = tex2D(ReShade::BackBuffer, texCoord);
    const float2 pixelSize = 1.0 / (float2(LAYER_SIZE_X, LAYER_SIZE_Y) * Layer_Scale / BUFFER_SCREEN_SIZE);
    const float4 layer     = tex2D(Layer_Sampler, texCoord * pixelSize + Layer_Pos * (1.0 - pixelSize));

    passColor   = lerp(backColor, layer, layer.a * Layer_Blend);
    passColor.a = backColor.a;
}

technique Layer
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader  = PS_Layer;
    }
}
