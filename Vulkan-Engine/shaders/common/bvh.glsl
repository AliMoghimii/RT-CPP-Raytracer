#ifndef BVH_GLSL
#define BVH_GLSL

#include "ray.glsl"

struct GPUBVHNode {
	vec3 aabbMin;
    int leftFirst;	// leaf: index of first triangle, internal: index of left child
	vec3 aabbMax;
	int triCount;   // leaf: number of triangles, internal: 0
};

struct GPUTriangle {
    vec3 v0;    float uv0x;
    vec3 v1;    float uv0y;
    vec3 v2;    float uv1x;
    vec3 n0;    float uv1y;
    vec3 n1;    float uv2x;
    vec3 n2;
    int isSmooth;
    int materialIndex;
    float uv2y;
    float p7;
    float p8;
};

// Bindings -- assumes set=0, binding=8 for BVH and binding=3 for triangles
// Caller defines these via descriptor set layout
layout(std430, set = 0, binding = 3) readonly buffer TriangleBuffer { GPUTriangle triangles[]; };
layout(std430, set = 0, binding = 8) readonly buffer BVHBuffer { GPUBVHNode bvhNodes[]; };

#define BVH_STACK_SIZE 32

// Iterative depth-first BVH traversal using an explicit stack (GPU has no call stack).
// The BVH is a binary tree of axis-aligned bounding boxes.  Leaf nodes store a contiguous
// range of triangles (triCount > 0, leftFirst = first triangle index).  Internal nodes
// store the index of their left child in leftFirst; the right child is always leftFirst+1.
//
// Traversal is a DFS: push both children, then test AABB for each popped node, and recurse
// only if the AABB is hit and closer than the current best.  hit.t tracks the closest hit
// so far -- any AABB farther than hit.t is culled immediately (main source of speedup).
// invDir = 1/ray.direction is precomputed once so the AABB test avoids per-node division.
bool traverseBVH(Ray ray, int bvhNodeCount, out HitInfo hit) {
    hit.t = ray.tMax;
    int hitIndex = -1;
    float hitU = 0, hitV = 0;

    if (bvhNodeCount == 0) return false;

    int stack[BVH_STACK_SIZE];
    int stackPtr = 0;
    stack[stackPtr++] = 0;              // start at the root (node 0)
    vec3 invDir = 1.0 / ray.direction;  // precomputed for the slab test

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        GPUBVHNode node = bvhNodes[nodeIdx];

        float distAABB = intersectAABB(ray, invDir, node.aabbMin, node.aabbMax);
        if (distAABB >= hit.t) continue;  // AABB is farther than best hit so far: prune

        if (node.triCount > 0) {
            // Leaf: test all triangles stored in this node
            for (int i = 0; i < node.triCount; i++) {
                int triIdx = node.leftFirst + i;
                GPUTriangle tri = triangles[triIdx];
                float u, v;
                float t = triangleIntersect(ray, tri.v0, tri.v1, tri.v2, u, v);
                if (t > ray.tMin && t < hit.t) {
                    hit.t = t;
                    hitIndex = triIdx;
                    hitU = u;
                    hitV = v;
                }
            }
        } else {
            // Internal: push both children; right first so left is processed first (DFS order)
            if (stackPtr + 2 <= BVH_STACK_SIZE) {
                stack[stackPtr++] = node.leftFirst;
                stack[stackPtr++] = node.leftFirst + 1;
            }
        }
    }

    if (hitIndex == -1) return false;

    // Reconstruct hit surface data from the winning triangle.
    // Barycentric coordinates (w, u, v) weight the per-vertex attributes:
    //   w = 1 - u - v  (weight for v0)
    //   u              (weight for v1, returned from Moller-Trumbore)
    //   v              (weight for v2)
    GPUTriangle tri = triangles[hitIndex];
    float w = 1.0 - hitU - hitV;

    hit.position = ray.origin + ray.direction * hit.t;
    if (tri.isSmooth == 1) {
        // Phong normal interpolation: interpolate per-vertex normals for a smooth silhouette.
        // This gives the visual impression of a smooth curved surface even on a coarse mesh.
        hit.normal = normalize(w * tri.n0 + hitU * tri.n1 + hitV * tri.n2);
    } else {
        // Flat shading: compute the geometric face normal from the two edge vectors.
        hit.normal = normalize(cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
    }
    // Interpolate per-vertex texture UV using barycentric weights (w already defined above)
    hit.uv = w    * vec2(tri.uv0x, tri.uv0y)
           + hitU * vec2(tri.uv1x, tri.uv1y)
           + hitV * vec2(tri.uv2x, tri.uv2y);
    hit.materialIndex = tri.materialIndex;

    return true;
}

#endif
