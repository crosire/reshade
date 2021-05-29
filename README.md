ReShade
=======

This is a generic post-processing injector for games and video software. It exposes an automated way to access both frame color and depth information and a custom shader language called ReShade FX to write effects like ambient occlusion, depth of field, color correction and more which work everywhere.

ReShade can optionally load **add-ons**, DLLs that make use of the ReShade API to extend functionality of both ReShade and/or the application ReShade is being applied to. To get started on how to write your own add-on, check out the [documentation in the include directory of this repository](include/README.md).

The ReShade FX shader compiler contained in this repository is standalone, so can be integrated into other projects as well. Simply add all `source/effect_*.*` files to your project and use it similar to the [fxc example](tools/fxc.cpp).

## Building

You'll need Visual Studio 2017 or higher to build ReShade and Python for the `gl3w` dependency.

1. Clone this repository including all Git submodules
2. Open the Visual Studio solution
3. Select either the `32-bit` or `64-bit` target platform and build the solution.\
   This will build ReShade and all dependencies. To build the setup tool, first build the `Release` configuration for both `32-bit` and `64-bit` targets and only afterwards build the `Release Setup` configuration (does not matter which target is selected then).

A quick overview of what some of the source code files contain:

|File                                                                  |Description                                                            |
|----------------------------------------------------------------------|-----------------------------------------------------------------------|
|[dll_log.cpp](source/dll_log.cpp)                                     |Simple file logger implementation                                      |
|[dll_main.cpp](source/dll_main.cpp)                                   |Main entry point (and optional test application)                       |
|[dll_resources.cpp](source/dll_resources.cpp)                         |Access to DLL resource data (e.g. built-in shaders)                    |
|[effect_lexer.cpp](source/effect_lexer.cpp)                           |Lexical analyzer for C-like languages                                  |
|[effect_parser.cpp](source/effect_parser.cpp)                         |Parser for the ReShade FX shader language                              |
|[effect_preprocessor.cpp](source/effect_preprocessor.cpp)             |C-like preprocessor implementation                                     |
|[hook.cpp](source/hook.cpp)                                           |Wrapper around MinHook which tracks associated function pointers       |
|[hook_manager.cpp](source/hook_manager.cpp)                           |Automatic hook installation based on DLL exports                       |
|[input.cpp](source/input.cpp)                                         |Keyboard and mouse input management and window message queue hooks     |
|[runtime.cpp](source/runtime.cpp)                                     |Core ReShade runtime including effect and preset management            |
|[runtime_gui.cpp](source/runtime_gui.cpp)                             |Overlay GUI and everything related to that                             |
|[d3d9/reshade_api_device.cpp](source/d3d9/reshade_api_device.cpp)     |Rendering API abstraction for Direct3D 9                               |
|[d3d10/reshade_api_device.cpp](source/d3d10/reshade_api_device.cpp)   |Rendering API abstraction for Direct3D 10                              |
|[d3d11/reshade_api_device.cpp](source/d3d11/reshade_api_device.cpp)   |Rendering API abstraction for Direct3D 11                              |
|[d3d12/reshade_api_device.cpp](source/d3d12/reshade_api_device.cpp)   |Rendering API abstraction for Direct3D 12                              |
|[opengl/reshade_api_device.cpp](source/opengl/reshade_api_device.cpp) |Rendering API abstraction for OpenGL                                   |
|[vulkan/reshade_api_device.cpp](source/vulkan/reshade_api_device.cpp) |Rendering API abstraction for Vulkan                                   |

## Contributing

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## License

All source code in this repository is licensed under a [BSD 3-clause license](LICENSE.md).
