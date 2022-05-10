// Copyright 2021 Michael Fabian Dirks <info@xaymar.com>
// Modified by Radegast Stravinsky <radegast.ffxiv@gmail.com>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//	this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//	this list of conditions and the following disclaimer in the documentation
//	and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors
//	may be used to endorse or promote products derived from this software
//	without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#undef fmod
#define fmod(x, y) x - y * floor(x/y)

#define MAX_LINE 5
#define MAX_PTS 5

#include "Include/RadegastShaders.Depth.fxh"
#include "Include/RadegastShaders.Transforms.fxh"
#include "Include/RadegastShaders.BlendingModes.fxh"

// Shader Parameters
uniform float2 p_drunk_strength<
	 #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label = "Strength";
    ui_category = "Distortion";
    ui_min = 0.0; 
    ui_max = 100.0;
    ui_step = 0.1;
    ui_tooltip = "Controls the magnitude of the distortion along the X and Y axis.";    
> = float2(25.0, 25.0);

uniform float2 p_drunk_speed<
	 #if __RESHADE__ < 40000
        ui_type = "drag";
    #else
        ui_type = "slider";
    #endif
    ui_label = "Speed";
    ui_category = "Distortion";
    ui_min = 0.0; 
    ui_max = 10.0;
    ui_step = 0.1;
    ui_tooltip = "Controls the speed of the distortion along the X and Y axis.";    

> = float2(2.0, 2.0);

uniform float angle <
    ui_type = "slider";
    ui_label = "Bending Angle";
    ui_category = "Distortion";
    ui_min = -1800.0; 
    ui_max = 1800.0; 
    ui_tooltip = "Controls how much the distortion bends.";
    ui_step = 1.0;
> = 180.0;

uniform float angle_speed<
    ui_type = "slider";
    ui_label = "Bending Speed";
    ui_category = "Distortion";
    ui_min = 0.0; 
    ui_max = 10.0;
    ui_step = 0.1;
    ui_tooltip = "Controls the speed of the bending.";    

> = 1.0;

uniform float anim_rate <
    source = "timer";
>;
