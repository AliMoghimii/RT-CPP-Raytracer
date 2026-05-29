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

// A probe cell (x,y,z) is packed into a single uint so it fits in a GPU hash map slot.
// Bit layout: X occupies bits [5:0] (6 bits, 0-63), Y bits [11:6], Z bits [18:12] (7 bits, 0-127).
// This supports grids up to 64x64x128 cells, which is enough for the 30x25x45-unit room at spacing=0.5.
// The sentinel value 0xFFFFFFFF is the "empty" marker; no valid key can equal it because
// the packed value of (63,63,127) = 0x0007FFFF which is well below 0xFFFFFFFF.
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

// Packs (radiance.rgb, transmittance) as two pairs of FP16 values.
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
// Open-addressing (linear probing) hash map insert.
// GPU parallel insert: many threads may call probeInsert simultaneously for the same
// frame.  atomicCompSwap (compare-and-swap) is the critical primitive: it atomically
// checks if hashKeys[idx] is empty (0xFFFFFFFF) and, if so, writes the new key.
// Only the one thread that wins the CAS claims the slot; all others keep probing.
// Once a slot is claimed, atomicAdd(nextSlot,1) gives this probe a unique dense slot
// index used to index into the fixed-size cascadeData[] array.
// The Knuth multiplicative hash (key * 2654435761) distributes keys across the table
// to minimize clustering.  Linear probing finds the next slot on collision.
#ifdef RC_ALLOC_PASS
bool probeInsert(uint key, uint tableSize)
{
    uint h = (key * 2654435761u) % tableSize;  // Knuth multiplicative hash
    for (uint i = 0u; i < tableSize; ++i) {
        uint idx = (h + i) % tableSize;         // linear probe step
        uint prev = atomicCompSwap(hashKeys[idx], 0xFFFFFFFFu, key);  // try to claim slot

        if (prev == 0xFFFFFFFFu) {
            // This thread won the CAS: slot was empty and is now ours.
            uint slot = atomicAdd(nextSlot, 1u);  // get the next available dense slot
            hashValues[idx] = slot;               // map hash slot -> dense slot
            slotToKey[slot] = key;                // map dense slot -> cell key (for trace dispatch)
            return true;
        }

        if (prev == key) return false;  // already inserted by another thread this frame
    }
    return false;  // table full (should never happen if tableSize is sized correctly)
}
#endif // RC_ALLOC_PASS

// Open-addressing lookup. Follows the same probe sequence as probeInsert.
// An empty slot (0xFFFFFFFF) is a hard stop: if the key were present it would be
// in a slot at or before the first empty slot (insertion never skips empty slots).
// Returns the slot index, or 0xFFFFFFFFu if the probe is not active this frame.
uint probeLookup(uint key, uint tableSize) {
    uint h = (key * 2654435761u) % tableSize;
    for (uint i = 0u; i < tableSize; i++) {
        uint idx = (h + i) % tableSize;
        uint k   = hashKeys[idx];
        if (k == key) return hashValues[idx];
        if (k == 0xFFFFFFFFu) return 0xFFFFFFFFu;  // empty slot: key absent
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