ReShade API Examples
====================

## [api_trace](/examples/api_trace)

Simply logs all graphics API calls done by the application to an overlay (can be useful to understand what it is doing during a frame).

## [generic_depth](/examples/generic_depth)

Built-in add-on that attempts to find the depth buffer used for most scene rendering and makes it available to ReShade effects.

## [shadermod_dump](/examples/shadermod_dump)

Dumps all shader binaries used by the application to disk (into `shader_0x[CRC-32 hash].cso/spv/glsl` files).

## [shadermod_replace](/examples/shadermod_replace)

Replaces shader binaries before they are used by the application with binaries from disk (looks for a matching `shader_0x[CRC-32 hash].cso/spv/glsl` file and will then load it and overwrite the data from the application before shader creation).\
Can use the [shadermod_dump](#shadermod_dump) add-on to dump all shader binaries, then modify some and use [shadermod_replace](#shadermod_replace) to inject those modifications back into the application.

## [texturemod_dump](/examples/texturemod_dump)

Dumps all textures used by the application to image files on disk (into `[executable name]_0x[CRC-32 hash].bmp` files).

## [texturemod_overlay](/examples/texturemod_overlay)

Adds an overlay to inspect textures used by the application in-game and allows dumping individual ones to disk. This allows for more control over which textures to dump, in constrast to the [texturemod_dump](#texturemod_dump) add-on, which simply dumps them all.

## [texturemod_replace](/examples/texturemod_replace)

Replaces textures before they are used by the application with image files from disk (looks for a matching `[executable name]_0x[CRC-32 hash].bmp` file and will then load it annd overwrite the image data from the application before texture creation).\
Can use the [texturemod_dump](#texturemod_dump) add-on to dump all textures, then modify some and use [texturemod_replace](#texturemod_replace) to inject those modifications back into the application.

## [video_capture](/examples/video_capture)

Basic example capturing the screen after effects were rendered and using [FFmpeg](https://ffmpeg.org/) to create a video file from that. Currently does a device to host copy before encoding, so is rather slow.\
To build this example, first place a built version of the FFmpeg SDK into a subdirectory called `ffmpeg` inside the `video_capture` directory and don't forget to copy the FFmpeg binaries to the location this add-on is to be used as well.
