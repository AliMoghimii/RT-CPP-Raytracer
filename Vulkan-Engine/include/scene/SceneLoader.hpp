#pragma once
#include "scene/Scene.hpp"
#include <memory>
#include <string>

// Pure parser: reads a scene JSON file, resolves named texture/material references into
// indices, and wraps the resulting SceneData in a GenericScene. Does not call build() --
// callers (SceneManager) are responsible for that.
class SceneLoader {
public:
    static std::unique_ptr<Scene> loadFromFile(const std::string& path);
};
