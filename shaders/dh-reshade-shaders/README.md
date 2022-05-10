# dh-reshade-shaders
Shaders for ReShade injector

## DH_Lain
This shader tries to reproduce the design of the shadows from the Anime "Serial Experiment Lain".

For example :
### Half-life
![DH_Lain](screenshots/dh_lain/hl_original.png?raw=true "DH_Lain")
![DH_Lain](screenshots/dh_lain/hl_lain.png?raw=true "DH_Lain")



## DH_Anime
This shader can be used to create drawing/anime/manga effects :
- lining
- shading
- saturation

For example :
### Skyrim - Original
![Skyrim - Original](screenshots/dh_anime/TESV%202019-11-05%2013-54-50%20original.png?raw=true "Skyrim - Original")

### Skyrim - DH_Anime
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-54-50.png?raw=true "Skyrim - DH_Anime")
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-55-17.png?raw=true "Skyrim - DH_Anime")
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-56-23.png?raw=true "Skyrim - DH_Anime")
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-56-34.png?raw=true "Skyrim - DH_Anime")
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-57-14.png?raw=true "Skyrim - DH_Anime")
![Skyrim - DH_Anime](screenshots/dh_anime/TESV%202019-11-05%2013-57-31.png?raw=true "Skyrim - DH_Anime")


## DH_Undither
This shader reduces dithering.
It doesn't use Depth Buffer so it can be used with DosBox in old games.

For example :
### Little Big Adventure 2
![DH_Undither](screenshots/dh_undither/dh_undither1.png?raw=true "DH_Undither")
![DH_Undither](screenshots/dh_undither/dh_undither2.png?raw=true "DH_Undither")


## DH_Uniformity_correction
This shader tries to correct screen uniformity.

### HOW TO

1. Run a game with ReShade and the shader enabled, check "Solid color", set "Correction" to 0 and set "Brightness" to a value that shows imperfections the best

2. Take a photo of the screen in the dark before correction
![Photo of the screen before correction](screenshots/dh_uniformity_correction/0_without_correction.jpg?raw=true "Photo of the screen before correction")

3. From this, create a texture of the defects by cropping and spreading histogram in a image editor (like Photoshop or Paint.net)
![Defects of the screen](screenshots/dh_uniformity_correction/1_dh_uniformity_correction.jpg?raw=true "Defects of the screen")
Save this file in Textures folder with the name "dh_uniformity_correction.bmp"

4. Restart the game with the new texture and increase "Correction" until you don't see imperfections anymore. Finally uncheck "Solid color"
No correction test (photo and amplified contrasts for visibility):
![photo](screenshots/dh_uniformity_correction/2_without_correction_test.jpg?raw=true "photo")
![amplified contrasts for visibility](screenshots/dh_uniformity_correction/2_without_correction_test_amplified.jpg?raw=true "amplified contrasts for visibility")

Correction set to 0.20 :
![photo](screenshots/dh_uniformity_correction/3_correction_20.jpg?raw=true "photo")
![amplified contrasts for visibility](screenshots/dh_uniformity_correction/3_correction_20-amplified.jpg?raw=true "amplified contrasts for visibility")

Correction set to 0.25 :
![photo](screenshots/dh_uniformity_correction/4_correction_25.jpg?raw=true "photo")
![amplified contrasts for visibility](screenshots/dh_uniformity_correction/4_correction_25-amplified.jpg?raw=true "amplified contrasts for visibility")

5. ????

6. Profit !


