#pragma once

namespace FXShaders
{

//#region Digit Enum

static const int Digit_Space = 0;
static const int Digit_Exclamation = 1;
static const int Digit_DoubleQuote = 2;
static const int Digit_Hash = 3;
static const int Digit_Dollar = 4;
static const int Digit_Percent = 5;
static const int Digit_And = 6;
static const int Digit_SingleQuote = 7;
static const int Digit_OpenParentheses = 8;
static const int Digit_CloseParentheses = 9;
static const int Digit_Asterisk = 10;
static const int Digit_Plus = 11;
static const int Digit_Comma = 12;
static const int Digit_Hyphen = 13;
static const int Digit_Period = 14;
static const int Digit_Slash = 15;
static const int Digit_Zero = 16;
static const int Digit_One = 17;
static const int Digit_Two = 18;
static const int Digit_Three = 19;
static const int Digit_Four = 20;
static const int Digit_Five = 21;
static const int Digit_Six = 22;
static const int Digit_Seven = 23;
static const int Digit_Eight = 24;
static const int Digit_Nine = 25;
static const int Digit_Colon = 26;
static const int Digit_Semicolon = 27;
static const int Digit_LessThan = 28;
static const int Digit_Equals = 29;
static const int Digit_GreaterThan = 30;
static const int Digit_Question = 31;
static const int Digit_At = 32;
static const int Digit_UpperA = 33;
static const int Digit_UpperB = 34;
static const int Digit_UpperC = 35;
static const int Digit_UpperD = 36;
static const int Digit_UpperE = 37;
static const int Digit_UpperF = 38;
static const int Digit_UpperG = 39;
static const int Digit_UpperH = 40;
static const int Digit_UpperI = 41;
static const int Digit_UpperJ = 42;
static const int Digit_UpperK = 43;
static const int Digit_UpperL = 44;
static const int Digit_UpperM = 45;
static const int Digit_UpperN = 46;
static const int Digit_UpperO = 47;
static const int Digit_UpperP = 48;
static const int Digit_UpperQ = 49;
static const int Digit_UpperR = 50;
static const int Digit_UpperS = 51;
static const int Digit_UpperT = 52;
static const int Digit_UpperU = 53;
static const int Digit_UpperV = 54;
static const int Digit_UpperW = 55;
static const int Digit_UpperX = 56;
static const int Digit_UpperY = 57;
static const int Digit_UpperZ = 58;
static const int Digit_OpenSquareBracket = 59;
static const int Digit_Backslash = 60;
static const int Digit_CloseSquareBracket = 61;
static const int Digit_Caret = 62;
static const int Digit_Underscore = 63;
static const int Digit_Backtick = 64;
static const int Digit_LowerA = 65;
static const int Digit_LowerB = 66;
static const int Digit_LowerC = 67;
static const int Digit_LowerD = 68;
static const int Digit_LowerE = 69;
static const int Digit_LowerF = 70;
static const int Digit_LowerG = 71;
static const int Digit_LowerH = 72;
static const int Digit_LowerI = 73;
static const int Digit_LowerJ = 74;
static const int Digit_LowerK = 75;
static const int Digit_LowerL = 76;
static const int Digit_LowerM = 77;
static const int Digit_LowerN = 78;
static const int Digit_LowerO = 79;
static const int Digit_LowerP = 80;
static const int Digit_LowerQ = 81;
static const int Digit_LowerR = 82;
static const int Digit_LowerS = 83;
static const int Digit_LowerT = 84;
static const int Digit_LowerU = 85;
static const int Digit_LowerV = 86;
static const int Digit_LowerW = 87;
static const int Digit_LowerX = 88;
static const int Digit_LowerY = 89;
static const int Digit_LowerZ = 90;
static const int Digit_OpenCurlyBrace = 91;
static const int Digit_VerticalBar = 92;
static const int Digit_CloseCurlyBrace = 93;
static const int Digit_Tilde = 94;
static const int Digit_Block = 95;

//#endregion

static const int2 DigitSize = int2(3, 5);

static const int DigitPixelCount = DigitSize.x * DigitSize.y;

static const int DigitCount = 95;

static const int2 DigitMapSize =
	int2(DigitSize.x * DigitCount + DigitCount - 1, DigitSize.y);

//#region Digit Array

/*static const float Digits[] =
{
	// Space
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// !
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 0,
	0, 0, 1,

	// "
	0, 1, 1,
	0, 1, 1,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// #
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,

	// $
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,

	// %
	1, 0, 1,
	0, 1, 1,
	0, 1, 0,
	1, 1, 0,
	1, 0, 1,

	// &
	0, 1, 1,
	1, 0, 0,
	0, 1, 0,
	1, 0, 0,
	1, 1, 1,

	// '
	0, 0, 1,
	0, 0, 1,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// (
	0, 0, 1,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 0, 1,

	// )
	1, 0, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	1, 0, 0,

	// *
	0, 0, 0,
	1, 0, 1,
	0, 1, 0,
	1, 0, 1,
	0, 0, 0,

	// +
	0, 0, 0,
	0, 1, 0,
	1, 1, 1,
	0, 1, 0,
	0, 0, 0,

	// ,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 1,
	0, 0, 1,

	// -
	0, 0, 0,
	0, 0, 0,
	1, 1, 1,
	0, 0, 0,
	0, 0, 0,

	// .
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 1,

	// /
	0, 0, 1,
	0, 1, 1,
	0, 1, 0,
	1, 1, 0,
	1, 0, 0,

	// 0
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,

	// 1
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,

	// 2
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,

	// 3
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,

	// 4
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,
	0, 0, 1,
	0, 0, 1,

	// 5
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,

	// 6
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,
	1, 0, 1,
	1, 1, 1,

	// 7
	1, 1, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,

	// 8
	1, 1, 1,
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,
	1, 1, 1,

	// 9
	1, 1, 1,
	1, 0, 1,
	1, 1, 1,
	0, 0, 1,
	1, 1, 1,

	// :
	0, 0, 0,
	0, 1, 0,
	0, 0, 0,
	0, 1, 0,
	0, 0, 0,

	// ;
	0, 0, 0,
	0, 1, 0,
	0, 0, 0,
	0, 1, 0,
	0, 1, 0,

	// <
	0, 0, 1,
	0, 1, 0,
	1, 0, 0,
	0, 1, 0,
	0, 0, 1,

	// =
	0, 0, 0,
	1, 1, 1,
	0, 0, 0,
	1, 1, 1,
	0, 0, 0,

	// >
	1, 0, 0,
	0, 1, 0,
	0, 0, 1,
	0, 1, 0,
	1, 0, 0,

	// ?
	0, 1, 0,
	1, 0, 1,
	0, 0, 1,
	0, 0, 0,
	0, 1, 0,

	// @
	1, 1, 0,
	0, 0, 1,
	1, 1, 1,
	1, 0, 1,
	0, 1, 0,

	// @
	1, 1, 0,
	0, 0, 1,
	1, 1, 1,
	1, 0, 1,
	0, 1, 0,

	// A
	0, 1, 0,
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,

	// B
	1, 1, 0,
	1, 0, 1,
	1, 1, 0,
	1, 0, 1,
	1, 1, 1,

	// C
	0, 1, 1,
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,
	0, 1, 1,

	// D
	1, 1, 0,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 1, 0,

	// E
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,

	// F
	1, 1, 1,
	1, 0, 0,
	1, 1, 1,
	1, 0, 0,
	1, 0, 0,

	// G
	0, 1, 1,
	1, 0, 0,
	1, 1, 1,
	1, 0, 1,
	0, 1, 1,

	// H
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,

	// I
	1, 1, 1,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	1, 1, 1,

	// J
	1, 1, 1,
	0, 0, 1,
	0, 0, 1,
	0, 0, 1,
	1, 1, 0,

	// K
	1, 0, 1,
	1, 0, 0,
	1, 1, 0,
	1, 0, 1,
	1, 0, 1,

	// L
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,
	1, 1, 1,

	// M
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,

	// N
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,

	// O
	0, 1, 0,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,

	// P
	1, 1, 0,
	1, 0, 1,
	1, 1, 0,
	1, 0, 0,
	1, 0, 0,

	// Q
	0, 1, 0,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,
	0, 0, 1,

	// R
	1, 1, 0,
	1, 0, 1,
	1, 1, 0,
	1, 0, 1,
	1, 0, 1,

	// S
	0, 1, 1,
	1, 0, 0,
	0, 1, 0,
	0, 0, 1,
	1, 1, 0,

	// T
	1, 1, 1,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,

	// U
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,

	// V
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,

	// W
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,
	1, 1, 1,
	1, 0, 1,

	// X
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,
	1, 0, 1,
	1, 0, 1,

	// Y
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,
	0, 1, 0,
	0, 1, 0,

	// Z
	1, 1, 1,
	0, 0, 1,
	0, 1, 0,
	1, 0, 0,
	1, 1, 1,

	// [
	0, 1, 1,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 1,

	// \
	1, 0, 0,
	1, 1, 0,
	0, 1, 0,
	0, 1, 1,
	0, 0, 1,

	// ]
	1, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	1, 1, 0,

	// ^
	0, 1, 0,
	1, 0, 1,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// _
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	1, 1, 1,

	// `
	0, 1, 0,
	0, 0, 1,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// a
	1, 1, 0,
	0, 0, 1,
	0, 1, 1,
	1, 0, 1,
	0, 1, 1,

	// b
	1, 0, 0,
	1, 0, 0,
	1, 1, 0,
	1, 0, 1,
	1, 1, 0,

	// c
	0, 0, 0,
	0, 1, 1,
	1, 0, 0,
	1, 0, 0,
	0, 1, 1,

	// d
	0, 0, 1,
	0, 0, 1,
	0, 1, 1,
	1, 0, 1,
	0, 1, 1,

	// e
	0, 1, 0,
	1, 0, 1,
	1, 1, 1,
	1, 0, 0,
	0, 1, 1,

	// f
	0, 1, 1,
	1, 0, 0,
	1, 1, 0,
	1, 0, 0,
	1, 0, 0,

	// g
	0, 1, 1,
	1, 0, 1,
	0, 1, 1,
	0, 0, 1,
	1, 1, 0,

	// h
	1, 0, 0,
	1, 0, 0,
	1, 1, 0,
	1, 0, 1,
	1, 0, 1,

	// i
	0, 1, 0,
	0, 0, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,

	// j
	0, 1, 0,
	0, 0, 0,
	0, 1, 0,
	0, 1, 0,
	1, 1, 0,

	// k
	1, 0, 0,
	1, 0, 1,
	1, 1, 0,
	1, 1, 0,
	1, 0, 1,

	// l
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 0, 1,

	// m
	0, 0, 0,
	0, 0, 0,
	1, 1, 1,
	1, 1, 1,
	1, 0, 1,

	// n
	0, 0, 0,
	0, 0, 0,
	1, 1, 1,
	1, 0, 1,
	1, 0, 1,

	// o
	0, 0, 0,
	0, 1, 0,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,

	// p
	0, 1, 0,
	1, 0, 1,
	1, 1, 0,
	1, 0, 0,
	1, 0, 0,

	// q
	0, 1, 0,
	1, 0, 1,
	0, 1, 1,
	0, 0, 1,
	0, 0, 1,

	// r
	0, 0, 0,
	0, 1, 1,
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,

	// s
	0, 1, 0,
	1, 0, 1,
	0, 1, 0,
	0, 0, 1,
	1, 1, 0,

	// t
	0, 1, 0,
	1, 1, 1,
	0, 1, 0,
	0, 1, 0,
	0, 0, 1,

	// u
	0, 0, 0,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,

	// v
	0, 0, 0,
	1, 0, 1,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,

	// w
	0, 0, 0,
	1, 0, 1,
	1, 0, 1,
	1, 1, 1,
	1, 0, 1,

	// x
	0, 0, 0,
	0, 0, 0,
	1, 0, 1,
	0, 1, 0,
	1, 0, 1,

	// y
	0, 0, 0,
	1, 0, 1,
	1, 0, 1,
	0, 1, 0,
	1, 0, 0,

	// z
	0, 0, 0,
	1, 1, 1,
	0, 0, 1,
	0, 1, 0,
	1, 1, 1,

	// {
	0, 1, 0,
	1, 0, 0,
	0, 1, 0,
	1, 0, 0,
	0, 1, 0,

	// |
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,
	0, 1, 0,

	// }
	0, 1, 0,
	0, 0, 1,
	0, 1, 0,
	0, 0, 1,
	0, 1, 0,

	// ~
	0, 0, 0,
	1, 1, 1,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,

	// Block
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1
};*/

//#endregion

//#region Textures

texture DigitMapTex <source = "DigitMap.png";>
{
	Width = DigitMapSize.x;
	Height = DigitMapSize.y;
	Format = R8;
};

sampler DigitMap
{
	Texture = DigitMapTex;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = REPEAT;
	AddressV = REPEAT;
};

//#endregion

//#region Functions

float GetDigitPixel(int digit, int2 pos)
{
	return tex2Dfetch(DigitMap, int4(pos.x + digit * DigitSize.x + digit, pos.y, 0, 0)).x;

	//return Digits[pos.x + DigitSize.x * (pos.y + digit)];
}

void RenderDigit(
	inout float4 color,
	float2 coord,
	int digit,
	int digitScale,
	int2 digitOffset,
	float4 digitColor)
{
	// if (digit > DigitCount)
		// return;

	int2 arrayPos = coord;
	arrayPos /= digitScale;
	arrayPos -= digitOffset;

	if (
		arrayPos.x < 0 || arrayPos.x >= DigitSize.x ||
		arrayPos.y < 0 || arrayPos.y >= DigitSize.y)
	{
		return;
	}

	float digitPixel = GetDigitPixel(digit, arrayPos);
	color.rgb = lerp(color.rgb, digitColor.rgb, digitPixel * digitColor.a);
	color.a = max(color.a, digitColor.a * digitPixel);
}

void RenderInteger(
	inout float4 color,
	float2 coord,
	int value,
	int digits,
	bool zeroFill,
	int digitScale,
	int2 digitOffset,
	float4 digitColor)
{
	if (zeroFill)
	{
		[unroll]
		for (int i = digits - 1; i >= 0; --i)
		{
			RenderDigit(
				color,
				coord,
				Digit_Zero - 1 + value % 10,
				digitScale,
				digitOffset + int2(i * DigitSize.x + i, 0),
				digitColor);

			value /= 10;
		}
	}
	else
	{
		[unroll]
		for (int i = digits - 1; i >= 0; --i)
		{
			// Skip rendering zero digits to the left.
			// We *could* just use break, but that'd require the loop to be
			// dynamic.
			if (value != 0 || i == digits - 1)
				RenderDigit(
					color,
					coord,
					Digit_Zero - 1 + value % 10,
					digitScale,
					digitOffset + int2(i * DigitSize.x + i, 0),
					digitColor);

			value /= 10;
		}
	}
}

//#endregion

}
