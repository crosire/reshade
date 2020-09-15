ReShade
=======

This is a generic post-processing injector for games and video software. It exposes an automated way to access both frame color and depth information and a custom shader language called ReShade FX to write effects like ambient occlusion, depth of field, color correction and more which work everywhere.

The ReShade FX shader compiler contained in this repository is standalone, so can be integrated into other projects as well. Simply add all `source/effect_*.*` files to your project and use it similar to the [fxc example](tools/fxc.cpp).

## Building

You'll need Visual Studio 2017 or higher to build ReShade and Python for the `gl3w` dependency.

1. Clone this repository including all Git submodules
2. Open the Visual Studio solution
3. Select either the `32-bit` or `64-bit` target platform and build the solution.\
   This will build ReShade and all dependencies. To build the setup tool, first build the `Release` configuration for both `32-bit` and `64-bit` targets and only afterwards build the `Release Setup` configuration (does not matter which target is selected then).

A quick overview of what some of the source code files contain:

|File                                                      |Description                                                            |
|----------------------------------------------------------|-----------------------------------------------------------------------|
|[dll_log.cpp](source/dll_log.cpp)                         |Simple file logger implementation                                      |
|[dll_main.cpp](source/dll_main.cpp)                       |Main entry point and test application when building for debug          |
|[dll_resources.cpp](source/dll_resources.cpp)             |Access to DLL resource data (e.g. built-in shaders)                    |
|[effect_lexer.cpp](source/effect_lexer.cpp)               |Lexical analyzer for C-like languages                                  |
|[effect_parser.cpp](source/effect_parser.cpp)             |Parser for the ReShade FX shader language                              |
|[effect_preprocessor.cpp](source/effect_preprocessor.cpp) |C-style preprocessor implementation                                    |
|[hook.cpp](source/hook.cpp)                               |Wrapper around MinHook which tracks associated function pointers       |
|[hook_manager.cpp](source/hook_manager.cpp)               |Automatic hook installation based on DLL exports                       |
|[input.cpp](source/input.cpp)                             |Keyboard and mouse input management and window message queue hooks     |
|[runtime.cpp](source/runtime.cpp)                         |Core ReShade runtime including effect and preset management            |
|[runtime_gui.cpp](source/runtime_gui.cpp)                 |Overlay GUI and everything related to that                             |
|[d3d9/runtime_d3d9.cpp](source/d3d9/runtime_d3d9.cpp)     |Effect runtime implementation for D3D9                                 |
|[d3d10/runtime_d3d10.cpp](source/d3d10/runtime_d3d10.cpp) |Effect runtime implementation for D3D10                                |
|[d3d11/runtime_d3d11.cpp](source/d3d11/runtime_d3d11.cpp) |Effect runtime implementation for D3D11                                |
|[d3d12/runtime_d3d12.cpp](source/d3d12/runtime_d3d12.cpp) |Effect runtime implementation for D3D12                                |
|[opengl/runtime_gl.cpp](source/opengl/runtime_gl.cpp)     |Effect runtime implementation for OpenGL                               |
|[vulkan/runtime_vk.cpp](source/vulkan/runtime_vk.cpp)     |Effect runtime implementation for Vulkan                               |

## Contributing

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## License

All source code in this repository is licensed under a [BSD 3-clause license](LICENSE.md).
