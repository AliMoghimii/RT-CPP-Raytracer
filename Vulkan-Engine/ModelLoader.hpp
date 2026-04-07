#pragma once
#include <glm/glm.hpp>
#include "GPUData.hpp"
#include <vector>
#include <string>

class ModelLoader {
public:
    static void load(
        const std::string& filename,
        std::vector<GPUTriangle>& sceneTriangles,
        std::vector<GPUBVHNode>& bvhNodes,
        int materialIndex,
        const glm::vec3& position,
        const glm::vec3& rotation,
        float scale
    );

private:
    static void loadOBJ(
        const std::string& filename,
        std::vector<GPUTriangle>& sceneTriangles,
        int materialIndex,
        const glm::vec3& position,
        const glm::vec3& rotation,
        float scale
    );

    static void buildBVH(
        std::vector<GPUTriangle>& triangles,
        std::vector<GPUBVHNode>& bvhNodes
    );

    static void updateNodeBounds(
        int nodeIdx,
        std::vector<GPUBVHNode>& bvhNodes,
        const std::vector<GPUTriangle>& triangles
    );

    static void subdivide(
        int nodeIdx,
        std::vector<GPUBVHNode>& bvhNodes,
        std::vector<GPUTriangle>& triangles,
        int& nodesUsed
    );
};