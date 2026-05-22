#pragma once
#include "scene/GPUData.hpp"
#include <vector>

void tessellateSphere(const GPUSphere& sphere, int materialIndex,
    std::vector<GPUTriangle>& outTriangles,
    int segmentsU = 16, int segmentsV = 12);

void tessellateCube(const GPUCube& cube, int materialIndex,
    std::vector<GPUTriangle>& outTriangles);

void tessellateQuad(const GPUQuad& quad, int materialIndex,
    std::vector<GPUTriangle>& outTriangles);

void tessellatePlane(const GPUPlane& plane, int materialIndex,
    std::vector<GPUTriangle>& outTriangles,
    float extent = 50.0f);