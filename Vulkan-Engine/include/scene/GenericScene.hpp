#pragma once
#include "scene/Scene.hpp"

// JSON-driven concrete state. SceneLoader fills sceneData and wraps it in a GenericScene;
// build() then runs the tessellation + BVH step (ported from main.cpp's pre-refactor flow).
class GenericScene : public Scene {
public:
    explicit GenericScene(SceneData data) { sceneData = std::move(data); }

    void build() override;
};
