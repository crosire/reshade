/*
 *  Blend functions across shaders by prod80
 *  Required for proper effect execution
 *  These can be used by calling the function blendmode() taking input of base, blend, blendmode, opacity
 *  The effects only operate in LDR space
 *  UI elements

    uniform int blendmode < __UNIFORM_COMBO_INT1
        ui_label = "Blendmode";
        ui_tooltip = "Description";
        ui_category = "Category";
        ui_items = "Default\0Darken\0Multiply\0Linearburn\0Colorburn\0Lighten\0Screen\0Colordodge\0Lineardodge\0Overlay\0Softlight\0Vividlight\0Linearlight\0Pinlight\0Hardmix\0Reflect\0Glow\0Hue\0Saturation\0Color\0Luminosity\0";
        > = 0;
    uniform float opacity <
        ui_label = "Opacity";
        ui_tooltip = "Description";
        ui_category = "Category";
        ui_type = "slider";
        ui_min = 0.0;
        ui_max = 1.0;
        > = 1.0;

 */

float getAvgColor( float3 col )
{
    return dot( col.xyz, float3( 0.333333f, 0.333334f, 0.333333f ));
}

// nVidia blend modes
// Source: https://www.khronos.org/registry/OpenGL/extensions/NV/NV_blend_equation_advanced.txt
float3 ClipColor( float3 color )
{
    float lum         = getAvgColor( color.xyz );
    float mincol      = min( min( color.x, color.y ), color.z );
    float maxcol      = max( max( color.x, color.y ), color.z );
    color.xyz         = ( mincol < 0.0f ) ? lum + (( color.xyz - lum ) * lum ) / ( lum - mincol ) : color.xyz;
    color.xyz         = ( maxcol > 1.0f ) ? lum + (( color.xyz - lum ) * ( 1.0f - lum )) / ( maxcol - lum ) : color.xyz;
    return color;
}

// Luminosity: base, blend
// Color: blend, base
float3 blendLuma( float3 base, float3 blend )
{
    float lumbase     = getAvgColor( base.xyz );
    float lumblend    = getAvgColor( blend.xyz );
    float ldiff       = lumblend - lumbase;
    float3 col        = base.xyz + ldiff;
    return ClipColor( col.xyz );
}

// Hue: blend, base, base
// Saturation: base, blend, base
float3 blendColor( float3 base, float3 blend, float3 lum )
{
    float minbase     = min( min( base.x, base.y ), base.z );
    float maxbase     = max( max( base.x, base.y ), base.z );
    float satbase     = maxbase - minbase;
    float minblend    = min( min( blend.x, blend.y ), blend.z );
    float maxblend    = max( max( blend.x, blend.y ), blend.z );
    float satblend    = maxblend - minblend;
    float3 color      = ( satbase > 0.0f ) ? ( base.xyz - minbase ) * satblend / satbase : 0.0f;
    return blendLuma( color.xyz, lum.xyz );
}

float3 darken(float3 c, float3 b)       { return min(c,b);}
float3 multiply(float3 c, float3 b) 	{ return c*b;}
float3 linearburn(float3 c, float3 b) 	{ return max(c+b-1.0f,0.0f);}
float3 colorburn(float3 c, float3 b)    { return b<=0.0f ? b:saturate(1.0f-((1.0f-c)/b)); }
float3 lighten(float3 c, float3 b) 		{ return max(b,c);}
float3 screen(float3 c, float3 b) 		{ return 1.0f-(1.0f-c)*(1.0f-b);}
float3 colordodge(float3 c, float3 b) 	{ return b>=1.0f ? b:saturate(c/(1.0f-b));}
float3 lineardodge(float3 c, float3 b) 	{ return min(c+b,1.0f);}
float3 overlay(float3 c, float3 b) 		{ return c<0.5f ? 2.0f*c*b:(1.0f-2.0f*(1.0f-c)*(1.0f-b));}
float3 softlight(float3 c, float3 b) 	{ return b<0.5f ? (2.0f*c*b+c*c*(1.0f-2.0f*b)):(sqrt(c)*(2.0f*b-1.0f)+2.0f*c*(1.0f-b));}
float3 vividlight(float3 c, float3 b) 	{ return b<0.5f ? colorburn(c,(2.0f*b)):colordodge(c,(2.0f*(b-0.5f)));}
float3 linearlight(float3 c, float3 b) 	{ return b<0.5f ? linearburn(c,(2.0f*b)):lineardodge(c,(2.0f*(b-0.5f)));}
float3 pinlight(float3 c, float3 b) 	{ return b<0.5f ? darken(c,(2.0f*b)):lighten(c, (2.0f*(b-0.5f)));}
float3 hardmix(float3 c, float3 b)      { return vividlight(c,b)<0.5f ? float3(0.0,0.0,0.0):float3(1.0,1.0,1.0);}
float3 reflect(float3 c, float3 b)      { return b>=1.0f ? b:saturate(c*c/(1.0f-b));}
float3 glow(float3 c, float3 b)         { return reflect(b,c);}
float3 blendhue(float3 c, float3 b)         { return blendColor(b,c,c);}
float3 blendsaturation(float3 c, float3 b)  { return blendColor(c,b,c);}
float3 blendcolor(float3 c, float3 b)       { return blendLuma(b,c);}
float3 blendluminosity(float3 c, float3 b)  { return blendLuma(c,b);}

float3 blendmode( float3 c, float3 b, int mode, float o )
{
    float3 ret;
    switch( mode )
    {
        case 0:  // Default
        { ret.xyz = b.xyz; } break;
        case 1:  // Darken
        { ret.xyz = darken( c, b ); } break;
        case 2:  // Multiply
        { ret.xyz = multiply( c, b ); } break;
        case 3:  // Linearburn
        { ret.xyz = linearburn( c, b ); } break;
        case 4:  // Colorburn
        { ret.xyz = colorburn( c, b ); } break;
        case 5:  // Lighten
        { ret.xyz = lighten( c, b ); } break;
        case 6:  // Screen
        { ret.xyz = screen( c, b ); } break;
        case 7:  // Colordodge
        { ret.xyz = colordodge( c, b ); } break;
        case 8:  // Lineardodge
        { ret.xyz = lineardodge( c, b ); } break;
        case 9:  // Overlay
        { ret.xyz = overlay( c, b ); } break;
        case 10:  // Softlight
        { ret.xyz = softlight( c, b ); } break;
        case 11: // Vividlight
        { ret.xyz = vividlight( c, b ); } break;
        case 12: // Linearlight
        { ret.xyz = linearlight( c, b ); } break;
        case 13: // Pinlight
        { ret.xyz = pinlight( c, b ); } break;
        case 14: // Hard Mix
        { ret.xyz = hardmix( c, b ); } break;
        case 15: // Reflect
        { ret.xyz = reflect( c, b ); } break;
        case 16: // Glow
        { ret.xyz = glow( c, b ); } break;
        case 17: // Hue
        { ret.xyz = blendhue( c, b ); } break;
        case 18: // Saturation
        { ret.xyz = blendsaturation( c, b ); } break;
        case 19: // Color
        { ret.xyz = blendcolor( c, b ); } break;
        case 20: // Luminosity
        { ret.xyz = blendluminosity( c, b ); } break;
    }
    return saturate( lerp( c.xyz, ret.xyz, o ));
}