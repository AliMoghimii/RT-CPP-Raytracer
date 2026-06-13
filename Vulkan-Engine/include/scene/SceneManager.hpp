#pragma once
#include "scene/Scene.hpp"
#include <memory>
#include <string>

class Renderer;

// Context of the State Pattern: owns the current Scene (state) and knows how to apply its
// data onto the Renderer. loadInitial() runs before Renderer::run()/initVulkan() (CPU-side
// data only); swapScene() performs a full runtime GPU teardown/recreate.
class SceneManager {
public:
    explicit SceneManager(Renderer& r) : renderer(r) {}

    void loadInitial(const std::string& path);
    void swapScene(const std::string& path);

    // Re-loads the current scene file with useLegacyPipeline overridden, then rebuilds
    // (re-tessellates or restores analytical primitives as appropriate) and reuploads.
    void setLegacyPipeline(bool useLegacy);

private:
    Renderer& renderer;
    std::unique_ptr<Scene> currentScene;
    std::string currentScenePath;

    void applyToRenderer(const SceneData& data);
};
