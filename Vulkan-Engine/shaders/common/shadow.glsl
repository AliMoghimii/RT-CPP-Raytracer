#ifndef SHADOW_GLSL
#define SHADOW_GLSL

// Traverses the BVH for shadow attenuation, accumulating color tinting through
// transparent surfaces. Returns vec3(1) if the path is unobstructed, vec3(0) if
// fully blocked, and a tinted value for transparent occluders in between.
// Requires: traverseBVH (bvh.glsl), GPUMaterial / materials[] (material.glsl).
// Include this file AFTER both bvh.glsl and material.glsl.
vec3 traverseBVHShadow(Ray ray, int bvhCount) {
    vec3 attenuation = vec3(1.0);
    for (int pass = 0; pass < 4; pass++) {
        HitInfo hit;
        if (!traverseBVH(ray, bvhCount, hit)) break;

        GPUMaterial mat = materials[hit.materialIndex];

        if (mat.castShadows == 0) {
            ray.tMin = hit.t + 0.001;
            continue;
        }
        if (mat.transparency > 0.01) {
            attenuation *= mat.color * mat.transparency;
            if (dot(attenuation, attenuation) < 0.001) return vec3(0.0);
            ray.tMin = hit.t + 0.001;
        } else {
            return vec3(0.0);
        }
    }
    return attenuation;
}

#endif
