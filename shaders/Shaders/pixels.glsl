#include <perlin>
#include <math_constants>
#include <multisample>

parameter vec2 offset = vec2(0.0, 0.0);
parameter float timeScale = 0.2 : range(0.03, 0.5);
parameter float zoomParam = 1.0 : logrange(0.1, 10.0);

const float thisSaturation = 0.8;
const float coordMult = 1000.0;

// Generates a rotation matrix
glsl mat2 rotMat(float t) {
    return mat2(
        cos(t), -sin(t),
        sin(t), cos(t));
}

// Converts colors in HSV space to RBG space
glsl vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * normalize(mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y));
}

glsl float hueFromPos(vec2 pos, float zoom) {
  vec2 quantPos = floor(pos / zoom) * zoom;
  float hue = perlinNoise(quantPos / (coordMult * 1.5));
  return hue;
}

glsl vec4 drawPixels(vec2 pos) {
    // Transform coordinate space
    vec2 scale = vec2(coordMult);
    vec2 origin = scale / 2;
    pos = (pos + offset) * scale;

    float scaledTime = shadron_Time * timeScale * PI;

    // Rotate coordinate around origin
    float t = -scaledTime; // Rotation angle, theta
    pos = rotMat(t) * (pos - origin);

    float zoom = zoomParam * (sin(scaledTime) * 15 + 65);
    float stepPos = sin(scaledTime) * 0.752 - 0.0325;
    float upperValue = step(stepPos, perlinNoise(floor(pos / zoom)));
    float upperHue = hueFromPos(pos, zoom);

    // Add drop shadow
    vec2 lowerOffset = rotMat(t + PI / 4) * vec2(-0.15, 0.15) * zoom;
    float lowerValue = 0.3 * step(stepPos, perlinNoise(floor((pos + lowerOffset) / zoom)));
    float lowerHue = hueFromPos(pos + lowerOffset, zoom);

    // Combine lower and upper
    float thisHue = upperValue == 0.0 ? lowerHue : upperHue;
    float thisValue = clamp(upperValue + lowerValue, 0.0, 1.0);

    // Add color
    vec3 thisColor = hsv2rgb(vec3(thisHue, thisSaturation, thisValue));

    return vec4(thisColor, 1.0);
}

animation pixels = glsl(multisample<drawPixels, 4>, 1920, 1080);
