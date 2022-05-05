//===================================================================================================================
//===================================================================================================================
#define PixelSize  	float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

uniform float Timer < source = "timer"; >;
uniform float Frametime < source = "frametime"; >;

texture2D 	texColor : COLOR;
sampler2D 	SamplerColor {Texture = texColor; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

//===================================================================================================================
//===================================================================================================================
// Vertex, no reason to have anything fancy here so far, just a big olde triangle

void VS_PostProcess(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	pos = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
//===================================================================================================================
//===================================================================================================================
// Global functions

float3 BlendScreen(float3 a, float3 b) {
	return 1 - ((1 - a) * (1 - b));
}
float3 BlendSoftLight(float3 a, float3 b) {
	return (1 - 2 * b) * pow(a, 2) + 2 * b * a;
}
float3 BlendColorDodge(float3 a, float3 b) {
	return a / (1 - b);
}
float3 BlendColorBurn(float3 a, float3 b) {
	return 1 - (1 - a) / b;
}