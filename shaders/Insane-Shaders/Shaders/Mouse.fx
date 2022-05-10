uniform float2 MousePoint < source = "mousepoint"; >;

uniform int MouseRadius
<
	ui_type = "slider";
	ui_label = "Cursor Size";
	ui_min = 2; ui_max = 64;
> = 16;

uniform float MouseOpacity
<
	ui_type = "slider";
	ui_label = "Cursor Opacity";
	ui_min = 0; ui_max = 1;
> = 1;

uniform int2 MouseOffset
<
	ui_type = "slider";
	ui_label = "Cursor Offset";
	ui_min = -128; ui_max = 128;
> = int2(0, 0);

uniform float3 MouseColor
<
	ui_type = "color";
	ui_label = "Cursor Color";
> = float3(1, 1, 1);


texture BackBuffer : COLOR;
sampler sBackBuffer {Texture = BackBuffer;};

void MouseVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 1) ? 0.75 :
				 (id == 2) ? 0.3 : 
				 (id == 3) ? 0.75 : 0.0;
	texcoord.y = (id == 1) ? 0.3 :
				 (id == 2) ? 0.75 :
				 (id == 3) ? 0.75 : 0.0;
	float2 cursorCorner = (MousePoint + MouseOffset) * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	float2 boxSize = MouseRadius * float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT);
	cursorCorner += boxSize;
	boxSize *= 2;
	cursorCorner = cursorCorner * float2(2, -2) + float2(-1, 1);

	position.yz = float2(0.0, 1.0);
	position.xy += cursorCorner;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	position.xy = position.xy * boxSize + cursorCorner;
	texcoord = (position.xy * float2(0.5, -0.5)) + float2(0.5, 0.5);
}

void MousePS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 mouseColor : SV_Target0)
{
	mouseColor = tex2D(sBackBuffer, texcoord).rgb;
	mouseColor = lerp(mouseColor, MouseColor, MouseOpacity);
}

technique MouseReplacement <ui_tooltip = "Simple mouse tool for times when the ingame cursor is broken or otherwise unusable.\n\n"
										 "Part of Insane Shaders\n"
										 "By: Lord of Lunacy";>
{

	pass
	{
		VertexShader = MouseVS;
		PixelShader = MousePS;
		
		PrimitiveTopology = TRIANGLESTRIP;
		VertexCount = 4;
	}
}