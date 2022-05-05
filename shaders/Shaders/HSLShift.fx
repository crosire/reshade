////////////////////////////////////////////////////
// HSL Processing Shader                          //
// Author: kingeric1992                           //
////////////////////////////////////////////////////

#include "ReShade.fxh"

// UI ELEMENTS /////////////////////////////////////
////////////////////////////////////////////////////

#include "ReShadeUI.fxh"

  uniform float3 HUERed < __UNIFORM_COLOR_FLOAT3
      ui_label="Red";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Red Default Value:\n"
      "255 : R:  191, G:   64, B:   64\n"
      "0.00: R:0.750, G:0.250, B:0.250";
  > = float3(0.75, 0.25, 0.25);

  uniform float3 HUEOrange < __UNIFORM_COLOR_FLOAT3
      ui_label = "Orange";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Orange Default Value:\n"
      "255 : R:  191, G:  128, B:   64\n"
      "0.00: R:0.750, G:0.500, B:0.250";
  > = float3(0.75, 0.50, 0.25);

  uniform float3 HUEYellow < __UNIFORM_COLOR_FLOAT3
      ui_label = "Yellow";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Yellow Default Value:\n"
      "255 : R:  191, G:  191, B:   64\n"
      "0.00: R:0.750, G:0.750, B:0.250";
  > = float3(0.75, 0.75, 0.25);

  uniform float3 HUEGreen < __UNIFORM_COLOR_FLOAT3
      ui_label = "Green";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Green Default Value:\n"
      "255 : R:   64, G:  191, B:   64\n"
      "0.00: R:0.250, G:0.750, B:0.250";
  > = float3(0.25, 0.75, 0.25);

  uniform float3 HUECyan < __UNIFORM_COLOR_FLOAT3
      ui_label = "Cyan";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Cyan Default Value:\n"
      "255 : R:   64, G:  191, B:  191\n"
      "0.00: R:0.250, G:0.750, B:0.750";
  > = float3(0.25, 0.75, 0.75);

  uniform float3 HUEBlue < __UNIFORM_COLOR_FLOAT3
      ui_label="Blue";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Blue Default Value:\n"
      "255 : R:   64, G:   64, B:  191\n"
      "0.00: R:0.250, G:0.250, B:0.750";
  > = float3(0.25, 0.25, 0.75);

  uniform float3 HUEPurple < __UNIFORM_COLOR_FLOAT3
      ui_label="Purple";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Purple Default Value:\n"
      "255 : R:  128, G:   64, B:  191\n"
      "0.00: R:0.500, G:0.250, B:0.750";
  > = float3(0.50, 0.25, 0.75);

  uniform float3 HUEMagenta < __UNIFORM_COLOR_FLOAT3
      ui_label="Magenta";
      ui_tooltip =
      "Be careful. Do not to push too far!\n"
      "You can only shift as far as the next\n"
      "or previous hue's current value.\n\n"
      "Editing is easiest using the widget\n"
      "Click the colored box to open it.\n\n"
      "RGB Magenta Default Value:\n"
      "255 : R:  191, G:   64, B:  191\n"
      "0.00: R:0.750, G:0.250, B:0.750";
  > = float3(0.75, 0.25, 0.75);


// SETUP & FUNCTIONS ///////////////////////////////
////////////////////////////////////////////////////
  static const float HSL_Threshold_Base  = 0.05;
  static const float HSL_Threshold_Curve = 1.0;

  float3 RGB_to_HSL(float3 color)
  {
      float3 HSL   = 0.0f;
      float  M     = max(color.r, max(color.g, color.b));
      float  C     = M - min(color.r, min(color.g, color.b));
             HSL.z = M - 0.5 * C;

      if (C != 0.0f)
      {
          float3 Delta  = (color.brg - color.rgb) / C + float3(2.0f, 4.0f, 6.0f);
                 Delta *= step(M, color.gbr); //if max = rgb
          HSL.x = frac(max(Delta.r, max(Delta.g, Delta.b)) / 6.0);
          HSL.y = (HSL.z == 1)? 0.0: C/ (1 - abs( 2 * HSL.z - 1));
      }

      return HSL;
  }

  float3 Hue_to_RGB( float h)
  {
      return saturate(float3( abs(h * 6.0f - 3.0f) - 1.0f,
                              2.0f - abs(h * 6.0f - 2.0f),
                              2.0f - abs(h * 6.0f - 4.0f)));
  }

  float3 HSL_to_RGB( float3 HSL )
  {
      return (Hue_to_RGB(HSL.x) - 0.5) * (1.0 - abs(2.0 * HSL.z - 1)) * HSL.y + HSL.z;
  }

  float LoC( float L0, float L1, float angle)
  {
      return sqrt(L0*L0+L1*L1-2.0*L0*L1*cos(angle));
  }

  float3 HSLShift(float3 color)
  {
      float3 hsl = RGB_to_HSL(color);
      static const float4 node[9]=
      {
          float4(HUERed, 0.0),//red
          float4(HUEOrange, 30.0),
          float4(HUEYellow, 60.0),
          float4(HUEGreen, 120.0),
          float4(HUECyan, 180.0),
          float4(HUEBlue, 240.0),
          float4(HUEPurple, 270.0),
          float4(HUEMagenta, 300.0),
          float4(HUERed, 360.0),//red
      };

      int base;
      for(int i=0; i<8; i++) if(node[i].a < hsl.r*360.0 )base = i;

      float w = saturate((hsl.r*360.0-node[base].a)/(node[base+1].a-node[base].a));

      float3 H0 = RGB_to_HSL(node[base].rgb);
      float3 H1 = RGB_to_HSL(node[base+1].rgb);

      H1.x += (H1.x < H0.x)? 1.0:0.0;

      float3 shift = frac(lerp( H0, H1 , w));
      w = max( hsl.g, 0.0)*max( 1.0-hsl.b, 0.0);
      shift.b = (shift.b - 0.5)*(pow(w, HSL_Threshold_Curve)*(1.0-HSL_Threshold_Base)+HSL_Threshold_Base)*2.0;

      return HSL_to_RGB(saturate(float3(shift.r, hsl.g*(shift.g*2.0), hsl.b*(1.0+shift.b))));
  }

// PIXEL SHADER ////////////////////////////////////
////////////////////////////////////////////////////
  float4	PS_HSLShift(float4 position : SV_Position, float2 txcoord : TexCoord) : SV_Target
  {
      return float4(HSLShift(tex2D(ReShade::BackBuffer, txcoord).rgb), 1.0);
  }


// TECHNIQUE ///////////////////////////////////////
////////////////////////////////////////////////////
  technique HSLShift
  {
      pass HSLPass
      {
          VertexShader = PostProcessVS;
          PixelShader = PS_HSLShift;
      }
  }
