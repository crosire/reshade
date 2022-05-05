/**
 * Calculate the approximate gradients of an image on both the X and Y axis.
 * Vertical gradient is mapped to the green channel, horizontal gradient is
 * mapped to the red channel.
 */

#include <hsv>

image Source = input();

parameter float original = 0.1 : range(0.0, 1.0);
parameter float vertical = 1.0 : range(0.0, 1.0);
parameter float horizontal = 1.0 : range(0.0, 1.0);
parameter int power = 5 : range(2, 10);

glsl float gradientFromOffset(vec2 pos, vec2 offset) {
    vec2 leftPos = pos + offset;
    vec2 rightPos = pos - offset;

    float leftPixel = texture(Source, leftPos).r;
    float rightPixel = texture(Source, rightPos).r;

    float localGradient = (leftPixel - rightPixel) / 2;
    // Increase strength of gradient
    float intenseGradient = 1 - pow(1 - localGradient, power);

    // Transform gradient from domain of -1..1 to 0..1
    return (intenseGradient + 1) / 2;
}

glsl vec4 gradient(vec2 pos) {
    vec2 offsetH = vec2(1 / float(shadron_Dimensions.x), 0.0);
    vec2 offsetV = vec2(0.0, 1 / float(shadron_Dimensions.y));

    vec3 pixel = vec3(
        gradientFromOffset(pos, offsetH) * horizontal,
        gradientFromOffset(pos, offsetV) * vertical,
        0.0
    ) + value(texture(Source, pos)) * original;

    return vec4(pixel, 1.0);
}

image Gradient = glsl(gradient, 1600, 900);
