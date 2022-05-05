#include <multisample>
#include <perlin>

parameter float timeScale = 1.0 : range(0.5, 6.0);

glsl vec4 drawStars(vec2 pos) {
    vec2 origin = shadron_Dimensions / 2;
    vec2 loc = pos * shadron_Dimensions;
    float distFromOrigin = distance(loc, origin);

    float noiseMult = 10 + distFromOrigin + shadron_Time * timeScale;

    // Color some of the stars
    float blueValue = step(0.75, perlinNoise((pos + vec2(0.33, -0.1)) * noiseMult));
    float redValue = step(0.75, perlinNoise((pos + vec2(0.39, 0.94)) * noiseMult));
    vec4 red = vec4(redValue, 0.0, 0.0, redValue);
    vec4 blue = vec4(0.0, 0.0, blueValue, blueValue);
    vec4 background = red + blue + vec4(1.0);

    vec4 starMask = vec4(vec3(0.0), 1.0 - step(0.9, perlinNoise(pos * noiseMult)));

    return vec4(mix(background.rgb, starMask.rgb, starMask.a), 1.0);
}

animation distortionField = glsl(multisample<drawStars, 4>, 1600, 900);
