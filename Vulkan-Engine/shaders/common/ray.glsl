#ifndef RAY_GLSL
#define RAY_GLSL

struct Ray {
	vec3 origin;
	vec3 direction;
	float tMin;
	float tMax;
};

struct HitInfo {
	float t;	// -1 if no hit
	vec3 position;
	vec3 normal;
	vec2 uv;
	int materialIndex;
};

float intersectAABB(Ray ray, vec3 invDir, vec3 bMin, vec3 bMax);
float sphereIntersect(Ray ray, vec3 center, float radius);
float triangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float u, out float v);

#endif