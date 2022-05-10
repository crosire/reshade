uniform bool bFakeDOF_reverse <
      ui_label = "FakeDOF Reverse depth mask";
> = false;

uniform float fFakeDOF_coordx <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "FakeDOF Focuspoint X-Coordinate";
> = 0.5;

uniform float fFakeDOF_coordy <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "FakeDOF Focuspoint Y-Coordinate";
> = 0.5;

uniform float fFakeDOF_shape_hor <
	ui_type = "drag";
	ui_min = -1.0; ui_max = 1.5;
	ui_label = "FakeDOF Focuspoint Horizontal Shape";
> = 0.0;

uniform float fFakeDOF_shape_ver <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 1.0;
	ui_label = "FakeDOF Focuspoint Vertical Shape";
> = 0.0;

uniform float fFakeDOF_curve_param_a <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_label = "FakeDOF BlurCurve Parameter a";
> = 1.0;

uniform float fFakeDOF_curve_param_b <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_label = "FakeDOF BlurCurve Parameter b";
> = 0.15;

uniform float fFakeDOF_curve_param_c <
	ui_type = "drag";
	ui_min = 0.0; ui_max = 2.0;
	ui_label = "FakeDOF BlurCurve Parameter c";
> = 0.36;



float GetLinearizedDepth(float2 texcoord)
{
    float2 dist_xy = float2 (texcoord.x - fFakeDOF_coordx, texcoord.y - fFakeDOF_coordy);
    float pixelsize_ratio = PixelSize.y / PixelSize.x;
    dist_xy = dist_xy * float2(1.5 - fFakeDOF_shape_hor, 1.0 - fFakeDOF_shape_ver);
    float dist = dot(dist_xy, dist_xy);

    float depth = fFakeDOF_curve_param_a + (-fFakeDOF_curve_param_a / (1.0 + pow(dist / (fFakeDOF_curve_param_c * fFakeDOF_curve_param_b), fFakeDOF_curve_param_c / fFakeDOF_curve_param_b)));
    
    depth = bFakeDOF_reverse ? 1 - depth : depth;
    return depth;
}



