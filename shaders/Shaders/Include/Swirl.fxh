#include "Include/RadegastShaders.CommonPositional.fxh"
#include "Include/RadegastShaders.BlendingModes.fxh"
#include "Include/RadegastShaders.Transforms.fxh"

uniform float angle <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label="Angle";
    ui_min = -1800.0; 
    ui_max = 1800.0; 
    ui_step = 1.0;
> = 180.0;

uniform int inverse <
    ui_type = "combo";
    ui_label = "Inverse Angle";
    ui_items = "No\0Yes\0";
    ui_tooltip = "Inverts the angle of the swirl, making the edges the most distorted.";
> = 0;

uniform int animate <
    ui_type = "combo";
    ui_label = "Animate";
    ui_items = "No\0Yes\0";
    ui_tooltip = "Animates the swirl, moving it clockwise and counterclockwise.";
> = 0;
