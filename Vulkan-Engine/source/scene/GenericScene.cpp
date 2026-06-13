#include "scene/GenericScene.hpp"
#include "scene/Primitive.hpp"
#include "scene/ModelLoader.hpp"

void GenericScene::build() {
    if (!sceneData.settings.useLegacyPipeline) {
        // Tessellate object primitives into the BVH so RC probes can see them.
        for (const auto& sphere : sceneData.spheres)
            tessellateSphere(sphere, sphere.materialIndex, sceneData.triangles);
        for (const auto& cube : sceneData.cubes)
            tessellateCube(cube, cube.materialIndex, sceneData.triangles);

        sceneData.spheres.clear();
        sceneData.cubes.clear();

        ModelLoader::buildBVH(sceneData.triangles, sceneData.bvhNodes);
    }
}
