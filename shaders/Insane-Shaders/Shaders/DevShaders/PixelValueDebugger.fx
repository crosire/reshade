/**
	Pixel Value Debugger
	By: Lord Of Lunacy
	
	Outputs the pixel value at the position of your mouse for either the backbuffer, or a texture you define.
**/
uniform float2 mouse_point < source = "mousepoint"; >;
uniform float Scale<
	ui_type = "slider";
	ui_category = "Text Settings";
	ui_label = "Scale";
	ui_min = 0; ui_max = 1;
> = 0.25;

uniform float3 TextColor<
	ui_type = "color";
	ui_category = "Text Settings";
	ui_label = "Text Color";
> = 1;

uniform float Zoom<
	ui_type = "slider";
	ui_category = "Image Positioning";
	ui_label = "Zoom";
	ui_min = 1; ui_max = 16;
> = 1;

uniform float2 Center<
	ui_type = "slider";
	ui_category = "Image Positioning";
	ui_label = "Image Position";
	ui_min = -1; ui_max = 1;
> = 0;

uniform int TextureSelection<
	ui_type = "radio";
	ui_category = "Texture Selection";
	ui_items = "Backbuffer \0Custom Texture \0";
> = 0;
							
#if __RENDERER__ == 0x9000
	#define D3D9 1
#else
	#define D3D9 0
#endif

#define __I uint2(0x01F2, 0x109F)
#define __N uint2(0x011C, 0xD671)
#define __F uint2(0x01F8, 0x7E10)
#define __A uint2(0x00E8, 0xFE31)
#define __E uint2(0x01F8, 0x7E1F)
#define __0 uint2(0x01F8, 0xC63F)
#define __1 uint2(0x0042, 0x1084)
#define __2 uint2(0x01F0, 0xFE1F)
#define __3 uint2(0x01F0, 0xFC3F)
#define __4 uint2(0x0118, 0xFC21)
#define __5 uint2(0x01F8, 0x7C3F)
#define __6 uint2(0x01F8, 0x7E3F)
#define __7 uint2(0x01F0, 0x8421)
#define __8 uint2(0x00E8, 0xBA2E)
#define __9 uint2(0x01F8, 0xFC21)

#define __DOT   uint2(0x0000, 0x0004)
#define __PLUS	uint2(0x0002, 0x3880)
#define __MINUS uint2(0x0000, 0x3800)
#define __NULL  uint2(0x0000, 0x0000)

#ifndef CUSTOM_TEXTURE_NAME
	#define CUSTOM_TEXTURE_NAME example
#endif

#ifndef CUSTOM_TEXTURE_WIDTH
	#define CUSTOM_TEXTURE_WIDTH 1
#endif

#ifndef CUSTOM_TEXTURE_HEIGHT
	#define CUSTOM_TEXTURE_HEIGHT 1
#endif

#ifndef CUSTOM_TEXTURE_FORMAT
	#define CUSTOM_TEXTURE_FORMAT RGBA8
#endif

#ifndef CUSTOM_TEXTURE_MIPLEVELS
	#define CUSTOM_TEXTURE_MIPLEVELS 0
#endif

#define CUSTOM_TEXTURE_SIZE uint2(CUSTOM_TEXTURE_WIDTH, CUSTOM_TEXTURE_HEIGHT)
#define CUSTOM_TEXTURE_ASPECT_RATIO float(CUSTOM_TEXTURE_WIDTH / CUSTOM_TEXTURE_HEIGHT)

static const uint2 CHARS[15] = {__0, __1, __2, __3, __4, __5, __6, __7, __8, __9, __DOT, __PLUS, __MINUS, __E, __NULL};

texture BackBuffer : COLOR;
texture CUSTOM_TEXTURE_NAME {Width = CUSTOM_TEXTURE_WIDTH; Height = CUSTOM_TEXTURE_HEIGHT; Format = CUSTOM_TEXTURE_FORMAT; MipLevels = CUSTOM_TEXTURE_MIPLEVELS;};

sampler sBackBuffer {Texture = BackBuffer;};
sampler sCustomTexture {Texture = CUSTOM_TEXTURE_NAME;	MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};

/*void floatingPointToComponents(float floatValue, out bool signBit, out int exponent, out uint mantissa)
{
	signBit = sign(floatValue);
	uint value = asuint(floatValue);
	exponent = (value & 0x7F800000) >> 23;
	mantissa = (value & 0x007FFFFF);
}*/

void floatToDecimal(float floatValue, out bool signBit, out uint decimal, out int power10, out bool infinity, out bool nan)
{
    signBit = (sign(floatValue) == -1);
	
	infinity = isinf(floatValue);
	
	power10 = (floatValue != 0) ? floor(log10(floatValue)) : 0;
    
    
    decimal = uint(floatValue * pow(10, -power10) * 1000000);//asfloat(value) * exp2(-exponent) * 1000;
    
	
    nan = isnan(floatValue);
}

uint4 generateString(float floatValue)
{
	bool signBit;
	uint decimal;
	int power10;
	bool infinity;
	bool nan;
	floatToDecimal(floatValue, signBit, decimal, power10, infinity, nan);
	
	uint stringArray[16];
	[unroll]
	for(int i = 0; i < 16; i++)
	{
		stringArray[i] = 14; //Null
	}
	
	if(infinity)
	{
		stringArray[0] = (signBit) ? 12 : 11; //- : +
		[unroll]
		for(int i = 1; i < 14; i++)
		{
			stringArray[i] = stringArray[0];
		}
		
	}
	else if(nan)
	{
		[unroll]
		for(int i = 0; i < 14; i += 2)
		{
			stringArray[i] = 11; //+
			stringArray[i + 1] = 12; //-
		}
	}
#if D3D9
	else if(false) //will cause issues with dx9 otherwise
#else
	else if(power10 >= -4 && power10 <= 4)
#endif
	{
		[unroll]
		for(int i = 1; i < 14; i++)
		{
			stringArray[i] = 0;
		}
		stringArray[0] = (signBit) ? 12 : 11;//- : +
		uint pointIndex = (power10 < 0) ? 2 : 2 + (power10);
		uint iterations = (power10 < 0) ? 8 - power10 : (8);
		stringArray[pointIndex] = 10; //.
		for(int i = iterations; i > 0; i--)
		{
			if(i == pointIndex)
			{
				i--;
			}
			stringArray[i] = decimal % 10;
			decimal /= 10;
		}
	}
	else
	{
		stringArray[0] = (signBit) ? 12 : 11;
		stringArray[2] = 10;
		for(int i = 7; i > 2; i--)
		{
			stringArray[i] = decimal % 10;
			decimal /= 10;
		}
		stringArray[9] = 0;
		stringArray[8] = 0;
		stringArray[1] = decimal % 10;
		stringArray[10] = 13;//E
		stringArray[11] = (sign(power10) == -1) ? 12 : 11;
		power10 = abs(power10);
		stringArray[13] = (uint(power10) % 10);
		stringArray[12] = (uint(power10) / 10) % 10;
		
	}
	uint4 stringPointers = 0;
	for(int i = 3; i >= 0; i--)
	{
		stringPointers = (stringPointers << 4) + uint4(stringArray[i], stringArray[i + 4], stringArray[i + 8], stringArray[i + 12]);
	}
	return stringPointers;
}

//stringCoord is expected to be less than uint2(112, 5)
bool isInChar(uint2 stringCoord, uint4 stringPointers)
{
	if(any(stringCoord > uint2(112, 5))) return false;
	uint stringPos = stringCoord.x / 7;
	int2 charPos = uint2(4 - stringCoord.x % 7, 4 - stringCoord.y);
	if(charPos.x < 0) return false;
	uint stringIndex = stringPos / 4;
	uint stringChar = stringPos % 4;
	uint charIndex = (stringPointers[stringIndex] >> (4 * stringChar)) % 16;
	uint2 charHash = CHARS[charIndex];
	uint charBitShift = (charPos.y * 5 + charPos.x);
	charIndex = (charBitShift >= 16) ? 0 : 1;
	charBitShift = (charIndex == 0) ? charBitShift - 16 : charBitShift;
	return (charHash[charIndex] >> (charBitShift)) % 2;	
}

void TextureVS(uint id : SV_VertexID, out float4 vpos : SV_POSITION, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	vpos = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	texcoord /= Zoom;
	texcoord += (Zoom - 1) / (Zoom * 2);
	texcoord -= Center;
}

void DrawStringVS(uint id : SV_VertexID, out float4 vpos : SV_POSITION, out float2 texcoord : TEXCOORD, out float4 stringPointers[4] : TANGENT0)
{
	float4 debug;
	if(TextureSelection == 0)
		debug = tex2Dfetch(sBackBuffer, floor(mouse_point) + 0.5).xyzw;
	else
	{
		float2 debugCoord = (mouse_point / float2(BUFFER_WIDTH, BUFFER_HEIGHT));
		debugCoord /= Zoom;
		debugCoord += (Zoom - 1) / (Zoom * 2);
		debugCoord -= Center;
		debug = tex2Dfetch(sCustomTexture, floor(debugCoord * CUSTOM_TEXTURE_SIZE) + 0.5).xyzw;
	}
	stringPointers[0] = generateString(debug.x);
	stringPointers[1] = generateString(debug.y);
	stringPointers[2] = generateString(debug.z);
	stringPointers[3] = generateString(debug.w);
	texcoord.x = (id == 2 || id == 3) ? 1.0 : 0.0;
	texcoord.y = (id == 1 || id == 3) ? 1.0 : 0.0;
	float2 scaler = texcoord * Scale;
	scaler.x += (rcp(Scale) - 1) / rcp(Scale);
	scaler.y += 0.01;
	vpos = float4(scaler * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	texcoord *= float2(98, 28);
}

void OutputPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
{
	if(TextureSelection == 0)
		output = tex2Dfetch(sBackBuffer, floor(texcoord * float2(BUFFER_WIDTH, BUFFER_HEIGHT)) + 0.5);
	else
	{
		output = tex2Dfetch(sCustomTexture, floor(texcoord * CUSTOM_TEXTURE_SIZE) + 0.5);
	}
}

void DrawStringPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, float4 stringPointers[4] : TANGENT0, out float3 output : SV_TARGET0)
{
	uint stringIndex = texcoord.y / 7;
	uint2 coord = texcoord;
	coord.y = coord.y % 7;
	if(!isInChar(coord, stringPointers[stringIndex])) discard;
	output = TextColor;	
}

technique PixelValueDebugger<ui_tooltip = "Outputs the pixel value at the position of your mouse for either the backbuffer, or a texture you define.\n\n"
										  "Part of Insane Shaders\n"
										  "By: Lord of Lunacy";>
{
	pass
	{
		VertexShader = TextureVS;
		PixelShader = OutputPS;
	}
	pass
	{
		VertexShader = DrawStringVS;
		PixelShader = DrawStringPS;
		PrimitiveType = TRIANGLESTRIP;
		VertexCount = 4;
	}
}
