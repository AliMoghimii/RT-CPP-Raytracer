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

// Lambert diffuse: intensity proportional to the cosine of the angle between the surface
// normal and the light direction (Lambert's cosine law). mat.diffuse scales the contribution.
vec3 lambertianShading(GPUMaterial mat, vec3 baseColor, vec3 hitNormal, vec3 lightDir, vec3 lightColor) {
    return baseColor * mat.diffuse * max(dot(hitNormal, lightDir), 0.0) * lightColor;
}

// Blinn-Phong specular: uses the half-vector (bisector between L and V) as a cheaper
// approximation to Phong. Larger specExp = tighter, shinier highlight.
vec3 blingPhongShading(GPUMaterial mat, vec3 lightColor, vec3 hitNormal, vec3 lightDir, vec3 viewDir, float specExp) {
    vec3 halfVector = normalize(lightDir + viewDir);
    return lightColor * mat.specular * pow(max(dot(hitNormal, halfVector), 0.0), specExp);
}

// GGX/Trowbridge-Reitz Normal Distribution Function (NDF).
// Models the statistical distribution of microfacet normals: how many microfacets are
// aligned with the half-vector H at roughness alpha. Rough surfaces spread the highlight;
// smooth surfaces concentrate it. Alpha is roughness^2 (perceptually linear in alpha^2).
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

// Schlick's approximation to the Smith masking-shadowing function for one direction.
// Accounts for microfacets that are either shadowed (blocked from the light) or masked
// (blocked from the view). k = (roughness+1)^2/8 is the remapped roughness for direct lighting.
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's combined masking-shadowing: separates the view side (G1_V) from the light side
// (G1_L) and multiplies them, assuming statistical independence.
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick's approximation to the Fresnel reflectance equation.
// F0 is the specular reflectance at normal incidence (0 degrees). For dielectrics
// F0 ~= 0.04 (4%); for metals, F0 is the tinted specular color.
// As the viewing angle grazes 90 degrees (cosTheta -> 0), reflectance approaches 1.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cook-Torrance microfacet BRDF for physically-based rendering.
// The BRDF is: f = kD * (albedo/PI) + kS * (NDF * G * F) / (4 * NdotV * NdotL)
//   kD = diffuse fraction  = (1 - F) * (1 - metallic)
//         Fresnel energy conservation: reflected light (kS=F) cannot also be diffused.
//         Metals absorb all transmission so they have no diffuse (kD *= 1-metallic).
//   kS = F (Fresnel factor = specular fraction)
//   NDF = microfacet normal distribution (GGX)
//   G = masking-shadowing (Smith)
//   The denominator 4*NdotV*NdotL normalizes the microfacet solid angle.
//   0.0001 prevents division by zero at grazing angles.
vec3 pbrShading(GPUMaterial mat, vec3 baseColor, vec3 N, vec3 V, vec3 L, vec3 lightColor) {
    vec3 H = normalize(V + L);                          // half-vector between view and light
    vec3 F0 = mix(vec3(0.04), baseColor, mat.metallic); // dielectric (0.04) -> metal (albedo)

    float NDF = DistributionGGX(N, H, mat.roughness);
    float G = GeometrySmith(N, V, L, mat.roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - mat.metallic);  // energy-conserving diffuse weight
    float NdotL = max(dot(N, L), 0.0);

    return (kD * baseColor / PI + specular) * lightColor * NdotL;
}

#endif