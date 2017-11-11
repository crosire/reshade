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
