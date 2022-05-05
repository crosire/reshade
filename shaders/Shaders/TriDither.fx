////////////////////////////////////////////////////////////
// Triangular Dither                                      //
// By The Sandvich Maker                                  //
// Ported to ReShade by TreyM                             //
////////////////////////////////////////////////////////////

#include "ReShade.fxh"

uniform float Timer < source = "timer"; >;
#define remap(v, a, b) (((v) - (a)) / ((b) - (a)))

// FUNCTIONS /////////////////////////////////////
    float rand21(float2 uv)
    {
        float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
        return (noise.x + noise.y) * 0.5;
    }
    float rand11(float x) { return frac(x * 0.024390243); }
    float permute(float x) { return ((34.0 * x + 1.0) * x) % 289.0; }

    float3 triDither(float3 color, float2 uv, float timer)
    {
        static const float bitstep = pow(2.0, 8) - 1.0;
        static const float lsb = 1.0 / bitstep;
        static const float lobit = 0.5 / bitstep;
        static const float hibit = (bitstep - 0.5) / bitstep;

        float3 m = float3(uv, rand21(uv + timer)) + 1.0;
        float h = permute(permute(permute(m.x) + m.y) + m.z);

        float3 noise1, noise2;
        noise1.x = rand11(h); h = permute(h);
        noise2.x = rand11(h); h = permute(h);
        noise1.y = rand11(h); h = permute(h);
        noise2.y = rand11(h); h = permute(h);
        noise1.z = rand11(h); h = permute(h);
        noise2.z = rand11(h);

        float3 lo = saturate(remap(color.xyz, 0.0, lobit));
        float3 hi = saturate(remap(color.xyz, 1.0, hibit));
        float3 uni = noise1 - 0.5;
        float3 tri = noise1 - noise2;
   	 return lerp(uni, tri, min(lo, hi)) * lsb;
    }

// SHADER ////////////////////////////////////////
    float3 PS_TriDither(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;

        // Blend
        color.rgb += triDither(color.rgb, texcoord, Timer.x);

        // Output
        return color;
    }

// TECHNIQUE /////////////////////////////////////
    technique TriDither
    {
    	pass
    	{
    		VertexShader = PostProcessVS;
    		PixelShader = PS_TriDither;
    	}
    }
