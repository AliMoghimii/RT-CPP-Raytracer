#pragma once
#include <glm/vec4.hpp>

// All structs use ivec4/vec4 instead of ivec3/vec3.
// Why: in GLSL std430 (used for push constants), vec3/ivec3 have base alignment = 16,
// but glm::vec3 has alignment = 4. Using vec4/ivec4 makes C++ and GLSL agree — no padding mismatch.

struct RCAllocPC {                  // 48 bytes — well under 128-byte minimum
glm::ivec4 gridSizeHashSize;        // xyz = gridSize, w = hashTableSize
    glm::vec4  worldOriginSpacing;  // xyz = worldOrigin, w = spacing
    int allocMode;                  // 0 = from G-buffer, 1 = from parent slots
    int parentMaxSlots;
    int pad[2];
};

struct RCTracePC {                  // 48 bytes
    glm::ivec4 gridSizeOctRes;      // xyz = gridSize (unused in trace — only w used), w = octRes
    glm::vec4 worldOriginSpacing;   // xyz = worldOrigin, w = spacing
    float intervalStart;
    float intervalEnd;
    int bvhCount;
    int maxActiveSlots;
};

struct RCMergePC {                          // 64 bytes
    glm::ivec4 currentGridOctRes;           // xyz = current gridSize, w = current octRes
    glm::ivec4 parentGridOctRes;            // xyz = parent gridSize,  w = parent octRes
    glm::vec4  worldOriginCurrentSpacing;   // xyz = worldOrigin, w = current spacing
    float parentSpacing;
    int currentHashSize;
    int parentHashSize;
    int currentMaxSlots;
};

struct RCGatherPC {                 // 48 bytes
    glm::vec4 worldOriginSpacing;   // xyz = worldOrigin, w = cascade-0 spacing
    glm::ivec4 gridSizeOctRes;      // xyz = cascade-0 gridSize, w = cascade-0 octRes
    int hashSize;
    int bvhCount;
    int lightCount;
    int pad;
};

struct TonemapPC {  // 16 bytes
    float exposure;
    int pad[3];
};