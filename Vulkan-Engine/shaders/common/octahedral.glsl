#ifndef OCTAHEDRAL_GLSL
#define OCTAHEDRAL_GLSL

vec2 octEncode(vec3 dir) {
    dir /= (abs(dir.x) + abs(dir.y) + abs(dir.z));
    vec2 uv = (dir.z >= 0.0)
              ? dir.xy
              : (vec2(1.0) - abs(dir.yx)) * sign(dir.xy);
    return uv * 0.5 + 0.5;
}

vec3 octDecode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 dir = vec3(uv.xy, 1.0 - abs(uv.x) - abs(uv.y));
    if (dir.z < 0.0) {
        dir.xy = (1.0 - abs(dir.yx)) * sign(dir.xy);
    }
    return normalize(dir);
}

// Sample uv from octahedral resolution index
vec2 octIndexToUV(ivec2 idx, int resolution) {
    return (vec2(idx) + 0.5) / float(resolution);
}

#endif