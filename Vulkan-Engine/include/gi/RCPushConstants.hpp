#pragma once
#include <glm/vec4.hpp>

// All structs use ivec4/vec4 instead of ivec3/vec3.
// Why: in GLSL std430 (used for push constants), vec3/ivec3 have base alignment = 16,
// but glm::vec3 has alignment = 4. Using vec4/ivec4 makes C++ and GLSL agree -- no padding mismatch.

struct RCAllocPC {                  // 48 bytes -- well under 128-byte minimum
glm::ivec4 gridSizeHashSize;        // xyz = gridSize, w = hashTableSize
    glm::vec4  worldOriginSpacing;  // xyz = worldOrigin, w = spacing
    int allocMode;                  // 0 = from G-buffer, 1 = from parent slots
    int parentMaxSlots;
    int pad[2];
};

struct RCTracePC {                          // 68 bytes
    glm::ivec4 gridSizeOctRes;              // xyz = gridSize (unused in trace -- only w used), w = octRes
    glm::vec4 worldOriginSpacing;           // xyz = worldOrigin, w = spacing
    float intervalStart;
    float intervalEnd;
    int bvhCount;
    int maxActiveSlots;
    int lightCount;
    int planeCount;
    int quadCount;
    int evaluateDirect;                     // 1 = cascade-0 (direct+emission), 0 = levels 1+ (emission-only)
    int softShadowSamples;                  // 1 = hard shadow (original), >1 = area-light jitter in BVH shadow
};

struct RCMergePC {                          // 64 bytes
    glm::ivec4 currentGridOctRes;           // xyz = current gridSize, w = current octRes
    glm::ivec4 parentGridOctRes;            // xyz = parent gridSize,  w = parent octRes
    glm::vec4 worldOriginCurrentSpacing;    // xyz = worldOrigin, w = current spacing
    float parentSpacing;
    int currentHashSize;
    int parentHashSize;
    int currentMaxSlots;
};

struct RCGatherPC {                         // 128 bytes
    glm::vec4 worldOriginSpacing;           // xyz = worldOrigin, w = cascade-0 spacing
    glm::ivec4 gridSizeOctRes;              // xyz = cascade-0 gridSize, w = cascade-0 octRes
    glm::vec4 camPos;                       // xyz = camera world position for specular/viewDir
    int hashSize;
    int bvhCount;
    int lightCount;
    int planeCount;
    int quadCount;
	int debugMode;                          // 0=Final 1=Albedo 2=Normal 3=Depth 4=Emissive
    int enableDirect;                       // 0 = skip per-light loop
    int enableIndirect;                     // 0 = skip cascade lookup
    glm::vec4 skyBottomColor;               // xyz = horizon sky color
    glm::vec4 skyTopColor;                  // xyz = zenith sky color
    float kIndirectScale;                   // GI irradiance → color scale (probe path only)
    float fogDensity;                       // exponential fog decay coefficient
    int fogBlendWithSky;                    // 1 = fog samples sky gradient; 0 = no fog applied
    int pad;
};

struct TransparentPC {                      // 128 bytes
    glm::vec4 camPos;                       // xyz = camera position
    glm::vec4 camRight;                     // xyz = right direction
    glm::vec4 camUp;                        // xyz = up direction
    glm::vec4 camForward;                   // xyz = forward direction
    glm::vec4 worldOriginSpacing;           // xyz = cascade-0 origin, w = spacing
    glm::ivec4 gridSizeOctRes;              // xyz = cascade-0 grid, w = octRes
    int bvhCount;
    int lightCount;
    int planeCount;
    int quadCount;
    int fogBlendWithSky;                    // 1 = fog samples sky gradient; 0 = no fog applied
    int hashSize;
    float kIndirectScale;                   // GI irradiance → color scale (probe path only)
    float fogDensity;                       // exponential fog decay coefficient
};

struct TonemapPC {                          // 16 bytes
    float exposure;
    int tonemapMode;                        // 0=ACES Film  1=Reinhard  2=Linear
    int pad[2];
};
