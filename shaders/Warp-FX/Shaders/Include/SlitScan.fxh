#include "Include/RadegastShaders.Depth.fxh"

uniform float x_col <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label = "Position";
    ui_tooltip = "The position on the screen to start scanning. (Does not work with Animate enabled.)"; 
    ui_max = 1.0;
    ui_min = 0.0;
> = 0.250;

uniform float scan_speed <
 #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label="Scan Speed";
    ui_tooltip=
        "Adjusts the rate of the scan. Lower values mean a slower scan, which can get you better images at the cost of scan speed.";
    ui_max = 3.0;
    ui_min = 0.0;
> = 1.0;

uniform int direction <
    ui_type = "combo";
    ui_label = "Scan Direction";
    ui_items = "Left\0Right\0Up\0Down\0";
    ui_tooltip = "Changes the direction of the scan to the direction specified.";
> = 0;

uniform int animate <
    ui_type = "combo";
    ui_label = "Animate";
    ui_items = "No\0Yes\0";
    ui_tooltip = "Animates the scanned column, moving it from one end to the other.";
> = 0;

uniform float frame_rate <
    source = "framecount";
>;

uniform float2 anim_rate <
    source = "pingpong";
    min = 0.0;
    max = 1.0;
    step = 0.001;
    smoothing = 0.0;
>;