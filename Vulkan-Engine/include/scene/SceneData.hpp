#pragma once
#include "scene/GPUData.hpp"
#include "scene/MeshInstance.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Mirrors the render-setting fields main.cpp used to hardcode (see Renderer.hpp public
// members) -- copied onto Renderer's public fields by SceneManager::applyToRenderer().
struct SceneSettings {
    bool useLegacyPipeline = false;
    int  globalShadingModel = 0;     // 0=Blinn-Phong, 1=PBR
    bool enableSoftShadows = false;
    int  maxDepth = 5;
    int  shadowRays = 4;
    bool enableDOF = false;
    float focalDistance = 1.6f;
    float lensRadius = 0.02f;
    bool enableFog = false;
    glm::vec3 fogColor{ 1.0f };
    bool enableSkybox = false;
    glm::vec3 skyBottomColor{ 1.0f };
    glm::vec3 skyTopColor{ 0.1f, 0.3f, 0.7f };
    bool enableTextures = true;
    int  primaryRaysPerPixel = 1;
    bool enableCaustics = false;
    int  photonCount = 1000000;
    float causticIntensity = 15.0f;
    glm::vec3 gridMin{ -20.0f };
    glm::vec3 gridMax{ 20.0f };
    int  gridResolution = 256;
    float gatherRadius = 0.4f;
};

// Plain-data scene blob produced by SceneLoader and consumed by SceneManager. GenericScene's
// build() mutates triangles/bvhNodes/spheres/cubes in place (tessellation + BVH build).
struct SceneData {
    SceneSettings settings;
    std::vector<GPUMaterial> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUTriangle> triangles;
    std::vector<GPULight> lights;
    std::vector<GPUPlane> planes;
    std::vector<GPUQuad> quads;
    std::vector<GPUCube> cubes;
    std::vector<GPUBVHNode> bvhNodes;
    std::vector<std::string> texturePaths;
    std::vector<std::string> meshFiles;
    std::vector<int> meshMaterialIndices;
    std::vector<MeshInstance> meshInstances;
};
