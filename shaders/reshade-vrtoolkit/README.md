<img src="https://github.com/retroluxfilm/reshade-vrtoolkit/blob/gh-pages/docs/assets/images/vrtoolkit_logo.png" width=500px>

The VRToolkit is a modular shader created for [ReShade](https://github.com/crosire/reshade)
to enhance the clarity & sharpness in VR to get most out of your HMD while keeping the performance impact minimal.

### Main Features

- Sharpening Modes for enhanced clarity while only processing the pixels that are in the sweet spot of your HMD
- Color Correction Modes to be able to adjust your HMD colors & contrast to your liking
- Dithering to reduce banding effects of gradients and sharpening artifacts
- Antialiasing option to reduce aliasing/shimmering effects when the in game AA modes are not enough
- All modules are processed in a single render pass post shader to improve performance instead of having them all separate

*Note: ReShade currently only works on games run through SteamVR. (OpenVR API)*

Installation & Instructions
---
Visit https://vrtoolkit.retrolux.de/

Contribute
---
There might be a few things that can be improved and optimized for VR.
Or there might be more games and HMD settings to be added to the list.
If you want to contribute feel free to use pull requests.

 Credits
---
- [ReShade](https://github.com/crosire/reshade)
  from Patrick Mours aka crosire
- [Filmic Anamorphic Sharpen.fx](https://github.com/crosire/reshade-shaders/blob/master/Shaders/FilmicAnamorphSharpen.fx)
  by Jakub Maximilian Fober
- [LUT.fx](https://github.com/crosire/reshade-shaders/blob/slim/Shaders/LUT.fx)
  by Marty McFly  
- [Curves.fx](https://github.com/crosire/reshade-shaders/blob/master/Shaders/Curves.fx)
  by Christian Cann Schuldt Jensen ~ CeeJay.dk
- [CAS.fx](https://github.com/CeeJayDK/SweetFX/blob/master/Shaders/CAS.fx)
  by Advanced Micro Devices and contributors
- [Sharpening Mask & Contribution for Reshade VR support](https://github.com/fholger)
  by Holger Frydrych
- [Dithering](https://gdcvault.com/play/1021771/Advanced-VR)
  by Value, programmer Lestyn


![GitHub all releases](https://img.shields.io/github/downloads/retroluxfilm/reshade-vrtoolkit/total)
