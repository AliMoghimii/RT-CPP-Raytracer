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

    static void buildBVH(
        std::vector<GPUTriangle>& triangles,
        std::vector<GPUBVHNode>& bvhNodes
    );

    // Loads an OBJ in local space (identity transform) -- used to build a BLAS mesh
    // that gets positioned per-instance via a transform matrix instead of baked vertices.
    // Triangles lacking `vn` data get smooth per-vertex normals averaged across shared positions.
    static void loadOBJLocal(
        const std::string& filename,
        std::vector<GPUTriangle>& sceneTriangles,
        int materialIndex
    );

    // Builds a BVH over triangles[firstTri .. firstTri+triCount) in-place (leaves reference
    // global indices into `triangles`); used both by buildBVH (full range) and appendBLAS (sub-range).
    static void buildBVHRange(
        std::vector<GPUTriangle>& triangles,
        int firstTri,
        int triCount,
        std::vector<GPUBVHNode>& outNodes
    );

    // Builds a standalone BVH over sceneTriangles[triOffset .. triOffset+triCount) and appends
    // its nodes onto the end of sceneBVH (offsetting internal-node child indices to match their
    // new position). Returns the index of the appended BLAS's root node within sceneBVH.
    static int appendBLAS(
        std::vector<GPUTriangle>& sceneTriangles,
        std::vector<GPUBVHNode>& sceneBVH,
        int triOffset,
        int triCount
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

    static void subdivideSAH(
        int nodeIdx,
        std::vector<GPUBVHNode>& bvhNodes,
        std::vector<GPUTriangle>& triangles,
        int& nodesUsed
    );
};