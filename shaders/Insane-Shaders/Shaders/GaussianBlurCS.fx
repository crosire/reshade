/*
	Gaussian Blur Compute Shader for ReShade
	By: Lord Of Lunacy
	
	A heavily optimized gaussian blur with 3 different kernel sizes, and wrote in a way that it could
	easily be adapted to perform other seperable convolutions.
	
	Bilodeau, B. (2012, October). Efficient Compute Shader Programming [PPT]. Advanced Micro Devices.
	https://developer.amd.com/wordpress/media/2012/10/Efficient%20Compute%20Shader%20Programming.pps
*/


#if (((__RENDERER__ >= 0xb000 && __RENDERER__ < 0x10000) || (__RENDERER__ >= 0x14300)) && __RESHADE__ >=40800)
	#define GAUSSIAN_COMPUTE 1
#else
	#define GAUSSIAN_COMPUTE 0
#endif




#ifndef CS_GAUSSIAN_BLUR_SIZE
	#define CS_GAUSSIAN_BLUR_SIZE 1
#endif

#if CS_GAUSSIAN_BLUR_SIZE == 0
	#define KERNEL_SIZE 23
#elif CS_GAUSSIAN_BLUR_SIZE == 1
	#define KERNEL_SIZE 33
#elif CS_GAUSSIAN_BLUR_SIZE == 2
	#define KERNEL_SIZE 65
#else
	#undef CS_GAUSSIAN_BLUR_SIZE
	#define CS_GAUSSIAN_BLUR_SIZE 1
	#define KERNEL_SIZE 33
#endif


#define KERNEL_RADIUS (KERNEL_SIZE / 2)
#define SAMPLES_PER_THREAD 4

//This is the size of the workgroups that will be dispatched. Notice that the width is set to 32 and the height is 2, this is to allow for both AMD and Nvidia
//to be optimized. The size of an SM on Nvidia GPUs is 32 threads, this means that when a wave is dispatched it is made up of 32. However, on AMD this is 64,
//so by setting the workgroups to work on 2 rows it allows for AMD to fill each SM with work and not have half of the GPU running at idle, while also not placing
//limitations on the Nvidia GPU as the rows will not be sharing anything between them, and therefore allowing almost everything to stay on a single SM.
#define GROUP_WIDTH 32
#define GROUP_HEIGHT 2

#define ARRAY_DIMENSIONS int2(GROUP_WIDTH * SAMPLES_PER_THREAD + (KERNEL_RADIUS * 2), GROUP_HEIGHT)
#define DIVIDE_ROUNDING_UP(numerator, denominator) (int(numerator + denominator - 1) / int(denominator))

#if GAUSSIAN_COMPUTE != 0
texture BackBuffer : COLOR;
texture PlaceHolder {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2;};
texture Convolution {Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2;};

sampler sBackBuffer {Texture = BackBuffer;};
sampler sPlaceHolder {Texture = PlaceHolder;};
sampler sConvolution {Texture = Convolution;};

storage wPlaceHolder {Texture = PlaceHolder;};
storage wConvolution {Texture = Convolution;};

uniform float Strength<
	ui_type = "slider";
	ui_label = "Strength";
	ui_tooltip = "Use negative strength values to apply an unsharp mask to the image.";
	ui_category = "General";
	ui_min = -1; ui_max = 1;
	ui_step = 0.001;
> = 1;

#if KERNEL_SIZE == 23
static const float KERNEL[KERNEL_RADIUS + 1] = {0.002436706343803515, 0.004636114237302254, 0.008296608238596322, 0.013965003570314828, 0.022109289813789203, 
												0.032923196406968135, 0.0461130337346424, 0.06074930837220863, 0.07527587082752966, 0.08773365683170255,
												0.09617695873269712, 0.09916850578089087};
												
#elif KERNEL_SIZE == 33												
static const float KERNEL[KERNEL_RADIUS + 1] = {0.001924, 0.002957, 0.004419, 0.006424, 0.009084, 0.012493, 0.016713, 0.021747, 0.027524, 0.033882, 0.04057,
												0.04725, 0.053526, 0.058978, 0.063209, 0.065892, 0.066812};
												
#elif KERNEL_SIZE == 35
static const float KERNEL[KERNEL_RADIUS + 1] = {0.00131687903069317, 0.00205870530023407, 0.0031324441255622245, 0.004638874539538358, 0.006686232410653146,
												0.0093797098953999, 0.01280666839519681, 0.017018514961456375, 0.02201131599327036, 0.027708274880658742, 
												0.03394786563326623, 0.040481381619799764, 0.0469827265022342, 0.053071451295281444, 0.05834759746582313, 
												0.0624343492097772, 0.06502246303378822, 0.0659090914147332};
#elif KERNEL_SIZE == 65
static const float KERNEL[KERNEL_RADIUS + 1] = {0.000958, 0.001192, 0.001473, 0.001808, 0.002203, 0.002666, 0.003204, 0.003825, 0.004534, 0.005337, 0.006239,
												0.007243, 0.00835, 0.009561, 0.010871, 0.012274, 0.013764, 0.015327, 0.01695, 0.018614, 0.020302, 0.021988,
												0.023651, 0.025262, 0.026798, 0.028229, 0.029532, 0.030681, 0.031655, 0.032433, 0.033001, 0.033346, 0.033462};
#endif

uint ArrayIndex(const uint2 accessCoord, const uint2 arrayDimensions)
{
	return (arrayDimensions.x * accessCoord.y) + accessCoord.x;
}

//functions for packing/unpacking a float3 into a single uint (10 bits of precision)
uint Float3ToUint(float3 fValue)
{
	fValue = saturate(fValue);
	uint uValue = 0;
	uValue = uint(fValue[0] * 1023);
	uValue = uint(fValue[1] * 1023) | (uValue << 10);
	uValue = uint(fValue[2] * 1023) | (uValue << 10);
	return uValue;
}

float3 UintToFloat3(uint uValue)
{
	float3 fValue;
	fValue[0] = float(uValue >> 20) / 1023;
	fValue[1] = float((uValue & 0x000FFC00) >> 10) / 1023;
	fValue[2] = float((uValue & 0x000003FF)) / 1023;
	return fValue;
}

groupshared uint samples[ARRAY_DIMENSIONS.x*ARRAY_DIMENSIONS.y];

//The current iteration of the convolution, multiplies the samples by the correct kernel value, and then adds it to the cumulative
//output value
void KernelIteration(uint iteration, float3 currentSamples[SAMPLES_PER_THREAD], inout float3 kernelOutput[SAMPLES_PER_THREAD])
{
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD; i++)
	{
		kernelOutput[i] += KERNEL[iteration] * currentSamples[i];
	}
}

//Updates the current samples by moving the array index and only accessing 1 new value from shared memory per thread
void UpdateCurrentSamples(int2 indexCoords, inout float3 currentSamples[SAMPLES_PER_THREAD])
{
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD - 1; i++)
	{
		currentSamples[i] = currentSamples[i + 1];
	}
	currentSamples[SAMPLES_PER_THREAD - 1] = UintToFloat3(samples[ArrayIndex(indexCoords, ARRAY_DIMENSIONS)]);
}

void HorizontalPassCS(uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
	float3 kernelOutput[SAMPLES_PER_THREAD];
	float3 currentSamples[SAMPLES_PER_THREAD];
	int2 indexCoords;
	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	indexCoords.y = gtid.y;
	int2 sampleOffset = indexCoords;
	sampleOffset.x -= KERNEL_RADIUS - SAMPLES_PER_THREAD;
	int2 groupCoord = int2(gid.x * SAMPLES_PER_THREAD * GROUP_WIDTH, gid.y * GROUP_HEIGHT);
	
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD; i++)
	{
		uint arrayIndex = ArrayIndex(int2(indexCoords.x + i, indexCoords.y), ARRAY_DIMENSIONS);
		int2 sampleCoord = sampleOffset + groupCoord;
		sampleCoord.x += i;
		sampleCoord = clamp(sampleCoord, 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT));
		currentSamples[i] = tex2Dfetch(sBackBuffer, sampleCoord).rgb;
		samples[arrayIndex] = Float3ToUint(currentSamples[i]);
	}
	
	indexCoords.x = SAMPLES_PER_THREAD * GROUP_WIDTH + gtid.x;
	
	
	//The extra samples needed for padding the kernel
	[unroll]
	while(indexCoords.x < ARRAY_DIMENSIONS.x)
	{
		sampleOffset.x = indexCoords.x - KERNEL_RADIUS + SAMPLES_PER_THREAD;
		int2 sampleCoord = sampleOffset + groupCoord;
		sampleCoord = clamp(sampleCoord, 0, float2(BUFFER_WIDTH, BUFFER_HEIGHT));
		uint arrayIndex = ArrayIndex(indexCoords, ARRAY_DIMENSIONS);
		samples[arrayIndex] = Float3ToUint(tex2Dfetch(sBackBuffer, sampleCoord).rgb);
		indexCoords.x += GROUP_WIDTH;
	}
	
	barrier();
	

	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	
	//left side and center of the kernel
	[unroll]
	for(int i = 0; i <= KERNEL_RADIUS; i++)
	{
		KernelIteration(i, currentSamples, kernelOutput);
		UpdateCurrentSamples(indexCoords, currentSamples);
		indexCoords.x++;
	}
	
	//right side of the kernel
	[unroll]
	for(int i = KERNEL_RADIUS - 1; i >= 0; i--)
	{
		KernelIteration(i, currentSamples, kernelOutput);
		UpdateCurrentSamples(indexCoords, currentSamples);
		indexCoords.x++;
	}
	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	int2 coord = indexCoords + groupCoord;
	//coord.x -= SAMPLES_PER_THREAD;
	//store the horizontal to a placeholder texture
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD; i++)
	{
		tex2Dstore(wPlaceHolder, coord, float4(kernelOutput[i], 1));
		coord.x++;
	}
}

void VerticalPassCS(uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
	float3 kernelOutput[SAMPLES_PER_THREAD];
	float3 currentSamples[SAMPLES_PER_THREAD];
	int2 indexCoords;
	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	indexCoords.y = gtid.y;
	int2 sampleOffset = indexCoords;
	sampleOffset.x -= KERNEL_RADIUS - SAMPLES_PER_THREAD;
	int2 groupCoord = int2(gid.x * SAMPLES_PER_THREAD * GROUP_WIDTH, gid.y * GROUP_HEIGHT);
	
	//load the samples into shared memory
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD; i++)
	{
		uint arrayIndex = ArrayIndex(int2(indexCoords.x + i, indexCoords.y), ARRAY_DIMENSIONS);
		int2 sampleCoord = sampleOffset + groupCoord;
		sampleCoord.x += i;
		sampleCoord = clamp(sampleCoord, 0, float2(BUFFER_HEIGHT, BUFFER_WIDTH));
		currentSamples[i] = tex2Dfetch(sPlaceHolder, sampleCoord.yx).rgb;
		samples[arrayIndex] = Float3ToUint(currentSamples[i]);
	}
	
	indexCoords.x = SAMPLES_PER_THREAD * GROUP_WIDTH + gtid.x;
	
	
	//The extra samples needed for padding the kernel
	[unroll]
	while(indexCoords.x < ARRAY_DIMENSIONS.x)
	{
		sampleOffset.x = indexCoords.x - KERNEL_RADIUS + SAMPLES_PER_THREAD;
		int2 sampleCoord = sampleOffset + groupCoord;
		sampleCoord = clamp(sampleCoord, 0, float2(BUFFER_HEIGHT, BUFFER_WIDTH));
		uint arrayIndex = ArrayIndex(indexCoords, ARRAY_DIMENSIONS);
		samples[arrayIndex] = Float3ToUint(tex2Dfetch(sPlaceHolder, sampleCoord.yx).rgb);
		indexCoords.x += GROUP_WIDTH;
	}
	
	barrier();
	

	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	
	//Top side and center of the kernel
	[unroll]
	for(int i = 0; i <= KERNEL_RADIUS; i++)
	{
		KernelIteration(i, currentSamples, kernelOutput);
		UpdateCurrentSamples(indexCoords, currentSamples);
		indexCoords.x++;
	}
	
	//Bottom side of the kernel
	[unroll]
	for(int i = KERNEL_RADIUS - 1; i >= 0; i--)
	{
		KernelIteration(i, currentSamples, kernelOutput);
		UpdateCurrentSamples(indexCoords, currentSamples);
		indexCoords.x++;
	}
	
	indexCoords.x = (gtid.x * SAMPLES_PER_THREAD);
	int2 coord = indexCoords + groupCoord;
	//coord.x -= SAMPLES_PER_THREAD;
	[unroll]
	for(int i = 0; i < SAMPLES_PER_THREAD; i++)
	{
		tex2Dstore(wConvolution, coord.yx, float4(kernelOutput[i], 1));
		coord.x++;
	}
}

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

void OutputPS(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float4 color : SV_Target0)
{
	color = tex2D(sConvolution, texcoord);
	if(Strength < 1)
	{
		color = lerp(tex2D(sBackBuffer, texcoord), color, Strength);
		//color = (color - tex2D(sBackBuffer, texcoord)) + 0.5;
	}
}

technique GaussianBlurCS < ui_tooltip = "Use negative strength values to apply an unsharp mask to the image. \n\n"
										"The following are your options for the CS_GAUSSIAN_BLUR_SIZE definition: \n"
										"0 corresponds to a kernel size of 23\n"
										"1 corresponds to a kernel size of 33\n"
										"2 corresponds to a kernel size of 65\n";>
{
	pass
	{
		ComputeShader = HorizontalPassCS<GROUP_WIDTH, GROUP_HEIGHT>;
		DispatchSizeX = DIVIDE_ROUNDING_UP(BUFFER_WIDTH, GROUP_WIDTH * SAMPLES_PER_THREAD);
		DispatchSizeY = DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, GROUP_HEIGHT);
	}
	pass
	{
		ComputeShader = VerticalPassCS<GROUP_WIDTH, GROUP_HEIGHT>;
		DispatchSizeX = DIVIDE_ROUNDING_UP(BUFFER_HEIGHT, GROUP_WIDTH * SAMPLES_PER_THREAD);
		DispatchSizeY = DIVIDE_ROUNDING_UP(BUFFER_WIDTH, GROUP_HEIGHT);
	}
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = OutputPS;
	}
}
#endif //GAUSSIAN_COMPUTE
