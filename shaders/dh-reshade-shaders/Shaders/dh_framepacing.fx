#include "Reshade.fxh"

#define fExpectedFrametime (1000.0/iTargetFramerate)

namespace DH_FRAMEPACING {

//// uniform

	uniform int iTargetFramerate <
		ui_label = "Target Framerate";
		ui_type = "slider";
	    ui_min = 24;
	    ui_max = 300;
	    ui_step = 1;
	> = 60;
	
	uniform float frametime < source = "frametime"; >;
	uniform int framecount < source = "framecount"; >;

//// textures

	texture operationsPingTex { Width = 1; Height = 1; };
	sampler operationsPingSampler { Texture = operationsPingTex; };

	texture operationsPongTex { Width = 1; Height = 1; };
	sampler operationsPongSampler { Texture = operationsPongTex; };

//// Functions

	int getPreviousOperationCount() {
		float4 value;
		if(framecount%2==0) {
			value = tex2D(operationsPongSampler,0.5);
		} else {
			value = tex2D(operationsPingSampler,0.5);
		}
		int result = int(value.r*value.r*256*256)+value.g*256;
		return result==0 ? 1 : result;
	}

	float4 saveOpeationCount(float count) {
		float sq = sqrt(count);
		float r = count-floor(sq)*floor(sq);
		return float4(sq/256,r/256,0,0.1);
	}

//// PS


	void PS_Framepacing(float4 vpos : SV_Position, in float2 coords : TEXCOORD0, out float4 outPixel : SV_Target)
	{
		int previousOperationCount = getPreviousOperationCount();
		float frametimeRatio = fExpectedFrametime/frametime;
		int targetOperation = previousOperationCount*frametimeRatio;
		
		int count;
		for(int i=0;i<targetOperation*fExpectedFrametime;i++) {
			count++;
		}
		outPixel = saveOpeationCount(targetOperation);
	}

	void PS_FramepacingPing(float4 vpos : SV_Position, in float2 coords : TEXCOORD0, out float4 outPixel : SV_Target)
	{
		if(framecount%2==1) discard;
		PS_Framepacing(vpos,coords,outPixel);
	}

	void PS_FramepacingPong(float4 vpos : SV_Position, in float2 coords : TEXCOORD0, out float4 outPixel : SV_Target)
	{
		if(framecount%2==0) discard;
		PS_Framepacing(vpos,coords,outPixel);
	}



//// Techniques

	technique dh_framepacing <
	>
	{
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_FramepacingPing;
			RenderTarget = operationsPingTex;
		}
		pass
		{
			VertexShader = PostProcessVS;
			PixelShader = PS_FramepacingPong;
			RenderTarget = operationsPongTex;
		}
	}

}