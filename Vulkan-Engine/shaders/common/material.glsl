#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 9) uniform sampler2D textures[];

struct GPUMaterial {
    vec3 color;
    float ambient;
    vec3 emission;
    float diffuse;
    vec3 color2;
    float specular;

    float reflection;
    float transparency;
    float ior;

    int shadingModel;

    int patternType;
    float roughness;
    float metallic;

    int castShadows;

    int useTexture;
    int albedoIndex;
    int normalMapIndex;
    int roughnessIndex;

    int aoIndex;
    int heightMapIndex;
    float proceduralScale;
    float proceduralWobble;

    float bumpStrength;
    float parallaxScale;
    float p5;
    float p6;
};

layout(std430, binding = 1) readonly buffer MaterialBuffer { GPUMaterial materials[]; };

mat3 getTBN(vec3 n) {
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

vec3 sampleAlbedo(GPUMaterial mat, vec2 uv) {
    if (mat.useTexture == 1 && mat.albedoIndex >= 0) {
        vec3 texCol = textureLod(textures[nonuniformEXT(mat.albedoIndex)], uv, 0.0).rgb;
        return pow(max(texCol, vec3(0.0)), vec3(2.2));
    }
    return mat.color;
}

#endif