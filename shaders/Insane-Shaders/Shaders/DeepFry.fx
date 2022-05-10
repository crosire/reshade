/*
A simple shader that uses JPEG DCT quantization and turns up the reds in the image to create a deep fried meme effect.
*/

#undef IMAGE_WIDTH
#undef IMAGE_HEIGHT
#define IMAGE_WIDTH int((BUFFER_WIDTH + 7) / 8) * 8
#define IMAGE_HEIGHT int((BUFFER_HEIGHT + 7) / 8) * 8

/*
The DCTCoefficents are precalculated using the following equation:
	DCTCoefficents[i] = cos(((2 * int(i % 8)) * int(i / 8) * PI)/16)
*/
static const float DCTCoefficents[64] = {1, 1, 1, 1, 1, 1, 1, 1, 
										0.980785, 0.83147, 0.55557, 0.19509, -0.19509, -0.55557, -0.83147, -0.980785, 
										0.92388, 0.382683, -0.382683, -0.92388, -0.92388, -0.382683, 0.382683, 0.92388, 
										0.83147, -0.19509, -0.980785, -0.55557, 0.55557, 0.980785, 0.19509, -0.83147, 
										0.707107, -0.707107, -0.707107, 0.707107, 0.707107, -0.707107, -0.707107, 0.707107, 
										0.55557, -0.980785, 0.19509, 0.83147, -0.83147, -0.19509, 0.980785, -0.55557, 
										0.382683, -0.92388, 0.92388, -0.382683, -0.382683, 0.92388, -0.92388, 0.382683, 
										0.19509, -0.55557, 0.83147, -0.980785, 0.980785, -0.83147, 0.55557, -0.19509};
										
static const float LuminanceQuantization[64] = {16, 11, 10, 16, 24, 40, 51, 61,
												12, 12, 14, 19, 26, 58, 60, 55, 
												14, 13, 16, 24, 40, 57, 69, 56, 
												14, 17, 22, 29, 51, 87, 80, 62, 
												18, 22, 37, 56, 68, 109, 103, 77, 
												24, 36, 55, 64, 81, 104, 113, 92,
												49, 64, 78, 87, 103, 121, 120, 101, 
												72, 92, 95, 98, 112, 100, 103, 99};
												
static const float ChrominanceQuantization[64] = {17, 18, 24, 47, 99, 99, 99, 99, 
												  18, 21, 26, 66, 99, 99, 99, 99, 
												  24, 26, 56, 99, 99, 99, 99, 99,
												  47, 66, 99, 99, 99, 99, 99, 99, 
												  99, 99, 99, 99, 99, 99, 99, 99,
												  99, 99, 99, 99, 99, 99, 99, 99,
												  99, 99, 99, 99, 99, 99, 99, 99,
												  99, 99, 99, 99, 99, 99, 99, 99};
												  
												  
uniform float Quality<
	ui_type = "slider";
	ui_min = 0.5; ui_max = 10;
> = 6;

uniform float Reds
<
	ui_type = "slider";
	ui_min = 0; ui_max = 1;
> = 0.2;
										
texture BackBuffer : COLOR;
sampler sBackBuffer {Texture = BackBuffer;};

texture Image0 {Width = IMAGE_WIDTH; Height = IMAGE_HEIGHT; Format = RGBA16f;};
texture Image1 {Width = IMAGE_WIDTH; Height = IMAGE_HEIGHT; Format = RGBA16f;};

sampler sImage0 {Texture = Image0;};
sampler sImage1 {Texture = Image1;};


// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float3 DCT_Horizontal(sampler sTexture, float2 texcoord, bool isInverse)
{
	uint2 coord = uint2(texcoord * float2(IMAGE_WIDTH, IMAGE_HEIGHT));
	uint index = coord.x % 8;
	uint offset = coord.x - index;
	
	float3 output = 0;
	
	for(uint i = 0; i < 8; i++)
	{
		uint xCoord = i;
		float weight;
		
		if(isInverse)
		{
			weight = DCTCoefficents[xCoord * 8 + index];
			weight *= (xCoord == 0) ? 0.707107 : 1;
		}
		else
		{
			weight = DCTCoefficents[index * 8 + xCoord];
		}
		
		xCoord += offset;
		float3 value = tex2Dfetch(sTexture, float2(xCoord, coord.y) + 0.5).xyz - 0.5; //The subtraction of 0.5 is needed to be consistent with JPEG
		value *= weight;
		output += value;
	}
	
	if(!isInverse)
	{
		output *= (index == 0) ? 0.707107 : 1;
	}
	
	output *= 0.5;
	return output + 0.5;
}

float3 DCT_Vertical(sampler sTexture, float2 texcoord, bool isInverse)
{
	uint2 coord = uint2(texcoord * float2(IMAGE_WIDTH, IMAGE_HEIGHT));
	uint index = coord.y % 8;
	uint offset = coord.y - index;
	
	float3 output = 0;
	
	for(uint i = 0; i < 8; i++)
	{
		uint yCoord = (i) % 8;
		float weight;
		
		if(isInverse)
		{
			weight = DCTCoefficents[yCoord * 8 + index];
			weight *= (yCoord == 0) ? 0.707107 : 1;
		}
		else
		{
			weight = DCTCoefficents[index * 8 + yCoord];
		}
		
		yCoord += offset;
		float3 value = tex2Dfetch(sTexture, float2(coord.x, yCoord) + 0.5).xyz - 0.5; //The subtraction of 0.5 is needed to be consistent with JPEG
		value *= weight;
		output += value;
	}
	
	if(!isInverse)
	{
		output *= (index == 0) ? 0.707107 : 1;
	}
	
	output *= 0.5;
	return output + 0.5;
}

void ycbcrPS(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 output : SV_Target0)
{
	float3 color = tex2D(sBackBuffer, texcoord).rgb;
	output.x = dot(color, float3(0.299, 0.587, 0.114));
	output.y = dot(color, float3(-0.168736, -0.331264, 0.5)) + 0.5 - Reds;
	output.z = dot(color, float3(0.5, -0.418688, -0.081312)) + 0.5 + Reds;
}

void HorizontalPS0(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 output : SV_Target0)
{
	output.xyz = DCT_Horizontal(sBackBuffer, texcoord, false);
	output.w = 1;
}

void VerticalPS0(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 output : SV_Target0)
{
	output.xyz = DCT_Vertical(sImage0, texcoord, false);
	output.w = 1;
	output *= 255;
	output += -128;
	uint2 coord = uint2(texcoord * float2(IMAGE_WIDTH, IMAGE_HEIGHT));
	uint index = (coord.y % 8) * 8 + coord.x % 8;
	float3 quantization = float3(LuminanceQuantization[index], ChrominanceQuantization[index].xx) * Quality;
	output.xyz = round(output.xyz * rcp(quantization));
	output *= quantization;
	output /= 255;
	output += 0.5;
}

void HorizontalPS1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 output : SV_Target0)
{
	output.xyz = DCT_Horizontal(sImage1, texcoord, true);
	output.w = 1;
}

void VerticalPS1(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 output : SV_Target0)
{
	float3 color = DCT_Vertical(sImage0, texcoord, true);
	color.yz -= 0.5;
	output.x = dot(color, float3(1, 0, 1.402));
	output.y = dot(color, float3(1, -0.344136, -0.714136));
	output.z = dot(color, float3(1, 1.772, 0));
	output.w = 1;
}

technique DeepFry <ui_tooltip = "A simple shader that uses JPEG DCT quantization, \n"
								"and turns up the reds in the image to create a \n"
								"deep fried meme effect.\n\n"
								"Part of Insane Shaders\n"
								"By: Lord of Lunacy";>
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = ycbcrPS;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = HorizontalPS0;
		
		RenderTarget = Image0;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = VerticalPS0;
		
		RenderTarget = Image1;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = HorizontalPS1;
		RenderTarget = Image0;
	}
	
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = VerticalPS1;
	}
}