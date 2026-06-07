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

// Slope-scaled normal-offset bias for secondary ray origins (shadow/bounce rays
// leaving a hit surface). A flat constant offset (e.g. `pos + normal * 0.001`)
// works for coarse, large-scale geometry but produces shadow acne on densely
// tessellated, smoothly-shaded curved meshes (e.g. a 76K-tri torus instanced at
// world-scale): the *shading* normal (interpolated/averaged from the mesh's `vn`
// data) diverges from each triangle's flat geometric plane, most severely at
// grazing angles -- exactly where self-intersection is most likely. The offset
// point can then remain on the wrong side of a neighbouring facet's plane, and the
// shadow ray immediately re-hits the mesh it just left, producing flickering dark
// patches that shift as the mesh rotates (worst on high-curvature regions like a
// torus's inner ring, where facet-to-facet angles are largest).
//
// Scaling the bias by 1/max(|N.dir|, minNdotD) widens it precisely at those grazing
// angles -- where the shading/geometric normal mismatch is largest -- while staying
// tight at near-perpendicular angles (so thin contact shadows on flat geometry don't
// peter-pan). This is the ray-tracing analogue of "slope-scaled depth bias" used to
// fight shadow-map acne on curved surfaces.
vec3 offsetRayOrigin(vec3 worldPos, vec3 normal, vec3 dir, float baseBias) {
    float nDotD = max(abs(dot(normal, dir)), 0.05);
    return worldPos + normal * (baseBias / nDotD);
}

// Slab test AABB intersection (Kay & Kajiya 1986).
// An AABB is the intersection of 3 "slabs" (pairs of parallel planes). For each axis,
// compute where the ray enters and exits the slab: t0 = (bMin - origin) / dir,
// t1 = (bMax - origin) / dir.  Taking min/max handles negative dir components.
// The ray hits the box if the latest slab-entry (maxTmin) comes before the
// earliest slab-exit (minTmax), and the exit is in front of the ray.
// Passing invDir = 1/ray.direction avoids per-axis division inside the BVH loop.
float intersectAABB(Ray ray, vec3 invDir, vec3 bMin, vec3 bMax) {
    vec3 t0 = (bMin - ray.origin) * invDir;
    vec3 t1 = (bMax - ray.origin) * invDir;
    vec3 tmin = min(t0, t1);   // per-axis entry time (handles negative direction)
    vec3 tmax = max(t0, t1);   // per-axis exit  time
    float maxTmin = max(max(tmin.x, tmin.y), tmin.z);  // latest entry across all axes
    float minTmax = min(min(tmax.x, tmax.y), tmax.z);  // earliest exit across all axes
    if (maxTmin <= minTmax && minTmax > 0.0) {
        // Ray is inside the box when it arrives: clamp entry to 0 so we don't
        // step backwards and return 0 as the hit distance.
        return maxTmin < 0.0 ? 0.0 : maxTmin;
    }
    return 999999.0;
}

// Ray-sphere intersection via the quadratic formula.
// Substituting P(t) = origin + t*dir into |P - center|^2 = r^2 gives
//   t^2 + 2*(dir·oc)*t + (|oc|^2 - r^2) = 0,   oc = origin - center.
// Two solutions exist when discriminant = b^2 - 4ac >= 0.
// We take the nearer positive root first; if it's behind the ray origin we try the far root.
// The 0.0001 bias avoids self-intersection at the origin surface point.
float sphereIntersect(Ray ray, vec3 center, float radius) {
    vec3 oc = ray.origin - center;
    float a = 1.0;                                    // |dir|^2, normalized so = 1
    float b = 2.0 * dot(ray.direction, oc);
    float c = dot(oc, oc) - (radius * radius);
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant >= 0.0) {
        float dist = (-b - sqrt(discriminant)) / (2.0 * a);  // near root
        if (dist > 0.0001) return dist;
        dist = (-b + sqrt(discriminant)) / (2.0 * a);         // far root (ray exits inside)
        if (dist > 0.0001) return dist;
    }
    return -1.0;
}

// Möller-Trumbore ray-triangle intersection.
// Solves the system: origin + t*dir = v0 + u*edge1 + v*edge2
// where (u,v) are barycentric coordinates (u>=0, v>=0, u+v<=1 means inside the triangle).
// Rearranging: t*dir - u*edge1 - v*edge2 = origin - v0.
// Using Cramer's rule and the scalar triple product gives the 3 unknowns (t, u, v) without
// computing the triangle normal, which makes this the fastest practical CPU/GPU test.
// The tiny epsilon (1e-7) guards against rays nearly parallel to the triangle plane.
float triangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float outU, out float outV) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.direction, edge2);   // perpendicular to dir and edge2
    float a = dot(edge1, h);                // scalar triple product = 0 when ray || triangle

    // |a| scales with |rayDir|*|edge1|*|edge2| -- a fixed epsilon silently rejects valid
    // hits on small-scale local-space geometry (e.g. a tiny BLAS instanced with a large
    // scale factor divides the local ray direction down, shrinking |a| by orders of
    // magnitude even for well-conditioned, non-parallel intersections). Scale the epsilon
    // by the same magnitudes so the parallel test stays scale-invariant.
    float aScale = length(edge1) * length(edge2) * length(ray.direction);
    if (abs(a) < 0.0000001 * aScale) return -1.0;

    float f = 1.0 / a;                      // reciprocal for Cramer's rule
    vec3 s = ray.origin - v0;               // vector from v0 to ray origin
    float u = f * dot(s, h);                // first barycentric coordinate
    if (u < 0.0 || u > 1.0) return -1.0;   // outside edge v0-v1

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);    // second barycentric coordinate
    if (v < 0.0 || u + v > 1.0) return -1.0;  // outside edge v1-v2 or v2-v0

    float t = f * dot(edge2, q);            // ray parameter t
    if (t > 0.0000001) {
        outU = u;
        outV = v;
        return t;
    }
    return -1.0;
}

#endif