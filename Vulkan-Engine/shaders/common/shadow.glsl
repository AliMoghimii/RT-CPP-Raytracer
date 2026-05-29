#ifndef SHADOW_GLSL
#define SHADOW_GLSL

// Any-hit BVH shadow traversal. Returns vec3(0) on the first opaque blocker found,
// vec3(1) if the path is clear. Transparent BVH objects are treated as non-occluders
// (they do not tint shadows); planes/quads handle transparency via analyticalShadow.
// This is ~4x faster than the old nearest-hit loop because:
//   (a) Single pass — no 4x traverseBVH loop
//   (b) Early exit as soon as any opaque triangle is found
vec3 traverseBVHShadow(Ray ray, int bvhCount) {
    if (bvhCount == 0) return vec3(1.0);

    int stack[BVH_STACK_SIZE];
    int stackPtr = 0;
    stack[stackPtr++] = 0;
    vec3  invDir = 1.0 / ray.direction;
    float tMax   = ray.tMax;

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        GPUBVHNode node = bvhNodes[nodeIdx];

        float distAABB = intersectAABB(ray, invDir, node.aabbMin, node.aabbMax);
        if (distAABB >= tMax) continue;

        if (node.triCount > 0) {
            for (int i = 0; i < node.triCount; i++) {
                GPUTriangle tri = triangles[node.leftFirst + i];
                float u, v;
                float t = triangleIntersect(ray, tri.v0, tri.v1, tri.v2, u, v);
                if (t > ray.tMin && t < tMax) {
                    GPUMaterial mat = materials[tri.materialIndex];
                    if (mat.castShadows != 0 && mat.transparency <= 0.01)
                        return vec3(0.0);  // opaque blocker: early exit
                    // transparent or no-shadow: keep traversing
                }
            }
        } else {
            if (stackPtr + 2 <= BVH_STACK_SIZE) {
                stack[stackPtr++] = node.leftFirst;
                stack[stackPtr++] = node.leftFirst + 1;
            }
        }
    }
    return vec3(1.0);
}

#endif
