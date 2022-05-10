# Insane-Shaders
This is my personal collection of the shaders I've created for reshade.

## Installation
To make use of this shader repository you need to set-up ReShade which is a generic post-processing injector,
compatible with DirectX, OpenGL and Vulkan.

[ReShade Website](https://reshade.me/)

Conveniently, this shader repository is one of the options that can be installed during the reshade set-up,
when prompted to select effect packages, you can just check the box to install Insane-Shaders by Lord Of Lunacy.

Alternatively for a manual install, you can download this repository to a folder, and in the reshade GUI after
launching your application, you can set the shaders folder for this repository as your effect path.

## Overview

**ContrastStretch** as its namesake suggests uses histogram equalization in combination with global variance
calculations to attempt to increase the dynamic range and contrast of images that contain "crushed" detail,
while also being careful to avoid introducing banding to the image.

[Image Comparison](https://imgsli.com/NzE1NTY)

**Oilify** attempts to transform an image into an oil painting and is based on a technique known as an
anisotropic kuwahara filter which is an edge aware denoising filter commonly used for this application.

[Image Comparison](https://imgsli.com/NzE1NTI)

**SharpContrast** is a sharpen shader that entered development after doing some testing on the Kuwahara filter
used in Oilify. It is different from other sharpens due to its use of Kuwahara in combination with its
unique edge/noise filtering that reduces haloing and other undesirable sharpening artifacts.

[Image Comparison](https://imgsli.com/NjYxMjU)

**ReVeil** is a unique shader that is meant to be used in combination with depth based effect, especially those
where lighting is concerned such as ambient occlusion. Due to the nature of how reshade is set-up whenever
an effect is applied to the image, any sort of in-air haze is unable to be accounted for resulting in artifacts
where parts of the effect that one would expect to be covered by fog is actually overtop the fog. What this shader
does is it attempts to detect regions of the image where haze is prevalent in the image and then attempts to 
remove the artifacting by properly blending it with the fog to look more like one would expect.
