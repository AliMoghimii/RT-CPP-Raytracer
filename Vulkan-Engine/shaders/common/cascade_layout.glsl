#ifndef CASCADE_LAYOUT_GLSL
#define CASCADE_LAYOUT_GLSL

// ============================================================
// NAMING CONVENTION � enforced by convention, not the compiler.
//
// Every shader that #includes this file MUST declare SSBOs with
// exactly the following names (the functions below reference them):
//
//   Current cascade:
//     hashKeys[]       � uint[], size = tableSize
//     hashValues[]     � uint[], size = tableSize
//     slotToKey[]      � uint[], size = maxActiveSlots
//     nextSlot         � uint   (single-element buffer)
//     cascadeData[]    � uvec2[], size = maxActiveSlots * octRes * octRes
//
//   Parent cascade (cascade_merge.comp only):
//     parentHashKeys[]     � uint[]
//     parentHashValues[]   � uint[]
//     parentCascadeData[]  � uvec2[]
// ============================================================

// --- Key encoding ---

// Layout: X[5:0]=6 bits (max 63), Y[11:6]=6 bits (max 63), Z[18:12]=7 bits (max 127)
// Supports grids up to 64×64×128 — covers the full 30×25×45 room at spacing0=0.5.
uint packCellKey(ivec3 cell) {
    return (uint(cell.x) & 0x3Fu) | ((uint(cell.y) & 0x3Fu) << 6) | ((uint(cell.z) & 0x7Fu) << 12);
}

ivec3 unpackCellKey(uint key) {
    return ivec3(
        int(key & 0x3Fu),
        int((key >> 6) & 0x3Fu),
        int((key >> 12) & 0x7Fu)
    );
}

// --- Spatial helpers ---

vec3 probeWorldPos(ivec3 cell, vec3 worldOrigin, float spacing)
{
    return worldOrigin + (vec3(cell) + 0.5) * spacing;
}

// Returns the base index into cascadeData[] for a given slot and octRes.
// Direction d is stored at [base + d].
int probeStorageBase(uint slot, int octRes)
{
    return int(slot) * octRes * octRes;
}

// -- Radiance packing (uvec2, no uint64_t extension needed) ---

// Packs (radiance.rgb, transmittance) as two paris of FP16 values.
// packHalf2x16 / unpackHalf2x16 are GLSL builtins - no extension needed.
// Why uvec2 instead of uint64_t: 64-bit integers in GLSL require GL_ARB_gpu_shader_int64 or GL_EXT_shader_explicit_arithmetic_types_int64.
// These extensions are not universally available and are not declared in this project. uvec2 (two 32-bit uints) stores the identical 8 bytes.
// packHalf2x16/unpackHalf2x16 operate on plain uint and need no extension.
uvec2 packRadianceTransmittance(vec3 L, float T)
{
    return uvec2(
        packHalf2x16(vec2(L.xy)),
        packHalf2x16(vec2(L.z, T))
    );
}

vec4 unpackRadianceTransmittance(uvec2 packed)
{
    vec2 xy = unpackHalf2x16(packed.x);
    vec2 zw = unpackHalf2x16(packed.y);
    return vec4(xy.x, xy.y, zw.x, zw.y);    // rgb = radiance, a = transmittance
}

// --- Hash map: insert (writes to current cascade's hashKeys/hashValues/slotToKey/nextSlot) ---
// Only compiled in shaders that define RC_ALLOC_PASS (probe_alloc.comp).
// Excluded from readonly passes (probe_trace.comp, cascade_gather.comp) to avoid
// conflicting with readonly buffer qualifiers on those SSBOs.
#ifdef RC_ALLOC_PASS
bool probeInsert(uint key, uint tableSize)
{
    uint h = (key * 2654435761u) % tableSize;
    for (uint i = 0u; i < tableSize; ++i) {
        uint idx = (h + i) % tableSize;
        uint prev = atomicCompSwap(hashKeys[idx], 0xFFFFFFFFu, key);

        if (prev == 0xFFFFFFFFu) {
            uint slot = atomicAdd(nextSlot, 1u);
            hashValues[idx] = slot;
            slotToKey[slot] = key;
            return true;
        }

        if (prev == key) return false; // already inserted by another thread
    }
    return false; // table full
}
#endif // RC_ALLOC_PASS

// --- Hash map: lookup in current cascade ---
// Returns the slot index, or 0xFFFFFFFFu if the probe is not active.
uint probeLookup(uint key, uint tableSize) {
    uint h = (key * 2654435761u) % tableSize;
    for (uint i = 0u; i < tableSize; i++) {
        uint idx = (h + i) % tableSize;
        uint k   = hashKeys[idx];
        if (k == key) return hashValues[idx];
        if (k == 0xFFFFFFFFu) return 0xFFFFFFFFu; // empty slot -> key not in table
    }
    return 0xFFFFFFFFu;
}

// --- Hash map: lookup in parent cascade ---
// Only compiled in shaders that define RC_MERGE_PASS (cascade_merge.comp).
// Requires parentHashKeys[] and parentHashValues[] to be declared in the shader.
#ifdef RC_MERGE_PASS
uint probeLookupInParent(uint key, uint tableSize) {
    uint h = (key * 2654435761u) % tableSize;
    for (uint i = 0u; i < tableSize; i++) {
        uint idx = (h + i) % tableSize;
        uint k = parentHashKeys[idx];
        if (k == key) return parentHashValues[idx];
        if (k == 0xFFFFFFFFu) return 0xFFFFFFFFu;
    }
    return 0xFFFFFFFFu;
}
#endif // RC_MERGE_PASS

#endif // CASCADE_LAYOUT_GLSL