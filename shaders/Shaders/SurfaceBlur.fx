
//Surface Blur by Ioxa
//Version 1.1 for ReShade 3.0
//Based on the  filter by mrharicot at https://www.shadertoy.com/view/4dfGDH

//Settings
#if !defined SurfaceBlurIterations
	#define SurfaceBlurIterations 1
#endif

#include "ReShadeUI.fxh"

uniform int BlurRadius
< __UNIFORM_SLIDER_INT1
	ui_min = 1; ui_max = 4;
	ui_tooltip = "1 = 3x3 mask, 2 = 5x5 mask, 3 = 7x7 mask, 4 = 9x9 mask. For more blurring add SurfaceBlurIterations=2 or SurfaceBlurIterations=3 to Preprocessor Definitions";
> = 1;

uniform float BlurOffset
< __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Additional adjustment for the blur radius. Values less than 1.00 will reduce the blur radius.";
> = 1.000;

uniform float BlurEdge
< __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.000; ui_max = 10.000;
	ui_tooltip = "Adjusts the strength of edge detection. Lower values will exclude finer edges from blurring";
> = 0.500;

uniform float BlurStrength
< __UNIFORM_SLIDER_FLOAT1
	ui_min = 0.00; ui_max = 1.00;
	ui_tooltip = "Adjusts the strength of the effect";
> = 1.00;

uniform int DebugMode
<
	ui_type = "combo";
	ui_items = "\None\0EdgeChannel\0BlurChannel\0";
	ui_tooltip = "Helpful for adjusting settings";
> = 0;

#include "ReShade.fxh"

#define sOffset1x ReShade::PixelSize.x
#define sOffset1y ReShade::PixelSize.y

#define sOffset2x 2.0*ReShade::PixelSize.x
#define sOffset2y 2.0*ReShade::PixelSize.y

#define sOffset3ax 1.3846153846*ReShade::PixelSize.x
#define sOffset3bx 3.2307692308*ReShade::PixelSize.x
#define sOffset3ay 1.3846153846*ReShade::PixelSize.y
#define sOffset3by 3.2307692308*ReShade::PixelSize.y

#define sOffset4ax 1.4584295168*ReShade::PixelSize.x
#define sOffset4bx 3.4039848067*ReShade::PixelSize.x
#define sOffset4cx 5.3518057801*ReShade::PixelSize.x
#define sOffset4ay 1.4584295168*ReShade::PixelSize.y
#define sOffset4by 3.4039848067*ReShade::PixelSize.y
#define sOffset4cy 5.3518057801*ReShade::PixelSize.y
	
#if SurfaceBlurIterations >= 2
	texture SurfaceBlurTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
	sampler SurfaceBlurSampler { Texture = SurfaceBlurTex;};
#endif

#if SurfaceBlurIterations >= 3
	texture SurfaceBlurTex2 { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
	sampler SurfaceBlurSampler2 { Texture = SurfaceBlurTex2;};
#endif

float4 SurfaceBlurFinal(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	#if SurfaceBlurIterations == 2 
		#define SurfaceBlurFinalSampler SurfaceBlurSampler
		
		float3 color = tex2D(SurfaceBlurFinalSampler, texcoord).rgb;
		float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;
	#elif SurfaceBlurIterations == 3
		#define SurfaceBlurFinalSampler SurfaceBlurSampler2
		
		float3 color = tex2D(SurfaceBlurFinalSampler, texcoord).rgb;
		float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;
	#else
		#define SurfaceBlurFinalSampler ReShade::BackBuffer
		
		float3 color = tex2D(SurfaceBlurFinalSampler, texcoord).rgb;
		float3 orig = color;
	#endif
	
	float Z;
	float3 final_color;
	
	if (BlurRadius == 1)
	{
		static const float sampleOffsetsX[5] = {  0.0, sOffset1x, 		   0, 	 sOffset1x,     sOffset1x};
		static const float sampleOffsetsY[5] = {  0.0,      	0, sOffset1y, 	 sOffset1y,    -sOffset1y};	
		static const float sampleWeights[5] = { 0.225806, 0.150538, 0.150538, 0.0430108, 0.0430108 };
		
		final_color = color * 0.225806;
		Z = 0.225806;
		
		[unroll]
		for(int i = 1; i < 5; ++i) {
			
			float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
			float3 colorA = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
			float3 diffA = (orig-colorA);
			float factorA = (dot(diffA,diffA));
			factorA = 1+(factorA/((BlurEdge)));
			factorA = sampleWeights[i]*rcp(factorA*factorA*factorA*factorA*factorA);
			
			float3 colorB = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
			float3 diffB = (orig-colorB);
			float factorB = (dot(diffB,diffB));
			factorB = 1+(factorB/((BlurEdge)));
			factorB = sampleWeights[i]*rcp(factorB*factorB*factorB*factorB*factorB);
			
			Z += factorA;
			final_color += factorA*colorA;
			Z += factorB;
			final_color += factorB*colorB;
		}
	}
	else
	{
		if (BlurRadius == 2)
		{
			static const float sampleOffsetsX[13] = {  0.0, 	   sOffset1x, 	  0, 	 sOffset1x,     sOffset1x,     sOffset2x,     0,     sOffset2x,     sOffset2x,     sOffset1x,    sOffset1x,     sOffset2x,     sOffset2x };
			static const float sampleOffsetsY[13] = {  0.0,     0, 	  sOffset1y, 	 sOffset1y,    -sOffset1y,     0,     sOffset2y,     sOffset1y,    -sOffset1y,     sOffset2y,     -sOffset2y,     sOffset2y,    -sOffset2y};
			static const float sampleWeights[13] = { 0.1509985387665926499, 0.1132489040749444874, 0.1132489040749444874, 0.0273989284225933369, 0.0273989284225933369, 0.0452995616018920668, 0.0452995616018920668, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0043838285270187332, 0.0043838285270187332 };
		
			final_color = color * 0.1509985387665926499;
			Z = 0.1509985387665926499;
		
			[unroll]
			for(int i = 1; i < 13; ++i) {
				
				float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
				float3 colorA = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
				float3 diffA = (orig-colorA);
				float factorA = dot(diffA,diffA);
				factorA = 1+(factorA/((BlurEdge)));
				factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
				float3 colorB = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
				float3 diffB = (orig-colorB);
				float factorB = dot(diffB,diffB);
				factorB = 1+(factorB/((BlurEdge)));
				factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
				Z += factorA;
				final_color += factorA*colorA;
				Z += factorB;
				final_color += factorB*colorB;
			}
		}
		else
		{
			if (BlurRadius == 3)
			{
				static const float sampleOffsetsX[13] = { 				  0.0, 			    sOffset3ax, 			 			  0, 	 		  sOffset3ax,     	   	 sOffset3ax,     		    sOffset3bx,     		  			  0,     		 sOffset3bx,     		   sOffset3bx,     		 sOffset3ax,    		   sOffset3ax,     		  sOffset3bx,     		  sOffset3bx };
				static const float sampleOffsetsY[13] = {  				  0.0,   					   0, 	  		   sOffset3ay, 	 		  sOffset3ay,     		-sOffset3ay,     					   0,     		   sOffset3by,     		 sOffset3ay,    		  -sOffset3ay,     		 sOffset3by,   		  -sOffset3by,     		  sOffset3by,    		     -sOffset3by };
				static const float sampleWeights[13] = { 0.0957733978977875942, 0.1333986613666725565, 0.1333986613666725565, 0.0421828199486419528, 0.0421828199486419528, 0.0296441469844336464, 0.0296441469844336464, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0020831022264565991,  0.0020831022264565991 };
		
				final_color = color * 0.0957733978977875942;
				Z = 0.0957733978977875942;
		
				[unroll]
				for(int i = 1; i < 13; ++i) {
					float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
					float3 colorA = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
					float3 diffA = (orig-colorA);
					float factorA = dot(diffA,diffA);
					factorA = 1+(factorA/((BlurEdge)));
					factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
					float3 colorB = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
					float3 diffB = (orig-colorB);
					float factorB = dot(diffB,diffB);
					factorB = 1+(factorB/((BlurEdge)));
					factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
					Z += factorA;
					final_color += factorA*colorA;
					Z += factorB;
					final_color += factorB*colorB;
				}
			}
			else
			{
				if (BlurRadius >= 4)
				{
					static const float sampleOffsetsX[25] = {0.0, sOffset4ax, 0, sOffset4ax, sOffset4ax, sOffset4bx, 0, sOffset4bx, sOffset4bx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, 0.0, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, sOffset4cx};
					static const float sampleOffsetsY[25] = {0.0, 0, sOffset4ay, sOffset4ay, -sOffset4ay, 0, sOffset4by, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4by, -sOffset4by, 0.0, sOffset4cy, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy};
					static const float sampleWeights[25] = {0.05299184990795840687999609498603, 0.09256069846035847440860469965371, 0.09256069846035847440860469965371, 0.02149960564023589832299078385165, 0.02149960564023589832299078385165, 0.05392678246987847562647201766774, 0.05392678246987847562647201766774, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.00729770438775005041467389567467, 0.00729770438775005041467389567467, 0.02038530184304811960185734706054,	0.02038530184304811960185734706054,	0.00473501127359426108157733854484,	0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799,	0.00473501127359426108157733854484, 0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799, 0.00104282525148620420024312363461, 0.00104282525148620420024312363461};
		
					final_color = color * 0.05299184990795840687999609498603;
					Z = 0.05299184990795840687999609498603;
		
					[unroll]
					for(int i = 1; i < 25; ++i) {
						float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
						float3 colorA = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
						float3 diffA = (orig-colorA);
						float factorA = dot(diffA,diffA);
						factorA = 1+(factorA/((BlurEdge)));
						factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
						float3 colorB = tex2Dlod(SurfaceBlurFinalSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
						float3 diffB = (orig-colorB);
						float factorB = dot(diffB,diffB);
						factorB = 1+(factorB/((BlurEdge)));
						factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
						Z += factorA;
						final_color += factorA*colorA;
						Z += factorB;
						final_color += factorB*colorB;
					}
				}	
			}
		}
	}		
	
if(DebugMode == 1)
{
	return float4(Z,Z,Z,0);
}

if(DebugMode == 2)
{
	return float4(final_color/Z,0);
}

	return float4(saturate(lerp(orig.rgb, final_color/Z, BlurStrength)),0.0);
}

#if SurfaceBlurIterations >= 2
float3 SurfaceBlur1(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{
	float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;

	float Z;
	float3 final_color;
	
	if (BlurRadius == 1)
	{
		static const float sampleOffsetsX[5] = {  0.0, sOffset1x, 		   0, 	 sOffset1x,     sOffset1x};
		static const float sampleOffsetsY[5] = {  0.0,      	0, sOffset1y, 	 sOffset1y,    -sOffset1y};	
		static const float sampleWeights[5] = { 0.225806, 0.150538, 0.150538, 0.0430108, 0.0430108 };
		
		final_color = orig * 0.225806;
		Z = 0.225806;
		
		[unroll]
		for(int i = 1; i < 5; ++i) {
			
			float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
			float3 colorA = tex2Dlod(ReShade::BackBuffer, float4(texcoord + coord, 0.0, 0.0)).rgb;
			float3 diffA = (orig-colorA);
			float factorA = dot(diffA,diffA);
			factorA = 1+(factorA/((BlurEdge)));
			factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
			float3 colorB = tex2Dlod(ReShade::BackBuffer, float4(texcoord - coord, 0.0, 0.0)).rgb;
			float3 diffB = (orig-colorB);
			float factorB = dot(diffB,diffB);
			factorB = 1+(factorB/((BlurEdge)));
			factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
			Z += factorA;
			final_color += factorA*colorA;
			Z += factorB;
			final_color += factorB*colorB;
		}
	}
	else
	{
		if (BlurRadius == 2)
		{
			static const float sampleOffsetsX[13] = {  0.0, 	   sOffset1x, 	  0, 	 sOffset1x,     sOffset1x,     sOffset2x,     0,     sOffset2x,     sOffset2x,     sOffset1x,    sOffset1x,     sOffset2x,     sOffset2x };
			static const float sampleOffsetsY[13] = {  0.0,     0, 	  sOffset1y, 	 sOffset1y,    -sOffset1y,     0,     sOffset2y,     sOffset1y,    -sOffset1y,     sOffset2y,     -sOffset2y,     sOffset2y,    -sOffset2y};
			static const float sampleWeights[13] = { 0.1509985387665926499, 0.1132489040749444874, 0.1132489040749444874, 0.0273989284225933369, 0.0273989284225933369, 0.0452995616018920668, 0.0452995616018920668, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0043838285270187332, 0.0043838285270187332 };
		
			final_color = orig * 0.1509985387665926499;
			Z = 0.1509985387665926499;
		
			[unroll]
			for(int i = 1; i < 13; ++i) {
				
				float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
				float3 colorA = tex2Dlod(ReShade::BackBuffer, float4(texcoord + coord, 0.0, 0.0)).rgb;
				float3 diffA = (orig-colorA);
				float factorA = dot(diffA,diffA);
				factorA = 1+(factorA/((BlurEdge)));
				factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
				float3 colorB = tex2Dlod(ReShade::BackBuffer, float4(texcoord - coord, 0.0, 0.0)).rgb;
				float3 diffB = (orig-colorB);
				float factorB = dot(diffB,diffB);
				factorB = 1+(factorB/((BlurEdge)));
				factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
				Z += factorA;
				final_color += factorA*colorA;
				Z += factorB;
				final_color += factorB*colorB;
			}
		}
		else
		{
			if (BlurRadius == 3)
			{
				static const float sampleOffsetsX[13] = { 				  0.0, 			    sOffset3ax, 			 			  0, 	 		  sOffset3ax,     	   	 sOffset3ax,     		    sOffset3bx,     		  			  0,     		 sOffset3bx,     		   sOffset3bx,     		 sOffset3ax,    		   sOffset3ax,     		  sOffset3bx,     		  sOffset3bx };
				static const float sampleOffsetsY[13] = {  				  0.0,   					   0, 	  		   sOffset3ay, 	 		  sOffset3ay,     		-sOffset3ay,     					   0,     		   sOffset3by,     		 sOffset3ay,    		  -sOffset3ay,     		 sOffset3by,   		  -sOffset3by,     		  sOffset3by,    		     -sOffset3by };
				static const float sampleWeights[13] = { 0.0957733978977875942, 0.1333986613666725565, 0.1333986613666725565, 0.0421828199486419528, 0.0421828199486419528, 0.0296441469844336464, 0.0296441469844336464, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0020831022264565991,  0.0020831022264565991 };
		
				final_color = orig * 0.0957733978977875942;
				Z = 0.0957733978977875942;
		
				[unroll]
				for(int i = 1; i < 13; ++i) {
					float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
					float3 colorA = tex2Dlod(ReShade::BackBuffer, float4(texcoord + coord, 0.0, 0.0)).rgb;
					float3 diffA = (orig-colorA);
					float factorA = dot(diffA,diffA);
					factorA = 1+(factorA/((BlurEdge)));
					factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
					float3 colorB = tex2Dlod(ReShade::BackBuffer, float4(texcoord - coord, 0.0, 0.0)).rgb;
					float3 diffB = (orig-colorB);
					float factorB = dot(diffB,diffB);
					factorB = 1+(factorB/((BlurEdge)));
					factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
					Z += factorA;
					final_color += factorA*colorA;
					Z += factorB;
					final_color += factorB*colorB;
				}
			}
			else
			{
				if (BlurRadius >= 4)
				{
					static const float sampleOffsetsX[25] = {0.0, sOffset4ax, 0, sOffset4ax, sOffset4ax, sOffset4bx, 0, sOffset4bx, sOffset4bx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, 0.0, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, sOffset4cx};
					static const float sampleOffsetsY[25] = {0.0, 0, sOffset4ay, sOffset4ay, -sOffset4ay, 0, sOffset4by, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4by, -sOffset4by, 0.0, sOffset4cy, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy};
					static const float sampleWeights[25] = {0.05299184990795840687999609498603, 0.09256069846035847440860469965371, 0.09256069846035847440860469965371, 0.02149960564023589832299078385165, 0.02149960564023589832299078385165, 0.05392678246987847562647201766774, 0.05392678246987847562647201766774, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.00729770438775005041467389567467, 0.00729770438775005041467389567467, 0.02038530184304811960185734706054,	0.02038530184304811960185734706054,	0.00473501127359426108157733854484,	0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799,	0.00473501127359426108157733854484, 0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799, 0.00104282525148620420024312363461, 0.00104282525148620420024312363461};
		
					final_color = orig * 0.05299184990795840687999609498603;
					Z = 0.05299184990795840687999609498603;
		
					[unroll]
					for(int i = 1; i < 25; ++i) {
						float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
						float3 colorA = tex2Dlod(ReShade::BackBuffer, float4(texcoord + coord, 0.0, 0.0)).rgb;
						float3 diffA = (orig-colorA);
						float factorA = dot(diffA,diffA);
						factorA = 1+(factorA/((BlurEdge)));
						factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
						float3 colorB = tex2Dlod(ReShade::BackBuffer, float4(texcoord - coord, 0.0, 0.0)).rgb;
						float3 diffB = (orig-colorB);
						float factorB = dot(diffB,diffB);
						factorB = 1+(factorB/((BlurEdge)));
						factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
						Z += factorA;
						final_color += factorA*colorA;
						Z += factorB;
						final_color += factorB*colorB;
					}
				}	
			}
		}
	}	
	
	return saturate(final_color/Z);
}
#endif

#if SurfaceBlurIterations >= 3
float3 SurfaceBlur2(in float4 pos : SV_Position, in float2 texcoord : TEXCOORD) : COLOR
{

	float3 color = tex2D(SurfaceBlurSampler, texcoord).rgb;
	float3 orig = tex2D(ReShade::BackBuffer, texcoord).rgb;
	
	float Z;
	float3 final_color;
	
	if (BlurRadius == 1)
	{
		static const float sampleOffsetsX[5] = {  0.0, sOffset1x, 		   0, 	 sOffset1x,     sOffset1x};
		static const float sampleOffsetsY[5] = {  0.0,      	0, sOffset1y, 	 sOffset1y,    -sOffset1y};	
		static const float sampleWeights[5] = { 0.225806, 0.150538, 0.150538, 0.0430108, 0.0430108 };
		
		final_color = color * 0.225806;
		Z = 0.225806;
		
		[unroll]
		for(int i = 1; i < 5; ++i) {
			
			float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
			float3 colorA = tex2Dlod(SurfaceBlurSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
			float3 diffA = (orig-colorA);
			float factorA = dot(diffA,diffA);
			factorA = 1+(factorA/((BlurEdge)));
			factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
			float3 colorB = tex2Dlod(SurfaceBlurSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
			float3 diffB = (orig-colorB);
			float factorB = dot(diffB,diffB);
			factorB = 1+(factorB/((BlurEdge)));
			factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
			Z += factorA;
			final_color += factorA*colorA;
			Z += factorB;
			final_color += factorB*colorB;
		}
	}
	else
	{
		if (BlurRadius == 2)
		{
			static const float sampleOffsetsX[13] = {  0.0, 	   sOffset1x, 	  0, 	 sOffset1x,     sOffset1x,     sOffset2x,     0,     sOffset2x,     sOffset2x,     sOffset1x,    sOffset1x,     sOffset2x,     sOffset2x };
			static const float sampleOffsetsY[13] = {  0.0,     0, 	  sOffset1y, 	 sOffset1y,    -sOffset1y,     0,     sOffset2y,     sOffset1y,    -sOffset1y,     sOffset2y,     -sOffset2y,     sOffset2y,    -sOffset2y};
			static const float sampleWeights[13] = { 0.1509985387665926499, 0.1132489040749444874, 0.1132489040749444874, 0.0273989284225933369, 0.0273989284225933369, 0.0452995616018920668, 0.0452995616018920668, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0109595713409516066, 0.0043838285270187332, 0.0043838285270187332 };
		
			final_color = color * 0.1509985387665926499;
			Z = 0.1509985387665926499;
		
			[unroll]
			for(int i = 1; i < 13; ++i) {
				
				float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
				float3 colorA = tex2Dlod(SurfaceBlurSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
				float3 diffA = (orig-colorA);
				float factorA = dot(diffA,diffA);
				factorA = 1+(factorA/((BlurEdge)));
				factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
				float3 colorB = tex2Dlod(SurfaceBlurSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
				float3 diffB = (orig-colorB);
				float factorB = dot(diffB,diffB);
				factorB = 1+(factorB/((BlurEdge)));
				factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
				Z += factorA;
				final_color += factorA*colorA;
				Z += factorB;
				final_color += factorB*colorB;
			}
		}
		else
		{
			if (BlurRadius == 3)
			{
				static const float sampleOffsetsX[13] = { 				  0.0, 			    sOffset3ax, 			 			  0, 	 		  sOffset3ax,     	   	 sOffset3ax,     		    sOffset3bx,     		  			  0,     		 sOffset3bx,     		   sOffset3bx,     		 sOffset3ax,    		   sOffset3ax,     		  sOffset3bx,     		  sOffset3bx };
				static const float sampleOffsetsY[13] = {  				  0.0,   					   0, 	  		   sOffset3ay, 	 		  sOffset3ay,     		-sOffset3ay,     					   0,     		   sOffset3by,     		 sOffset3ay,    		  -sOffset3ay,     		 sOffset3by,   		  -sOffset3by,     		  sOffset3by,    		     -sOffset3by };
				static const float sampleWeights[13] = { 0.0957733978977875942, 0.1333986613666725565, 0.1333986613666725565, 0.0421828199486419528, 0.0421828199486419528, 0.0296441469844336464, 0.0296441469844336464, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0093739599979617454, 0.0020831022264565991,  0.0020831022264565991 };
		
				final_color = color * 0.0957733978977875942;
				Z = 0.0957733978977875942;
		
				[unroll]
				for(int i = 1; i < 13; ++i) {
					float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
					float3 colorA = tex2Dlod(SurfaceBlurSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
					float3 diffA = (orig-colorA);
					float factorA = dot(diffA,diffA);
					factorA = 1+(factorA/((BlurEdge)));
					factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
					float3 colorB = tex2Dlod(SurfaceBlurSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
					float3 diffB = (orig-colorB);
					float factorB = dot(diffB,diffB);
					factorB = 1+(factorB/((BlurEdge)));
					factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
					Z += factorA;
					final_color += factorA*colorA;
					Z += factorB;
					final_color += factorB*colorB;
				}
			}
			else
			{
				if (BlurRadius >= 4)
				{
					static const float sampleOffsetsX[25] = {0.0, sOffset4ax, 0, sOffset4ax, sOffset4ax, sOffset4bx, 0, sOffset4bx, sOffset4bx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, 0.0, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4cx, sOffset4ax, sOffset4ax, sOffset4bx, sOffset4bx, sOffset4cx, sOffset4cx};
					static const float sampleOffsetsY[25] = {0.0, 0, sOffset4ay, sOffset4ay, -sOffset4ay, 0, sOffset4by, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4by, -sOffset4by, 0.0, sOffset4cy, sOffset4ay, -sOffset4ay, sOffset4by, -sOffset4by, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy, sOffset4cy, -sOffset4cy};
					static const float sampleWeights[25] = {0.05299184990795840687999609498603, 0.09256069846035847440860469965371, 0.09256069846035847440860469965371, 0.02149960564023589832299078385165, 0.02149960564023589832299078385165, 0.05392678246987847562647201766774, 0.05392678246987847562647201766774, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.01252588384627371007425549277902, 0.00729770438775005041467389567467, 0.00729770438775005041467389567467, 0.02038530184304811960185734706054,	0.02038530184304811960185734706054,	0.00473501127359426108157733854484,	0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799,	0.00473501127359426108157733854484, 0.00473501127359426108157733854484,	0.00275866461027743062478492361799,	0.00275866461027743062478492361799, 0.00104282525148620420024312363461, 0.00104282525148620420024312363461};
		
					final_color = color * 0.05299184990795840687999609498603;
					Z = 0.05299184990795840687999609498603;
		
					[unroll]
					for(int i = 1; i < 25; ++i) {
						float2 coord = float2(sampleOffsetsX[i], sampleOffsetsY[i]) * BlurOffset;
			
						float3 colorA = tex2Dlod(SurfaceBlurSampler, float4(texcoord + coord, 0.0, 0.0)).rgb;
						float3 diffA = (orig-colorA);
						float factorA = dot(diffA,diffA);
						factorA = 1+(factorA/((BlurEdge)));
						factorA = (sampleWeights[i]/(factorA*factorA*factorA*factorA*factorA));
			
						float3 colorB = tex2Dlod(SurfaceBlurSampler, float4(texcoord - coord, 0.0, 0.0)).rgb;
						float3 diffB = (orig-colorB);
						float factorB = dot(diffB,diffB);
						factorB = 1+(factorB/((BlurEdge)));
						factorB = (sampleWeights[i]/(factorB*factorB*factorB*factorB*factorB));
			
						Z += factorA;
						final_color += factorA*colorA;
						Z += factorB;
						final_color += factorB*colorB;
					}
				}	
			}
		}
	}	
	
	return saturate(final_color/Z);
}
#endif

technique SurfaceBlur
{
#if SurfaceBlurIterations >= 2
	pass Blur1
	{
		VertexShader = PostProcessVS;
		PixelShader = SurfaceBlur1;
		RenderTarget = SurfaceBlurTex;
	}
#endif 

#if SurfaceBlurIterations >= 3
	pass Blur2
	{
		VertexShader = PostProcessVS;
		PixelShader = SurfaceBlur2;
		RenderTarget = SurfaceBlurTex2;
	}
#endif
	
	pass BlurFinal
	{
		VertexShader = PostProcessVS;
		PixelShader = SurfaceBlurFinal;
	}

}
