ReShade API Examples
====================

Detailed API documentation can be found at https://crosire.github.io/reshade-docs/index.html.

## [01-api_trace](/examples/01-api_trace)

Logs the graphics API calls done by the application of the next frame after pressing F10. This can be a useful to help understanding what an application is doing during a frame.

## [02-shader_dump](/examples/02-shader_dump)

Dumps all shader binaries used by the application to disk (into `[executable name]_shader_0x[CRC-32 hash].cso/spv/glsl` files).

## [03-shader_replace](/examples/03-shader_replace)

Replaces shader binaries before they are used by the application with binaries from disk (looks for a matching `[executable name]_shader_0x[CRC-32 hash].cso/spv/glsl` file and will then load it and overwrite the data from the application before shader creation).\
One can use the [shader_dump](#02-shader_dump) add-on to dump all shaders, then modify some and use [shader_replace](#03-shader_replace) to inject those modifications back into the application.

## [04-texture_dump](/examples/04-texture_dump)

Dumps all textures used by the application to image files on disk (into `[executable name]_0x[CRC-32 hash].bmp` files).

## [05-texture_replace](/examples/05-texture_replace)

Replaces textures before they are used by the application with image files from disk (looks for a matching `[executable name]_0x[CRC-32 hash].bmp` file and will then load it annd overwrite the image data from the application before texture creation).\
One can use the [texture_dump](#04-texture_dump) add-on to dump all textures, then modify some and use [texture_replace](#05-texture_replace) to inject those modifications back into the application.

## [06-history_window](/examples/06-history_window)

Adds an overlay that keeps track of changes to techniques and uniform variables and allows reverting and redoing them.

## [07-generic_depth](/examples/07-generic_depth)

The built-in add-on that attempts to find the depth buffer the application uses for scene rendering and makes it available to ReShade effects.

## [08-texture_overlay](/examples/08-texture_overlay)

Adds an overlay to inspect textures used by the application in-game and allows dumping individual ones to disk. This allows for more control over which textures to dump, in constrast to the [texture_dump](#04-texture_dump) add-on, which simply dumps them all.

## [09-obs_capture](/examples/09-obs_capture)

An [OBS](https://obsproject.com/) capture driver which overrides the one OBS ships with to be able to give more control over where in the frame to send images to OBS.

## [10-video_capture](/examples/10-video_capture)

Captures the screen after effects were rendered and uses [FFmpeg](https://ffmpeg.org/) to create a video file from that. Currently does a device to host copy before encoding, so is rather slow.\
To build this example, first place a built version of the FFmpeg SDK into a subdirectory called `ffmpeg` inside the add-on project directory and don't forget to copy the FFmpeg binaries to the location this add-on is to be used as well.

## [11-effects_during_frame](/examples/11-effects_during_frame)

Renders the ReShade post-processing effects at a different point during the frame, e.g. to apply them before the user interface of the game.
