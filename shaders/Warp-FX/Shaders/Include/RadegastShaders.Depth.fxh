uniform float depth_threshold <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_label="Depth Threshold";
    ui_category="Depth";
    ui_min=0.0;
    ui_max=1.0;
> = 0;

uniform int depth_mode <
    ui_type = "combo";
    ui_label = "Depth Mode";
    ui_category="Depth";
    ui_items = "Minimum\0Maximum\0";
    ui_tooltip = "Mask the effect by using the depth of the scene.";
> = 0;

uniform bool set_max_depth_behind <
    ui_label = "Set Distortion Behind Foreground";
    ui_category="Depth";
    ui_tooltip = "(Maximum Depth Threshold Mode only) When enabled, sets the distorted area behind the objects that should come in front of it.";
> = 0;