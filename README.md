ReShade
=======

This is a generic post-processing injector for games and video software. It exposes an automated way to access both frame color and depth information and a custom shader language called ReShade FX to write effects like ambient occlusion, depth of field, color correction and more which work everywhere.

ReShade can optionally load **add-ons**, DLLs that make use of the ReShade API to extend functionality of both ReShade and/or the application ReShade is being applied to. To get started on how to write your own add-on, check out the [API reference](REFERENCE.md).

The ReShade FX shader compiler contained in this repository is standalone, so can be integrated into other projects as well. Simply add all `source/effect_*.*` files to your project and use it similar to the [fxc example](tools/fxc.cpp).

## Building

You'll need Visual Studio 2017 or higher to build ReShade. And Python in the PATH environment variable for the `glad` dependency to build.

1. Clone this repository including all Git submodules\
```git clone --recurse-submodules https://github.com/crosire/reshade```
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
|[effect_parser_stmt.cpp](source/effect_parser_stmt.cpp)               |Parser for the ReShade FX shader language                              |
|[effect_preprocessor.cpp](source/effect_preprocessor.cpp)             |C-like preprocessor implementation                                     |
|[hook.cpp](source/hook.cpp)                                           |Wrapper around MinHook which tracks associated function pointers       |
|[hook_manager.cpp](source/hook_manager.cpp)                           |Automatic hook installation based on DLL exports                       |
|[input.cpp](source/input.cpp)                                         |Keyboard and mouse input management and window message queue hooks     |
|[runtime.cpp](source/runtime.cpp)                                     |Core ReShade runtime including effect and preset management            |
|[runtime_gui.cpp](source/runtime_gui.cpp)                             |Overlay rendering and everything user interface related                |

## Contributing

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## Feedback and Support

See the [ReShade Forum](https://reshade.me/forum) and [Discord](https://discord.gg/PrwndfH) server for feedback and support.

## License

ReShade is licensed under the terms of the [BSD 3-clause license](LICENSE.md).\
Some source code files are dual-licensed and are also available under the terms of the MIT license, when stated as such at the top of those files.
