#include "ReShade.fxh"

uniform int TrooPalette <
	ui_type = "combo";
	ui_items = "Doom\0Doom (PSX)\0Doom 64\0Hexen\0Heretic\0Cathy's Ray Tube";
	ui_label = "Palette [TrooCullers]";
> = 0;

uniform float TrooSaturation <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "SW Saturation [TrooCullers]";
	ui_tooltip = "Valid for 24-Bit version only.";
> = 100.0;

uniform float TrooGreyed <
	ui_type = "drag";
	ui_min = 0.0;
	ui_max = 100.0;
	ui_step = 1.0;
	ui_label = "Colormap Greys [TrooCullers]";
	ui_tooltip = "Valid for 24-Bit version only.";
> = 0.0;

texture tPaletteDoom <source="TrooCullers/Doom.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPaletteDoomPSX <source="TrooCullers/PSX.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPaletteDoom64 <source="TrooCullers/D64.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPaletteHexen <source="TrooCullers/Hexen.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPaletteHeretic <source="TrooCullers/Heretic.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPaletteCathy <source="TrooCullers/Cathy.png";> { Width=512; Height=512; Format = RGBA8;};
texture tPalette24 <source="TrooCullers/24BitBlend.png";> { Width=4096; Height=8192; Format = RGBA32F;};

sampler sPaletteDoom { 
    Texture = tPaletteDoom; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPaletteDoomPSX { 
    Texture = tPaletteDoomPSX; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPaletteDoom64 { 
    Texture = tPaletteDoom64; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPaletteHexen { 
    Texture = tPaletteHexen; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPaletteHeretic { 
    Texture = tPaletteHeretic; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPaletteCathy { 
    Texture = tPaletteCathy; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

sampler sPalette24 { 
    Texture = tPalette24; 
    AddressU = CLAMP; 
	AddressV = CLAMP; 
    MagFilter = LINEAR; 
	MinFilter = LINEAR;
};

//TrooPal.fp
float WeightedLum(float3 colour)
{
	colour *= colour;
	colour.r *= 0.299;
	colour.g *= 0.587;
	colour.b *= 0.114;
	return sqrt(colour.r + colour.g + colour.b);
}

float3 Tonemap(float3 color)
{
	int3 c = int3(clamp(color.rgb, float3(0.0,0.0,0.0), float3(1.0,1.0,1.0)) * 63.0 + 0.5);
	float index = (c.r * 64 + c.g) * 64 + c.b;
	int tx = index % 512;
	int ty = index / 512;
	if (TrooPalette == 0){
		return float3(tex2Dfetch(sPaletteDoom, int4(tx, ty,0,0)).rgb);
	} else if (TrooPalette == 1){
		return float3(tex2Dfetch(sPaletteDoomPSX, int4(tx, ty,0,0)).rgb);
	} else if (TrooPalette == 2){
		return float3(tex2Dfetch(sPaletteDoom64, int4(tx, ty,0,0)).rgb);
	} else if (TrooPalette == 4){
		return float3(tex2Dfetch(sPaletteHexen, int4(tx, ty,0,0)).rgb);
	} else if (TrooPalette == 5){
		return float3(tex2Dfetch(sPaletteHeretic, int4(tx, ty,0,0)).rgb);
	} else if (TrooPalette == 6){
		return float3(tex2Dfetch(sPaletteCathy, int4(tx, ty,0,0)).rgb);
	} else {
		return float3(tex2Dfetch(sPaletteDoom, int4(tx, ty,0,0)).rgb);
	}
}

void PS_TrooPal(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD0, out float4 col : COLOR0)
{
    float2 texcoord = uv * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	float3 base = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 blend = Tonemap(base);
	float3 RGB = blend / WeightedLum(blend) * WeightedLum(base);
	col = float4(RGB, 1.0);
}

//TrooGL.fp
float RGBToSat(float3 color)
{
	float sat;

	float fmin = min(min(color.r, color.g), color.b);    //Min. value of RGB
	float fmax = max(max(color.r, color.g), color.b);    //Max. value of RGB
	float delta = fmax - fmin;             //Delta RGB value

	if (delta == 0.0)		//This is a gray, no chroma...
	{
		sat = 0.0;	// Saturation
	}
	else                                    //Chromatic data...
	{
		float fsum = fmax + fmin;
	
		if (fsum * 0.5 < 0.5) // Luminosity below half
			sat = delta / fsum; // Saturation
		else
			sat = delta / (2.0 - fsum); // Saturation
	}
	return sat;
}

float3 RGBToHSL(float3 color)
{
  float3 hsl; // init to 0 to avoid warnings ? (and reverse if + remove first part)
   
   float fmin = min(min(color.r, color.g), color.b);
   float fmax = max(max(color.r, color.g), color.b);
   float delta = fmax - fmin;

   hsl.z = (fmax + fmin) / 2.0;

   if (delta == 0.0) //No chroma
   {
      hsl.x = 0.0;   // Hue
      hsl.y = 0.0;   // Saturation
   }
   else //Chromatic data
   {
      if (hsl.z < 0.5)
         hsl.y = delta / (fmax + fmin); // Saturation
      else
         hsl.y = delta / (2.0 - fmax - fmin); // Saturation
      
      float deltaR = (((fmax - color.r) / 6.0) + (delta / 2.0)) / delta;
      float deltaG = (((fmax - color.g) / 6.0) + (delta / 2.0)) / delta;
      float deltaB = (((fmax - color.b) / 6.0) + (delta / 2.0)) / delta;

      if (color.r == fmax )
         hsl.x = deltaB - deltaG; // Hue
      else if (color.g == fmax)
         hsl.x = (1.0 / 3.0) + deltaR - deltaB; // Hue
      else if (color.b == fmax)
         hsl.x = (2.0 / 3.0) + deltaG - deltaR; // Hue

      if (hsl.x < 0.0)
         hsl.x += 1.0; // Hue
      else if (hsl.x > 1.0)
         hsl.x -= 1.0; // Hue
   }

   return hsl;
}

float HueToRGB(float f1, float f2, float hue)
{
   if (hue < 0.0)
      hue += 1.0;
   else if (hue > 1.0)
      hue -= 1.0;
   float res;
   if ((6.0 * hue) < 1.0)
      res = f1 + (f2 - f1) * 6.0 * hue;
   else if ((2.0 * hue) < 1.0)
      res = f2;
   else if ((3.0 * hue) < 2.0)
      res = f1 + (f2 - f1) * ((2.0 / 3.0) - hue) * 6.0;
   else
      res = f1;
   return res;
}

float3 HSLToRGB(float3 hsl)
{
   float3 rgb;
   
   if (hsl.y == 0.0)
      rgb = float3(hsl.z, hsl.z, hsl.z); // Luminance
   else
   {
      float f2;
      
      if (hsl.z < 0.5)
         f2 = hsl.z * (1.0 + hsl.y);
      else
         f2 = (hsl.z + hsl.y) - (hsl.y * hsl.z);
         
      float f1 = 2.0 * hsl.z - f2;
      
      rgb.r = HueToRGB(f1, f2, hsl.x + (1.0/3.0));
      rgb.g = HueToRGB(f1, f2, hsl.x);
      rgb.b= HueToRGB(f1, f2, hsl.x - (1.0/3.0));
   }
   
   return rgb;
}

// Hue Blend mode creates the result color by combining the luminance and saturation of the base color with the hue of the blend color.
float3 BlendHue(float3 base, float3 blend)
{
	float3 blendHSL = RGBToHSL(blend);
	if (blendHSL.y == 0.0)
	{
		return base; //prevents grey from showing as red due to hue equalling zero
	}
	else
	{
		float3 RGB = HSLToRGB(float3(blendHSL.x, RGBToSat(base), blendHSL.z));
		return RGB / WeightedLum(RGB) * WeightedLum(base);
	}
}

void PS_TrooGL(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD0, out float4 col : COLOR0)
{
    float2 texcoord = uv * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	float3 base = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 blend = Tonemap(base);
	col = float4(BlendHue(base, blend), 1.0);
}

//TrooPalPat.fp
void PS_TrooPalPat(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD0, out float4 col : COLOR0)
{
    float2 texcoord = uv * ReShade::ScreenSize; //this is because the original shader uses OpenGL's fragCoord, which is in texels rather than pixels
	float3 base = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 blend = Tonemap(base);
	
	float fmin = min(min(blend.r, blend.g), blend.b);    //Min. value of RGB
	float fmax = max(max(blend.r, blend.g), blend.b);    //Max. value of RGB
	
	if (fmax - fmin == 0.0)		//This is a gray, no chroma...
	{
		col = float4(base, 1.0);
	}
	else
	{
		float3 RGB = blend / WeightedLum(blend) * WeightedLum(base);
		col = float4(RGB, 1.0);
	}
}

//Troo24Bit
float WeightedLum24(float3 colour)
{
	colour *= colour;
	colour.r *= 0.299;
	colour.g *= 0.587;
	colour.b *= 0.114;
	return sqrt(colour.r + colour.g + colour.b);
}

float3 Tonemap24(float3 color)
{
	float sat = TrooSaturation*0.01;
	float desat = (100.0-TrooSaturation)*0.01;

	int3 c = int3(clamp(color, float3(0.0,0.0,0.0), float3(1.0,1.0,1.0)) * 255.0 + 0.5);
	int index = (c.r * 256 + c.g) * 256 + c.b;
	int tx = index % 4096;
	int ty = int(index * 0.000244140625);
	
	float3 hueblend = tex2Dfetch(sPalette24, int4(tx, ty,0,0)).rgb * desat;
	float3 colourblend = tex2Dfetch(sPalette24, int4(tx, ty + 4096,0,0)).rgb * sat;
	
	return hueblend + colourblend;
}

void PS_Troo24Bit(in float pos: SV_POSITION, in float2 uv: TEXCOORD0, out float4 col : COLOR0)
{

	float greyed = TrooGreyed * 0.01;
	float degreyed = (100.0-TrooGreyed)*0.01;

	float3 base = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 blend = Tonemap24(base);
	float3 colour;
	
	float maxRGB = max(blend.r, max(blend.g, blend.b));
	float minRGB = min(blend.r, min(blend.g, blend.b));
	
	if (maxRGB - minRGB == 0.0)
	{
		colour = base * degreyed + (WeightedLum24(base) * greyed);
	}
	else
	{
		colour = blend / clamp(WeightedLum24(blend), 0.000001, 1.0) * WeightedLum24(base);
	}
	
	col = float4(colour, 1.0);
}

technique TrooCullersPal
{
   pass TrooPal {
	 VertexShader = PostProcessVS;
	 PixelShader = PS_TrooPal;
   }
}

technique TrooCullersGL
{
    pass TrooGL{
        VertexShader = PostProcessVS;
        PixelShader = PS_TrooGL;
    }
}

technique TrooCullersPalPat
{
    pass TrooPalPat
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_TrooPalPat;
    }
}

technique TrooCullers24Bit
{
	pass Troo24
	{
        VertexShader = PostProcessVS;
        PixelShader = PS_Troo24Bit;	
	}
}