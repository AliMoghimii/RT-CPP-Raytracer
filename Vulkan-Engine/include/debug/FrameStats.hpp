#pragma once
#include <cstdint>

struct FrameStats {
    float frameTimeMs = 0.0f;
    float gpuTotalMs = 0.0f;
    float gpuPrimaryMs = 0.0f;
    float gpuAllocMs = 0.0f;
    float gpuTraceMs = 0.0f;
    float gpuMergeMs = 0.0f;
    float gpuGatherMs = 0.0f;
    float gpuTransparentMs = 0.0f;
    float gpuTonemapMs = 0.0f;
    uint64_t vramUsedBytes = 0;
    uint64_t vramBudgetBytes = 0;
};
