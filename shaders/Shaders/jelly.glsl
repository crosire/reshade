/**
 * NOTE: This file requires a multisampleFeedback function which isn't included
 * with shadron as of the time I last checked.
 */
#extension ffmpeg

#include <multisample>
#include <hsv>
#include <math_constants>
#include <perlin>

const ivec2 DIMENSIONS = ivec2(1600, 900);

sound audioInput = file();
parameter float baseRadius = 0.3 : range(0.2, 0.5);
parameter vec4 jellyColor = vec4(0.0, 1.0, 1.0, 1.0) : color();

/**
 * Convert an RGB vector into an HSV vector.
 *
 * vec3 rgbVec : A color vector in RGB format.
 * returns : A color vector in HSV format.
 */
glsl vec4 toHSV(vec4 rgbVec) {
    return vec4(hue(rgbVec), saturation(rgbVec), value(rgbVec), rgbVec.a);
}

/**
 * Convert an HSV vector into an RGB vector.
 *
 * vec3 hsvVec : A color vector in HSV format.
 * returns : A color vector in RGB format.
 */
glsl vec4 toRGB(vec4 hsvVec) {
    return vec4(hsv(hsvVec.x, hsvVec.y, hsvVec.z), hsvVec.a);
}

/**
 * Return an amplitude given a specified angle.
 *
 * float angle : A value in the range -PI..PI.
 * returns : A vec2 of two values in the range 0..1.
 */
glsl vec2 freqSample(float angle) {
    // Convert the range of angle from -PI..PI to 0..1
    float normalizedAngle = (angle + PI) / (2 * PI);
    // Get the the amplitude at the frequency specified by the angle, but first
    // restrict the frequency range by a half, since not much happens in the
    // upper half.
    vec2 lrSample = shadron_Spectrum(audioInput, normalizedAngle / 2);
    // A scaling function that scales back values near 0 and 1. This corresponds
    // to a semi-circle from 0 to 1 with a maximum radius of 1.
    float scale = sqrt(1 - pow((normalizedAngle * 2 - 1), 2));

    // Scale the amplitudes
    return pow(scale, 2) * lrSample;
}

/**
 * Call freqSample multiple times around a specified angle and average the results.
 *
 * float angle: A value in the range -PI..PI.
 * int numSamples: Number of times to call freqSample. Should be greater than 1.
 * returns : A vec2 of two values in the range 0..1.
 */
glsl vec2 smoothFreqSample(float angle, int numSamples) {
    vec2 sum = vec2(0.0);
    float stepSize = 0.005;
    int perDirection = int(floor(numSamples / 2.0));

    for (int i = 1; i <= perDirection; i++)
        sum += freqSample(max(angle - (i * stepSize), -PI));

    for (int i = 0; i <= perDirection; i++)
        sum += freqSample(min(angle + (i * stepSize), PI));

    return sum / numSamples;
}

/**
 * Provides a radius that varies with time.
 *
 * float timeIndex: When to sample the radius for.
 * returns: Radius modifier.
 */
glsl float radiusFunc(float timeIndex) {
    return 0.04 * sin(timeIndex) +
           0.03 * sin(timeIndex * 0.5 + 1.0) +
           0.03 * sin(timeIndex * 0.3 + 0.5);
}

/**
 * Starts the feedback loop with a transparent background.
 */
glsl vec4 initViz(vec2 pos) {
    return vec4(0.0);
}

/**
 * Draw an animated jellyfish-like shape that responds to audio.
 */
glsl vec4 jellyViz(sampler2D self, vec2 pos, float deltaTime) {
    // Scale radius with time
    float radius = baseRadius + radiusFunc(shadron_Time);
    // Scale the X-dimension of the center and position vectors appropriately,
    // given the images aspect ratio.
    vec2 center = vec2(0.5 * shadron_Aspect, 0.5);
    vec2 scaledPos = vec2(pos.x * shadron_Aspect, pos.y * 2.5 - 1.0);
    float distFromOrigin = distance(scaledPos, center);

    // Vector from the center of the image to the current position.
    vec2 normalVec = scaledPos - center;
    // Calculate the angle of the normal vector from the X-axis.
    float angle = atan(normalVec.y, normalVec.x);

    // Get the FFT data for both channels
    vec2 ripples = smoothFreqSample(angle, 5);

    // Use the FFT as a radius modifier
    bool innerCircle = distFromOrigin < (radius + 0.02 * (ripples.x + ripples.y));
    bool outerCircle = innerCircle || distFromOrigin < (radius * 1.06 + 0.04 * (ripples.x + ripples.y));

    // Hue-shift the right channel color with the distance from the center
    vec4 rightHSV = toHSV(jellyColor);
    float hueShift = pow(7 * (distFromOrigin - radius), 2);
    vec4 rightHueShift = toRGB(vec4(rightHSV.x + hueShift, rightHSV.y, rightHSV.z, rightHSV.a));

    // Sample previous frame
    vec2 offsetPos = vec2(pos.x, min(1.0, pos.y + 0.002));
    // Progressively decay older frames' opacity
    vec4 previousSample = texture(self, offsetPos) * vec4(vec3(1.0), 0.93);

    vec4 waveSample = innerCircle ? previousSample : (outerCircle ? rightHueShift : previousSample);

    return waveSample;
}

feedback Jellyfish = glsl(multisampleFeedback<jellyViz, 3>, DIMENSIONS) : initialize(initViz), full_range(true), filter(nearest);

/**
 * Draw an animated stary background.
 */
glsl vec4 background(vec2 pos) {
    float starNoise = (perlinNoise(pos * 80.0) + perlinNoise(pos * 200.0)) / 2.0;
    float stars = pow(smoothstep(0.7, 0.9, starNoise), 2);
    float sparkle = (shadron_Spectrum(audioInput, abs(pos.x * 2.0 - 1.0)).x +
                    shadron_Spectrum(audioInput, abs(pos.y * 2.0 - 1.0)).y) *
                    perlinNoise(pos * 4.0 + shadron_Time / 4.0);
    vec3 sparklyStars = vec3(sparkle * stars + 0.4 * stars);

    return vec4(sparklyStars, 1.0);
}

animation Background = glsl(background, DIMENSIONS);

/**
 * Composite together the background with the foreground.
 */
glsl vec4 combineLayers(vec2 pos) {
    vec3 bg = texture(Background, pos).rgb;

    // jelly position adjustments are 1 second behind radius changes
    vec2 jellyPos = vec2(pos.x, pos.y + radiusFunc(shadron_Time - 1.0) * 0.75);
    vec4 fg = texture(Jellyfish, jellyPos);

    return vec4(mix(bg, fg.rgb, fg.a), 1.0);
}

animation Composite = glsl(combineLayers, DIMENSIONS);

export mp4(Composite, "output.mp4", h264, yuv420, "preset=slow,crf=15", 60.0, 200);
