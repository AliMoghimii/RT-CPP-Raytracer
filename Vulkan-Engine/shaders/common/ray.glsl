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

float intersectAABB(Ray ray, vec3 invDir, vec3 bMin, vec3 bMax) {
    vec3 t0 = (bMin - ray.origin) * invDir;
    vec3 t1 = (bMax - ray.origin) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float maxTmin = max(max(tmin.x, tmin.y), tmin.z);
    float minTmax = min(min(tmax.x, tmax.y), tmax.z);
    if (maxTmin <= minTmax && minTmax > 0.0) {
        return maxTmin < 0.0 ? 0.0 : maxTmin;
    }
    return 999999.0;
}

float sphereIntersect(Ray ray, vec3 center, float radius) {
    vec3 oc = ray.origin - center;
    float a = 1.0;
    float b = 2.0 * dot(ray.direction, oc);
    float c = dot(oc, oc) - (radius * radius);
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant >= 0.0) {
        float dist = (-b - sqrt(discriminant)) / (2.0 * a);
        if (dist > 0.0001) return dist;
        dist = (-b + sqrt(discriminant)) / (2.0 * a);
        if (dist > 0.0001) return dist;
    }
    return -1.0;
}

float triangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float outU, out float outV) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    if (a > -0.0000001 && a < 0.0000001) return -1.0;

    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return -1.0;

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return -1.0;

    float t = f * dot(edge2, q);
    if (t > 0.0000001) {
        outU = u;
        outV = v;
        return t;
    }
    return -1.0;
}

#endif