#pragma once
#include "scene/SceneData.hpp"

// State-pattern interface: a Scene wraps a SceneData blob and knows how to turn raw parsed
// data into render-ready data (build()). SceneManager holds a unique_ptr<Scene> as its
// current state and swaps it on scene change.
class Scene {
public:
    virtual ~Scene() = default;

    // Tessellate primitives into triangles and build the BVH, in place. Must be called once
    // before data() is handed to Renderer::loadScene().
    virtual void build() = 0;

    const SceneData& data() const { return sceneData; }

protected:
    SceneData sceneData;
};
