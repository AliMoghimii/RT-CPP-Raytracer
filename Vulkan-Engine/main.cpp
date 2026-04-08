#include "VulkanCore.hpp"
#include "ModelLoader.hpp"
#include "MathUtils.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>

using namespace std;

int main() {
    vector<GPUMaterial> materials;
    vector<GPUSphere> spheres;
    vector<GPUTriangle> triangles;
    vector<GPULight> lights;
    vector<GPUPlane> planes;
    vector<GPUQuad> quads;
    vector<GPUCube> cubes;
    vector<GPUBVHNode> bvhNodes;

    // --- MATERIALS ---
    materials.push_back({ glm::vec3(0.067f, 0.067f, 0.067f), 0.2f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 0: Mat Dark Mirror (#111111)
    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 1: Mat Red Mirror (#FF0000) 
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.05f, 1.0f, 1.0f, 0.01f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 2: Mat White Shiny (#FFFFFF)
    materials.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.01f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 3: Mat Black Shiny (#000000)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 4: Mat Yellow Mirror (#FFFF00)
    materials.push_back({ glm::vec3(0.0f, 1.0f, 0.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 5: Mat Green Mirror (#00FF00)
	materials.push_back({ glm::vec3(0.0f, 0.0f, 1.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 1, 0.0f, 0.0f }); // Material 6: Mat Checkered Blue and White (#0000FF and #FFFFFF)
    materials.push_back({ glm::vec3(0.0f, 1.0f, 1.0f), 0.05f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 7: Mat Cyan Mirror (#00FFFF)
    materials.push_back({ glm::vec3(1.0f, 0.647f, 0.0f), 0.05f, 1.0f, 1.0f, 0.1f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 8: Mat Orange Mirror (#FFA500)
    materials.push_back({ glm::vec3(0.5f, 0.0f, 0.5f), 0.05f, 1.0f, 1.0f, 0.1f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 9: Mat Purple Mirror (#800080)
    materials.push_back({ glm::vec3(0.54f, 0.17f, 0.886f), 0.1f, 1.0f, 1.0f, 0.2f, 0.0f, 1.0f, 0, 0.0f, 0.0f }); // Material 10: Mat Purple Mirror (#8A2BE2)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, 0.0f, 1.0f, 0.1f, 0.9f, 1.5f, 0, 0.0f, 0.0f }); // Material 11: Mat Solid Glass (IOR 1.5)

    // --- PLANES ---
    planes.push_back({ glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 0, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 0: Floor Plane

    // --- SPHERES ---
    spheres.push_back({ glm::vec3(-0.25f, 0.0f, 0.6f), 0.5f, 1, 0.0f, 0.0f, 0.0f }); // Material 1: Red Sphere
    spheres.push_back({ glm::vec3(-0.15f, 0.1f, 0.15f), 0.1f, 2, 0.0f, 0.0f, 0.0f }); // Material 2: White Eyes
    spheres.push_back({ glm::vec3(-0.12f, 0.12f, 0.05f), 0.05f, 3, 0.0f, 0.0f, 0.0f }); // Material 3: Black Pupils
    spheres.push_back({ glm::vec3(-0.4f, 0.1f, 0.15f), 0.1f, 2, 0.0f, 0.0f, 0.0f }); // Material 2: White Eyes
    spheres.push_back({ glm::vec3(-0.37f, 0.12f, 0.05f), 0.05f, 3, 0.0f, 0.0f, 0.0f }); // Material 3: Black Pupils

    spheres.push_back({ glm::vec3(1.3f, 0.15f, 1.0f), 0.75f, 4, 0.0f, 0.0f, 0.0f }); // Material 4: Yellow Mirror Sphere
    spheres.push_back({ glm::vec3(1.0f, 1.85f, 8.0f), 0.75f, 5, 0.0f, 0.0f, 0.0f }); // Material 5: Green Sphere
    spheres.push_back({ glm::vec3(-5.0f, -3.0f, 5.0f), 5.0f, 6, 0.0f, 0.0f, 0.0f }); // Material 6: Checkered Sphere
	spheres.push_back({ glm::vec3(1.0f, 0.5f, 0.0f), 0.25f, 11, 0.0f, 0.0f, 0.0f }); // Material 11: Solid Glass Sphere

    // --- CUBES ---
    cubes.push_back({ glm::vec3(-0.125f, -0.125f, -0.125f), 0.0f, glm::vec3(0.125f, 0.125f, 0.125f), 0.0f, glm::vec3(0.375f, -0.375f, 0.375f), 0.0f, glm::vec3(0.0f, 35.0f, 0.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 7: Cyan Cube
    cubes.push_back({glm::vec3(-0.15f, -0.15f, -0.15f), 0.0f, glm::vec3(0.15f, 0.15f, 0.15f), 0.0f, glm::vec3(-1.5f, 0.0f, -0.2f), 0.0f, glm::vec3(0.0f, 20.0f, 0.0f), 11, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 11: Glass Cube

    // --- PYRAMID ---
    glm::vec3 pts[4] = {
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(-1.0f, -1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f),
        glm::vec3(0.0f, -1.0f, -1.0f)
    };

    for (int i = 0; i < 4; i++) {
        pts[i] = pts[i] * 0.1f;
        pts[i] = MathUtils::rotateVec(pts[i], glm::vec3(0.0f, 125.0f, 0.0f));
        pts[i] = pts[i] + glm::vec3(0.35f, 0.0f, 0.35f);
    }

    auto pushTri = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int matIdx) {
        glm::vec3 flatN = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        triangles.push_back({ v0, 0.0f, v1, 0.0f, v2, 0.0f, flatN, 0.0f, flatN, 0.0f, flatN, 0, matIdx, 0.0f, 0.0f, 0.0f });
    };

    pushTri(pts[0], pts[1], pts[2], 8); // Material 8: Orange Triangle
    pushTri(pts[0], pts[2], pts[3], 9); // Material 9: Purple Triangle
    pushTri(pts[0], pts[3], pts[1], 7); // Material 7: Cyan Triangle
    pushTri(pts[1], pts[3], pts[2], 5); // Material 5: Green Triangle

    // --- MODELS ---
    //ModelLoader::load("assets/model_shuttle.obj", triangles, bvhNodes, 10, glm::vec3(-1.3f, 0.0f, 0.4f), glm::vec3(270.0f, 45.0f, 0.0f), 0.07f); // Material 10: Model Shuttle Purple
    //ModelLoader::load("assets/model_teapot.obj", triangles, bvhNodes, 10, glm::vec3(-1.2f, 0.0f, 0.5f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0045f); // Material 10: Model Shuttle Purple
    ModelLoader::load("assets/model_doughnut.obj", triangles, bvhNodes, 10, glm::vec3(-1.2f, 0.0f, 0.5f), glm::vec3(0.0f, 0.0f, 0.0f), 5.0f); // Material 10: Model Shuttle Purple

    // --- LIGHTS ---
    lights.push_back({ glm::vec3(-1.0f, 15.0f, -1.0f), 0.0f, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f }); // Light 0: POINT LIGHT (For hardshadows)
    lights.push_back({ glm::vec3(2.0f, 1.0f, -10.0f), 0.0f, glm::vec3(1.0f, 0.674f, 0.957f), 0.0f }); // Light 1: POINT LIGHT (For hardshadows)
    //lights.push_back({ glm::vec3(-1.0f, 15.0f, -1.0f), 5.0f, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f }); // Light 0: AREA LIGHT (For softshadows)
    //lights.push_back({ glm::vec3(2.0f, 1.0f, -10.0f), 6.0f, glm::vec3(1.0f, 0.674f, 0.957f), 0.0f }); // Light 1: POINT LIGHT (For softshadows)

    VulkanCore engine;
    engine.loadScene(materials, spheres, triangles, lights, planes, quads, cubes, bvhNodes);

    try {
        engine.run();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}