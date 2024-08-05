ReShade API Examples
====================

Detailed API documentation can be found at https://crosire.github.io/reshade-docs/index.html.

## [01-framerate_limit](/examples/01-framerate_limit)

Limits the framerate of an application to a specified FPS value.

## [02-freepie](/examples/02-freepie)

Adds support for reading FreePIE input data in effects via uniform variables.

## [03-history_window](/examples/03-history_window)

Adds an overlay that keeps track of changes to techniques and uniform variables and allows reverting and redoing them.

## [04-api_trace](/examples/04-api_trace)

Logs the graphics API calls done by the application of the next frame after pressing a keyboard shortcut. This can be a useful to help understanding what an application is doing during a frame.

## [05-shader_dump](/examples/05-shader_dump)

Dumps all shader binaries used by the application to disk (into `0x[CRC-32 hash].cso/spv/glsl` files).

## [06-shader_replace](/examples/06-shader_replace)

Replaces shader binaries before they are used by the application with binaries from disk (looks for a matching `0x[CRC-32 hash].cso/spv/glsl` file and will then load it and overwrite the data from the application before shader creation).\
One can use the [shader_dump](#05-shader_dump) add-on to dump all shaders, then modify some and use [shader_replace](#06-shader_replace) to inject those modifications back into the application.

## [07-texture_dump](/examples/07-texture_dump)

Dumps all textures used by the application to image files on disk (into `0x[CRC-32 hash].png` files).

## [08-texture_replace](/examples/08-texture_replace)

Replaces textures before they are used by the application with image files from disk (looks for a matching `0x[CRC-32 hash].png` file and will then load it annd overwrite the image data from the application before texture creation).\
One can use the [texture_dump](#07-texture_dump) add-on to dump all textures, then modify some and use [texture_replace](#08-texture_replace) to inject those modifications back into the application.

## [09-depth](/examples/09-depth)

Built-in add-on that attempts to find the depth buffer the application uses for scene rendering and makes it available to ReShade effects.

## [10-texture_overlay](/examples/10-texture_overlay)

Adds an overlay to inspect textures used by the application in-game and allows dumping individual ones to disk. This allows for more control over which textures to dump, in constrast to the [texture_dump](#07-texture_dump) add-on, which simply dumps them all.

This example makes use of a standalone utility (see [descriptor_tracking.cpp and descriptor_tracking.hpp](/examples/utils/descriptor_tracking.hpp)) that tracks the contents of all descriptor sets, making it possible to query information about which resources those contain at any time.

## [11-obs_capture](/examples/11-obs_capture)

An [OBS](https://obsproject.com/) capture driver which overrides the one OBS ships with to be able to give more control over where in the frame to send images to OBS.

## [12-video_capture](/examples/12-video_capture)

Captures the screen after effects were rendered and uses [FFmpeg](https://ffmpeg.org/) to create a video file from that. Currently does a device to host copy before encoding, so is rather slow.\
To build this example, first place a built version of the FFmpeg SDK into a subdirectory called `ffmpeg` inside the add-on project directory and don't forget to copy the FFmpeg binaries to the location this add-on is to be used as well.

## [13-effects_during_frame](/examples/13-effects_during_frame)

Renders the ReShade post-processing effects at a different point during the frame, e.g. to apply them before the user interface of the game.

This example makes use of a standalone utility (see [state_tracking.cpp and state_tracking.hpp](/examples/utils/state_tracking.hpp)) that tracks all render state currently bound on a command list, making it possible to query information about it at any time, e.g. to re-apply state after it was modified by a call to `reshade::api::effect_runtime::render_effects()`.

## [14-ray_tracing](/examples/14-ray_tracing)

Shows how to use the ReShade API to create acceleration structures and trace rays using DXR or Vulkan Ray Tracing. The example traces rays against a single triangle and blits the resulting image to the back buffer.

## [15-effect_runtime_sync](/examples/15-effect_runtime_sync)

Built-in add-on that adds preset synchronization between different effect runtime instances, e.g. to have changes in a desktop window reflect in VR.

## [16-swapchain_override](/examples/16-swapchain_override)

Adds options to ReShade.ini to force the application into windowed or fullscreen mode, or force a specific resolution or the default refresh rate.

```
[APP]
ForceWindowed=0
ForceFullscreen=0
Force10BitFormat=0
ForceDefaultRefreshRate=0
ForceResolution=0,0
```
