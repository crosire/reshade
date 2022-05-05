#define RENDER_SIZE_X BUFFER_WIDTH
#define RENDER_SIZE_Y BUFFER_HEIGHT

#define INV_RENDER_SIZE_X ( 1.0 / RENDER_SIZE_X )
#define INV_RENDER_SIZE_Y ( 1.0 / RENDER_SIZE_Y )

// render targets & textures
texture target0
{
    Width = RENDER_SIZE_X;
    Height = RENDER_SIZE_Y;
    MipLevels = 1;
	Format = RGBA32F;
};
sampler samplerTarget0 { Texture = target0; };

texture target1
{
    Width = RENDER_SIZE_X;
    Height = RENDER_SIZE_Y;
    MipLevels = 1;
	Format = RGBA32F;
};
sampler samplerTarget1 { Texture = target1; };

texture pixel_mask < source = "pixelmask_960x480.png"; >
{
	Width = 960;
	Height = 480;
	MipLevels = 1;
	
    MinFilter = POINT;
    MagFilter = POINT;
};
sampler sampler_pixel_mask { Texture = pixel_mask; AddressU = REPEAT; AddressV = REPEAT;};

texture tv_border < source = "TV_Border.png"; >
{
	Width = 1280;
	Height = 720;
	MipLevels = 1;

};
sampler sampler_tv_border { Texture = tv_border; };

#include "ReShade.fxh"
#include "RetroTV.fxh"

// just samples and returns backbuffer
float4 BlitCopyScreenPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	return tex2D(ReShade::BackBuffer, texcoord).rgba;
}

// samples and returns target1
float4 BlitCopyTargetPS(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	return tex2D(samplerTarget1, texcoord).rgba;
}

// Simulates using a composite NTSC cable
technique Composite
{
	// first pass: blit to target0
	pass p0
	{
		RenderTarget = target0;
		
		VertexShader = PostProcessVS;
		PixelShader = BlitCopyScreenPS;
	}
	
	// second pass: encode composite signal
	pass p1
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = CompositeEncodePS;
	}
	
	// third pass: decode composite signal
	pass p2
	{
		RenderTarget = target0;
		
		VertexShader = RetroTV_VS;
		PixelShader = CompositeDecodePS;
	}
	
	// fourth pass: final composite pass
	pass p3
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = CompositeFinalPS;
	}
	
	// final pass: blit to screen, add CRT curvature
	pass p4
	{
		VertexShader = PostProcessVS;
		PixelShader = TVCurvaturePS;
	}
}

// Simulates using an RF cable
technique RF
{
	// first pass: blit to target0
	pass p0
	{
		RenderTarget = target0;
		
		VertexShader = PostProcessVS;
		PixelShader = BlitCopyScreenPS;
	}
	
	// second pass: encode composite signal, add RF noise
	pass p1
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = RFEncodePS;
	}
	
	// third pass: decode composite signal
	pass p2
	{
		RenderTarget = target0;
		
		VertexShader = RetroTV_VS;
		PixelShader = RFDecodePS;
	}
	
	// fourth pass: final RF pass, adds noise to signal
	pass p3
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = CompositeFinalPS;
	}
	
	// final pass: blit to screen, add CRT curvature
	pass p4
	{
		VertexShader = PostProcessVS;
		PixelShader = TVCurvaturePS;
	}
}

// Simulates using an S-Video cable
technique SVideo
{
	// first pass: blit to target0
	pass p0
	{
		RenderTarget = target0;
		
		VertexShader = PostProcessVS;
		PixelShader = BlitCopyScreenPS;
	}
	
	// second pass: encode s-video signal
	pass p1
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = SVideoEncodePS;
	}
	
	// third pass: decode composite signal
	pass p2
	{
		RenderTarget = target0;
		
		VertexShader = RetroTV_VS;
		PixelShader = SVideoDecodePS;
	}
	
	// fourth pass: final RF pass, adds noise to signal
	pass p3
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = SVideoFinalPS;
	}
	
	// final pass: blit to screen, add CRT curvature
	pass p4
	{
		VertexShader = PostProcessVS;
		PixelShader = TVCurvaturePS;
	}
}

technique VGA
{	
	// second pass: return rgb signal
	pass p1
	{
		RenderTarget = target1;
		
		VertexShader = RetroTV_VS;
		PixelShader = VGAFinalPS;
	}
	// final pass: blit to screen, add CRT curvature
	pass p2
	{
		VertexShader = PostProcessVS;
		PixelShader = TVCurvaturePS;
	}
}