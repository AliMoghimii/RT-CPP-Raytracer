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

// Blend weights for triplanar projection: each world axis contributes proportionally to
// how aligned the surface normal is with it. Sharpened (pow 4) so the blend stays tight
// near edges instead of smearing across large flat faces.
vec3 triplanarWeights(vec3 n) {
    vec3 w = pow(abs(n), vec3(4.0));
    return w / (w.x + w.y + w.z);
}

// Samples one texture three times -- once per world-axis projection (YZ, XZ, XY planes)
// -- and blends by `weights`. Lets UV-less meshes (e.g. OBJs with no `vt` data) wear a
// texture without stretch artifacts. `scale` sets the world-space tiling frequency.
vec3 sampleTriplanar(int texIndex, vec3 worldPos, vec3 weights, float scale) {
    vec3 p = worldPos * scale;
    vec3 cx = textureLod(textures[nonuniformEXT(texIndex)], p.yz, 0.0).rgb;
    vec3 cy = textureLod(textures[nonuniformEXT(texIndex)], p.xz, 0.0).rgb;
    vec3 cz = textureLod(textures[nonuniformEXT(texIndex)], p.xy, 0.0).rgb;
    return cx * weights.x + cy * weights.y + cz * weights.z;
}

// useTexture == 2: triplanar world-space projection (no UVs needed). Falls back to the
// regular UV-mapped / flat-color paths for useTexture == 1 / 0.
vec3 sampleAlbedo(GPUMaterial mat, vec2 uv, vec3 worldPos, vec3 worldNormal) {
    if (mat.useTexture == 2 && mat.albedoIndex >= 0) {
        float scale = mat.proceduralScale > 0.0 ? mat.proceduralScale : 1.0;
        vec3 texCol = sampleTriplanar(mat.albedoIndex, worldPos, triplanarWeights(worldNormal), scale);
        return pow(max(texCol, vec3(0.0)), vec3(2.2));
    }
    return sampleAlbedo(mat, uv);
}

// Procedural pattern helpers (hash → value noise → fbm, used by patternType 2 and 3)
float _patternHash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float _patternNoise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(_patternHash(i), _patternHash(i + vec3(1,0,0)), f.x),
            mix(_patternHash(i + vec3(0,1,0)), _patternHash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(_patternHash(i + vec3(0,0,1)), _patternHash(i + vec3(1,0,1)), f.x),
            mix(_patternHash(i + vec3(0,1,1)), _patternHash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float _patternFbm(vec3 x) {
    float v = 0.0, a = 0.5;
    vec3 shift = vec3(100.0);
    for (int i = 0; i < 5; ++i) { v += a * _patternNoise(x); x = x * 2.0 + shift; a *= 0.5; }
    return v;
}

// Applies procedural coloring on top of an already-sampled baseColor.
// Call this after sampleAlbedo() in any G-buffer fill pass, passing the hit world position.
vec3 applyProceduralPattern(GPUMaterial mat, vec3 baseColor, vec3 worldPos) {
    if (mat.useTexture != 1 || mat.patternType == 0) return baseColor;
    if (mat.patternType == 1) {  // checkerboard XZ
        float scale = mat.proceduralScale > 0.0 ? mat.proceduralScale : 3.0;
        float p = mod(floor((worldPos.x + 5.0) * scale) + floor(worldPos.z * scale), 2.0);
        return (p > 0.5) ? mat.color2 : mat.color;
    }
    if (mat.patternType == 2) {  // wood grain
        float spacing = mat.proceduralScale  > 0.0 ? mat.proceduralScale  : 16.0;
        float wobble  = mat.proceduralWobble > 0.0 ? mat.proceduralWobble : 3.0;
        float wood    = fract(length(worldPos.xz) * spacing + _patternFbm(worldPos * 2.0) * wobble);
        return mix(mat.color2, mat.color, wood);
    }
    if (mat.patternType == 3) {  // marble
        float scale  = mat.proceduralScale > 0.0 ? mat.proceduralScale : 2.0;
        float marble = sin(worldPos.x * 5.0 + _patternFbm(worldPos * scale) * 10.0) * 0.5 + 0.5;
        return mix(mat.color, mat.color2, marble);
    }
    return baseColor;
}

#endif