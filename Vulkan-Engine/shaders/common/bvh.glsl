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
    vec3 v0;    float p1;
    vec3 v1;    float p2;
    vec3 v2;    float p3;
    vec3 n0;    float p4;
    vec3 n1;    float p5;
    vec3 n2;
    int isSmooth;
    int materialIndex;
    float p6;
    float p7;
    float p8;
};

// Bindings — assumes set=0, binding=8 for BVH and binding=3 for triangles
// Caller defines these via descriptor set layout
layout(std430, set = 0, binding = 3) readonly buffer TriangleBuffer { GPUTriangle triangles[]; };
layout(std430, set = 0, binding = 8) readonly buffer BVHBuffer { GPUBVHNode bvhNodes[]; };

#define BVH_STACK_SIZE 64

bool traverseBVH(Ray ray, int bvhNodeCount, out HitInfo hit) {
    hit.t = ray.tMax;
    int hitIndex = -1;
    float hitU = 0, hitV = 0;
    
    if (bvhNodeCount == 0) return false;
    
    int stack[BVH_STACK_SIZE];
    int stackPtr = 0;
    stack[stackPtr++] = 0;
    vec3 invDir = 1.0 / ray.direction;
    
    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        GPUBVHNode node = bvhNodes[nodeIdx];
        
        float distAABB = intersectAABB(ray, invDir, node.aabbMin, node.aabbMax);
        if (distAABB >= hit.t) continue;
        
        if (node.triCount > 0) {
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
            if (stackPtr + 2 <= BVH_STACK_SIZE) {
                stack[stackPtr++] = node.leftFirst;
                stack[stackPtr++] = node.leftFirst + 1;
            }
        }
    }
    
    if (hitIndex == -1) return false;
    
    GPUTriangle tri = triangles[hitIndex];
    float w = 1.0 - hitU - hitV;
    
    hit.position = ray.origin + ray.direction * hit.t;
    if (tri.isSmooth == 1) {
        hit.normal = normalize(w * tri.n0 + hitU * tri.n1 + hitV * tri.n2);
    } else {
        hit.normal = normalize(cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
    }
    hit.uv = vec2(hitU, hitV);
    hit.materialIndex = tri.materialIndex;
    
    return true;
}

#endif