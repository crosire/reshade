
/////////////////////////////////////////////////////D3D Starts Here/////////////////////////////////////////////////////////////////
#define pix float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)
static const float Pi = 3.1415926535;

texture BackBufferTex : COLOR;

sampler BackBuffer
	{
		Texture = BackBufferTex;
	};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float3 Out(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{

    float3 col;

    /*** Sobel kernels ***/
    // Note: GLSL's mat3 is COLUMN-major ->  mat3[col][row]
    float3 sobelX[3] = {float3(-1.0, -2.0, -1.0),
                       float3(0.0,  0.0, 0.0),
                       float3(1.0,  2.0,  1.0)};
    float3 sobelY[3] = {float3(-1.0,  0.0,  1.0),
                      float3( -2.0,  0.0, 2.0),
                       float3(-1.0,  0.0,  1.0)};

    float sumX = 0.0;	// x-axis change
    float sumY = 0.0;	// y-axis change

    for(int i = -1; i <= 1; i++)
    {
        for(int j = -1; j <= 1; j++)
        {
            // texture coordinates should be between 0.0 and 1.0
            float x = (texcoord.x + i * pix.x  );
    		float y =  (texcoord.y + j * pix.y );

            // Convolve kernels with image
            sumX += length(tex2D( BackBuffer, float2(x, y) ).xyz) * float(sobelX[1+i][1+j]);
            sumY += length(tex2D( BackBuffer, float2(x, y) ).xyz) * float(sobelY[1+i][1+j]);
        }
    }
float edge = sqrt(pow(sumX,2) + pow(sumY,2)) * 0.5;

float angle = atan2(sumX,sumY);

float R = edge * sin(angle);
float G = edge * sin(angle + 2 * Pi / 3.); // + 60°
float B = edge * sin(angle + 4 * Pi / 3.); // + 120°

    return float3(R,G,B);
}

///////////////////////////////////////////////////////////ReShade.fxh/////////////////////////////////////////////////////////////

// Vertex shader generating a triangle covering the entire screen
void PostProcessVS(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

//*Rendering passes*//

technique Sobel_Shader
{
			pass Sobel
		{
			VertexShader = PostProcessVS;
			PixelShader = Out;
		}
}
