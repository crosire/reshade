# CobraFX
My shaders for ReShade are saved here. They are designed for in-game photography.

Please read this first: All my compute shaders (currently computeGravity.fx and Colorsort.fx) only work with ReShade 4.8 or newer!
Also compute shaders do not work with DirectX 10 or lower and some older OpenGL versions.


**Gravity.fx** lets pixels gravitate towards the bottom of the screen inside the games 3D environment. This shader consumes an insane amount of resources on high resolutions (4k+), so keep this in mind as a warning.
Don't forget to include the texture inside the Textures folder!
About the texture: You can replace it with your own texture, if you want. It has to be 1920x1080 and greyscale. The brighter the pixel inside the texture, the more intense the effect will be at this location ingame.

**computeGravity.fx** is the compute shader version of Gravity.fx. It has a better color selection, and inverse gravity option.
It runs slower on normal resolution, but a lot faster than Gravity.fx on higher resolutions, so you can downsample/hotsample without issues.
Don't forget to include the texture inside the Textures folder!


**Colorsort.fx** is a compute shader, which sorts colors from brightest to darkest. You can filter the selection by depth and color. Place your own shaders between ColorSort_Masking and ColorSort_Main to only affect the sorted area.


**realLongExposure.fx** is a shader, which enables you to capture changes over time, like in long-exposure photography. It does only work for a fixed time interval. For continuous effects make sure to also check out the old LongExposure.fx which fakes the effect or Trails.fx by BlueSkyDefender for similar brightness results with improved smoothness and depth effects: https://github.com/BlueSkyDefender/AstrayFX/blob/master/Shaders/Trails.fx .
