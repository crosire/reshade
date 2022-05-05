#include <multisample>
#include <perlin>

parameter float timeScale = 0.5 : range(0.1, 1.0);
const float numPeaks = 20;

glsl vec4 waves(vec2 pos) {
    vec3 waveSample = vec3(0.1);
    float sinValue = sin(pos.x * numPeaks * 2) / 35 + 0.5;
    // Add noise and time components
    sinValue += perlinNoise(vec2((pos.x + shadron_Time * timeScale) * 2, 0.0)) / 4;

    if (pos.y < sinValue) {
        // Everything below the wave is white
        waveSample = vec3(1.0);
    } else {
        // Give drop shadow to wave
        waveSample = mix(vec3(0.05), waveSample, clamp((pos.y - sinValue) * 50, 0.0, 1.0));
    }

    return vec4(waveSample, 1.0);
}

animation waveAnimation = glsl(multisample<waves, 4>, 1600, 900);
