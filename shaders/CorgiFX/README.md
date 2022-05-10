# <span style="color:#bb4400">CorgiFX</span>
 
Just shaders that I edit or make.
 
I am new at writing shaders so please take that in mind while reading the code. Any code used from other shaders will be stated in the shader comments. If i forgot to mention someone please let me know.
 
The side black bars you see in some images are because of the aspect ratio of the window of the game when I took the picture.
 
# <p align="center"><span style="color:#bb0044">CanvasFog</span></p>
 
If you are familiar with the Adaptive Fog shader then you know how the fog from that shader looks like, if not it's basically a color you choose to blend with the depth. Taking that as an initial point I added the option to use gradients in the fog instead of a single color.
 
## Features
 
- **Gradients**: The available gradients (with their own settings) are:
    - Linear
    - Radial
    - Strip
    - Diamond
- **Colors with alpha channel**: Besides having the option to choose 2 colors in the gradient, you can also change the alpha channel values to play around
- **Color samplers**: You can pick the colors of the gradients from the screen
- **Fog Types**: The name isn't the best, but it means that you can choose to use the adaptive fog-type or the emphasize fog-type which isn't a fog technically speaking, but it affects the "surface" of objects in the fog range. Will let some pictures below.
- **Blending modes**: I added different ways for the fog to blend with the screen. The available blending modes are the following:
    - Normal
    - Multiply
    - Screen
    - Overlay
    - Darken
    - Lighten
    - Color Dodge
    - Color Burn
    - Hard Light
    - Soft Light
    - Difference
    - Exclusion
    - Hue
    - Saturation
    - Color
    - Luminosity
    - Linear burn
    - Linear dodge
    - Vivid light
    - Linearlight
    - Pin light
    - Hardmix
    - Reflect
    - Glow
 
Last blending modes functions kindly provided by prod80.
 
## Some example images
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476225-9eedbc80-4370-11ea-8a58-57447dadf76e.png">
<i>Simple adaptive-type fog with linear gradient and normal blending</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476249-a745f780-4370-11ea-8b9a-72c1f0d28f88.png">
<i>Adaptive-type fog with linear gradient using color mode</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476250-a7de8e00-4370-11ea-8d36-cb49df021fba.png">
<i>Adaptive-type fog with linear gradient using a color with transparency</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476254-a7de8e00-4370-11ea-9af2-2b1df0a66b2c.png">
<i>Radial gradient with high gradient sharpness</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476256-a8772480-4370-11ea-8579-7dd8df7e8755.png">
<i>Strip type gradient with one of the colors with 0 alpha value</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476258-a8772480-4370-11ea-8184-d2b90a51fcfe.png">
<i>Radial gradient with screen blending mode</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476261-a8772480-4370-11ea-85e7-735c53225421.png">
<i>You can alter the x and y values separately on the radial gradient as well as rotating it</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476265-a9a85180-4370-11ea-924d-98513728f30c.png">
<i>Emphasize-type fog with lineal gradient and color blending mode</i></p>
 
 
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74173946-d0d50d80-4c11-11ea-8d39-b7df1f82a613.png">
<i>Adaptive-type fog with diamond gradient and CanvasMask</i></p>
 
 
# <p align="center"><span style="color:#bb0044">CanvasMask</span></p>
 
While doing the CanvasFog shader I thought that it would be cool to have these gradient stuff alongside the depth buffer to use as a mask, so here it is.
 
It's basically has the same features from the CanvasFog shader (that are relevant for a masking shaders), not very much to add.
 
## Some example images
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476266-a9a85180-4370-11ea-8d86-d723fe54d3b3.png">
<i>Using emphasize-type mask with a LUT shader</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476267-a9a85180-4370-11ea-9ea3-51acd224bb12.png">
<i>Using emphasize-type mask with the Comic shader</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476269-aa40e800-4370-11ea-82b0-11361c59dc63.png">
<i>Using adaptivefog-type mask with the DisplayDepth shader</i></p>
 
# <p align="center"><span style="color:#bb0044">StageDepthPlus</span></p>
 
So again in one of those "I want to control x from shader y" moments I made some changes to the StageDepth shader. Here is the stuff it can do.
 
- **Scale**: Possibility to adjust the scale of the image in both axis individually
- **Rotation**: This one speaks for itself, you can rotate the image
- **Flip**: You can flip the image horizontally and vertically
- **Blending Modes**: I wrote them for the CanvasFog shader and figured out they can be usefully applied to an image
- **Depth Control**: It already was in the original StageDepth. It uses the depth buffer to decide to show the image or not
- **Masking**: It allows you to use an image as a mask alongside the image itself.
- **Depth map usage**: Kind of an experimental feature. You can provide a depthmap image alongside the stageplus image. This one will determine if the image is shown or not.
- **AR Correction**: You can now enter the definition of the image being used as preprocessor definitions so it can be automatically loaded with those values.

- **StageDepthPlus_WithDepthBufferMod.fx**
In the `StageDepthPlus with depth buffer modification` folder you will find what the name suggests. This version of StageDepthPlus allows you to blend a depth map image with the actual games depth buffer, so you can load an image of a subject and include it in the depth buffer, so the subject can interact with other shaders that use the depth buffer. If you wanna use this shader be sure to replace the ReShade.fxh file from your shaders folder with the one in the repos folder and edit the global preprocessor definition RESHADE_MIX_STAGE_DEPTH_PLUS_MAP to 1.
 
    This is a very experimental "feature", and because of reshade limitations you will need to set up the image controls (scale, position, rotation, etc.) in each shader that interacts with the depth buffer (extra controls will appear in each shader, alongside the same preprocessor definitions from StageDepthPlus, but you can ignore those) to match the same as the one used in `StageDepthPlus_WithDepthBufferMod.fx`. I apologize if it's not easy to use, if people find it useful I will try to improve it later.
 
    I edited the ReShade.fxh that was around with reshade 4.9.1. Don't expect it to work on newer or older versions of reshade.

 
## Some example images
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/73476247-a745f780-4370-11ea-930c-fe813ae3200b.png">
<i>I like corgis</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74969051-abf34e00-53fa-11ea-9448-d93621c3c9c2.png">
<i>Image using a depth map</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74969102-b57cb600-53fa-11ea-81ad-6df4c1623e59.png">
<i>Same image using cineDOF while mixing the depth map with the depth buffer</i></p>
 
# <p align="center"><span style="color:#bb0044">FreezeShot</span></p>
 
I noticed a lot of double exposure shots recently, and I thought shooting and putting the image in a layer shader must be a bummer, so I made a thing for that.
 
Introducing FreezeShot, align the camera, adjust the depth of the subject you want to take the screenshot from and press the Freeze bool and uala! You can move and adjust it like any layer shader.
 
It has the same controls as the StageDepthPlus. It also saves the depth buffer when you freeze the image, so you can make the layer interact with the scene.
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74969164-caf1e000-53fa-11ea-8291-c80527ea385b.jpg">
<i>Freezing and flipping the image</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74975605-12319e00-5406-11ea-850a-828c13f42636.png">
<i>Frozen image using the saved depth to interact with the scene</i></p>
 
 
# <p align="center"><span style="color:#bb0044">Flip</span></p>
 
I can't take credit for this one since it's a really easy shader Marty wrote in the reshade forums, I just added a couple of bools parameters to choose if you want to flip the image horizontally or vertically. I put it here since it can be useful for artistics purposes like the image below.
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/74970280-d6dea180-53fc-11ea-8ed6-7b9c6ff15004.png">
<i>Flip shader with a couple of instance of CanvasMask</i></p>
 
# <p align="center"><span style="color:#bb0044">Color Mask</span></p>
 
As the title suggests its a shader for masking purposes using colors as the target. My intention was to imitate the "Color Range" selection function from Photoshop, so if you are familiar with that you shouldn't have much trouble using it. The shader is based on the Color Isolation shader from Daodan, so all credits goes to him.
 
## Features
 
- **Target Hue**: You can select the color you want with a variable or with a eyedropper.
- **Hue Overlap**: The bigger the number the lesser the amount of colors being masked. Think of it as controlling how close does the colors need to be in order to be accepted.
- **Curve Steepness**: The brightness of the colors not being masked.
- **Accept / Reject Colors**: Once you have the objective color setted up you can either mask all colors except the one selected or vice versa.
- **Mask Strength**: Just a way to control the opacity of the shader.
- **Debug tools**: If you want to see exactly what you are masking the shader offers two debug tools:
    - **Hue Difference**: Shows a grey-scale image allowing you to see the part of the screen that is being accepted.
    - **Debug Overlay**: Show a color table with a black line that points out the colors being masked.
Both of these debug options were made by Daodan and are quite useful, would suggest you to use them.
- **Highlights and Shadows selection**: As the "Color Range" function in Photoshop you can use this to mask shadows independently of the hue, and if you use the "Reject Colors" option you can mask highlights.
 
## Some example images
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/80437802-03210d80-88d9-11ea-943f-5dc5ce9b50b2.png">
<i>Original scene</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/80437789-fb616900-88d8-11ea-9e03-3f81c2f974c0.png">
<i>HueFx shader applied while masking all colors except blue</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/80437811-0a481b80-88d9-11ea-9abd-fe60bb7a586d.png">
<i>Using the eyedropper to sample a color from the screen, again with the HueFX shader</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/80437825-116f2980-88d9-11ea-98ea-8c3859d4a550.png">
<i>Using the "hue difference" debugging option while using the "Lineal" windows option to accept shadows in the mask</i></p>
 
<p align="center"><img src="https://user-images.githubusercontent.com/24371572/80437832-146a1a00-88d9-11ea-978b-8c17c18d3658.png">
<i>Masking highlights using the HueFX shader</i></p>
 
I think the result is pretty close to what photoshop does, if you are masking shadows or highlights don't be afraid of using values outsides of the boundaries (Control + click) in the "Fuzziness" and "Curve Steepness" variables, still have figure that out, but it's quite usable.
 
# Things to do
- Change how the fog values work
- Use a bicubic or another filter for scaling the image in StageDepthPlus since it's essentially using a nearest neighbor method.
- Make the Color Mask shader work with color selection, saturation range and light range.
- Make the shaders interface more user friendly
- Fix the strip gradient don't maintaining the scale while changing the angle
- Fix the diamond gradient to rotate with the modifications done in the x and y axes
- Change the gradient to other color space
 
Any bug or suggestion you got don't hesitate in hitting me up.
 
# Support and donations
A thank you is more than enough, but if you would also like to help me out you can do it through [PayPal](https://www.paypal.com/paypalme/originalnicodr). Thank you very much to the very gentle folks that supported me, you guys are the best:
 
- **Dread**
 
 


