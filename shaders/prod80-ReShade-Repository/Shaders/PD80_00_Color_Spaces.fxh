/*
 *  Color spaces shared across shaders by prod80
 *  Required for proper effect execution
 */

float3 HUEToRGB( in float H )
{
    return saturate( float3( abs( H * 6.0f - 3.0f ) - 1.0f,
                                  2.0f - abs( H * 6.0f - 2.0f ),
                                  2.0f - abs( H * 6.0f - 4.0f )));
}

float3 RGBToHCV( in float3 RGB )
{
    // Based on work by Sam Hocevar and Emil Persson
    float4 P         = ( RGB.g < RGB.b ) ? float4( RGB.bg, -1.0f, 2.0f/3.0f ) : float4( RGB.gb, 0.0f, -1.0f/3.0f );
    float4 Q1        = ( RGB.r < P.x ) ? float4( P.xyw, RGB.r ) : float4( RGB.r, P.yzx );
    float C          = Q1.x - min( Q1.w, Q1.y );
    float H          = abs(( Q1.w - Q1.y ) / ( 6.0f * C + 0.000001f ) + Q1.z );
    return float3( H, C, Q1.x );
}

float3 RGBToHSL( in float3 RGB )
{
    RGB.xyz          = max( RGB.xyz, 0.000001f );
    float3 HCV       = RGBToHCV(RGB);
    float L          = HCV.z - HCV.y * 0.5f;
    float S          = HCV.y / ( 1.0f - abs( L * 2.0f - 1.0f ) + 0.000001f);
    return float3( HCV.x, S, L );
}

float3 HSLToRGB( in float3 HSL )
{
    float3 RGB       = HUEToRGB(HSL.x);
    float C          = (1.0f - abs(2.0f * HSL.z - 1.0f)) * HSL.y;
    return ( RGB - 0.5f ) * C + HSL.z;
}

// Collected from
// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
float3 RGBToHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 HSVToRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

// Color temperature

float3 KelvinToRGB( in float k )
{
    float3 ret;
    float kelvin     = clamp( k, 1000.0f, 40000.0f ) / 100.0f;
    if( kelvin <= 66.0f )
    {
        ret.r        = 1.0f;
        ret.g        = saturate( 0.39008157876901960784f * log( kelvin ) - 0.63184144378862745098f );
    }
    else
    {
        float t      = max( kelvin - 60.0f, 0.0f );
        ret.r        = saturate( 1.29293618606274509804f * pow( t, -0.1332047592f ));
        ret.g        = saturate( 1.12989086089529411765f * pow( t, -0.0755148492f ));
    }
    if( kelvin >= 66.0f )
        ret.b        = 1.0f;
    else if( kelvin < 19.0f )
        ret.b        = 0.0f;
    else
        ret.b        = saturate( 0.54320678911019607843f * log( kelvin - 10.0f ) - 1.19625408914f );
    return ret;
}

// sRGB to Linear RGB conversion

float3 LinearTosRGB( float3 color )
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}

float3 SRGBToLinear( float3 color )
{
    float3 x = color / 12.92f;
    float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

    float3 clr = color;
    clr.r = color.r <= 0.04045f ? x.r : y.r;
    clr.g = color.g <= 0.04045f ? x.g : y.g;
    clr.b = color.b <= 0.04045f ? x.b : y.b;

    return clr;
}

// SRGB <--> CIELAB CONVERSIONS

// Reference white D65
#define reference_white     float3( 0.95047, 1.0, 1.08883 )

// Source
// http://www.brucelindbloom.com/index.html?Eqn_RGB_to_XYZ.html

#define K_val               float( 24389.0 / 27.0 )
#define E_val               float( 216.0 / 24389.0 )

float3 pd80_xyz_to_lab( float3 c )
{
    // .xyz output contains .lab
    float3 w       = max( c / reference_white, 0.0f );
    float3 v;
    v.x            = ( w.x >  E_val ) ? pow( w.x, 1.0 / 3.0 ) : ( K_val * w.x + 16.0 ) / 116.0;
    v.y            = ( w.y >  E_val ) ? pow( w.y, 1.0 / 3.0 ) : ( K_val * w.y + 16.0 ) / 116.0;
    v.z            = ( w.z >  E_val ) ? pow( w.z, 1.0 / 3.0 ) : ( K_val * w.z + 16.0 ) / 116.0;
    return float3( 116.0 * v.y - 16.0,
                   500.0 * ( v.x - v.y ),
                   200.0 * ( v.y - v.z ));
}

float3 pd80_lab_to_xyz( float3 c )
{
    float3 v;
    v.y            = ( c.x + 16.0 ) / 116.0;
    v.x            = c.y / 500.0 + v.y;
    v.z            = v.y - c.z / 200.0;
    return float3(( v.x * v.x * v.x > E_val ) ? v.x * v.x * v.x : ( 116.0 * v.x - 16.0 ) / K_val,
                  ( c.x > K_val * E_val ) ? v.y * v.y * v.y : c.x / K_val,
                  ( v.z * v.z * v.z > E_val ) ? v.z * v.z * v.z : ( 116.0 * v.z - 16.0 ) / K_val ) *
                  reference_white;
}

float3 pd80_srgb_to_xyz( float3 c )
{
    // Source: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    // sRGB to XYZ (D65) - Standard sRGB reference white ( 0.95047, 1.0, 1.08883 )
    const float3x3 mat = float3x3(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
    );
    return mul( mat, c );
}

float3 pd80_xyz_to_srgb( float3 c )
{
    // Source: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    // XYZ to sRGB (D65) - Standard sRGB reference white ( 0.95047, 1.0, 1.08883 )
    const float3x3 mat = float3x3(
    3.2404542,-1.5371385,-0.4985314,
   -0.9692660, 1.8760108, 0.0415560,
    0.0556434,-0.2040259, 1.0572252
    );
    return mul( mat, c );
}

// Maximum value in LAB, B channel is pure blue with 107.8602... divide by 108 to get 0..1 range values
// Maximum value in LAB, L channel is pure white with 100
float3 pd80_srgb_to_lab( float3 c )
{
    float3 lab = pd80_srgb_to_xyz( c );
    lab        = pd80_xyz_to_lab( lab );
    return lab / float3( 100.0, 108.0, 108.0 );
}

float3 pd80_lab_to_srgb( float3 c )
{
    float3 rgb = pd80_lab_to_xyz( c * float3( 100.0, 108.0, 108.0 ));
    rgb        = pd80_xyz_to_srgb( max( min( rgb, reference_white ), 0.0 ));
    return saturate( rgb );
}