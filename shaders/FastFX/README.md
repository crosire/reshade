
Glamarye Fast Effects for ReShade (version 6.0)
======================================

**New in 6.0:** New sharpening equation that gives cleaner shapes. New and improved Fake GI equations - more realistic colors calculation, and reduce effect of near objects on far ones. Optimized AO implementation for default AO quality level. Added "Cinematic DOF safe mode" option which disables AO and tweaks Fake GI to avoid artefacts in scenes that have a strong depth of field (out of focus background) effect. Allow tonemapping compensation in 10-bit SDR mode. Tweaked adaptive contrast to not overbrighten. 16 bit HDR mode fix for oversaturated GI color. HDR setup now uses ReShade's new BUFFER_COLOR_SPACE definition to help detect right default.


Author: Robert Jessop 

License: MIT
	
Copyright 2022 Robert Jessop 

About
-----
	
Designed for speed and quality, this shader for ReShade is for people who can't just run everything at max settings and want good enough post-processing without costing too much framerate. The FXAA quite a bit faster than as standard FXAA, and ambient occlusion more than twice as fast as other algorithms. Global Illumination is many times faster than other GI shaders, but it's not really the same - it's mostly a fake 2D rough approximation (though it works surprising well!) 
	
Glamarye Fast Effects combines several fast versions of common postprocessing enhancements in one shader for speed. Each can be enabled or disabled separately.
	
1. Fast FXAA (fullscreen approximate anti-aliasing). Fixes jagged edges. Almost twice as fast as normal FXAA, and it preserves fine details and GUI elements a bit better. However, long edges very close to horizontal or vertical aren't fixed so smoothly. 
2. Intelligent Sharpening. Improves clarity of texture details.
3. Fast ambient occlusion. Shades concave areas that would receive less scattered ambient light. This is faster than typical implementations (e.g. SSAO, HBAO+). The algorithm gives surprisingly good quality with few sample points. It's designed for speed, not perfection - for highest possible quality you might want to try the game's built-in AO options, or a different ReShade shader instead. There is also the option, AO shine, for it to highlight convex areas, which can make images more vivid, adds depth, and prevents the image overall becoming too dark. 
4. Subtle Depth of Field. Softens distant objects. A sharpened background can distract from the foreground action; softening the background can make the image feel more real too. 
5. Detect Menus and Videos. Depending on how the game uses the depth buffer it may be possible to detect when not in-game and disable the effects.
6. Detect Sky. Depending on how the game uses the depth buffer it may be possible to detect background images behind the 3D world and disable effects for them.
7. Fake Global Illumination. Attempts to look like fancy GI shaders but using very simple approximations instead. Not the most realistic solution but very fast. It makes the lighting look less flat; you can even see bright colors reflecting off other surfaces in the area. The main part of it can be run in 2D mode for when depth info isn't available. With depth it can approximate local and distant indirect illumination even better, and add large scale ambient occlusion.
8. Adaptive contrast enchancement.
	
3, 4, 5 & 6 require depth buffer access. 7 improves with depth, but can work without.

High Dynamic Range (HDR) is supported. 


Tested in Toussaint (and other places and games.)

![Screenshot](Glamayre%20v6.0%20double%20strength.png "Beauclair, The Witcher 3")

This was taken in version 6 with most effects' strengths doubled to make the effects clear. Default settings are more subtle than this.

Comparison (version 6)
----------

[Comparison of v6 default, double strength effects, no postprocessing, and Witcher 3's builtin FXAA, Sharpen and HBAO+](https://imgsli.com/MTA0Njg3)

	
Setup
-----
	
1. Install ReShade and configure it for your game (See https://reshade.me)
2. Copy Shaders/Glamarye_Fast_Effects.fx to ReShade's Shaders folder within the game's folder (e.g. C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\bin\x64\reshade-shaders\Shaders)
3. Run the game
4. **Turn off the game's own FXAA, sharpen, depth of field & AO/SSAO/HBAO options** (if it has them).
	- Some games do not have individual options, but have a single "post-processing" setting. Setting that to the lowest value will probably disable them all.
5. Call up ReShade's interface in game and enable Glamarye_Fast_Effects ("Home" key by default)
6. Check if depth buffer is working and set up correctly. If it's not available then Depth of field, Ambient Occlusion, Detect Menus and Detect Sky won't work - disable them. 	
	- Use the built-in "Debug: show depth buffer" to check it's there. Close objects should be black and distant ones white.
	- Use "Debug: show depth and FXAA edges" to check it's aligned.
	- If it isn't right see [Marty McFly's Depth Buffer guide](https://github.com/martymcmodding/ReShade-Guide/wiki/The-Depth-Buffer).		
7. High Dynamic Range (HDR). Glamayre detects HDR based on whether the game's output is 8, 10 or 16 bits, and mostly adapts automatically. However 10-bit mode can be HDR or SDR; ReShade cannot detect which yet so you may need to tell Glamayre which via the **Color Space curve** option.	
8. (Optional) Adjust based on personal preference and what works best & looks good in the game. 
	- Note: turn off "performance mode" in Reshade (bottom of panel) to configure, Turn it on when you're happy with the configuration.  
	

Troubleshooting
-----------

* For help installing and using ReShade see: (https://www.youtube.com/playlist?list=PLVJvgoR2kklkkDQxYjBsbR2y-ieWmgSZt) and for more troubleshooting (https://www.youtube.com/watch?v=hYUiWfvyafQ)
* Game crashes: disable anything else that also displays in game, such as Razer's FPS counter, or Steam's overlay. Some games have issues when more than one program is hooking DirectX.
* If you get "error X4505: maximum temp register index exceeded" on DirectX 9.0 games then in ReShade set FAST_AO_POINTS lower (4 should work).
* Fast Ambient Occlusion
	- If you see shadows in fog, mist, explosions etc then try tweaking **Reduce AO in bright areas** and **AO max distance** under Advanced Tuning and configuration.
	- If shadows in the wrong places then depth buffer needs configuring (see below)
	- If you see other artefacts (e.g. shadows look bad) then options are:
		- turn down the strength
		- increase the quality by setting FAST_AO_POINTS to 8.
		- Try tweaking the AO options in Advanced Tuning and configuration.
* Depth buffer issues:
	- Check ReShade's supported games list to see any notes about depth buffer first. 
	- Check in-game (playing, not in a menu or video cutscene):
	- If it's missing or looks different it may need configuration
		- Use right most tab in Reshade and try to manually find the depth buffer in the list of possible buffers.
		- Use ReShade's DisplayDepth shader to help find and set the right "global preprocessor definitions" to fix the depth buffer.
	- More guidance on depth buffer setup from Marty McFly: (https://github.com/martymcmodding/ReShade-Guide/wiki/The-Depth-Buffer)
* No effects do anything. Reset settings to defaults. If using "Detect menus & videos" remember it requires correct depth buffer - depth buffer is just black no effects are applied.
* HDR issues:	
	- In games with 10 bit output before ReShade 5.1 and earlier you will need to tell Glamayre whether the game is HDR or not - the option will appear first in the settings, if needed.
	- There is a preprocessor definition HDR_WHITELEVEL - it will help if this is correctly matched to the value set in your game.
	- Games behave differently in and for some cases I'm going on reports from others - I can't test every game. Please report any bugs, including ReShade's log, details of the game and your monitor.
* Things are too soft/blurry? 
	- Turn off DOF of turn down DOF blur. Depth of field blurs distant objects but not everyone wants that and if the depth buffer isn't set up well it might blur too much.
	- Make sure sharpen is on. FXAA may slightly blur fine texture details, sharpen can fix this.
* GShade: I have heard it works with GShade, but you might need to add "ReShade.fxh" and "ReShadeUI.fxh" from ReShade to your shaders directory too if you don't have them already.
* Everything is set up properly, but I want better quality.
	- use other reshade shaders instead. This is optimized for speed, some others have better quality.
	- Make sure you're not using two versions of the same effect at the same time. Modern games have built-in antialiasing and ambient occlusion (likely slower than Glamayre.) They might be called something like FXAA/SMAA/TAA and SSAO/HBAO or just hidden in a single postprocessing quality option. Disable the game's version or disable Glamayre's version.

		
Enabled/disable effects
-----------------------
	
**Fast FXAA** - Fullscreen approximate anti-aliasing. Fixes jagged edges. Recommendation: use with sharpen too, otherwise it can blur details slightly. 


**Intelligent Sharpen** - Sharpens details but not straight edges (avoiding artefacts). It works with FXAA and depth of field instead of fighting them. It darkens pixels more than it brightens them; this looks more realistic. 

**Fast Ambient Occlusion (AO) (requires depth buffer)** - Ambient occlusion shades places that are surrounded by points closer to the camera. It's an approximation of the reduced ambient light reaching occluded areas and concave shapes (e.g. under grass and in corners.)

**Depth of Field (DOF) (requires depth buffer)** - Softens distant objects subtly, as if slightly out of focus. 

**Detect menus & videos (requires depth buffer)** - Skip all processing if depth value is 0 (per pixel). Sometimes menus use depth 1 - in that case use Detect Sky option instead. Full-screen videos and 2D menus do not need anti-aliasing nor sharpenning, and may lose worse with them. Only enable if depth buffer always available in gameplay!

**Detect sky** - Skip all processing if depth value is 1 (per pixel). Background sky images might not need anti-aliasing nor sharpenning, and may look worse with them. Only enable if depth buffer always available in gameplay!
    

**Fake Global Illumination (Fake GI)** - Attempts to look like fancy GI shaders but using very simple approximations. Not as realistic, but very fast. 

_Note:_ Fake GI does not have a checkbox, instead you will see two versions of Glamayre in ReShade's effects list (with_Fake_GI and without_Fake_GI). Select the correct one to enable or disable Fake GI. This is because the extra shader passes it uses have some overhead that cannot be eliminated with a checkbox.

What is global illumination? The idea is to simulate how light moves around a scene - objects are lit not just by direct line of sight to lights, but also by light bouncing off other objects. This is hard to calculate in real time. Your game probably uses one of 3 types of lighting:

1. Modern games on modern GPUs may include actual global illumination, using ray tracing or path tracing. Adding Fake GI will make those look worse. 
2. Some games have pre-calculated static global illumination that doesn't change (or does in very limited ways). Moving objects might cast simple shadows but cannot cast or recieve bounced light. 
3. Some games have dynamic lighting, but only direct lighting and shadows. Secondary light bounces are not calculated and instead the a constant amount of ambient light is applied to every surface so they don't disappear completely when in shadow.

This Fake Global illumination doesn't calculate all the light bounces using physics. Instead it creates an effect that looks similar most of the time using some simple heuristics (e.g grey thing next to red thing will look a little bit redder). It's a fast approximation with no real understanding of the scene (in fact, it can work a purely 2D fashion if depth is not available). With luck, it looks like indirect bounced lighting and is close enough to make the image look better. It also tends to exagerate existing lighting in games, which if it is not too strong is okay.


Effects Intensity
-----------------

**Sharpen strength** - For values > 0.5 I suggest depth of field too. Values > 1 only recommended if you can't see individual pixels (i.e. high resolutions on small or far away screens.)

**AO strength** - Ambient Occlusion. Higher mean deeper shade in concave areas. Tip: if increasing also increase FAST_AO_POINTS preprocessor definition for higher quality.

**AO shine** - Normally AO just adds shade; with this it also brightens convex shapes. Maybe not realistic, but it prevents the image overall becoming too dark, makes it more vivid, and makes some corners clearer. Higher than 0.5 looks a bit unrealistic.

**DOF blur** - Depth of field. Applies subtle smoothing to distant objects. It's a small effect (1 pixel radius). At the default it more-or-less cancels out sharpenning, no more.

Fake Global Illumination Settings
------------------------

These only work if you are using the _with Fake GI_ version of the shader. The first three options work even without depth information.

**Fake GI lighting strength** - Fake Global Illumination wide-area effect. Every pixel gets some light added from the surrounding area of the image. Usually safe to increase, except in games with unusually vibrant colors.

**Fake GI saturation** - Fake Global Illumination can exaggerate colors in the image too much. Decrease this to reduce the color saturation of the added light. Increase for more vibrant colors.

**Adaptive contrast enhancement** - Increases contrast relative to average light in surrounding area. Fake Global Illumination can reduce overall contrast; this setting compensates for that. It actually may be useful on it's own with lighting strength zeroed - it can improve contrast and clarity of any image. Recommendation: set to roughly half of GI lighting strength.

**Enable Fake GI effects that require depth** - This must be checked for the following sliders to have effect. If you don't have depth buffer data or don't want the AO effects then unchecking this and just using 2D Fake GI will make it slightly faster. 

**Fake GI big AO strength (requires depth)** - Large scale ambient occlusion. Higher = darker shadows. A big area effect providing subtle shading of enclosed spaces, which would recieve less ambient light. It is fast but very approximate. It is subtle and smooth so it's imperfections are not obvious nor annoying. Use in combination with normal ambient occlusion, not instead of it.

**Fake GI local AO strength (requires depth)** - Provides additional ambient occlusion with a flatter shape, subtly enhancing Fast Ambient Occlusion and bounce lighting, at very little cost. This Higher = darker shadows. This would have visible artefacts at high strength; therefore maximum shade added is very small. Use in combination with normal ambient occlusion, not instead of it.

**Fake GI local bounce strength** (requires depth) - A bright red pillar next to a white wall will make the wall a bit red, but how red? It uses local depth and color information to approximate short-range bounced light. It only affects areas made darker by Glamayre's fast ambient occlusion and Fake GI local AO (not big AO). Recommendation: keep near 1.

**Fake GI offset** - Fake global illumination uses a very blurred version of the image to collect approximate light from a wide area around each pixel. If use of depth is enabled, it adjusts the offset based on the angle of the local surface. This makes it better pick up color from facing surfaces, but if set too big it may miss nearby surfaces in favour of further ones. The value is the maximum offset, as a fraction of screen size.


**Cinematic DOF safe mode** - The depth of field effect (out of focus background) is now common in games and sometimes cannot be disabled. It interacts badly with AO and GI effects using depth. Enabling this tweaks effects to use a blurred area depth instead of the depth at every pixel, which makes Fake GI depth effects usable in such games.


Color space options
-------------------

**color space curve (HDR or SDR?)** High dynamic range (HDR) and standard dynamic range (SDR) use different non-linear curves to represent color values. ReShade pre-5.1 cannot detect color space. ReShade 5.1 can but does update shaders if it changes while running the game. Glamayre tries to detect the right default for you, but for 10-bit HDR mode you pre-5.1 you need to manually select HDR10, and if colour spaces changes in game (e.g. you enable/disable HDR) you may need to manually select too. If set incorrectly some effects will look bad (e.g. strong brightness/color changes in effects that should be more subtle.) Recommendation: If it's Unknown but your game and screen are in HDR mode then select HDR10 ST2084. In most other cases the default should be correct. If in doubt, pick the option that where the images changes the least when you enable glamayre.  sRGB is the standard for non-HDR computer output - it's a curve similar to gamma 2.2 and was designed for 8 bit color output.\n\nThe Perceptual Quantizer (PQ) curve from the ST2084 HDR standard is a more complex and steeper curve; it more accurately models the full range of a human eye when you only have 10 bits of precision. scRGB is used in games with 16-bit HDR; it's simply linear - value in the computer is directly proportional to the light on screen.

**Transfer Function precision** Correct color space conversions (especially PQ) can be slow. A fast approximation is okay because we apply the conversion one way, apply our effects, then apply the opposite conversion - so inaccuracies in fast mode mostly cancel out. Most effects don't need perfect linear color, just something pretty close. However, you may select accurate for minor quality improvement.

Output modes
-----------

**Debug Modes:**

**Normal output** - Normal Glamayre output

**Debug: show FXAA edges** - Highlights edges that are being smoothed by FXAA. The brighter green the more smoothing is applied. Don't worry if moderate smoothing appears where you don't think it should - sharpening will compensate.

**Debug: show AO shade & bounce** - Shows amount of shading AO is adding, against a grey background. Includes colored AO bounce if enabled. Use this if tweaking AO settings to help get them just right see the effect of each setting clearly. However, don't worry if it doesn't look perfect - it exaggerates the effect and many issues won't be noticable in the final image. The best final check is in normal mode.

**Debug: show depth buffer** - This image shows the distance to each pixel. However not all games provide it and there are a few different formats they can use. Use to check if it works and is in the correct format. Close objects should be black and distant ones white. If it looks different it may need configuration - Use ReShade's DisplayDepth shader to help find and set the right "global preprocessor definitions" to fix the depth buffer. If you get no depth image, set Depth of Field, Ambient Occlusion and Detect Menus to off, as they won't work.		

**Debug: show depth and edges** - Shows depth buffer and highlights edges - it helps you check if depth buffer is correctly aligned with the image. Some games (e.g. Journey) do weird things that mean it's offset and tweaking ReShade's depth buffer settings might be required. It's as if you can see the Matrix.

The rest of the debug options only apply if Fake Global Illumination is enabled (_with Fake GI_ version of Glamayre).

**Debug: show GI area color** - Shows the color of the light from the wider area affecting to each pixel.

Advanced Tuning and Configuration
------------------------

You probably don't want to adjust these, unless your game has visible artefacts with the defaults.

**AO bigger dither** Ambient occlusion dither. Dithering means adjacent pixels shade is calculated using different nearby depth samples. If you look closely you may see patterns of alternating light/dark pixels. If checked, AO uses an 8 pixel dither pattern; otherwise it uses a 2 pixel pattern. From a distance, bigger dither gives better shadow shapes overall; however, you will see annoying repeating patterns up close. Recommendation: turn on if you have a high enough screen resolution and far enough distance from your screen that you cannot make out individual pixels by eye. The performance is about the same either way. Default: on if resultion 4k or higher.

**Reduce AO in bright areas** - Do not shade very light areas. Helps prevent unwanted shadows in bright transparent effects like smoke and fire, but also reduces them in solid white objects. Increase if you see shadows in white smoke; decrease for more shade on light objects. Doesn't help with dark smoke.

**AO max distance** - The ambient occlusion effect fades until it is zero at this distance. Helps avoid avoid artefacts if the game uses fog or haze. If you see deep shadows in the clouds then reduce this. If the game has long, clear views then increase it.

**AO radius** - Ambient Occlusion area size, as percent of screen. Bigger means larger areas of shade, but too big and you lose detail in the shade around small objects. Bigger can be slower too. 	
		
**AO shape modifier** - Ambient occlusion - weight against shading flat areas. Increase if you get deep shade in almost flat areas. Decrease if you get no-shade in concave areas areas that are shallow, but deep enough that they should be occluded. 
	
**AO max depth diff** - Ambient occlusion biggest depth difference to allow, as percent of depth. Prevents nearby objects casting shade on distant objects. Decrease if you get dark halos around objects. Increase if holes that should be shaded are not.

**FXAA bias** - Don't anti-alias edges with very small differences than this - this is to make sure subtle details can be sharpened and do not disappear. Decrease for smoother edges but at the cost of detail, increase to only sharpen high-contrast edges. This FXAA algorithm is designed for speed and doesn't look as far along the edges as others - for best quality you might want turn it off and use a different shader, such as SMAA.

**Tone mapping compensation** (available for non-HDR output only) - In the real world we can see a much wider range of brightness than a standard screen can produce. Games use tone mapping to reduce the dynamic range, especially in bright areas, to fit into display limits. To calculate lighting effects like Fake GI accurately on SDR images, we want to undo tone mapping first, then reapply it afterwards. Optimal value depends on tone mapping method the game uses. You won't find that info published anywhere for most games. Our compensation is based on Reinhard tone mapping, but hopefully will be close enough if the game uses another curve like ACES. At 5 it's pretty close to ACES in bright areas but never steeper. In HDR modes this is not required.";

**FAST_AO_POINTS** (preprocessor definition - bottom of GUI.) Number of depth sample points in Performance mode. This is your speed vs quality knob; higher is better but slower. Valid range: 2-16. Recommended: 4, 6 or 8 as these have optimized fast code paths. 

**HDR_WHITELEVEL** (preprocessor definition - bottom of GUI.) This is the brightness in nits of white (HDR can go brighter). If you have a similar setting in game then match this to it for maximum accuracy. If unshadowed areas in the "show AO shade" debug mode are much darker than the overall game then you maybe need to increase this. If the game's 16-bit output is actually in nits, not scRGB, then set this to 1 (I hear some Epic games may do this but have not seen it).

Tips
----

- Feel free to tweak the strength settings, to taste.
- Check if game provides depth buffer! If not turn of depth of field, ambient occlusion and detect menus for better performance (they won't affect the image if left on by mistake).
- If depth is always available during gameplay then enabling Detect menus and videos is recommended to make non-gamplay parts clearer. 
- If the game uses lots of dithering (i.e. ░▒▓ patterns), sharpen may exagerate it, so use less sharpenning. (e.g. Witcher 2's lighting & shadows.)		
- If you don't like an effect then reduce it or turn it off. Disabling effects improves performance, except sharpening, which is basically free if FXAA or depth of field is on.	
- If the game uses lots of semi-transparent effects like smoke or fog, and you get incorrect shadows/silluettes then you may need to tweak AO max distance. Alternatively, you could use the game's own slower SSAO option if it has one. This is a limitation of ReShade and similar tools - ambient occlusion should be done before transparent objects, but ReShade can only work with the output of the game and apply it after.
- You can mix and match with in-game options or other ReShade shaders, though you lose some the performance benefits of a combined shader. Make sure you don't have two effects of the same type enabled.
- Don't set FAST_AO_POINTS higher than 12 - the algorithm is designed for few points and won't use the extra points wisely.
- Experiment!
	* What strength settings looks best may depend on the game, your monitor, and its settings (your TV's "game mode" may have some built-in sharpening for example).
	* Depth of field, AO Shine, and Fake Glboal Illumination are really a matter of personal taste. 
	* Don't be afraid to try lower FAST_AO_POINTS (minimum: 2) and turn off bounce lighting if you want really fast AO. Even very low quality AO can make the image look better (but keep strength <= 0.5).
	* Be careful with the advanced ambient occlusion settings; what looks good in one scene may be too much in another. Try to test changes in different areas of the game with different fog and light levels. 
- If using fake GI but with game's builtin Ambient Occlusion instead of Glamayre's, and the game has options the pick a stronger one as, as Fake GI can reduce the effect of Ambient Occlusion if applied afterwards.
		
Benchmark
---------

- Game: Witcher 3 
- Scene: [Beauclair by the lake, looking into town](/Benchmark%20Location.jpg) 
- Settings: 1080p, Graphics settings: High but hairworks off.
- Hardware: Razer Blade Stealth 13 Late 2019 Laptop with NVIDIA GeForce GTX 1650 with Max-Q design
- Shader version: 6.0

**Baseline**

	FPS	Settings
	77	No post-processing

**Glamayre 6.0 results**

	FPS	Settings
	72	defaults (with Fake GI using depth)
	73	defaults (without Fake GI)
	75	Fast FXAA only
	74	Fast FXAA + sharpen
	74	Fast AO only	
	75	Fake GI only (no depth)
	74	Fake GI only (using depth)

**Witcher 3 builtin post-processing**

	FPS	Settings
	73	Anti-Aliasing
	72	Anti-Aliasing + sharpening
	71	SSAO
	68	Anti-Aliasing + sharpening + SSAO
	66	HBAO+
	

**Comparison to other Reshade shaders**

SMAA and FXAA are anti-aliasing. MXAO is a popular ambient occlusion shader.

	FPS	Shader and settings
	71	SMAA
	73	FXAA
	68	MXAO very low (4 samples)
	65	MXAO default (16 samples)
	63	MXAO default plus MXAO_ENABLE_IL on
	

Similar results (relatively) are seen in other locations and graphic settings and resolutions.
	
Tech details
------------
	
Combining FXAA, sharpening and depth of field in one shader works better than separate ones because the algorithms work together. Each pixel is sharpened or softened only once, and where FXAA detects strong edges they're not sharpened. With seperate shaders sometimes sharpening can undo some of the good work of FXAA. More importantly, it's much faster because we only read the pixels only once.
	
GPUs are so fast that memory is the performance bottleneck. While developing I found that number of texture reads was the biggest factor in performance. Interestingly, it's the same if you're reading one texel, or the bilinear interpolation of 4 adjacent ones (interpolation is implemented in hardware such that it is basically free). Each pass has noticable cost too. Therefore everything is combined in one pass, with the algorithms designed to use as few reads as possible. Fast FXAA makes 7 reads per pixel. Sharpen uses 5 reads, but 5 already read by FXAA so if both are enabled it's basically free. Depth of field also re-uses the same 5 pixels, but adds a read from the depth buffer. Ambient occlusion adds 3-9 more depth buffer reads (depending on quality level set). 
	
FXAA starts with the centre pixel, plus 4 samples each half a pixel away diagonally; each of the 4 samples is the average of four pixels. Based on their shape it then samples two more points 3.5 pixels away horizontally or vertically. We look at the diamond created ◊ and look at the change along each side of the diamond. If the change is bigger on one pair of parallel edges and small on the other then we have an edge. The size of difference determines the score, and then we blend between the centre pixel and a smoothed using the score as the ratio. 

The smooth option is the four nearby samples minus the current pixel. Effectively this convolution matrix:

	1 2 1
	2 0 2  / 12;
	1 2 1
	
Sharpening increases the difference between the centre pixel and it's neighbors. We want to sharpen details in textures without oversharpening object edges or making aliasing artefacts worse. Simple sharpening increases the difference between the current pixel and the surrounding ones. We add a simple but novel adjustment - we look at the 4 surrounding diagonals , add the middle two values and subtract the two outliers - this creates nicer shapes and avoids over brightening or darkening of fine details by pushing surrounding pixels a bit more in the opposite direction. To avoid oversharpening high contrast details and saturating or clamping we use a curve similar to a reinhard tone mapper to reduce the sharpening as it approaches 0 or 1. Fast FXAA is calculated before but applied after sharpening. If FXAA decides to smooth a pixel the maximum amount then sharpening is cancelled out completely. This prevents us from fighting FXAA and sharpening aliasing artefacts. 
	
Depth of field is subtle; this implementation isn't a fancy cinematic focus effect. Good to enable if using sharpening, as it takes the edge of the background and helps emphasise the foreground. At the default settings it just about cancels out sharpening, so sharpening gradually disappears in the distance. If DOF blur is higher, it also blurs pixels, increasing blur with distance - at 1 pixels at maximum depth as set to the smooth value. The total change in each pixel is limited to 50% to reduce problems at near-to-far edges when blur is high.
		
Ambient occlusion adds shade to concave areas of the scene. It's a screen space approximation of the amount of ambient light reaching every pixel. It uses the depth buffer generated by the rasterisation process. Without ambient occlusion everything looks flat. However, ambient occlusion should be subtle and not be overdone. Have a look around in the real world - the bright white objects with deep shading you see in research papers just aren't realistic. If you're seeing black shade on bright objects, or if you can't see details in dark scenes then it's too much. However, exagerating it just a little bit compared to the real-world is good - it's another depth clue for your brain and makes the flat image on the flat screen look more 3D.  
	
Fast Ambient occlusion is pretty simple, but has a couple of tricks that make it look good with few samples. Unlike well known techniques like SSAO and HBAO it doesn't try to adapt to the local normal vector of the pixel - it picks sample points in a simple circle around the pixel. Samples that are in closer to the camera than the current pixel add shade, ones further away don't. At least half the samples must be closer for any shade to be cast (so pixels on flat surfaces don't get shaded, as they'll be half in-front at most). It has 4 tricks to improve on simply counting how many samples are closer.

1. Any sample significantly closer is discarded and replaced with an estimated sample based on the opposite point. This prevents nearby objects casting unrealistic shadows and far away ones. Any sample more than significantly further away is clamped to a maximum. AO is a local effect - we want to measure shade from close objects and surface contours; distant objects should not affect it.
2. Our 2-10 samples are equally spaced in a circle. We approximate a circle by doing linear interpolation between adjacent points. Textbook algorithms do random sampling; with few sample points that is noisy and requires a lot of blur; also it's less cache efficient.
3. The average difference between adjacent points is calculated. This variance value is used to add fuzziness to the linear interpolation in step 2 - we assume points on the line randomlyh vary in depth by this amount. This makes shade smoother so you don't get solid bands of equal shade. 
4. Pixels are split into two groups in a checkerboard pattern (▀▄▀▄▀▄). Alternatve pixels use a circle of samples half of the radius. With small pixels, the eye sees a half-shaded checkerboard as grey, so this is almost as good taking twice as many samples per pixel. More complex dithering patterns were tested but don't look good (░░).

Amazingly, this gives quite decent results even with very few points in the circle.

There are two optional variations that change the Fast Ambient Occlusion algorithm:

1. Shine. Normally AO is used only to make pixels darker. With the AO Shine setting (which is set quite low by default), we allow AO amount to be negative. This brightens convex areas (corners and bumps pointing at the camera.) Not super realistic but it really helps emphasise the shapes. This is basically free - instead of setting negative AO to zero we multiply it by ao_shine_strength.
2. Bounce lighting. This is good where two surfaces of different color meet. However, most of the time this has little effect so it's not worth sampling many nearby points. We use same wide area brightness from the Fake GI algorithm to estimate the ambient light and therefore the unlit color of the pixel, and therefore how much light it will reflect. We blur the image to get the average light nearby. We multiply the unlit pixel color with the nearby light to get the light bounced. The value is multiplied by our AO value too, which is a measure of shape and makes sure only concave areas get bounced light added.

Fake Global Illumination without depth enabled is a quite simple 2D approximation of global illumination. Being 2D it's not very realistic but is fast. First we downsample and blur the main image to get the overall color of light in each area. Then at each pixel we tweak the area light brighness to be at least as bright as the current pixel, and smoothly reducing it as it approaches twice the brightness; this prevents loss of image contrast and overbrightening dark objects. The blurred and original pixel's brightness are used together to estimate the current pixel's true surface color; is likely a light surface in shadow, or a dark surface in the light? We reduce the saturation of the area color, reducing it more if it's dark relative to the current pixel (to prevent oversaturated output.) The pixel color is multiplied by the blurred area light and blended with the pixel. Overall the image appears less flat - even if the illumination isn't always realistic (it doesn't really know the shape of things), the subtle variations in shade and subtle color changes help make the image seem more real.

For the rest of the effects we actually use two levels of blur - the big one, and a smaller, less smooth one which is simply a 2.5 MIP level read of the first step of our blur. 

Adaptive Contrast enhancement uses our the average of our two blurred images as a centre value and moves each pixel value away from it. 

If depth is enabled then we also include a depth channel in the blurred images. Also, where depth==1 (sky) we brighten and completely desaturate the color image - this makes sky and rooftops look better. If depth is enabled and Fake GI offset set then we use ddx & ddy to read the local depth buffer surface slope and move the point we sample the big blur in the direction away from the camera.

Big AO (large scale ambient occlusion) simply uses the ratio between the current pixel's depth and the big blurred depth value. If the local depth is more than twice the distance it actually reduces the amount of AO to avoid overshading distant areas seen through small gaps or windows. We clamp it to only darken - we don't do AO shine at this scale.

Fake GI local AO simply compares the pixel's depth to the smaller blurred depth image. If it's further by a minimum amount then we add a small amount (max .1) to the AO value fast AO uses. This flat amount isn't smooth but is small enough that you can't see the edges. It's a different shape to fast AO so subtly enhances the overall shape and shade of AO (and local bounce light). This is basically free performance-wise.

Fake GI local bounce uses the color of the smaller blur, sharpens it relative to the current pixel. It then multiplies that light with an estimate of the current pixel's true surface color, similar to the main large area GI. However, change is applied in proportion to the AO value calculated by fast AO and Fake GI local AO. The effect is stronger indirect bounced light only in concave areas.

HDR, SDR, and non-linear color
-------------------------------

For both blending and lighting calculations and we want linear light values. ReShade doesn't give you that - game output is normally non-linear to take best advantage of the limited bits. 16-bit HDR is linear, which is nice. 10 bit HDR uses a special curve call Perceptual Quantizer (PQ) - we have to do the reverse of the PQ operation, do our thing, then reapply PQ. 8-bit SDR (standard dynamic range) uses the sRGB curve - ReShade can take care of that for us. There's also possibility of 10 bit but with sRGB color - ReShade can't detect that so we have to ask the user, and we have to do sRGB conversion ourselves.

Unforuntately, for SDR there's more... In the real world we can see a much wider range of brightness than a standard screen can produce. Before applying the sRGB curves, games apply a tone mapping curve to reduce the dynamic range, especially in bright areas. We don't know what tone mapping algorithm each game uses. Our equation to reverse it is assumes the game uses extended Reinhard tone mapping, because it's simple. With luck it will be close enough to whatever the game uses so we make the lighting look better and not worse. With Cwhite 5 it's actually pretty close to the popular ACES tonemapping curve for bright values but never steeper - so it should work okay on games using that or similar filmic curves. The slightly more conservative default of 3 was chosen as higher values are just too much for games like Deus Ex: Human Revolution. For performance and to avoid causing FXAA issues, this is applied only in the Fake GI parts of the code.

In HDR mode the situation is more complicated. We don't do tone mapping compensation in HDR mode because it's hard to know how much to apply. However, it's probably not needed as in HDR modes there is much less compression of highs anyway due to the higher maximum brightness. Games are supposed to let the display do the tone-mapping, or adapt tone mapping based on the display's capabilities. But if games make it user configured or TVs are trying to do "Intelligent" tone mapping then it will be suboptimal. Both games and TVs should follow HGIG guidelines (https://www.hgig.org/doc/ForBetterHDRGaming.pdf). However, my LG C1 didn't, even in game mode. However, finding and manually selecting HGIG mode in it's menu improved things. 

Ideas for future improvement
----------------------------

- Use ReShade 5's new plugin mechanism to allow it to run earlier in the game's swap chain, which will help with fog & smoke issues.
- Stop improving this a start playing the games again!

History
-------

(*) Feature (+) Improvement	(x) Bugfix (-) Information (!) Compatibility

6.0 (+) Fake GI: new equations for more realistic shading, fixing oversaturation of colors. Fake GI: reduced effect of close objects on distant ones. AO: optimized implementation for FAST_AO_POINTS==6 (the default) saves a few instructions. New sharpening equation that gives cleaner shapes - around points and fine lines it changes the surrounding pixels as much as the detail. Both Fake GI and sharpening now eases off smoothly near min and max instead of just clamping. (*) Added "Cinematic DOF safe mode" option which disables AO and tweaks Fake GI to avoid artefacts in scenes that have a strong depth of field (out of focus background) effect. Allow tonemapping compensation in 10-bit SDR mode. Tweaked adaptive contrast to not overbrighten. 16 bit color mode fix for oversaturated GI color. HDR setup now uses ReShade's new BUFFER_COLOR_SPACE definition to help detect right default. Reworked all HDR code and settings.

5.1.1 (+) Tweaked approximate PQ curve constants. Fixed probably insignificant error in (< vs <=) in sRGB curve.

5.1 (x) Option to tell Glamayre if game is really HDR, or SDR, if it detects 10 bit output. (+) Faster approximate PQ curve for HDR. (x) Fixed artefacts when using Fake GI offset with FSR or DLSS (cause was different resolution depth buffer).

5.0 (+) Faster AO. (+) Fake GI: better contrast, better color, if depth is available. (*) Fake GI: large scale AO, enhanced local AO and bounce. (*) HDR mode with automatic detection of format. Automatically uses sRGB curve, PQ curve or linear depending on whether input is 8, 10 or 16 bit. (*) Tone mapping compensation - in SDR mode this gives us better Fake GI. (+) Fix banding artefacts on high sharp values.

4.4_beta (*) Experimental HDR support

4.3 (+) More Fake GI tweaks. It should look better and not have any serious artefacts in difficult scenes. Amount applied is limited to avoid damaging details and it is actually simpler now - gi.w is gone (potentially free for some future use.)

4.2.1 (+) In response to feedback about the effect not being strong enough I have increased the maximum Fake GI strength and contrast.

4.2 (+) Improved quality of fake global illumination, and clearer settings for it. Tweaked defaults.

4.1.1 (x) Fixed typo that caused failure on older versions of ReShade (though current version 4.9.1 was okay)

4.1 (x) Fixed bug affecting AO quality for some FAST_AO_POINTS. Improved variance calculation for AO. Fixed compiler warnings.

4.0 (+) Allow stronger sharpening. Optimized Ambient Occlusion to use fewer instructions and registers, which should fix compilation issues on some D3D9 Games. Faster and better quality Fake Global illumination; also more tweakable. Make bounce lighting faster and smoother by using using some blurred values we calculate anyway for Fake GI.

3.2 (x) Make quality setting Preprocessor only as someone reported compatility issues with Prince of Persia  - looks like too many registers so need to simplify. Change AO to use float4x4 to sav intructions and registers.

3.1 (+) Option to Reduce AO in bright areas. Helps prevent unwanted shadows in bright transparent effects like smoke and fire.

3.0 (+) More tweaks to Fake GI - improving performance and smoothness. Improved FXAA quality (fixed rare divide by zero and fixed subtle issue where Fake GI combined badly with FXAA). Improved sharpen quality (better clamp limits). Simplified main settings. Added dropdown to select from two AO quality levels. Bounce tweaked and moved bounce strength to advanced settings (it is now equal to ao_strength by default - having them different is usually worse.)

2.1 (+) Smoother blur for Fake Global Illumination light, which fixes artefacts (thank you Alex Tuderan). Tweaked defaults. Minor tweaks elsewhere. Smoother where GI meets sky detect.

2.0 (*) Fake Global Illumination! Even better than the real thing (if speed is your main concern!)

1.2.3 (x) Regression fix: AO shade color when not using bounce. Actually tweaked this whole section so it works like bounce lighting but using the current pixel instead of reading nearby ones, and simplified the code in places too. 

1.2.2 (+) bounce_lighting performance improvement!

1.2.1 (x) Bugfix: didn't compile in OpenGL games.

1.2 (+) Better AO. Smoother shading transition at the inner circle of depth sample points - less artefacts at high strength. Tweaked defaults - The shading improvements enabled me go back to default FAST_AO_POINTS=6 for better performance. Allow slightly deeper shade at max.

1.1 (+) Improved sharpening. Tweaked bounce lightling strength. Tweaked defaults. Simplified settings. Quality is now only set in pre-processor - to avoid problems.

1.0 (-) Initial public release

Thank you:

Alex Tuduran for the previous blur algorithm, suggestions and inspiration for the brightness part of Fake GI algorithm.

macron, AlucardDH, NikkMann, Mirt81, distino, vetrogor, illuzio, geisalt for feedback and bug reports.

ReShade devs for ReShade.

Glamarye?
----------

In the Andrzej Sapkowski's Witcher novels, [Glamayre](https://witcher.fandom.com/wiki/Glamour) is magical make-up. Like Sapkowski's sourceresses, The Witcher 3 is very beautiful already, but still likes a bit of Glamayre.

	
