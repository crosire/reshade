#include <math_constants>
#include <multisample>
#include <perlin>

parameter float timeScale = 30.0 : range(10.0, 50.0);
parameter float stripeSize = 60.0 : range(1.0, 100.0);
parameter int numSpirals = 1 : range(1, 20);
parameter bool invert = false;

glsl vec4 drawSpiral(vec2 pos) {
    vec2 origin = shadron_Dimensions / 2;
    vec2 loc = pos * shadron_Dimensions;
    vec2 normalized = normalize(loc - origin);
    float distFromOrigin = distance(loc, origin);

    vec3 color1 = vec3(0.42, 0.18, 0.45);
    vec3 color2 = vec3(1.0);

    // Animation and spiraling
    float animationTime = shadron_Time * timeScale;
    float angularComponent = (atan(normalized.x, normalized.y) / PI) * (stripeSize * numSpirals);

    // Wavy edges
    // float numRotations = distFromOrigin - mod(distFromOrigin, stripeSize);
    // float noiseComponent = perlinNoise(vec2(angularComponent * 0.03 * distFromOrigin / stripeSize, 0.0)) * stripeSize / 10;

    // Stipes
    float stripe = mod(distFromOrigin - animationTime + angularComponent, (stripeSize * 2));
    bool ringType = (stripe - stripeSize) >= 0;

    vec3 thisColor = ringType ? color1 : mix(color2, vec3(0.0), stripe / (stripeSize * 2));

    if (invert) {
        thisColor = 1.0 - thisColor;
    }

    return vec4(thisColor, 1.0);
}

animation Spiral = glsl(multisample<drawSpiral, 4>, 1600, 900);
// export png_sequence(Spiral, "spiral_?.png", 50, 4);
