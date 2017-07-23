ReShade
=======

ReShade is an advanced, fully generic post-processing injector for games and video software. Imagine your favorite game with ambient occlusion, real depth of field effects, color correction and more ... ReShade exposes an automated and generic way to access both frame color and depth information and all the tools to make it happen.

## Building

You'll need both Git and Visual Studio 2015 or higher to build ReShade. Latter is required since the project makes use of various C++11 and C++17 features. Additionally a Python 2.7.9 or later (Python 3 is supported as well) installation is necessary for the `gl3w` dependency to build.

1. Clone this repository including all Git submodules
2. Open the Visual Studio solution
3. Select either the "32-bit" or "64-bit" target platform and build the solution (this will build ReShade and all dependencies)

## Contributing

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## License

All the source code is licensed under the conditions of the [BSD 3-clause license](LICENSE.md).

## File Overview

Path | Description
---- | -----------
[/deps](/deps) | Dependencies
[/res](/res) | Resources
[/setup](/setup) | Setup tool source code
[/source](/source) | ReShade source code
[/source/d3d9](/source/d3d9) | Direct3D 9 hooks and runtime implementation
[/source/d3d10](/source/d3d10) | Direct3D 10 hooks and runtime implementation
[/source/d3d11](/source/d3d11) | Direct3D 11 hooks and runtime implementation
[/source/dxgi](/source/dxgi) | DXGI hooks
[/source/opengl](/source/dxgi) | OpenGL hooks and runtime implementation
[/source/windows](/source/windows) | Network and window management hooks
[/source/constant_folding.cpp](/source/constant_folding.cpp) | Various rules that fold incoming constant abstract syntax tree expressions
[/source/directory_watcher.cpp](/source/directory_watcher.cpp) | Class which notifies on file modifications
[/source/filesystem.cpp](/source/filesystem.cpp) | Helper functions for file and path management
[/source/hook.cpp](/source/hook.cpp) | Function hooking
[/source/hook_manager.cpp](/source/hook_manager.cpp) | Management of the different hooking methods and automatic hooking based on DLL export matching
[/source/ini_file.cpp](/source/ini_file.cpp) | INI file format parser
[/source/input.cpp](/source/input.cpp) | Mouse and keyboard input management
[/source/lexer.cpp](/source/lexer.cpp) | Hand-written tokenizer for ReShade FX code
[/source/log.cpp](/source/log.cpp) | Logging
[/source/main.cpp](/source/main.cpp) | DLL main entry point
[/source/parser.cpp](/source/parser.cpp) | Hand-written recursive descent parser for ReShade FX code
[/source/preprocessor.cpp](/source/preprocessor.cpp) | C-style preprocessor for ReShade FX code
[/source/resource_loading.cpp](/source/resource_loading.cpp) | Win32 DLL resource loader
[/source/runtime.cpp](/source/runtime.cpp) | Main code interface managing the in-game user interface, effects etc.
[/source/runtime_objects.cpp](/source/runtime_objects.cpp) | Constant buffer read/write and data type conversion
[/source/symbol_table.cpp](/source/symbol_table.cpp) | Symbol table managing functions and variables while parsing ReShade FX code
[/tools](/tools) | Additional build tools
