uniform int blending_mode <
    ui_type = "combo";
    ui_label = "Blending Mode";
    ui_items = "Normal\0Add\0Multiply\0Subtract\0Divide\0Darker\0Lighter\0";
    ui_tooltip = "Choose a blending mode.";
> = 0;

float4 applyBlendingMode(float4 base, float4 color) {
    switch(blending_mode)
        {
            case 1:
                color += base;
                break;
            case 2:
                color *= base;
                break;
            case 3:
                color -= base;
                break;
            case 4:
                color /= base;
                break;
            case 5:
                if(length(color.rgb) > length(base.rgb))
                    color = base;
                break;
            case 6:
                if(length(color.rgb) < length(base.rgb))
                    color = base;
                break;
        }  

    return color;
}