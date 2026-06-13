#include "scene/SceneManager.hpp"
#include "scene/SceneLoader.hpp"
#include "renderer/Renderer.hpp"

void SceneManager::loadInitial(const std::string& path) {
    currentScenePath = path;
    currentScene = SceneLoader::loadFromFile(path);
    currentScene->build();
    applyToRenderer(currentScene->data());
}

void SceneManager::swapScene(const std::string& path) {
    renderer.unloadSceneGPU();

    currentScenePath = path;
    auto next = SceneLoader::loadFromFile(path);
    next->build();
    applyToRenderer(next->data());
    currentScene = std::move(next);

    renderer.reloadSceneGPU();
}

void SceneManager::setLegacyPipeline(bool useLegacy) {
    renderer.unloadSceneGPU();

    auto next = SceneLoader::loadFromFile(currentScenePath);
    next->setUseLegacyPipeline(useLegacy);
    next->build();
    applyToRenderer(next->data());
    currentScene = std::move(next);

    renderer.reloadSceneGPU();
}

void SceneManager::applyToRenderer(const SceneData& d) {
    const SceneSettings& s = d.settings;

    renderer.useLegacyPipeline = s.useLegacyPipeline;

    renderer.maxDepth = s.maxDepth;
    renderer.shadowRays = s.shadowRays;
    renderer.primaryRaysPerPixel = s.primaryRaysPerPixel;

    renderer.focalDistance = s.focalDistance;
    renderer.lensRadius = s.enableDOF ? s.lensRadius : 0.0f;

    renderer.enableFog = s.enableFog;
    renderer.fogColor = s.fogColor;

    renderer.enableSkybox = s.enableSkybox;
    renderer.skyBottomColor = s.skyBottomColor;
    renderer.skyTopColor = s.skyTopColor;

    renderer.enableTextures = s.enableTextures;

    renderer.enableCaustics = s.enableCaustics;
    renderer.totalEmittedPhotons = (uint32_t)s.photonCount;
    renderer.causticIntensity = s.causticIntensity;

    renderer.gridMin = s.gridMin;
    renderer.gridMax = s.gridMax;
    renderer.gridRes = s.gridResolution;
    renderer.gatherRadius = s.gatherRadius;

    renderer.loadScene(d.materials, d.spheres, d.triangles, d.lights, d.planes, d.quads, d.cubes, d.bvhNodes);
    renderer.loadTextures(d.texturePaths);
    renderer.loadDynamicMeshes(d.meshFiles, d.meshMaterialIndices, d.meshInstances);
}
