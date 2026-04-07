#include "VulkanCore.hpp"
#include "ModelLoader.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    std::vector<GPUMaterial> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUTriangle> triangles;
    std::vector<GPULight> lights;
    std::vector<GPUPlane> planes;
    std::vector<GPUQuad> quads;
    std::vector<GPUCube> cubes;

    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Red sphere mat
    materials.push_back({ glm::vec3(0.8f, 0.8f, 0.8f), 0.2f, 1.0f, 1.0f, 0.4f, 0.0f, 1.0f, 1, 0.0f, 0.0f }); // Floor checkerboard mat
    materials.push_back({ glm::vec3(0.0f, 1.0f, 1.0f), 0.05f, 1.0f, 1.0f, 0.1f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Cyan cube mat
    materials.push_back({ glm::vec3(0.9f, 0.9f, 0.9f), 0.1f, 1.0f, 1.0f, 0.1f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Purple shuttle mat
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, 0.0f, 1.0f, 0.1f, 0.9f, 1.5f, 0, 0.0f, 0.0f }); // Glass sphere mat

    spheres.push_back({ glm::vec3(-0.5f, 0.0f, 0.0f), 0.5f, 0, 0.0f, 0.0f, 0.0f });
    spheres.push_back({ glm::vec3(1.8f, 0.0f, 0.0f), 0.5f, 4, 0.0f, 0.0f, 0.0f });
    planes.push_back({ glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 1, 0.0f, 0.0f, 0.0f, 0.0f });
    cubes.push_back({ glm::vec3(-0.25f, -0.25f, -0.25f), 0.0f, glm::vec3(0.25f, 0.25f, 0.25f), 0.0f, glm::vec3(0.5f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 45.0f, 0.0f), 2, 0.0f, 0.0f, 0.0f, 0.0f });

    ModelLoader::load("model_shuttle.obj", triangles, 3, glm::vec3(-1.3f, 0.0f, 0.4f), glm::vec3(270.0f, 45.0f, 0.0f), 0.07f);

    lights.push_back({ glm::vec3(-1.0f, 15.0f, -1.0f), 0.0f, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f });

    VulkanCore engine;
    engine.loadScene(materials, spheres, triangles, lights, planes, quads, cubes);

    try {
        engine.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}