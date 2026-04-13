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

    // --- SETTINGS ---
    int GLOBAL_SHADING_MODEL = 0; // 0 = Blinn-Phong, 1 = PBR

    int ENABLE_SOFT_SHADOWS = 1; // 0 = Off, 1 = On
    int MAX_DEPTH = 5; // recursion depth for ray tracing
    int MAX_SHADOW_RAYS = 8; // 4, 8, 16, 32, 64, 128

    int ENABLE_DOF = 1; // 0 = Off, 1 = On 
    float FOCAL_DISTANCE = 1.6f; // 0.5f, 1.0f, 2.0f
    float LENS_RADIUS = 0.02f; // 0.01f, 0.05f, 0.1f

    int ENABLE_FOG = 1; // 0 = Off, 1 = On
    glm::vec3 FOG_COLOR = glm::vec3(1.0, 1.0, 1.0); // Grey: glm::vec3(0.05, 0.05, 0.05); Blue: glm::vec3(0.1, 0.2, 0.5);  White: glm::vec3(1.0, 1.0, 1.0); Green: glm::vec3(0.2, 0.8, 0.2);

    int ENABLE_SKYBOX = 1; // 0 = Off, 1 = On
    glm::vec3 SKY_BOTTOM_COLOR = glm::vec3(1.0f, 1.0f, 1.0f); // White at the horizon
    glm::vec3 SKY_TOP_COLOR = glm::vec3(0.1f, 0.3f, 0.7f); // Deep blue at the top

    int ENABLE_TEXTURES = 1; // 0 = Off, 1 = On

    int SAMPLES_PER_PIXEL = 2; //1 (Off), 2, 4, 8, 16, 32

    // --- MATERIALS ---
    // Format: { color, ambient, emission, diffuse, color2, specular, reflection, transparency, ior, shadingModel, patternType, roughness, metallic, castShadows, useTexture, textureIndex, p1, p2, p3, p4, p5, p6 }
    materials.push_back({ glm::vec3(0.067f, 0.067f, 0.067f), 0.2f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 0: Dark Mirror (#111111)
    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.2f, 0.1f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 1: Red Mirror (#FF0000)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.01f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 2: White Shiny (#FFFFFF)
    materials.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.01f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 3: Black Shiny (#000000)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.5f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 4: Yellow Mirror (#FFFF00)
    materials.push_back({ glm::vec3(0.0f, 1.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 5: Green Mirror (#00FF00)
    materials.push_back({ glm::vec3(0.0f, 0.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 1, 0.0f, 0.0f, 1, 1, -1, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 6: Checkered Blue and White
    materials.push_back({ glm::vec3(0.0f, 1.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 7: Cyan Mirror (#00FFFF)
    materials.push_back({ glm::vec3(1.0f, 0.647f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 8: Orange Mirror (#FFA500)
    materials.push_back({ glm::vec3(0.5f, 0.0f, 0.5f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 9: Purple Mirror (#800080)
    materials.push_back({ glm::vec3(0.54f, 0.17f, 0.886f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.3f, 0.8f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 10: Purple Mirror (#8A2BE2)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.9f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 11: Solid Glass (IOR 1.5)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 12: Matte White
    materials.push_back({ glm::vec3(1.0f, 0.1f, 0.1f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 13: Matte Red
    materials.push_back({ glm::vec3(0.1f, 1.0f, 0.1f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 14: Matte Green
    materials.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 15.0f, 15.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 0, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 15: Emissive Cyan (Cast Shadows = 0)
    materials.push_back({ glm::vec3(1.0f, 0.8f, 0.2f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.4f, 0.32f, 0.08f), 0.1f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 2, 0.8f, 0.0f, 1, 1, -1, 16.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 16: Matte Procedural Texture Wood
    materials.push_back({ glm::vec3(0.0f, 0.8f, 0.3f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.05f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 3, 0.8f, 0.0f, 1, 1, -1, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 17: Shiny Procedural Texture Marble
    materials.push_back({ glm::vec3(1.0f, 0.1f, 0.1f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.9f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 18: Red Stained Glass
    materials.push_back({ glm::vec3(0.1f, 0.1f, 1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.9f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 19: Blue Stained Glass

    // --- PLANES ---
    // Format: { center(x,y,z), padding1, normal(x,y,z), materialIndex, padding2, padding3, padding4, padding5 }
    planes.push_back({ glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 0, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 0: Floor (Dark Mirror)

    // --- QUADS ---
    // Format: { corner(x,y,z), p1, edge1(x,y,z), p2, edge2(x,y,z), p3, normalVector(x,y,z), materialIndex, p4, p5, p6, p7 }
    quads.push_back({ glm::vec3(-15.0f, 25.0f, -15.0f), 0.0f, glm::vec3(30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 45.0f), 0.0f, glm::vec3(0.0f, -1.0f, 0.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Ceiling (Matte White)
    quads.push_back({ glm::vec3(-15.0f, -0.5f, -15.0f), 0.0f, glm::vec3(0.0f, 0.0f, 45.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(1.0f, 0.0f, 0.0f), 13, 0.0f, 0.0f, 0.0f, 0.0f }); // Left Wall (Matte Red)
    quads.push_back({ glm::vec3(15.0f, -0.5f, 30.0f), 0.0f, glm::vec3(0.0f, 0.0f, -45.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(-1.0f, 0.0f, 0.0f), 14, 0.0f, 0.0f, 0.0f, 0.0f }); // Right Wall (Matte Green)
    quads.push_back({ glm::vec3(15.0f, -0.5f, 30.0f), 0.0f, glm::vec3(-30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, -1.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Back Wall (Matte White)
    quads.push_back({ glm::vec3(-15.0f, -0.5f, -15.0f), 0.0f, glm::vec3(30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Front Wall (Matte White)

    // --- SPHERES ---
    // Format: { center(x,y,z), radius, materialIndex, padding1, padding2, padding3 }
    spheres.push_back({ glm::vec3(-0.25f, 0.0f, 0.6f), 0.5f, 1, 0.0f, 0.0f, 0.0f }); // Material 1: Red Sphere
    spheres.push_back({ glm::vec3(-0.15f, 0.1f, 0.15f), 0.1f, 2, 0.0f, 0.0f, 0.0f }); // Material 2: White Eyes
    spheres.push_back({ glm::vec3(-0.12f, 0.12f, 0.05f), 0.05f, 0, 0.0f, 0.0f, 0.0f }); // Material 0: Black Pupils
    spheres.push_back({ glm::vec3(-0.4f, 0.1f, 0.15f), 0.1f, 2, 0.0f, 0.0f, 0.0f }); // Material 2: White Eyes
    spheres.push_back({ glm::vec3(-0.37f, 0.12f, 0.05f), 0.05f, 0, 0.0f, 0.0f, 0.0f }); // Material 0: Black Pupils
    spheres.push_back({ glm::vec3(1.3f, 0.15f, 1.5f), 0.75f, 4, 0.0f, 0.0f, 0.0f }); // Material 4: Yellow Mirror Sphere
    spheres.push_back({ glm::vec3(1.0f, 1.85f, 8.0f), 0.75f, 5, 0.0f, 0.0f, 0.0f }); // Material 5: Green Sphere
    spheres.push_back({ glm::vec3(-5.0f, 10.0f, 25.0f), 2.0f, 0, 0.0f, 0.0f, 0.0f }); // Material 0: Dark Sphere
    spheres.push_back({ glm::vec3(-5.0f, -3.0f, 5.0f), 5.0f, 6, 0.0f, 0.0f, 0.0f }); // Material 6: Checkered Sphere
    spheres.push_back({ glm::vec3(1.0f, 0.75f, 0.0f), 0.25f, 11, 0.0f, 0.0f, 0.0f }); // Material 11: Solid Glass Sphere
    spheres.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.2f, 15, 0.0f, 0.0f, 0.0f }); // Material 15: Glowing Orb
    spheres.push_back({ glm::vec3(10.0f, 2.0f, 20.0f), 2.5f, 18, 0.0f, 0.0f, 0.0f }); // Material 18: Red Stained Glass Sphere

    // --- CUBES ---
    // Format: { boundsMin(x,y,z), padding1, boundsMax(x,y,z), padding2, center(x,y,z), padding3, rotation(pitch,yaw,roll), materialIndex, padding4, padding5, padding6, padding7 }
    cubes.push_back({ glm::vec3(-0.125f, -0.125f, -0.125f), 0.0f, glm::vec3(0.125f, 0.125f, 0.125f), 0.0f, glm::vec3(0.375f, -0.375f, 0.375f), 0.0f, glm::vec3(0.0f, 35.0f, 0.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 7: Cyan Cube
    cubes.push_back({ glm::vec3(-0.15f, -0.15f, -0.15f), 0.0f, glm::vec3(0.15f, 0.15f, 0.15f), 0.0f, glm::vec3(-1.5f, 0.0f, -0.2f), 0.0f, glm::vec3(0.0f, 20.0f, 0.0f), 11, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 11: Glass Cube
    cubes.push_back({ glm::vec3(-0.15f, -0.15f, -0.15f), 0.0f, glm::vec3(0.15f, 0.15f, 0.15f), 0.0f, glm::vec3(-1.5f, 0.3f, -0.2f), 0.0f, glm::vec3(0.0f, 45.0f, 0.0f), 16, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 16: Wood Cube
    cubes.push_back({ glm::vec3(-0.15f, -0.15f, -0.15f), 0.0f, glm::vec3(0.15f, 0.15f, 0.15f), 0.0f, glm::vec3(-1.5f, 0.6f, -0.2f), 0.0f, glm::vec3(0.0f, 10.0f, 0.0f), 17, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 17: Marble Cube
    cubes.push_back({ glm::vec3(-2.0f, -2.0f, -2.0f), 0.0f, glm::vec3(2.0f, 2.0f, 2.0f), 0.0f, glm::vec3(-13.0f, 1.5f, 25.0f), 0.0f, glm::vec3(0.0f, 45.0f, 0.0f), 19, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 19: Blue Stained Glass Cube

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

    // Format: { vertex0(vec3), pad, vertex1(vec3), pad, vertex2(vec3), pad, normal0(vec3), pad, normal1(vec3), pad, normal2(vec3), isSmooth, materialIndex, pad, pad, pad }
    auto pushTri = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int matIdx) {
        glm::vec3 flatN = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        triangles.push_back({ v0, 0.0f, v1, 0.0f, v2, 0.0f, flatN, 0.0f, flatN, 0.0f, flatN, 0, matIdx, 0.0f, 0.0f, 0.0f });
        };

    pushTri(pts[0], pts[1], pts[2], 8); // Material 8: Orange Triangle
    pushTri(pts[0], pts[2], pts[3], 9); // Material 9: Purple Triangle
    pushTri(pts[0], pts[3], pts[1], 7); // Material 7: Cyan Triangle
    pushTri(pts[1], pts[3], pts[2], 5); // Material 5: Green Triangle

    // --- MODELS ---
    // Format: ModelLoader::load("filepath", trianglesVector, bvhNodesVector, materialIndex, position(x,y,z), rotation(pitch,yaw,roll), scale)
    //ModelLoader::load("assets/model_shuttle.obj", triangles, bvhNodes, 10, glm::vec3(-1.3f, 0.0f, 0.4f), glm::vec3(270.0f, 45.0f, 0.0f), 0.07f); // Material 10: Model Purple
    //ModelLoader::load("assets/model_teapot.obj", triangles, bvhNodes, 10, glm::vec3(-1.2f, 0.0f, 0.5f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0045f); // Material 10: Model Purple
    ModelLoader::load("assets/model_doughnut.obj", triangles, bvhNodes, 10, glm::vec3(-1.2f, 0.0f, 0.5f), glm::vec3(-30.0f, 0.0f, 5.0f), 5.0f); // Material 10: Model Purple

    // --- LIGHTS ---
    glm::vec3 topLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 backLightColor = glm::vec3(1.0f, 0.674f, 0.957f);
    glm::vec3 emissionLightColor = glm::vec3(0.0f, 1.0f, 1.0f);

    float lightRadius = (ENABLE_SOFT_SHADOWS == 1) ? 5.0f : 0.0f;
    float lightIntensity = (GLOBAL_SHADING_MODEL == 1) ? 200.0f : 100.0f;

    // Format: { position(x,y,z), radius, color(r,g,b), padding }  // radius 0.0 = Point Light, > 0.0 = Area Light
    lights.push_back({ glm::vec3(-1.0f, 15.0f, -1.0f), lightRadius, topLightColor * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(2.0f, 1.0f, -10.0f), lightRadius, backLightColor * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), lightRadius, emissionLightColor * glm::vec3(0.5f, 0.5f, 0.5f), 0.0f });

    VulkanCore engine;

    engine.maxDepth = MAX_DEPTH;
    engine.shadowRays = MAX_SHADOW_RAYS;
    engine.samplesPerPixel = SAMPLES_PER_PIXEL;

    engine.focalDistance = FOCAL_DISTANCE;
    engine.lensRadius = (ENABLE_DOF == 1) ? LENS_RADIUS : 0.0f;

    engine.enableFog = ENABLE_FOG;
    engine.fogColor = FOG_COLOR;

    engine.enableSkybox = ENABLE_SKYBOX;
    engine.skyBottomColor = SKY_BOTTOM_COLOR;
    engine.skyTopColor = SKY_TOP_COLOR;

    engine.enableTextures = ENABLE_TEXTURES;

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