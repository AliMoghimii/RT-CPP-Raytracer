#include "VulkanCore.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    std::vector<GPUMaterial> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUTriangle> triangles;
    std::vector<GPULight> lights;

    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f });

    materials.push_back({ glm::vec3(0.8f, 0.8f, 0.8f), 0.2f, 1.0f, 1.0f, 0.4f, 0.0f, 1.0f, 1, 0.0f, 0.0f });

    spheres.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.5f, 0, 0.0f, 0.0f, 0.0f });
    spheres.push_back({ glm::vec3(0.0f, -100.5f, 0.0f), 100.0f, 1, 0.0f, 0.0f, 0.0f });

    lights.push_back({ glm::vec3(-1.0f, 15.0f, -1.0f), 0.0f, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f });
    lights.push_back({ glm::vec3(2.0f, 1.0f, -10.0f),  0.0f, glm::vec3(1.0f, 0.67f, 0.95f), 0.0f });

    VulkanCore engine;
    engine.loadScene(materials, spheres, triangles, lights);

    try {
        engine.run();
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}