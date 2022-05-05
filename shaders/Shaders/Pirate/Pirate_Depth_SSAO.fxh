//===================================================================================================================
// AO Specific settings
uniform float DEPTH_AO_RADIUS <
	ui_label = "AO - Radius";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 200.0;
	> = 50.0;

uniform int DEPTH_AO_CURVE_MODE <
	ui_label = "AO - Curve Mode";
	ui_type = "combo";
	ui_items = "Linear\0Squared\0Log\0Sine\0";
	> = 1;
uniform float DEPTH_AO_CULL_HALO <
	ui_label = "AO - Cull Halo";
	ui_tooltip = "Try to keep as close to 0.0 as possible, only lift this up if there are bright lines around objects that have occlusion behind them.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.0;
uniform float DEPTH_AO_STRENGTH <
	ui_label = "AO - Strength";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 30.0;
	> = 5.0;
uniform int DEPTH_AO_BLEND_MODE <
	ui_label = "AO - Blend Mode";
	ui_type = "combo";
	ui_items = "Subtract\0Multiply\0Color burn\0";
	> = 1;		
// Distance culling and farplane specific settings
uniform float DEPTH_AO_MANUAL_NEAR <
	ui_label = "AO - Manual Z Depth - Near";
	ui_tooltip = "Increase this if nearby objects are casting black halos on far away objects.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 20.0;
	> = 1.0;
uniform float DEPTH_AO_MANUAL_FAR <
	ui_label = "AO - Manual Z Depth - Far";
	ui_tooltip = "Same as above, but for far away objects.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1000.0;
	> = 500.0;
uniform float DEPTH_AO_MANUAL_CURVE <
	ui_label = "AO - Manual Z Depth - Curve";
	ui_tooltip = "Controls the curve between near and far.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 1.0;
// Alchemy specific settings
uniform float DEPTH_AO_ALCHEMY_ANGLE <
	ui_label = "AO - Alchemy - Angle";
	ui_tooltip = "These two values are a pain to set, have to fiddle with really tiny values to get it to look good.";
	> = -0.0003;
uniform float DEPTH_AO_ALCHEMY_DISTANCE <
	ui_label = "AO - Alchemy - Distance";
	ui_tooltip = "Obviously the above deals with angle, this one with distance.";
	> = 0.00002;
uniform float DEPTH_AO_ALCHEMY_STRENGTH <
	ui_label = "AO - Alchemy - Strength";
	ui_tooltip = "Strength of the alchemy gatherer, very small values. Leave AO Strength at 1.0 if you're using alchemy and control it here.";
	> = 0.005;
// AO/GI Blur
uniform float DEPTH_AO_BLUR_RADIUS <
	ui_label = "AO - Blur - Radius";
	ui_tooltip = "Radius of the blur.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	> = 1.0;
uniform float DEPTH_AO_BLUR_NOISE <
	ui_label = "AO - Blur - Noise";
	ui_tooltip = "Controls how much noise should remain after the blur.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.0;	
uniform float DEPTH_AO_BLUR_WEIGHT <
	ui_label = "AO - Blur - Directional Weight";
	ui_tooltip = "When blur is set to weighted, it controls how much normals matter for the blur.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 1.0;	

// Quality and Debug stuff
uniform int DEPTH_AO_DEBUG <
	ui_label = "AO - Debug - AO/IL/Scatter";
	ui_type = "combo";
	ui_items = "None\0AO\0";
	> = 0;
uniform float DEPTH_AO_DEBUG_IMAGE <
	ui_label = "AO - Debug - Image";
	ui_tooltip = "In debug mode, the higher this value the more the background is shown.";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	> = 0.1;
//===================================================================================================================
texture2D	TexAO {Width = BUFFER_WIDTH * DEPTH_AO_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DEPTH_AO_TEXTURE_QUALITY; Format = R8;};
sampler2D	SamplerAO {Texture = TexAO; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
texture2D	TexAO2 {Width = BUFFER_WIDTH * DEPTH_AO_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * DEPTH_AO_TEXTURE_QUALITY; Format = R8;};
sampler2D	SamplerAO2 {Texture = TexAO2; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
//===================================================================================================================
float GetRandom(float2 co){ // From http://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
	return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}
float GetRandomT(float2 co){
	return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453 + Timer * 0.0002);
}
float2 RandomRotate(float2 coords) {
	return normalize(float2(coords.x * sin(GetRandom(coords.y)), coords.y * cos(GetRandom(coords.x))));
}
float2 Rotate45(float2 coords) {
	#define sincos45 0.70710678118
	float x = coords.x * sincos45;
	float y = coords.y * sincos45;
	return float2(x - y, x + y);
}
float2 Rotate90(float2 coords){
	return float2(-coords.y, coords.x);
}
float2 GetRandomVector(float2 coords) {
	return normalize(float2(GetRandom(coords)*2-1, GetRandom(1.42 * coords)*2-1));
}

// Cumulative method, mainly based on http://john-chapman-graphics.blogspot.com.br/2013/01/ssao-tutorial.html
// But instead of making a hemisphere of points, I first check if my points are on the right side of the
// sphere, then check if the distance of that point to the centre of the hemisphere is less than the maximum radius.
//
// Alternatively, there is the alchemy gatherer, based on http://graphics.cs.williams.edu/papers/AlchemyHPG11/VV11AlchemyAO.pdf
// As both methods are really similar.
//
// Might do an horizon method later, but that would require pretty much a full rewrite and I think the noise from
// gatherers is more pleasing.
float GetAO(float2 coords)
{
	float4 pointnd = tex2D(SamplerND, coords);
	[branch] if (pointnd.w > DEPTH_AO_FADE_END) discard; // Bye bye pixels
	pointnd.xyz = (pointnd.xyz * 2.0) - 1.0;
	//float3 pointeyevector = float3((coords * 2.0 - 1.0) * pointnd.w, pointnd.w);
	float3 pointeyevector = EyeVector(float3(coords, pointnd.w));
	
	float occlusion;
	float fade = smoothstep(DEPTH_AO_FADE_END - DEPTH_AO_FADE_START, 0.0, pointnd.w - DEPTH_AO_FADE_START);
	float zoomfactor = pow((1.0 - pointnd.w) * fade, 0.5); // Not being used right now. Could use later to compress taps that are far in the distance.
	zoomfactor = max(zoomfactor, 0.5);
	float3 tapeyevector;
	
	#if DEPTH_AO_USE_MANUAL_RADIUS
	// I don't have a projection matrix available, that I know of. When you have to use FARPLANE you get non-linear
	// depth, so this is to guesstimate the radius of the distance culling hemisphere.
	// Might even be useful on linear depth, who knows. Depends how you set it up.
	const float averagepixel = (PixelSize.x + PixelSize.y) / 2;
	float hemiradius = lerp(averagepixel * DEPTH_AO_MANUAL_NEAR, averagepixel * DEPTH_AO_MANUAL_FAR, pow(pointnd.w, DEPTH_AO_MANUAL_CURVE));
	#else
	const float hemiradius = ((PixelSize.x + PixelSize.y) / 2) * DEPTH_AO_RADIUS * DEPTH_AO_MANUAL_CURVE;//DEPTH_AO_CULLING;
	#endif
	const float2 pixelradius = DEPTH_AO_RADIUS * PixelSize;
	
	#if DEPTH_AO_LOOP_FIX
	int depth_passes = DEPTH_AO_PASSES + DEPTH_AO_MIN_PASSES;
	#else
	int depth_passes = ceil(smoothstep(DEPTH_AO_FADE_END, 0.0, pointnd.w) * DEPTH_AO_PASSES) + DEPTH_AO_MIN_PASSES;
	#endif
	
	#if DEPTH_AO_TAP_MODE
	const float passdiv = 8 * float(depth_passes);
	#else
	const float passdiv = 4 * float(depth_passes);
	#endif
	
	float2 randomvector = GetRandomVector(coords);

	for(int p=0; p < depth_passes; p++)
	{
		int miplevel = floor(smoothstep(0.0, depth_passes, p) * DEPTH_AO_MIPLEVELS);
		
		#if DEPTH_AO_TAP_MODE
		for(int i=0; i < 8; i++)
		{
			randomvector = Rotate45(randomvector);
		#else
		for(int i=0; i < 4; i++)
		{
			randomvector = Rotate90(randomvector);
		#endif
			#if DEPTH_AO_USE_TIMED_NOISE
			float2 tapcoords = coords + pixelradius * randomvector * zoomfactor * (0.5 + GetRandomT(coords)) * (p + 1) / depth_passes;
			#else
			float2 tapcoords = coords + pixelradius * randomvector * zoomfactor * (p + 1) / depth_passes;
			#endif
			
			float4 tapnd = tex2Dlod(SamplerND, float4(tapcoords, 0, miplevel));
			tapnd.rgb = tapnd.rgb * 2.0 - 1.0;
			tapeyevector = EyeVector(float3(tapcoords, tapnd.w)); //float3(((tapcoords) * 2.0 - 1.0) * tapnd.w, tapnd.w);
			float3 tapvector = tapeyevector - pointeyevector;
			#if DEPTH_AO_USE_ALCHEMY
			// Alchemy is actually faster than the method I used, that's why I added this as an optional.
			// By not normalizing the tapvector and not sqrting the distance, their integral takes
			// less computation to give a very similar result.
			// The only bad side of alchemy is that its settings are very sensitive and a bitch to adjust.
			float gatherer = max(0.0, dot(pointnd.xyz, tapvector)) + DEPTH_AO_ALCHEMY_ANGLE;
			gatherer = gatherer / (dot(tapvector, tapvector) + DEPTH_AO_ALCHEMY_DISTANCE);
			#else
			// My method basically checks if the point is inside the hemisphere and weights it by distance.
			// Sadly it's causing some halos with my normal set up, so I used Marty's for this instead.
			// Figures out, he gathers the AO pretty much the same way in his MXAO.
			float distance = length(tapvector);
			float distculling = smoothstep(hemiradius, 0.0, distance);
			float gatherer = saturate(dot(pointnd.xyz, tapvector/distance)) * distculling; // (hemiradius > distance); //This also works.
			#endif

			occlusion += gatherer;
		}
	}
	
	#if DEPTH_AO_USE_ALCHEMY
	occlusion = (2 * DEPTH_AO_ALCHEMY_STRENGTH / passdiv) * occlusion;
	#else
	occlusion /= passdiv;
	#endif
	

	//if (DEPTH_AO_CURVE_MODE == 0) // Linear
		// Do Nothing
	if (DEPTH_AO_CURVE_MODE == 1) // Squared
		occlusion = pow(occlusion, 2);
	else if (DEPTH_AO_CURVE_MODE == 2) // Logarithm
		occlusion = log10(occlusion * 10.0);
	else if (DEPTH_AO_CURVE_MODE == 3) // Sine
		occlusion = (sin(threepitwo + occlusion * pi) + 1) / 2;
	occlusion = saturate(occlusion * DEPTH_AO_STRENGTH);


	return fade * occlusion;
}

/*float4 AOBlur(float2 coords)
{
	const float2 offset[8]=
	{
		float2(1.0, 1.0),
		float2(1.0, -1.0),
		float2(-1.0, 1.0),
		float2(-1.0, -1.0),
		float2(0.0, 1.0),
		float2(0.0, -1.0),
		float2(1.0, 0.0),
		float2(-1.0, 0.0)
	};
	
	float4 tap = tex2D(SamplerAO, coords);
	float4 ret;
	
	for(int p=0; p < DEPTH_AO_BLUR_TAPS; p++)
	{
		ret += tap;
		for(int i=0; i < 8; i++)
		{
			ret += tex2D(SamplerAO, coords + offset[i] * (p + 1) * (PixelSize / DEPTH_AO_TEXTURE_QUALITY) * DEPTH_AO_BLUR_RADIUS);
		}
	}
	
	ret /= 8 * DEPTH_AO_BLUR_TAPS;
	//ret = max(tap, ret); // This can be uncommented in order to keep the noise.
	ret = lerp(ret, tap, tap);
	return ret;
}*/
//===================================================================================================================
float PS_GenAO(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GetAO(texcoord);
}

float PS_BlurX(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	#if (DEPTH_AO_BLUR_MODE == 1) // Weighted
	float3 pointnd = tex2D(SamplerND, texcoord).xyz;
	pointnd = normalize(pointnd * 2.0 - 1.0);
	#elif (DEPTH_AO_BLUR_MODE == 2) // Directional
	float3 pointnd = tex2D(SamplerND, texcoord).xyz;
	pointnd = (pointnd * 2.0) - 1.0;
	float2 c = normalize(pointnd.xy);
	#endif

	if (dot(pointnd, pointnd) == 0) pointnd = float3(0.0, 0.0, 1.0);
	float ret = tex2D(SamplerAO, texcoord).x;
	float tap = ret;

	
	float w;
	
	for(int i=1; i <= DEPTH_AO_BLUR_TAPS; i++)
	{
		#if (DEPTH_AO_BLUR_MODE == 1) // Weighted
		float2 tapcoord = float2(i * PixelSize.x * DEPTH_AO_BLUR_RADIUS, 0.0);
		w += abs(dot(pointnd, (tex2D(SamplerND, texcoord + tapcoord).xyz * 2.0 - 1.0)));
		w += abs(dot(pointnd, (tex2D(SamplerND, texcoord - tapcoord).xyz * 2.0 - 1.0)));
		ret += tex2D(SamplerAO, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO, texcoord - tapcoord).x;
		#elif (DEPTH_AO_BLUR_MODE == 2) // Directional
		float2 tapcoord = float2(i * c * PixelSize * DEPTH_AO_BLUR_RADIUS);
		ret += tex2D(SamplerAO, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO, texcoord - tapcoord).x;
		#else
		float2 tapcoord = float2(i * PixelSize.x * DEPTH_AO_BLUR_RADIUS, 0.0);
		ret += tex2D(SamplerAO, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO, texcoord - tapcoord).x;
		#endif
	}
	
	ret /= DEPTH_AO_BLUR_TAPS * 2 + 1;

	w /= DEPTH_AO_BLUR_TAPS * 2 + 1;
	w = lerp(1.0, w, DEPTH_AO_BLUR_WEIGHT);
	ret = lerp(tap, ret, w);

	ret = lerp(ret, tap, tap * DEPTH_AO_BLUR_NOISE);
	return ret;
}

float PS_BlurY(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	#if (DEPTH_AO_BLUR_MODE == 1) // Weighted
	float3 pointnd = tex2D(SamplerND, texcoord).xyz;
	pointnd = (pointnd * 2.0) - 1.0;
	#elif (DEPTH_AO_BLUR_MODE == 2) // Directional
	float3 pointnd = tex2D(SamplerND, texcoord).xyz;
	pointnd = (pointnd * 2.0) - 1.0;
	float2 c = Rotate90(normalize(pointnd.xy));
	#endif
	
	float ret = tex2D(SamplerAO2, texcoord).x;
	float tap = ret;
	float w;
	
	for(int i=1; i <= DEPTH_AO_BLUR_TAPS; i++)
	{
		#if (DEPTH_AO_BLUR_MODE == 1) // Weighted
		float2 tapcoord = float2(0.0, i * PixelSize.y * DEPTH_AO_BLUR_RADIUS);
		w += abs(dot(pointnd, (tex2D(SamplerND, texcoord + tapcoord).xyz * 2.0 - 1.0)));
		w += abs(dot(pointnd, (tex2D(SamplerND, texcoord - tapcoord).xyz * 2.0 - 1.0)));
		ret += tex2D(SamplerAO2, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO2, texcoord - tapcoord).x;
		#elif (DEPTH_AO_BLUR_MODE == 2) // Directional
		float2 tapcoord = float2(i * c * PixelSize * DEPTH_AO_BLUR_RADIUS);
		ret += tex2D(SamplerAO2, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO2, texcoord - tapcoord).x;
		#else
		float2 tapcoord = float2(0.0, i * PixelSize.y * DEPTH_AO_BLUR_RADIUS);
		ret += tex2D(SamplerAO2, texcoord + tapcoord).x;
		ret += tex2D(SamplerAO2, texcoord - tapcoord).x;
		#endif
	}
	
	ret /= DEPTH_AO_BLUR_TAPS * 2 + 1;

	w /= DEPTH_AO_BLUR_TAPS * 2 + 1;
	w = lerp(1.0, w, DEPTH_AO_BLUR_WEIGHT);
	ret = lerp(tap, ret, w);
	
	ret = lerp(ret, tap, tap * DEPTH_AO_BLUR_NOISE);
	return ret;
}

float4 PS_AOCombine(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 ret = tex2D(SamplerColor, texcoord);
	
	if (DEPTH_DEBUG) {
		ret.rgb = tex2D(SamplerND, texcoord).w;
		ret.w = 1.0;
		return ret;
	}
	if (FOV_DEBUG) {
		ret.rgb = tex2D(SamplerND, texcoord).z;
		ret.w = 1.0;
		return ret;
	}

	float ao = max(0.0, tex2D(SamplerAO, texcoord).x - saturate(tex2D(SamplerND, texcoord).z * 2 - 1) * DEPTH_AO_CULL_HALO);

	if (DEPTH_AO_BLEND_MODE == 0) //Subtract
		ret.rgb = saturate(ret.rgb - ao);
	if (DEPTH_AO_BLEND_MODE == 1) //Multiply
		ret.rgb *= 1.0 - ao;
	if (DEPTH_AO_BLEND_MODE == 2) //Color burn
		ret.rgb = BlendColorBurn(ret.rgb, 1.0 - ao);

	if (DEPTH_AO_DEBUG == 1)
		ret.rgb = 0.75 * (1.0 - DEPTH_AO_DEBUG_IMAGE) + (ret.rgb * DEPTH_AO_DEBUG_IMAGE) - ao;

	//ret.rgb = saturate(tex2D(SamplerND, texcoord).z * 2 - 1);
	return saturate(ret);
}

//===================================================================================================================
technique Pirate_SSAO
{
	pass AOPre
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_GenAO;
		RenderTarget = TexAO;
	}
	#if DEPTH_AO_USE_BLUR
	pass BlurX
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_BlurX;
		RenderTarget = TexAO2;
	}
	pass BlurY
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_BlurY;
		RenderTarget = TexAO;
	}
	#endif
	pass AOFinal
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_AOCombine;
	}
}