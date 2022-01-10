ReShade API Examples
====================

## [00-basic](/examples/00-basic)

Simple project template to get started with.

## [01-api_trace](/examples/01-api_trace)

Simply logs all graphics API calls done by the application to an overlay (can be useful to understand what it is doing during a frame).

## [02-shader_dump](/examples/02-shader_dump)

Dumps all shader binaries used by the application to disk (into `[executable name]_shader_0x[CRC-32 hash].cso/spv/glsl` files).

## [03-shader_replace](/examples/03-shader_replace)

Replaces shader binaries before they are used by the application with binaries from disk (looks for a matching `[executable name]_shader_0x[CRC-32 hash].cso/spv/glsl` file and will then load it and overwrite the data from the application before shader creation).\
Can use the [shader_dump](#02-shader_dump) add-on to dump all shader binaries, then modify some and use [shader_replace](#03-shader_replace) to inject those modifications back into the application.

## [04-texture_dump](/examples/04-texture_dump)

Dumps all textures used by the application to image files on disk (into `[executable name]_0x[CRC-32 hash].bmp` files).

## [05-texture_replace](/examples/05-texture_replace)

Replaces textures before they are used by the application with image files from disk (looks for a matching `[executable name]_0x[CRC-32 hash].bmp` file and will then load it annd overwrite the image data from the application before texture creation).\
Can use the [texture_dump](#04-texture_dump) add-on to dump all textures, then modify some and use [texture_replace](#05-texture_replace) to inject those modifications back into the application.

## [06-texture_overlay](/examples/06-texture_overlay)

Adds an overlay to inspect textures used by the application in-game and allows dumping individual ones to disk. This allows for more control over which textures to dump, in constrast to the [texture_dump](#04-texture_dump) add-on, which simply dumps them all.

## [10-generic_depth](/examples/10-generic_depth)

Built-in add-on that attempts to find the depth buffer used for most scene rendering and makes it available to ReShade effects.

## [11-ffmpeg_video_capture](/examples/11-ffmpeg_video_capture)

Basic example capturing the screen after effects were rendered and using [FFmpeg](https://ffmpeg.org/) to create a video file from that. Currently does a device to host copy before encoding, so is rather slow.\
To build this example, first place a built version of the FFmpeg SDK into a subdirectory called `ffmpeg` inside the add-on project directory and don't forget to copy the FFmpeg binaries to the location this add-on is to be used as well.
