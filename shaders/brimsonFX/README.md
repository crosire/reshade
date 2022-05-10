
# brimsonFX

ReShade shaders and more

## ...Why?

+ Fun
+ New developers can learn
+ Coping mechanism

## Goals

+ More mosiac shapes
+ Cascade multigrid fluid simulation

## Shaders

> Some shaders require [ReShade.fxh](/shaders/ReShade.fxh), ReShade's official helper file.

Shader (click to download) | Description
---------------------------|------------
[cAbberation](/shaders/cAbberation.fx)                  | Chromatic abberation using vertex shader offset
[cAutoExposure](/shaders/cAutoExposure.fx)              | 2-pass automatic exposure
[cBloom](/shaders/cBloom.fx)                            | Dual-filtering bloom
[cBlur](/shaders/cBlur.fx)                              | Convolution using Vogel spiral sampling
[cCheckerBoard](/shaders/cCheckerBoard.fx)              | Customizable 2-pattern checkerboard
[cColorBlendOp](/shaders/cColorBlendOp.fx)              | Blend to backbuffer without copying textures
[cColorNormalization](/shaders/cColorNormalization.fx)  | Various color normalization algoritms
[cDefault](/shaders/cDefault.fx)                        | Initialize graphics pipeline (useful for comparing effects)
[cDualFilter](/shaders/cDualFilter.fx)                  | Pyramidal convolutions (box, Jorge Jimenez, and Kawase)
[cEdgeDetection](/shaders/cEdgeDetection.fx)            | Edge detection kernels (4 bilinear, 2 discrete)
[cFilmGrain](/shaders/cFilmGrain.fx)                    | Film grain without copying texture
[cFrameBlending](/shaders/cFrameBlending.fx)            | Frame blending using the previous result
[cGaussianBlur](/shaders/cGaussianBlur.fx)              | HLSL implementation of RasterGrid's linear Gaussian blur
[cInterpolation](/shaders/cInterpolation.fx)            | Optical flow frame blending
[cLetterBox](/shaders/cLetterBox.fx)                    | LetterBox without copying textures
[cLuminance](/shaders/cLuminance.fx)                    | Various grayscale algoritms
[cMedian](/shaders/cMedian.fx)                          | 3x3 median filter
[cMotionBlur](/shaders/cMotionBlur.fx)                  | Color motion blur
[cMotionMask](/shaders/cMotionMask.fx)                  | Frame masking based on temporal derivative
[cNoiseConvolution](/shaders/cNoiseConvolution.fx)      | Convolution using rotated gradient noise sampling
[cOpticalFlow](/shaders/cOpticalFlow.fx)                | HLSL implementation of pyramidal Horn Schunck with visualization
[cOverlay](/shaders/cOverlay.fx)                        | Simple backbuffer overlay
[cPingPong](/shaders/cPingPong.fx)                      | Gaussian blur approximation using ping-pong box blurs
[cScale](/shaders/cScale.fx)                            | Buffer scaling using vertex shaders
[cShard](/shaders/cShard.fx)                            | Simple unmask sharpening
[cSimplexNoise](/shaders/cSimplexNoise.fx)              | Simple noise and noise warp shader
[cSMAA](/shaders/cSMAA.fx)                              | Minimalist version of SMAA, medium setting
[cSrcDestBlend](/shaders/cSrcDestBlend.fx)              | Backbuffer blending
[cThreshold](/shaders/cThreshold.fx)                    | Quadratic color thresholding
[kContour](/shaders/kContour.fx)                        | Contour line effect
[kDatamosh](/shaders/kDatamosh.fx)                      | Simulates video compression artifacts
[kMirror](/shaders/kMirror.fx)                          | Mirroring and kaleidoscope effect
[kVignette](/shaders/kVignette.fx)                      | Natural vignetting effect

## Coding Convention

Practice | Variable
-------- | --------
**ALLCAPS**     | System-Value semantics, state parameters
**ALL_CAPS**    | Preprocessor macros and parameters
**SnakeCase**   | Discrete local and global data
**Snake_Case**  | Namespace, structs, functions, textures, sampler, and packed data (i.e. `float4 TexCoord_Base_Water` stores 2 UVs for 2 textures, base and water)
**_Snake_Case** | Uniform data
**Suffix `VS` and `PS`** | `PixelShader` and `VertexShader`

## Acknowledgements

+ MartinBFFan and Pao on Discord for reporting bugs
+ BSD for bug propaganda and helping to solve my issue
+ TheGordinho for listening ear and funnies
