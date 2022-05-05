#include <multisample>
#include <hsv>
#include <math_constants>

sound audioInput = file();
parameter float radius = 0.25 : range(0.1, 0.4);
parameter vec3 rightColor = vec3(0.0, 1.0, 1.0) : color();
parameter vec3 leftColor = vec3(0.05) : color();

/**
 * Convert an RGB vector into an HSV vector.
 *
 * vec3 rgbVec : A color vector in RGB format.
 * returns : A color vector in HSV format.
 */
glsl vec3 toHSV(vec3 rgbVec) {
    return vec3(hue(rgbVec), saturation(rgbVec), value(rgbVec));
}

/**
 * Convert an HSV vector into an RGB vector.
 *
 * vec3 hsvVec : A color vector in HSV format.
 * returns : A color vector in RGB format.
 */
glsl vec3 toRGB(vec3 hsvVec) {
    return hsv(hsvVec.x, hsvVec.y, hsvVec.z);
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
 * Call freqSample 3 times around a specified angle and average the results.
 *
 * float angle: A value in the range -PI..PI.
 * returns : A vec2 of two values in the range 0..1.
 */
glsl vec2 smoothFreqSample(float angle) {
    vec2 sample1 = freqSample(max(angle - 0.005, -PI));
    vec2 sample2 = freqSample(angle);
    vec2 sample3 = freqSample(min(angle + 0.005, PI));

    return (sample1 + sample2 + sample3) / 3;
}

/**
 * Draw a sample from a provided coordinate on the screen.
 *
 * vec2 pos : The position of the sample.
 * returns : A sample.
 */
glsl vec4 visualizer(vec2 pos) {
    // Scale the X-dimension of the center and position vectors appropriately,
    // given the images aspect ratio.
    vec2 center = vec2(0.5 * shadron_Aspect, 0.5);
    vec2 scaledPos = vec2(pos.x * shadron_Aspect, pos.y);
    float distFromOrigin = distance(scaledPos, center);

    // Vector from the center of the image to the current position.
    vec2 normalVec = scaledPos - center;
    // Calculate the angle of the normal vector from the X-axis.
    float angle = atan(normalVec.x, normalVec.y);

    vec2 ripples = smoothFreqSample(angle);

    // Left channel circle
    bool leftCircle = distFromOrigin < (radius + 0.02 * ripples.x);
    // Right channel circle
    bool rightCircle = distFromOrigin < (radius * 1.008 + 0.04 * ripples.y);

    // Hue-shift the right channel color with the amplitude
    vec3 rightHSV = toHSV(rightColor);
    vec3 rightHueShift = toRGB(vec3(rightHSV.x + 0.5 * ripples.y, rightHSV.y, rightHSV.z));

    vec3 thisSample = leftCircle ? leftColor : (rightCircle ? rightHueShift : vec3(0.02));

    return vec4(thisSample, 1.0);
}

animation soundAnimation = glsl(multisample<visualizer, 4>, 1280, 720);
