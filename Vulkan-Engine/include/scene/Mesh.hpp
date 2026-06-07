#pragma once
#include "scene/GPUData.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>

// A BLAS: one mesh's triangles + local-space BVH, loaded once and appended into the
// shared sceneTriangles/sceneBVH arrays. Multiple MeshInstances can reference the same
// Mesh with different transforms/materials -- the mesh data itself never moves or rebuilds.
class Mesh {
public:
    static Mesh load(
        const std::string& filename,
        int materialIndex,
        std::vector<GPUTriangle>& sceneTriangles,
        std::vector<GPUBVHNode>& sceneBVH
    );

    int triOffset = 0;
    int triCount = 0;
    int blasRootIndex = 0;

    // Local-space bounds of the mesh's triangles -- lets MeshInstance derive a world-space
    // AABB (via the 8 transformed corners) without rescanning every triangle each frame.
    glm::vec3 localAabbMin{ 0.0f };
    glm::vec3 localAabbMax{ 0.0f };
};
