////////////////////////////////////////////////////////////////////////////////////////////////////////
// Droste.fx by SirCobra
// Version 0.4
// You can find info and my repository here: https://github.com/LordKobra/CobraFX
// This effect warps space inside itself.
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************                  *******************************************//
//***************************************   UI & Defines   *******************************************//
//***************************************                  *******************************************//
////////////////////////////////////////////////////////////////////////////////////////////////////////

// Shader Start
#include "Reshade.fxh"

// Namespace everything
namespace Droste
{

	//defines
	#define MASKING_M   "General Options\n"

	#ifndef M_PI
		#define M_PI 3.1415927
	#endif
	#ifndef M_E
		#define M_E 2.71828183
	#endif

	//ui
	uniform int Buffer1 <
		ui_type = "radio"; ui_label = " ";
	>;	
	uniform int EffectType <
		ui_type = "radio";
		ui_items = "Hyperdroste\0Droste\0";
		ui_label = "Effect Type";
		ui_category = MASKING_M;
	> = 0;
	uniform bool Spiral <
		ui_tooltip = "Warp space into a spiral.";
		ui_category = MASKING_M;
	> = true;
	uniform float InnerRing <
		ui_type = "slider";
		ui_min = 0.00; ui_max = 1;
		ui_step = 0.01;
		ui_tooltip = "The inner ring defines the texture border towards the center of the screen.";
		ui_category = MASKING_M;
	> = 0.3;
    uniform float OuterRing <
		ui_type = "slider";
		ui_min = 0.0; ui_max = 1;
		ui_step = 0.01;
		ui_tooltip = "The outer ring defines the texture border towards the edge of the screen.";
		ui_category = MASKING_M;
	> = 1.0;
	uniform float Zoom <
		ui_type = "slider";
		ui_min = 0.0; ui_max = 9.9;
		ui_step = 0.01;
		ui_tooltip = "Zoom in the output.";
		ui_category = MASKING_M;
	> = 1.0;
		uniform float Frequency <
		ui_type = "slider";
		ui_min = 0.1; ui_max = 10;
		ui_step = 0.01;
		ui_tooltip = "Defines the frequency of the intervals.";
		ui_category = MASKING_M;
	> = 1.0;
	uniform float X_Offset <
		ui_type = "slider";
		ui_min = -0.5; ui_max = 0.5;
		ui_step = 0.01;
		ui_tooltip = "Change the X position of the center. Keep it to 0 to get the best results. ";
		ui_category = MASKING_M;
	> = 0.0;
	uniform float Y_Offset <
		ui_type = "slider";
		ui_min = -0.5; ui_max = 0.5;
		ui_step = 0.01;
		ui_tooltip = "Change the Y position of the center. Keep it to 0 to get the best results.";
		ui_category = MASKING_M;
	> = 0.0;
	uniform int Buffer4 <
		ui_type = "radio"; ui_label = " ";
	>;


    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    //***************************************                  *******************************************//
    //*************************************** Helper Functions *******************************************//
    //***************************************                  *******************************************//
    ////////////////////////////////////////////////////////////////////////////////////////////////////////


	//vector mod and normal fmod
	float mod(float x, float y) 
	{
		return x - y * floor(x / y);
	}

    float atan2_approx(float y, float x)
    {
		return acos(x * rsqrt(y * y + x * x)) * (y < 0 ? -1 : 1);
	}


    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    //***************************************                  *******************************************//
    //***************************************      Effect      *******************************************//
    //***************************************                  *******************************************//
    ////////////////////////////////////////////////////////////////////////////////////////////////////////


	void droste(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target)
	{
		//transform coordinate system
		float ar = float(BUFFER_WIDTH) / BUFFER_HEIGHT;
		ar = EffectType == 0 ? ar : 1;
		float new_x = (texcoord.x - 0.5 + X_Offset) * (EffectType == 0 ? ar : 1);
		float new_y = (texcoord.y - 0.5 + Y_Offset);

		//calculate and normalize angle
		float val = atan2_approx(new_x, new_y) + M_PI;
		val /= 2 * M_PI;
		val = Spiral ? val : 0;

		//calculate distance from center
		float hyperdroste = val + log(sqrt(new_x * new_x + new_y * new_y) * (10 - Zoom)) * Frequency;
		float droste = val + log(max(abs(new_x), abs(new_y)) * (10 - Zoom)) * Frequency;
		val = EffectType == 0 ? hyperdroste : droste;
		val = (exp(mod(val, 1)) - 1) / (M_E - 1);

		//fix distortion
		const float y_top = 0.5;
		const float y_bottom = -0.5;
		const float x_right = 0.5;
		const float x_left = -0.5;
		//ar normalized towards projection borders
		float nny = (new_y < 0) ? y_bottom / new_y : y_top / new_y;
		float nnx = (new_x < 0) ? x_left * ar / new_x : x_right * ar / new_x; // überleeeegeeeen! Concept: AR+fixed Offset works?
		float nnc = min(nnx, nny);
		float normalized_x = new_x * nnc + X_Offset * ar;
		float normalized_y = new_y * nnc + Y_Offset;
		float d_normal_2 = sqrt((normalized_x - X_Offset * ar) * (normalized_x - X_Offset * ar) + (normalized_y - Y_Offset) * (normalized_y - Y_Offset)); //ar normalized

		//rounding screencentered borders
		float d_left = x_left + X_Offset;
		float d_right = x_right + X_Offset;
		float d_top = y_top + Y_Offset;
		float d_bottom = y_bottom + Y_Offset;
		float d_x = (new_x < 0) ? d_left * ar / new_x : d_right * ar / new_x;
		float d_y = (new_y < 0) ? d_bottom / new_y : d_top / new_y;
		//radial interpolation
		float xclose = d_x * new_x;
		float yclose = d_y * new_y;
		float ri = (abs(new_x * xclose) + abs(new_y * yclose)) / (abs(new_x) + abs(new_y)) / 0.5;
		//float tannorm = abs(mod((atan2_approx(new_x,new_y)+M_PI)/(2*M_PI)+(atan2_approx(X_Offset,-Y_Offset)+M_PI)/(2*M_PI)+0.5,1)-0.5);
		//ri = 0.5*tannorm; Pretty idea, looks bad
		nnc = min(d_x, d_y);
		float nx_2 = new_x * nnc;
		float ny_2 = new_y * nnc;
		float aar = saturate(ri - 0.4) + 0.15; //saturate((sqrt(nx_2 * nx_2 + ny_2 * ny_2)) / d_normal_2+0.15); // TODO: remove shitty version and make it usable with a smooth one
		float r = aar;						   // TODO: smaller r reduces visual look although it should increase it - fix one day
		float arr = (1 - r) * ar + r;
		float d_final = sqrt(new_x * new_x + new_y * new_y) / pow(pow(abs(new_x) / arr * 2, 2.0 / r) + pow(abs(new_y) * 2, 2.0 / r), r / 2.0);
		//TODO: See if the projection is correct in projectionspace (it once worked in screenspace, so fix it!!!!)
		float buffer_len = d_normal_2;
		//TODO: fix rectangle distortions
		float scale_normal = d_final / buffer_len;
		normalized_x = EffectType == 0 ? (normalized_x - X_Offset * ar) * scale_normal / ar + X_Offset : normalized_x; //here we leave AR space
		normalized_y = EffectType == 0 ? (normalized_y - Y_Offset) * scale_normal + Y_Offset : normalized_y;
		//calculate relative position towards outer and inner ring and interpolate
		float real_scale = (1 - val) * InnerRing + val * OuterRing;
		float adjusted_x = EffectType == 0 ? normalized_x * real_scale + 0.5 - X_Offset : normalized_x * real_scale + 0.5 - X_Offset;
		float adjusted_y = normalized_y * real_scale + 0.5 - Y_Offset;
		fragment = tex2D(ReShade::BackBuffer, float2(adjusted_x, adjusted_y));
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//***************************************                  *******************************************//
	//***************************************     Pipeline     *******************************************//
	//***************************************                  *******************************************//
	////////////////////////////////////////////////////////////////////////////////////////////////////////


	technique Droste < ui_tooltip = "Warp space inside a spiral."; >
	{
		pass spiral_step { VertexShader = PostProcessVS; PixelShader = droste; }
	}
}
