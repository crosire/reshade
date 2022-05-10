/*
This shader makes use of the scatter capabilities of a compute shader to perform an adaptive IIR filter rather than
the traditional FIR filters normally used in image processing.

LocalContrastCS

Arici, Tarik, and Yucel Altunbasak. “Image Local Contrast Enhancement Using Adaptive Non-Linear Filters.” 
2006 International Conference on Image Processing, 2006, https://doi.org/10.1109/icip.2006.313031. 
*/

#define COMPUTE 1
#define DIVIDE_ROUNDING_UP(n, d) uint(((n) + (d) - 1) / (d))
#define FILTER_WIDTH 128
#define PIXELS_PER_THREAD 128
#define H_GROUPS uint2((DIVIDE_ROUNDING_UP(BUFFER_WIDTH, PIXELS_PER_THREAD) * 2), DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, 64))
#define V_GROUPS uint2(DIVIDE_ROUNDING_UP(BUFFER_WIDTH, 64), (DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, PIXELS_PER_THREAD) * 2))
#define H_GROUP_SIZE uint2(1, 64)
#define V_GROUP_SIZE uint2(64, 1)
#define PI 3.1415962

#if __RESHADE__ >= 50000
	#define POOLED true
#else
	#define POOLED false
#endif

#if __RENDERER__ < 0xb000
	#warning "DX9 and DX10 APIs are unsupported by compute"
	#undef COMPUTE
	#define COMPUTE 0
#endif

#if __RESHADE__ < 50000 && __RENDERER__ == 0xc000
	#warning "Due to a bug in the current version of ReShade, this shader is disabled in DX12 until the release of ReShade 5.0."
	#undef COMPUTE
	#define COMPUTE 0
#endif

#if COMPUTE != 0
namespace Spatial_IIR_Clarity
{
	texture BackBuffer:COLOR;
	texture Luma <Pooled = POOLED;>{Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8;};
	texture Blur0 <Pooled = POOLED;>{Width = BUFFER_WIDTH * 2; Height = BUFFER_HEIGHT; Format = R8;};

	sampler sBackBuffer{Texture = BackBuffer;};
	sampler sLuma {Texture = Luma;};
	sampler sBlur0{Texture = Blur0;};
	
	
	storage wLuma{Texture = Luma;};
	storage wBlur0{Texture = Blur0;};
	
	uniform float Strength<
		ui_type = "slider";
		ui_label = "Strength";
		ui_min = 0; ui_max = 1;
		ui_step = 0.001;
	> = 1;
	
	
	
	uniform float WeightExponent<
		ui_type = "slider";
		ui_label = "Detail Sharpness";
		ui_tooltip = "Use this slider to determine how large of a region the shader considers to be local;\n"
					 "a larger number will correspond to a smaller region, and will result in sharper looking\n"
					 "details.";
		ui_min = 5; ui_max = 12;
	> = 5;
	
	//Constants used by research paper
	static const float a = 0.0039215686; 
	static const float b = 0.0274509804;
	static const float c = 0.0823529412;
	
	float GainCoefficient(float x, float a, float b, float c, float k)
	{
		float gain = (x < a) ? 0 :
			         (x < b) ? cos((PI * rcp(b - a) * x + PI - (PI * a) * rcp(b - a))) :
			         (x < c) ? cos((PI * rcp(c - b) * x - (PI * b) * rcp(c-b))) : 0;
		return gain * (k/2) + (k/2);
	}
	
	// Vertex shader generating a triangle covering the entire screen
	void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
	{
		texcoord.x = (id == 2) ? 2.0 : 0.0;
		texcoord.y = (id == 1) ? 2.0 : 0.0;
		position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	}

	void LumaPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float luma : SV_Target0)
	{
		luma = dot(tex2D(sBackBuffer, texcoord).rgb, float3(0.299, 0.587, 0.114));
	}
	
	void CombinePS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float output : SV_Target0)
	{
		texcoord.x /= 2;
		output = (tex2D(sBlur0, texcoord).x + tex2D(sBlur0, float2(texcoord.x + 0.5, texcoord.y)).x) * 0.5;//dot(tex2D(sBackBuffer, texcoord).rgb, float3(0.299, 0.587, 0.114));
	}
	
	void HorizontalFilterCS0(uint3 id : SV_DispatchThreadID)
	{
		if(id.x < (H_GROUPS.x / 2))
		{
			float2 coord = float2(id.x * PIXELS_PER_THREAD, id.y) + 0.5;
			float curr;
			float prev;
			float weight;
			prev = tex2Dfetch(sLuma, float2(coord.x - FILTER_WIDTH, coord.y)).x;
			
			for(int i = -FILTER_WIDTH + 1; i < PIXELS_PER_THREAD; i++)
			{
				curr = tex2Dfetch(sLuma, float2(coord.x + i, coord.y)).x;
				weight = 1 - abs(curr - prev);
				weight = pow(abs(weight), WeightExponent);
				prev = lerp(curr, prev, weight);
				if(i >= 0  && (coord.x + i) < BUFFER_WIDTH)
				{
					tex2Dstore(wBlur0, int2(coord.x + i, coord.y), prev.xxxx);
				}
			}
		}
		else
		{
			float2 coord = float2((id.x - (H_GROUPS.x / 2)) * PIXELS_PER_THREAD + PIXELS_PER_THREAD, id.y) + 0.5;
			float curr;
			float prev;
			float weight;
			prev = tex2Dfetch(sLuma, float2(coord.x + FILTER_WIDTH, coord.y)).x;

			for(int i = FILTER_WIDTH - 1; i > -PIXELS_PER_THREAD; i--)
			{
				curr = tex2Dfetch(sLuma, float2(coord.x + i, coord.y)).x;
				weight = 1 - abs(curr - prev);
				weight = pow(abs(weight), WeightExponent);
				prev = lerp(curr, prev, weight);
				if(i <= 0)
				{
					tex2Dstore(wBlur0, int2(BUFFER_WIDTH + coord.x + i, coord.y), prev.xxxx);
				}
			}
		}
	}
	
	void VerticalFilterCS0(uint3 id : SV_DispatchThreadID, uint3 tid : SV_GroupThreadID)
	{
		if(id.y < V_GROUPS.y / 2)
		{
			float2 coord = float2(id.x, id.y * PIXELS_PER_THREAD) + 0.5;
			float curr;
			float prev;
			float weight;
			prev = tex2Dfetch(sLuma, float2(coord.x, coord.y - FILTER_WIDTH)).x;

			for(int i = -FILTER_WIDTH + 1; i < PIXELS_PER_THREAD; i++)
			{
				curr = tex2Dfetch(sLuma, float2(coord.x, coord.y + i)).x;
				weight = 1 - abs(curr - prev);
				weight = pow(abs(weight), WeightExponent);
				prev = lerp(curr, prev, weight);
				if(i >= 0 && (coord.x) < BUFFER_WIDTH)
				{
					tex2Dstore(wBlur0, int2(coord.x, coord.y + i), prev.xxxx);
				}
			}
		}
		else
		{
			float2 coord = float2(id.x, (id.y - (V_GROUPS.y / 2)) * PIXELS_PER_THREAD + PIXELS_PER_THREAD) + 0.5;
			float curr;
			float prev;
			float weight;
			prev = tex2Dfetch(sLuma, float2(coord.x, coord.y + FILTER_WIDTH)).x;

			for(int i = FILTER_WIDTH - 1; i > -PIXELS_PER_THREAD; i--)
			{
				curr = tex2Dfetch(sLuma, float2(coord.x, coord.y + i)).x;
				weight = 1 - abs(curr - prev);
				weight = pow(abs(weight), WeightExponent);
				prev = lerp(curr, prev, weight);
				if(i <= 0)
				{
					tex2Dstore(wBlur0, int2(BUFFER_WIDTH + coord.x, coord.y + i), prev.xxxx);
				}
			}
		}
	}
	
	void OutputPS(float4 vpos : SV_POSITION, float2 texcoord : TEXCOORD, out float4 output : SV_TARGET0)
	{	
		float3 color = tex2D(sBackBuffer, texcoord).rgb;
		texcoord.x /= 2;
		float blur = (tex2D(sBlur0, texcoord).x + tex2D(sBlur0, float2(texcoord.x + 0.5, texcoord.y)).x) * 0.5;;//tex2D(sBlur1, texcoord).x;
		
		
		float y = dot(color, float3(0.299, 0.587, 0.114));
		y += (y - blur) * GainCoefficient(abs(y-blur), a, b, c, Strength);
		float cb = dot(color, float3(-0.168736, -0.331264, 0.5));
		float cr = dot(color, float3(0.5, -0.418688, -0.081312));


		output.r = dot(float2(y, cr), float2(1, 1.402));//y + 1.402 * cr;
		output.g = dot(float3(y, cb, cr), float3(1, -0.344135, -0.714136));
		output.b = dot(float2(y, cb), float2(1, 1.772));//y + 1.772 * cb;
		output.a = 1;
	}
	
	technique LocalContrastCS <ui_tooltip = "A local contrast shader based on an adaptive infinite impulse response filter,\n"
											"that adjusts the contrast of the image based on the amount of sorrounding contrast.\n\n"
											"Part of Insane Shaders\n"
											"By: Lord of Lunacy";>
	{	
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = LumaPS;
			RenderTarget0 = Luma;
		}
		
		pass
		{
			ComputeShader = HorizontalFilterCS0<H_GROUP_SIZE.x, H_GROUP_SIZE.y>;
			DispatchSizeX = H_GROUPS.x;
			DispatchSizeY = H_GROUPS.y;
		}
		
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = CombinePS;
			RenderTarget0 = Luma;
		}
		
		pass
		{
			ComputeShader = VerticalFilterCS0<V_GROUP_SIZE.x, V_GROUP_SIZE.y>;
			DispatchSizeX = V_GROUPS.x;
			DispatchSizeY = V_GROUPS.y;
		}
		
		
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = OutputPS;
		}
	}
}
#endif
