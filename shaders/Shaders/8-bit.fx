   /*-----------------------------------------------------------.
  /                        Config Settings                      /
  '-----------------------------------------------------------*/
  //Adjust these values as you like:
  #define palettes 7.0 //How many different shades of each color channel does the palette allow?
  #define brightnessAdjust 1.2 //Value to adjust Brightness back to normal after paletting

  #define pixelSize 6.0 //Adjust as you like
   /*-----------------------------------------------------------.
  /                        Shader Code                          /
  '-----------------------------------------------------------*/



float3 CustomPass(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
  
  float2 pixelTex = texcoord * 1.0;

  pixelTex.x = floor((texcoord.x * 1600) / pixelSize) * pixelSize / 1600;
  pixelTex.y = floor((texcoord.y * 900) / pixelSize) * pixelSize / 900;

  float3 colorInput = tex2D(s0, pixelTex).rgb;

  colorInput *= brightnessAdjust;

  colorInput.r = floor(colorInput.r * palettes) / palettes;
  colorInput.g = floor(colorInput.g * palettes) / palettes;
  colorInput.b = floor(colorInput.b * palettes) / palettes;


  return saturate(colorInput);
}