#ifndef TLAS_GLSL
#define TLAS_GLSL

// TLAS/BLAS animated-instance support.
//
// Each Mesh is a BLAS: its triangles + local-space BVH live appended inside the shared
// sceneTriangles/sceneBVH arrays (see ModelLoader::appendBLAS / Mesh::load), built ONCE.
// A MeshInstance places that BLAS in the world via a 4x4 transform, refreshed every frame
// on the CPU (cheap mat4 math -- no BVH rebuild). Rays are transformed into the BLAS's
// local space (the classic "ray-transform" trick) so the proven static traversal loop
// (traverseBVHFrom / traverseBVHShadowFrom) can be reused verbatim.
//
// The TLAS itself is a trivial single leaf: with only a handful of instances, a real
// BVH tree buys nothing, so tlasNodes[0] is one node listing every instance, and its
// triCount field doubles as "instance count" (mirrors how bvhCount==0 already disables
// static traversal -- see Architecture Decision 5 in the implementation plan).

#include "bvh.glsl"
#include "shadow.glsl"

struct GPUInstance {
    mat4 transform;       // local -> world
    mat4 invTransform;    // world -> local; transpose(mat3(.)) = normal matrix
    vec3 worldAabbMin;
    int  blasRootIndex;   // root node index into the shared bvhNodes[] (sceneBVH)
    vec3 worldAabbMax;
    int  materialOverride; // -1 = use the triangle's own materialIndex
};

layout(std430, set = 0, binding = 14) readonly buffer InstanceBuffer { GPUInstance instances[]; };
layout(std430, set = 0, binding = 15) readonly buffer TLASBuffer     { GPUBVHNode tlasNodes[]; };

// Transforms a world-space ray into a BLAS's local space. The direction is transformed
// WITHOUT renormalizing (w=0): this keeps the hit parameter `t` numerically identical in
// both spaces (t_local == t_world), so the world hit position can be safely recomputed as
// `worldRay.origin + worldRay.direction * t` -- never trust the BLAS-local hit position.
Ray transformRayToLocal(Ray worldRay, mat4 invTransform) {
    Ray localRay;
    localRay.origin = vec3(invTransform * vec4(worldRay.origin, 1.0));
    localRay.direction = vec3(invTransform * vec4(worldRay.direction, 0.0));
    localRay.tMin = worldRay.tMin;
    localRay.tMax = worldRay.tMax;
    return localRay;
}

// Nearest-hit traversal of the whole scene: static geometry first (unchanged proven path,
// preserving the analytical-walls/BVH-shadow-perf rules), then any dynamic instances.
// World hit position/normal/material are reconstructed from the world ray + shared `t` --
// see transformRayToLocal's doc comment for why this is exact.
//
// `bvhCount` gates ONLY the static traversal (it is the STATIC region's node count -- see
// Renderer::staticBvhNodeCount). It must NOT be reused to gate BLAS traversal: when the
// static scene is empty, a BLAS's appended subtree lands at node 0, and traverseBVH (always
// rooted at 0) would wrongly walk into it as "the static scene" if gated on a non-zero total.
// BLAS traversal is instead gated implicitly -- we're only here because instanceCount > 0,
// which guarantees valid appended BVH data exists -- so pass a constant non-zero count.
bool traverseScene(Ray ray, int bvhCount, out HitInfo hit) {
    bool hitAnything = traverseBVH(ray, bvhCount, hit);

    int instanceCount = tlasNodes[0].triCount;
    vec3 invDir = 1.0 / ray.direction;

    for (int i = 0; i < instanceCount; i++) {
        GPUInstance inst = instances[i];

        // World-space AABB cull before paying for the matrix multiply + local traversal.
        if (intersectAABB(ray, invDir, inst.worldAabbMin, inst.worldAabbMax) >= hit.t) continue;

        Ray localRay = transformRayToLocal(ray, inst.invTransform);
        localRay.tMax = hit.t;  // cap so the BLAS traversal can prune against the current best

        HitInfo localHit;
        if (traverseBVHFrom(localRay, inst.blasRootIndex, 1, localHit) && localHit.t < hit.t) {
            hit.t = localHit.t;
            hit.position = ray.origin + ray.direction * localHit.t;
            hit.normal = normalize(transpose(mat3(inst.invTransform)) * localHit.normal);
            hit.uv = localHit.uv;
            hit.materialIndex = (inst.materialOverride >= 0) ? inst.materialOverride : localHit.materialIndex;
            hitAnything = true;
        }
    }

    return hitAnything;
}

// Any-hit shadow traversal of the whole scene. Mirrors traverseScene's statics-then-
// instances ordering and early-outs the moment any opaque blocker is found. Material
// resolution honors materialOverride so e.g. a glass-override instance casts a glass
// shadow even if its BLAS was originally loaded with an opaque baseline material.
vec3 traverseSceneShadow(Ray ray, int bvhCount) {
    if (traverseBVHShadow(ray, bvhCount) == vec3(0.0)) return vec3(0.0);

    int instanceCount = tlasNodes[0].triCount;
    vec3 invDir = 1.0 / ray.direction;

    for (int i = 0; i < instanceCount; i++) {
        GPUInstance inst = instances[i];

        if (intersectAABB(ray, invDir, inst.worldAabbMin, inst.worldAabbMax) >= ray.tMax) continue;

        Ray localRay = transformRayToLocal(ray, inst.invTransform);

        int stack[BVH_STACK_SIZE];
        int stackPtr = 0;
        stack[stackPtr++] = inst.blasRootIndex;
        vec3 localInvDir = 1.0 / localRay.direction;
        float tMax = localRay.tMax;
        bool blocked = false;

        while (stackPtr > 0) {
            int nodeIdx = stack[--stackPtr];
            GPUBVHNode node = bvhNodes[nodeIdx];

            if (intersectAABB(localRay, localInvDir, node.aabbMin, node.aabbMax) >= tMax) continue;

            if (node.triCount > 0) {
                for (int t = 0; t < node.triCount; t++) {
                    GPUTriangle tri = triangles[node.leftFirst + t];
                    float u, v;
                    float dist = triangleIntersect(localRay, tri.v0, tri.v1, tri.v2, u, v);
                    if (dist > localRay.tMin && dist < tMax) {
                        int matIdx = (inst.materialOverride >= 0) ? inst.materialOverride : tri.materialIndex;
                        GPUMaterial mat = materials[matIdx];
                        if (mat.castShadows != 0 && mat.transparency <= 0.01) { blocked = true; break; }
                    }
                }
                if (blocked) break;
            } else {
                if (stackPtr + 2 <= BVH_STACK_SIZE) {
                    stack[stackPtr++] = node.leftFirst;
                    stack[stackPtr++] = node.leftFirst + 1;
                }
            }
        }

        if (blocked) return vec3(0.0);
    }

    return vec3(1.0);
}

#endif
