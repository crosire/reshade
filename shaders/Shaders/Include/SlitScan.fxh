uniform float x_col <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label="X";
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

uniform float anim_rate <
    source = "framecount";
>;

uniform float min_depth <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_label="Minimum Depth";
    ui_tooltip="Unmasks anything before a set depth.";
    ui_min=0.0;
    ui_max=1.0;
> = 0;
