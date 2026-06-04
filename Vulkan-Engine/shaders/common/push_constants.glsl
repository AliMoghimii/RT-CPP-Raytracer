#ifndef PUSH_CONSTANTS_GLSL
#define PUSH_CONSTANTS_GLSL

layout(push_constant) uniform CameraData {
    vec4 camPos;
    vec4 camForward;
    vec4 camRight;
    vec4 camUp;
    int sphereCount;
    int triangleCount;
    int planeCount;
    int quadCount;
    int cubeCount;
    int lightCount;
    int bvhCount;
    int maxDepth;
    int shadowRays;
    int primaryRaysPerPixel;
    float focalDistance;
    float lensRadius;

    vec3 fogColor;
    int enableFog;

    vec3 skyBottomColor;
    int enableSkybox;

    vec3 skyTopColor;
    int enableTextures;

int totalEmittedPhotons;
    int enableCaustics;
    float causticIntensity;
    float padding;

    vec4 gridMin;
    vec4 gridMax;
    int gridRes;
    float gatherRadius;
} cam;
#endif