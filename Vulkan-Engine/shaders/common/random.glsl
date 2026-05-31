#ifndef RANDOM_GLSL
#define RANDOM_GLSL

const float PI = 3.14159265359;

uint randState;

float rand() {
    randState = randState * 747796405u + 2891336453u;
    uint word = ((randState >> ((randState >> 28u) + 4u)) ^ randState) * 277803737u;
    return float((word >> 22u) ^ word) / 4294967295.0;
}

vec2 randomPointOnDisk() {
    float angle = rand() * 2.0 * PI;
    float r = sqrt(rand());
    return vec2(r * cos(angle), r * sin(angle));
}

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
        mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
        mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
        f.z
    );
}

float fbm(vec3 x) {
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100.0);
    for (int i = 0; i < 5; ++i) {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

#endif