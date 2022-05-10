uniform float radius <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label="Radius";
    ui_min = 0.0; 
    ui_max = 1.0;
> = 0.5;

uniform float2 coordinates <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label="Coordinates";
    ui_tooltip="The X and Y position of the center of the effect.";
    ui_min = 0.0; 
    ui_max = 1.0;
> = float2(0.5, 0.5);

uniform bool use_mouse_point <
    ui_label="Use Mouse Coordinates";
> = false;

uniform float tension <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label = "Tension";
    ui_min = 0.; ui_max = 10.; ui_step = 0.001;
> = 1.0;

uniform float aspect_ratio <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_label="Aspect Ratio"; 
    ui_min = -100.0; 
    ui_max = 100.0;
> = 0;

uniform bool use_offset_coords <
    ui_label = "Use Offset Coordinates";
    ui_tooltip = "Display the distortion in any location besides its original coordinates.";
    ui_category = "Offset";
> = 0;

uniform float2 offset_coords <
    #if __RESHADE__ < 40000
        ui_type = "drag";
    #else 
        ui_type = "slider";
    #endif
    ui_label = "Offset Coordinates";
    ui_category = "Offset";
    ui_min = 0.0;
    ui_max = 1.0;
> = float2(0.5, 0.5);

uniform float anim_rate <
    source = "timer";
>;

uniform float2 mouse_coordinates < 
source= "mousepoint";
>;
