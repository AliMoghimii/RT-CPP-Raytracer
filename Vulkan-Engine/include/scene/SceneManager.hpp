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

private:
    Renderer& renderer;
    std::unique_ptr<Scene> currentScene;

    void applyToRenderer(const SceneData& data);
};
