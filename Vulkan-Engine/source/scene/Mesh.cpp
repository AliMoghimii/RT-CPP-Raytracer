#include "scene/Mesh.hpp"
#include "scene/ModelLoader.hpp"
#include <limits>

Mesh Mesh::load(const std::string& filename, int materialIndex,
                std::vector<GPUTriangle>& sceneTriangles, std::vector<GPUBVHNode>& sceneBVH) {
    Mesh mesh;
    mesh.triOffset = static_cast<int>(sceneTriangles.size());

    ModelLoader::loadOBJLocal(filename, sceneTriangles, materialIndex);

    mesh.triCount = static_cast<int>(sceneTriangles.size()) - mesh.triOffset;

    mesh.localAabbMin = glm::vec3(std::numeric_limits<float>::infinity());
    mesh.localAabbMax = glm::vec3(-std::numeric_limits<float>::infinity());
    for (int i = mesh.triOffset; i < mesh.triOffset + mesh.triCount; i++) {
        const GPUTriangle& tri = sceneTriangles[i];
        mesh.localAabbMin = glm::min(mesh.localAabbMin, glm::min(glm::min(tri.v0, tri.v1), tri.v2));
        mesh.localAabbMax = glm::max(mesh.localAabbMax, glm::max(glm::max(tri.v0, tri.v1), tri.v2));
    }

    mesh.blasRootIndex = ModelLoader::appendBLAS(sceneTriangles, sceneBVH, mesh.triOffset, mesh.triCount);

    return mesh;
}
