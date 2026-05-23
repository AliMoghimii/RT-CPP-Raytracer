#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "random.glsl"
#include "material.glsl"

struct GPULight {
    vec3 position;
    float radius;
    vec3 color;
    float p2;
};

layout(std430, binding = 4) readonly buffer LightBuffer { GPULight lights[]; };

vec3 lambertianShading(GPUMaterial mat, vec3 baseColor, vec3 hitNormal, vec3 lightDir, vec3 lightColor) {
    return baseColor * mat.diffuse * max(dot(hitNormal, lightDir), 0.0) * lightColor;
}

vec3 blingPhongShading(GPUMaterial mat, vec3 lightColor, vec3 hitNormal, vec3 lightDir, vec3 viewDir, float specExp) {
    vec3 halfVector = normalize(lightDir + viewDir);
    return lightColor * mat.specular * pow(max(dot(hitNormal, halfVector), 0.0), specExp);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 pbrShading(GPUMaterial mat, vec3 baseColor, vec3 N, vec3 V, vec3 L, vec3 lightColor) {
    vec3 H = normalize(V + L);
    vec3 F0 = mix(vec3(0.04), baseColor, mat.metallic);

    float NDF = DistributionGGX(N, H, mat.roughness);
    float G   = GeometrySmith(N, V, L, mat.roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    vec3  kD    = (vec3(1.0) - F) * (1.0 - mat.metallic);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * baseColor / PI + specular) * lightColor * NdotL;
}

#endif