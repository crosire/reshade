ReShade
=======

This is a generic post-processing injector for games and video software. It exposes an automated way to access both frame color and depth information and a custom shader language called ReShade FX to write effects like ambient occlusion, depth of field, color correction and more which work everywhere.

## Building

You'll need both Git and Visual Studio 2017 or higher to build ReShade. Latter is required since the project makes use of several C++14 and C++17 features. Additionally a Python 2.7.9 or later (Python 3 is supported as well) installation is necessary for the `gl3w` dependency to build.

1. Clone this repository including all Git submodules
2. Open the Visual Studio solution
3. Select either the "32-bit" or "64-bit" target platform and build the solution (this will build ReShade and all dependencies).

After the first build, a `version.h` file will show up in the [res](/res) directory. Change the `VERSION_FULL` definition inside to something matching the current release version and rebuild so that shaders from the official repository at https://github.com/crosire/reshade-shaders won't cause a version mismatch error during compilation.

## Contributing

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## License

All source code in this repository is licensed under a [BSD 3-clause license](LICENSE.md).
