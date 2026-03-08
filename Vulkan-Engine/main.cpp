#include "VulkanCore.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    std::vector<GPUMaterial> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUTriangle> triangles;

    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, {0,0} });
    materials.push_back({ glm::vec3(0.8f, 0.8f, 0.8f), 0.2f, 1.0f, 1.0f, 0.4f, 0.0f, 1.0f, 0, {0,0} });
    spheres.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.5f, 0, {0,0,0} });
    spheres.push_back({ glm::vec3(0.0f, -100.5f, 0.0f), 100.0f, 1, {0,0,0} });

    VulkanCore engine;
    engine.loadScene(materials, spheres, triangles);

    try {
        engine.run();
    }
    catch (const std::exception& e) {
        std::cerr << "CRITICAL GPU ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}