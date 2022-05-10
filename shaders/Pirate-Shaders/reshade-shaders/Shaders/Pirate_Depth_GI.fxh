//===================================================================================================================
uniform float GI_DIFFUSE_RADIUS <
	ui_label = "GI - Radius";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 200.0;
	> = 1.0;
uniform float GI_DIFFUSE_STRENGTH <
	ui_label = "GI - Diffuse - Strength";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 30.0;
	> = 4.0;
#if (GI_VARIABLE_MIPLEVELS == 0)	
uniform int GI_DIFFUSE_MIPLEVEL <
	ui_label = "GI - Diffuse - Miplevel";
	ui_type = "drag";
	ui_min = 0; ui_max = DEPTH_AO_MIPLEVELS;
	> = 0;
#endif
uniform int GI_DIFFUSE_CURVE_MODE <
	ui_label = "GI - Diffuse - Curve Mode";
	ui_type = "combo";
	ui_items = "Linear\0Squared\0Log\0Sine\0Mid Range Sine\0";
	> = 4;
uniform int GI_DIFFUSE_BLEND_MODE <
	ui_label = "GI - Diffuse - Blend Mode";
	ui_type = "combo";
	ui_items = "Linear\0Screen\0Soft Light\0Color Dodge\0Hybrid\0";
	> = 2;
uniform float GI_REFLECT_RADIUS <
	ui_label = "GI - Reflection - Radius";
	ui_type = "drag";
	ui_min = 0.0; ui_max = 200.0;
	> = 1.0;
uniform int GI_DIFFUSE_DEBUG <
	ui_label = "GI - Debug";
	ui_type = "combo";
	ui_items = "None\0Color\0Gatherer\0";
	> = 0;
//===================================================================================================================
texture2D	TexColorLOD {Width = BUFFER_WIDTH * DEPTH_COLOR_TEXTUE_QUALITY; Height = BUFFER_HEIGHT * DEPTH_COLOR_TEXTUE_QUALITY; Format = RGBA8;}; //MipLevels = DEPTH_AO_MIPLEVELS;
sampler2D	SamplerColorLOD {Texture = TexColorLOD; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};

texture2D	TexGI {Width = BUFFER_WIDTH * GI_TEXTURE_QUALITY; Height = BUFFER_HEIGHT * GI_TEXTURE_QUALITY; Format = RGBA8;};
sampler2D	SamplerGI {Texture = TexGI; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = LINEAR; AddressU = Clamp; AddressV = Clamp;};
//===================================================================================================================
float4 GIDiffuse(float2 texcoord)
{
	float4 res;
	float4 pointnd = tex2D(SamplerND, texcoord);
	if (pointnd.w == 1.0) return 0.0;
	pointnd.xyz = (pointnd.xyz * 2.0) - 1.0;

	float3 pointvector = EyeVector(float3(texcoord, pointnd.w));

	#if (GI_VECTOR_MODE == 0)
	float2 randomvector = GetRandomVector(texcoord);
	#elif (GI_VECTOR_MODE == 1)
	float2 randomvector = GetRandomVector(texcoord * pointnd.w);
	#elif (GI_VECTOR_MODE == 2)
	float2 randomvector = GetRandomVector(texcoord * pointnd.w) * (0.5 + GetRandom(texcoord)/2);
	#endif

	float2 psize = lerp(GI_DIFFUSE_RADIUS, 1.0, pow(pointnd.w, 0.25)) * PixelSize;
	psize /= GI_TEXTURE_QUALITY;

	for(int p=1; p <= GI_DIFFUSE_PASSES; p++)
	{
		float2 coordmult = psize * p;
		#if GI_VARIABLE_MIPLEVELS
		int miplevel = round(smoothstep(1, GI_DIFFUSE_PASSES, p) * DEPTH_AO_MIPLEVELS);
		#endif
		
		for(int i=0; i < 4; i++)
		{
			randomvector = Rotate90(randomvector);
			float2 tapcoord = texcoord + randomvector * coordmult;
			#if GI_VARIABLE_MIPLEVELS
			float4 tapnd = tex2Dlod(SamplerND, float4(tapcoord, 0, miplevel));
			#else
			float4 tapnd = tex2Dlod(SamplerND, float4(tapcoord, 0, GI_DIFFUSE_MIPLEVEL));
			#endif
			tapnd.xyz = (tapnd.xyz * 2.0) - 1.0;
			if (tapnd.w == 1.0) continue;
			float3 tapvector = EyeVector(float3(tapcoord, tapnd.w));

			float3 pttvector = tapvector - pointvector;
				
			float weight = (1.0 - max(0.0, dot(pointnd.xyz, tapnd.xyz))); // How much normals are facing each other
			weight *= max(0.0, -dot(normalize(pttvector), tapnd.xyz)); // How much the normal is facing the center point
			weight *= saturate(coordmult.x - abs(pttvector.z)) / coordmult.x; // Z distance
			float3 coltap = tex2Dlod(SamplerColorLOD, float4(tapcoord, 0, 0)).rgb;
			res.rgb += coltap * weight;
			res.w += weight;
		}
		randomvector = Rotate45(randomvector);
	}

	res /= 4 * GI_DIFFUSE_PASSES;
	
	return res;
}
//===================================================================================================================
float4 PS_ColorLOD(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return tex2D(SamplerColor, texcoord);
}
float4 PS_GIDiffuse(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	return GIDiffuse(texcoord);
}
float4 PS_GICombine(float4 vpos : SV_Position, float2 texcoord : TEXCOORD) : COLOR
{
	float4 diffuse = tex2D(SamplerGI, texcoord);
	float4 res = tex2D(SamplerColor, texcoord);

	//if (GI_DIFFUSE_CURVE_MODE == 0) // Linear
		//Do Nothing
	if (GI_DIFFUSE_CURVE_MODE == 1) // Squared
		diffuse = pow(diffuse, 2);
	else if (GI_DIFFUSE_CURVE_MODE == 2) // Logarithm
		diffuse = log10(diffuse * 10.0);
	else if (GI_DIFFUSE_CURVE_MODE == 3) // Sine
		diffuse = (sin(threepitwo + diffuse * pi) + 1) / 2;
	else if (GI_DIFFUSE_CURVE_MODE == 4) // Mid range Sine
		diffuse = sin(diffuse * pi);
	diffuse = saturate(diffuse * GI_DIFFUSE_STRENGTH);

	if (GI_DIFFUSE_BLEND_MODE == 0) // Linear
		res.rgb += diffuse.rgb;
	else if (GI_DIFFUSE_BLEND_MODE == 1) // Screen
		res.rgb = BlendScreen(res.rgb, diffuse.rgb);
	else if (GI_DIFFUSE_BLEND_MODE == 2) // Soft Light
		res.rgb = BlendSoftLight(res.rgb, 0.5 + diffuse.rgb);
	else if (GI_DIFFUSE_BLEND_MODE == 3) // Color Dodge
		res.rgb = BlendColorDodge(res.rgb, diffuse.rgb);
	else // Hybrid based on point brightness
		res.rgb = lerp(res.rgb + diffuse.rgb, res.rgb * (1.0 + diffuse.rgb), dot(res.rgb, 0.3333));
	

	if (GI_DIFFUSE_DEBUG == 1)
		res.rgb = diffuse.rgb;
	else if (GI_DIFFUSE_DEBUG == 2)
		res.rgb = diffuse.w;


	res.w = 1.0;
	return res;
}
//===================================================================================================================
technique Pirate_GI
{
	pass ColorPre
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_ColorLOD;
		RenderTarget = TexColorLOD;
	}
	pass Diffuse
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_GIDiffuse;
		RenderTarget = TexGI;
	}
	pass GITest
	{
		VertexShader = VS_PostProcess;
		PixelShader  = PS_GICombine;
	}
}