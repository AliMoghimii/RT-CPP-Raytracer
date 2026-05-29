#ifndef OCTAHEDRAL_GLSL
#define OCTAHEDRAL_GLSL

// Octahedral encoding maps the unit sphere to a square [0,1]^2 with near-uniform
// area distribution, which lets us store probe radiance on a flat 2D grid.
//
// The projection works in L1 (taxicab) norm: fold the lower hemisphere onto the
// upper half by reflecting the x,y components, giving every direction a unique
// (u,v) in [-1,1]^2.  The sign() fold rule keeps the mapping continuous across
// the equator.  Finally remap to [0,1] for 0-based texel indexing.

// Encodes a unit direction to [0,1]^2 octahedral UV.
// Step 1: project onto the L1 unit octahedron by dividing by the L1 norm.
// Step 2: for the lower hemisphere (z<0) reflect with the "diamond fold" trick.
vec2 octEncode(vec3 dir) {
    dir /= (abs(dir.x) + abs(dir.y) + abs(dir.z));  // L1 projection onto octahedron face
    vec2 uv = (dir.z >= 0.0)
              ? dir.xy                                        // upper hemisphere: direct projection
              : (vec2(1.0) - abs(dir.yx)) * sign(dir.xy);   // lower hemisphere: fold the corners
    return uv * 0.5 + 0.5;  // [-1,1] -> [0,1]
}

// Decodes [0,1]^2 UV back to a unit direction (inverse of octEncode).
// Undo the [0,1] remap, reconstruct z from the L1 constraint (|x|+|y|+|z|=1),
// then undo the lower-hemisphere fold if z < 0.
vec3 octDecode(vec2 uv) {
    uv = uv * 2.0 - 1.0;                               // [0,1] -> [-1,1]
    vec3 dir = vec3(uv.xy, 1.0 - abs(uv.x) - abs(uv.y));  // z from L1 constraint
    if (dir.z < 0.0) {
        dir.xy = (1.0 - abs(dir.yx)) * sign(dir.xy);   // undo lower-hemisphere fold
    }
    return normalize(dir);
}

// Returns the [0,1]^2 UV at the center of texel (idx) in an octahedral grid of
// size resolution x resolution.  The +0.5 centers each sample within its texel.
vec2 octIndexToUV(ivec2 idx, int resolution) {
    return (vec2(idx) + 0.5) / float(resolution);
}

#endif