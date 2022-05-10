//#region Preprocessor

#include "ReShade.fxh"
#include "ReShadeUI.fxh"
#include "DrawText.fxh"
#include "FXShaders/Common.fxh"
#include "FXShaders/Canvas.fxh"
#include "FXShaders/Digits.fxh"
#include "FXShaders/Math.fxh"

#define DECLARE_VARIABLE_TEX(name, format) \
texture name##Tex \
{ \
	Format = format; \
}; \
\
sampler name \
{ \
	Texture = name##Tex; \
	MinFilter = POINT; \
	MagFilter = POINT; \
	MipFilter = POINT; \
}

//#endregion

namespace FXShaders
{

//#region Constants

static const float BallInitialYPercent = 0.2;
static const float2 BallSize = 50;

static const float BallSpeedIncreasePercent = 1.05;

static const float2 PaddleSize = float2(250, 50);
static const float PaddleYPercent = 0.9;

static const int Corner_TopLeft = 0;
static const int Corner_TopRight = 1;
static const int Corner_BottomLeft = 2;
static const int Corner_BottomRight = 3;

static const int2 EasterSize = 128;

//#endregion

//#region Uniforms

uniform float Opacity
<
	__UNIFORM_SLIDER_FLOAT1

	ui_category = "Appearance";
	ui_tooltip =
		"Global opacity of the pong game.\n"
		"\nDefault: 255";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

uniform float4 BackgroundColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_category = "Appearance";
	ui_label = "Background Color";
	ui_tooltip =
		"Color of the pong game background, can be transparent.\n"
		"\nDefault: 0 0 0 127";
> = float4(0.0, 0.0, 0.0, 1.0);

uniform float4 BallColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_category = "Appearance";
	ui_label = "Ball Color";
	ui_tooltip =
		"Color of the pong ball, can be transparent.\n"
		"\nDefault: 255 0 0 255";
> = float4(1.0, 0.0, 0.0, 1.0);

uniform float4 PaddleColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_category = "Appearance";
	ui_label = "Paddle Color";
	ui_tooltip =
		"Color of the pong paddle, can be transparent.\n"
		"\nDefault: 0 0 255 255";
> = float4(0.0, 0.0, 1.0, 1.0);

uniform float4 ScoreColor
<
	__UNIFORM_COLOR_FLOAT4

	ui_category = "Appearance";
	ui_label = "Score Color";
	ui_tooltip =
		"Color of the pong score, can be transparent.\n"
		"\nDefault: 0 255 0 255";
> = float4(0.0, 1.0, 0.0, 1.0);

uniform float LaunchDelay
<
	__UNIFORM_DRAG_FLOAT1

	ui_category = "Gameplay";
	ui_label = "Launch Delay";
	ui_tooltip =
		"Time in seconds before the ball is launched when the game starts or "
		"after a game over.\n"
		"\nDefault: 1.0";
	ui_min = 0.0;
	ui_max = 3.0;
	ui_step = 0.01;
> = 1.0;

uniform float MaxBallSpeed
<
	__UNIFORM_SLIDER_FLOAT1

	ui_category = "Gameplay";
	ui_label = "Max Ball Speed";
	ui_tooltip =
		"The maximum speed the ball can achieve.\n"
		"\nDefault: 3000.0";
	ui_min = 1000.0;
	ui_max = 10000.0;
> = 3000.0;

uniform float RandomSpeed <source = "random"; min = 400; max = 600;>;
uniform float RandomDirection <source = "random"; min = 45; max = 135;>;

uniform float2 MousePoint <source = "mousepoint";>;

uniform float FrameTime <source = "frametime";>;
#define DeltaTime (FrameTime * 0.001)

uniform float Timer <source = "timer";>;

//#endregion

//#region Textures

/*
	r: is game running
	g: score
	b: timer
	a: was ball colliding in the last frame?
*/
DECLARE_VARIABLE_TEX(GameState, RGBA32F);
DECLARE_VARIABLE_TEX(LastGameState, RGBA32F);

DECLARE_VARIABLE_TEX(BallPosSpeed, RGBA32F);
DECLARE_VARIABLE_TEX(LastBallPosSpeed, RGBA32F);

texture EasterTex <source = "Pong.png";>
{
	Width = EasterSize.x;
	Height = EasterSize.y;
};

sampler Easter
{
	Texture = EasterTex;
	AddressU = BORDER;
	AddressV = BORDER;
};

//#endregion

//#region Functions

void RenderBall(inout float4 color, float2 coord, float2 ballPos)
{
	float4 rect = ConvertToRect(ballPos, BallSize);
	FillRect(color, coord, rect, BallColor);
}

float2 GetPaddlePos()
{
	float2 paddlePos = float2(MousePoint.x, BUFFER_HEIGHT * PaddleYPercent);

	float paddleHalfWidth = PaddleSize.x * 0.5;
	paddlePos.x = clamp(
		paddlePos.x,
		paddleHalfWidth,
		BUFFER_WIDTH - paddleHalfWidth);

	return paddlePos;
}

void RenderPaddle(inout float4 color, float2 coord, float2 paddlePos)
{
	float4 rect = ConvertToRect(paddlePos, PaddleSize);
	FillRect(color, coord, rect, PaddleColor);
}

void RenderScore(inout float4 color, float2 coord, int score)
{
	RenderInteger(
		color,
		coord,
		score,
		4,
		false,
		32,
		0,
		ScoreColor);
}

float2 DeobfuscateCoord(float2 coord)
{
	if ((coord.y % 2.0) > 1.0)
		coord.x = EasterSize.x - coord.x;

	if ((coord.x % 2.0) > 1.0)
		coord.y = EasterSize.y - coord.y;

	return coord;
}

void RenderEaster(inout float4 color, float2 coord, int corner)
{
	switch (corner)
	{
		case Corner_TopLeft:
			coord.y -= (Timer * 0.1) % 128 - 128;
			coord.y = EasterSize.y - coord.y;
			coord = DeobfuscateCoord(coord);
			break;
		case Corner_TopRight:
			coord.x -= BUFFER_WIDTH - EasterSize.x;
			coord.x = EasterSize.x - coord.x;
			coord = DeobfuscateCoord(coord);
			coord.y = EasterSize.y - coord.y;
			break;
	}

	float4 easter = tex2Dfetch(Easter, int4(coord, 0, 0));
	easter.rgb = 1.0 - easter.rgb;

	color.rgb = lerp(color.rgb, easter.rgb, easter.a);
	color.a = max(color.a, easter.a);
}

//#endregion

//#region Shaders

void CalcLogicPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD,
	out float4 gameState : SV_TARGET0,
	out float4 posSpeed : SV_TARGET1)
{
	gameState = tex2Dfetch(LastGameState, 0);

	// If game is not running.
	if (gameState.x == 0)
	{
		// Start game.
		gameState.x = 1;

		// Reset score.
		gameState.y = 0;

		// Set timer to wait before launching the ball.
		gameState.z = Timer;

		// Ball is no longer colliding with the paddle.
		gameState.w = 0.0;

		float2 startPos = BUFFER_SCREEN_SIZE * float2(0.5, BallInitialYPercent);

		float2 startSpeed = GetDirectionFromAngleMagnitude(
			RandomDirection * DegreesToRadians,
			RandomSpeed);

		posSpeed = float4(startPos, startSpeed);

		return;
	}

	// Wait for the set launch delay.
	if (Timer - gameState.z < (LaunchDelay * 1000.0))
		return;

	posSpeed = tex2Dfetch(LastBallPosSpeed, 0);
	posSpeed.xy += posSpeed.zw * DeltaTime;

	// Game over.
	if (posSpeed.y - BallSize.y > BUFFER_HEIGHT)
	{
		// Game is no longer running, entering a "game over" state.
		gameState.x = 0;

		return;
	}

	float2 paddlePos = GetPaddlePos();

	float4 ballRect = ConvertToRect(posSpeed.xy, BallSize);
	float4 paddleRect = ConvertToRect(paddlePos, PaddleSize);

	if (AABBCollision(ballRect, paddleRect))
	{
		if (gameState.w < 1.0)
		{
			float2 delta = posSpeed.xy - paddlePos;
			float angle = atan2(delta.y, delta.x);
			float speed = length(posSpeed.zw) * BallSpeedIncreasePercent;

			posSpeed.zw = GetDirectionFromAngleMagnitude(angle, speed);

			++gameState.y;
			gameState.w = 1.0;
		}
	}
	else
	{
		gameState.w = 0.0;
	}

	if (ballRect.x < 0.0)
		posSpeed.z = abs(posSpeed.z);// * BallSpeedIncreasePercent;
	else if (ballRect.z > BUFFER_WIDTH)
		posSpeed.z = -abs(posSpeed.z);// * BallSpeedIncreasePercent;

	if (ballRect.y < 0.0)
		posSpeed.w = abs(posSpeed.w);// * BallSpeedIncreasePercent;

	posSpeed.zw = ClampMagnitude(posSpeed.zw, float2(0.0, MaxBallSpeed));
}

void SaveLogicPS(
	float4 p : SV_POSITION,
	float2 uv : TEXCOORD,
	out float4 gameState : SV_TARGET0,
	out float4 posSpeed : SV_TARGET1)
{
	gameState = tex2Dfetch(GameState, 0);
	posSpeed = tex2Dfetch(BallPosSpeed, 0);
}

float4 RenderPS(float4 p : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
	float2 coord = uv * BUFFER_SCREEN_SIZE;
	float4 color = tex2D(ReShade::BackBuffer, uv);

	float4 gameState = tex2Dfetch(GameState, 0);
	float4 posSpeed = tex2Dfetch(BallPosSpeed, 0);
	float4 pongGame = BackgroundColor;

	RenderEaster(pongGame, coord, Corner_TopLeft);

	RenderBall(pongGame, coord, posSpeed.xy);

	RenderPaddle(pongGame, coord, GetPaddlePos());

	RenderScore(pongGame, coord, gameState.y);

	color.rgb = lerp(color.rgb, pongGame.rgb, pongGame.a * Opacity);

	return color;
}

//#endregion

//#region Technique

technique Pong
{
	pass CalcLogic
	{
		VertexShader = PostProcessVS;
		PixelShader = CalcLogicPS;
		RenderTarget0 = GameStateTex;
		RenderTarget1 = BallPosSpeedTex;
	}
	pass SaveLogic
	{
		VertexShader = PostProcessVS;
		PixelShader = SaveLogicPS;
		RenderTarget0 = LastGameStateTex;
		RenderTarget1 = LastBallPosSpeedTex;
	}
	pass Render
	{
		VertexShader = PostProcessVS;
		PixelShader = RenderPS;
	}
}

//#endregion

} // Namespace.
