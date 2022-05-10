qUINT
========================================================

qUINT is a shader collection for ReShade, written in ReShade's proprietary shader language, ReShade FX. It aims to condense most of ReShade's use cases into a small set of shaders, to improve performance, ease of use and accelerate preset prototyping. Many extended features can be enabled via preprocessor definitions in each of the shaders, so make sure to check them out. 

Even though qUINT effects do not use the [ReShade.fxh](https://github.com/crosire/reshade-shaders/blob/slim/Shaders/ReShade.fxh) header - unlike most ReShade effects - all common preprocessor definitions, e.g. regarding depth buffer linearization, are supported. The reasons behind this decision are licensing concerns, clashes of naming conventions and ensuring that qUINT is not broken by changes to the common header.

Requirements
------------------------

- Latest ReShade version to ensure best compatibility
- Some effects require depth access

Setup
------------------------

qUINT is among the repositories listed in the ReShade installer, so it can be installed and registered during ReShade installation directly. The following guide still applies when a non-standard way of installing ReShade was chosen.

Assuming that you knew what you were doing if you chose a non-standard way of installing ReShade, just merge the `Shaders` and `Textures` folders of qUINT with the ones in any currently installed ReShade shader library.

Contents
------------------------

|Effect                                                      |Description                                                            |
|----------------------------------------------------------|-----------------------------------------------------------------------|
|MXAO|Screen-Space Ambient Occlusion effect, adds diffuse shadows under objects and inside cracks. MXAO also contains an indirect lighting solution, to compute a coarse first bounce of light similar to SSDO. It also features surface smoothing (surfaces in depth buffer appear blocky) and a double layer option to compute AO simultaneously at multiple scales with no added performance cost.|
|ADOF|Depth of Field effect that adds an out-of-focus blur to the scene. The bokeh shapes are highly customizable and range from discs to polygons, allow for aperture vignetting and longitudinal chromatic aberration.|
|Lightroom|qUINT Lightroom is a comprehensive set of color grading algorithms, modeled after industry applications such as Adobe Lightroom, Da Vinci Resolve and others. Aside from the common contrast/gamma/exposure adjustments, it can remove color tints, adjust temperature, adjust selective hues of the image and more.|
|Bloom                         |Adds a glow around bright light sources. The effect is relatively basic, as bloom is mostly already present in many games. It also contains a simple eye adaptation effect to prevent over/underexposure.                                     |
|Screen-Space Reflections|This effect adds glossy reflections to the scene. It cannot distinguish between different materials and as such will add reflections on everything, but in the right scenes, SSR can greatly improve realism.|
|DELC Sharpen|Depth-enhanced local contrast sharpen is an image sharpen filter that leverages both depth and color to increase local contrast in desired areas, while avoiding many common artifacts usually found in sharpen algorithms, such as haloing around objects.|
|Deband                       |High-performance banding and quantization artifact removal effect. It smoothens stripy color gradients.        |

License
------------------------

### Copyright (c) Pascal Gilcher / Marty McFly. All rights reserved.
