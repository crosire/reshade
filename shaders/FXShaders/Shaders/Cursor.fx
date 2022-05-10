#include "FXShaders/Common.fxh"

#ifndef CURSOR_TEXTURE_NAME
#define CURSOR_TEXTURE_NAME "Cursor.png"
#endif

#ifndef CURSOR_TEXTURE_WIDTH
#define CURSOR_TEXTURE_WIDTH 12
#endif

#ifndef CURSOR_TEXTURE_HEIGHT
#define CURSOR_TEXTURE_HEIGHT 19
#endif

namespace FXShaders
{

static const float2 CursorSize = float2(
    CURSOR_TEXTURE_WIDTH,
    CURSOR_TEXTURE_HEIGHT);

uniform float2 MousePoint <source = "mousepoint";>;

texture BackBufferTex : COLOR;

sampler BackBuffer
{
	Texture = BackBufferTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
};

texture CursorTex <source = CURSOR_TEXTURE_NAME;>
{
    Width = CursorSize.x;
    Height = CursorSize.y;
};

sampler Cursor
{
    Texture = CursorTex;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = POINT;
    AddressU = BORDER;
    AddressV = BORDER;
};

float4 MainPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 color = tex2D(BackBuffer, uv);

    float2 cursorUv = (uv * GetResolution() - MousePoint) / CursorSize;
    float4 cursor = tex2D(Cursor, cursorUv);

	color.rgb = lerp(color.rgb, cursor.rgb, cursor.a);

    return color;
}

technique Cursor
{
	pass
	{
		VertexShader = ScreenVS;
		PixelShader = MainPS;
	}
}

} // Namespace.
