#include "renderer/Renderer.hpp"
#include "scene/ModelLoader.hpp"
#include "math/MathUtils.hpp"
#include "scene/ImageLoader.hpp"
#include "scene/Primitive.hpp"
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
    vector<string> texturePaths;

    // --- SETTINGS ---
	bool USE_LEGACY_PIPELINE = true; // true = raytacing using legacy shader, false = ray tracing + RC

    int GLOBAL_SHADING_MODEL = 1; // 0 = Blinn-Phong, 1 = PBR

    int ENABLE_SOFT_SHADOWS = 0; // 0 = Off, 1 = On
    int MAX_DEPTH = 5; // recursion depth for ray tracing
    int MAX_SHADOW_RAYS = 4; // 4, 8, 16, 32, 64, 128

    int ENABLE_DOF = 0; // 0 = Off, 1 = On 
    float FOCAL_DISTANCE = 1.6f; // 0.5f, 1.0f, 2.0f
    float LENS_RADIUS = 0.02f; // 0.01f, 0.05f, 0.1f

    int ENABLE_FOG = 1; // 0 = Off, 1 = On
    glm::vec3 FOG_COLOR = glm::vec3(1.0, 1.0, 1.0); // Grey: glm::vec3(0.05, 0.05, 0.05); Blue: glm::vec3(0.1, 0.2, 0.5);  White: glm::vec3(1.0, 1.0, 1.0); Green: glm::vec3(0.2, 0.8, 0.2);

    int ENABLE_SKYBOX = 1; // 0 = Off, 1 = On
    glm::vec3 SKY_BOTTOM_COLOR = glm::vec3(1.0f, 1.0f, 1.0f); // White at the horizon
    glm::vec3 SKY_TOP_COLOR = glm::vec3(0.35f, 0.55f, 0.95f); // bright blue at the top

    int ENABLE_TEXTURES = 1; // 0 = Off, 1 = On

    int PRIMARY_RAYS_PER_PIXEL = 1; //1 (Off), 2, 4, 8, 16, 32

    int ENABLE_CAUSTICS = 0; // 0 = Off, 1 = On
    int PHOTON_COUNT = 1000000; // Max 5,000,000
    float CAUSTIC_INTENSITY = 4.0f; // 5.0f, 10.0f, 4.0f, 30.0f

    glm::vec3 GRID_MIN = glm::vec3(-40.0f); // -10.0f, ~ -40.0f 
    glm::vec3 GRID_MAX = glm::vec3(40.0f); // 10.0f, ~ 40.0f 
    int GRID_RESOLUTION = 256; // 64 ~ 256
    float GATHER_RADIUS = 0.6f; // 0.2f ~ 0.6f

    // --- TEXTURES ---
    // Format: ImageLoader::load("path", vector) - If a material lacks a map pass -1 to the material constructor.

    // Paving Stone Textures
    int pavingColor = ImageLoader::load("assets/Textures/PavingStone/Color.jpg", texturePaths);
    int pavingNormal = ImageLoader::load("assets/Textures/PavingStone/Normal.jpg", texturePaths);
    int pavingRoughness = ImageLoader::load("assets/Textures/PavingStone/Roughness.jpg", texturePaths);
    int pavingAO = ImageLoader::load("assets/Textures/PavingStone/AmbientOcclusion.jpg", texturePaths);
    int pavingDisp = ImageLoader::load("assets/Textures/PavingStone/Displacement.jpg", texturePaths);

    int stoneColor = ImageLoader::load("assets/Textures/StoneWall/Color.png", texturePaths);
    int stoneNormal = ImageLoader::load("assets/Textures/StoneWall/Normal.png", texturePaths);
    int stoneRoughness = ImageLoader::load("assets/Textures/StoneWall/Roughness.png", texturePaths);
    int stoneAO = ImageLoader::load("assets/Textures/StoneWall/AmbientOcclusion.png", texturePaths);
    int stoneDisp = ImageLoader::load("assets/Textures/StoneWall/Displacement.png", texturePaths);

    int concreteColor = ImageLoader::load("assets/Textures/Concrete/Color.png", texturePaths);
    int concreteNormal = ImageLoader::load("assets/Textures/Concrete/Normal.png", texturePaths);
    int concreteRoughness = ImageLoader::load("assets/Textures/Concrete/Roughness.png", texturePaths);
    int concreteAO = ImageLoader::load("assets/Textures/Concrete/AmbientOcclusion.png", texturePaths);
    int concreteDisp = ImageLoader::load("assets/Textures/Concrete/Displacement.png", texturePaths);

    // --- MATERIALS ---
    // Format: { color, ambient, emission, diffuse, color2, specular, reflection, transparency, ior, shadingModel, patternType, roughness, metallic, castShadows, useTexture, albedoIdx, normalIdx, roughIdx, aoIdx, heightIdx, procScale, procWobble, bumpStr, parallax, p5, p6 }
    materials.push_back({ glm::vec3(0.067f, 0.067f, 0.067f), 0.2f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 0: Dark Mirror (#111111)
    materials.push_back({ glm::vec3(0.0f, 0.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.2f, 0.1f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 1: Blue Mirror (#FF0000)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.75f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 2: Solid Glass (IOR 1.5)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 3: Matte White
    materials.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 4.0f, 4.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 0, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 4: Emissive Cyan (Cast Shadows = 0)
    materials.push_back({ glm::vec3(1.0f), 0.5f, glm::vec3(0.0f), 1.0f, glm::vec3(0.0f), 1.0f, 0.0f, 0.0f, 1.0f, 1, 0, 1.0f, 0.0f, 1, 1, pavingColor, pavingNormal, pavingRoughness, pavingAO, pavingDisp, 1.5f, 0.0f, 2.0f, 0.05f, 0.0f, 0.0f }); // Material 5: Paving (Tiled 4x)
    materials.push_back({ glm::vec3(1.0f), 0.5f, glm::vec3(0.0f), 1.0f, glm::vec3(0.0f), 1.0f, 0.0f, 0.0f, 1.0f, 1, 0, 1.0f, 0.0f, 1, 1, stoneColor, stoneNormal, stoneRoughness, stoneAO, stoneDisp, 1.5f, 0.0f, 2.0f, 0.05f, 0.0f, 0.0f }); // Material 6: Stone (Tiled 4x)
    materials.push_back({ glm::vec3(1.0f), 0.5f, glm::vec3(0.0f), 1.0f, glm::vec3(0.0f), 1.0f, 0.0f, 0.0f, 1.0f, 1, 0, 1.0f, 0.0f, 1, 1, concreteColor, concreteNormal, concreteRoughness, concreteAO, concreteDisp, 1.0f, 0.0f, 2.0f, 0.05f, 0.0f, 0.0f }); // Material 7: Concrete (Tiled 4x)
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(0.0f), 0.0f, glm::vec3(0.0f), 1.0f, 0.1f, 0.95f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 8: Caustics Glass Suited
    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 1, 0.0f, 0.0f, 1, 1, -1, -1, -1, -1, -1, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 9: Checkered Red and White
    materials.push_back({ glm::vec3(0.54f, 0.17f, 0.886f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.3f, 0.8f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // Material 10: Purple Mirror (#8A2BE2)



    // --- QUADS ---
    // Format: { corner(x,y,z), p1, edge1(x,y,z), p2, edge2(x,y,z), p3, normalVector(x,y,z), materialIndex, p4, p5, p6, p7 }

    float roomWidth = 9.75f;
    float roomLength = 21.9f;
    float wallHeight = 7.0f;

    float halfW = roomWidth / 2.0f;
    float halfL = roomLength / 2.0f;

    glm::vec3 roomOffset = glm::vec3(0.0f, 0.0f, 0.0f);

    quads.push_back({ roomOffset + glm::vec3(-halfW, 0.0f, -halfL), 0.0f, glm::vec3(roomWidth, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, roomLength), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 5, 0.0f, 0.0f, 0.0f, 0.0f });     // Floor
    quads.push_back({ roomOffset + glm::vec3(-halfW, 0.0f, -halfL), 0.0f, glm::vec3(0.0f, 0.0f, roomLength), 0.0f, glm::vec3(0.0f, wallHeight, 0.0f), 0.0f, glm::vec3(1.0f, 0.0f, 0.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f });    // Left Wall
    quads.push_back({ roomOffset + glm::vec3(halfW, 0.0f, halfL), 0.0f, glm::vec3(0.0f, 0.0f, -roomLength), 0.0f, glm::vec3(0.0f, wallHeight, 0.0f), 0.0f, glm::vec3(-1.0f, 0.0f, 0.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f });    // Right Wall
    quads.push_back({ roomOffset + glm::vec3(halfW, 0.0f, halfL), 0.0f, glm::vec3(-roomWidth, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, wallHeight, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, -1.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f });    // Back Wall
    quads.push_back({ roomOffset + glm::vec3(-halfW, 0.0f, -halfL), 0.0f, glm::vec3(roomWidth, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, wallHeight, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f), 7, 0.0f, 0.0f, 0.0f, 0.0f });    // Front Wall


    // --- SPHERES ---
    // Format: { center(x,y,z), radius, materialIndex, padding1, padding2, padding3 }

    spheres.push_back({ glm::vec3(0.0f, 1.5f, 6.0f), 0.75f, 1, 0.0f, 0.0f, 0.0f }); // Material 1: Red Mirror Sphere
    spheres.push_back({ glm::vec3(-5.0f, -3.0f, 11.0f), 4.0f, 9, 0.0f, 0.0f, 0.0f }); // Material 9: Checkered Sphere
    spheres.push_back({ glm::vec3(0.0f, 1.5f, -6.0f), 0.75f, 2, 0.0f, 0.0f, 0.0f }); // Material 2: Solid Glass Sphere
    spheres.push_back({ glm::vec3(3.5f, 1.5f, 9.0f), 0.75f, 4, 0.0f, 0.0f, 0.0f }); // Material 4: Glowing Orb

    // --- MODELS ---
    // Format: ModelLoader::load("filepath", trianglesVector, bvhNodesVector, materialIndex, position(x,y,z), rotation(pitch,yaw,roll), scale)
    //ModelLoader::load("assets/models/model_doughnut.obj", triangles, bvhNodes, 10, glm::vec3(-3.5f, 1.0f, -3.0f), glm::vec3(-30.0f, 0.0f, 5.0f), 20.0f); // Material 10: Model Purple
    //ModelLoader::load("assets/models/model_shotGlass.obj", triangles, bvhNodes, 8, glm::vec3(-3.5f, 0.0f, -8.0f), glm::vec3(0.0f, 90.0f, 0.0f), 0.5f);
    ModelLoader::load("assets/models/model_atrium.obj", triangles, bvhNodes, 6, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 90.0f, 0.0f), 1.0f);
    
    // --- LIGHTS ---
    glm::vec3 lightColor = glm::vec3(1.0f, 0.674f, 0.957f); 
    glm::vec3 sunColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 emissionLightColor = glm::vec3(0.0f, 1.0f, 1.0f);

    float lightRadius = (ENABLE_SOFT_SHADOWS == 1) ? 5.0f : 0.0f;
    float lightIntensity = (GLOBAL_SHADING_MODEL == 1) ? 200.0f : 100.0f;

    // Format: { position(x,y,z), radius, color(r,g,b), padding }  // radius 0.0 = Point Light, > 0.0 = Area Light
    lights.push_back({ glm::vec3(-4.0f, 0.5f, -10.0f), lightRadius, lightColor * (lightIntensity / 7), 0.0f });
    lights.push_back({ glm::vec3(0.0f, 8.0f, 5.0f), lightRadius, sunColor * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(0.0f, 8.0f, 0.0f), lightRadius, sunColor * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(0.0f, 8.0f, -5.0f), lightRadius, sunColor * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(3.5f, 1.5f, 9.0f), lightRadius, emissionLightColor * (lightIntensity / 15), 0.0f });

    Renderer engine;

	engine.useLegacyPipeline = USE_LEGACY_PIPELINE;

    engine.maxDepth = MAX_DEPTH;
    engine.shadowRays = MAX_SHADOW_RAYS;
    engine.primaryRaysPerPixel = PRIMARY_RAYS_PER_PIXEL;

    engine.focalDistance = FOCAL_DISTANCE;
    engine.lensRadius = (ENABLE_DOF == 1) ? LENS_RADIUS : 0.0f;

    engine.enableFog = ENABLE_FOG;
    engine.fogColor = FOG_COLOR;

    engine.enableSkybox = ENABLE_SKYBOX;
    engine.skyBottomColor = SKY_BOTTOM_COLOR;
    engine.skyTopColor = SKY_TOP_COLOR;

    engine.enableTextures = ENABLE_TEXTURES;

    engine.enableCaustics = ENABLE_CAUSTICS;
    engine.totalEmittedPhotons = PHOTON_COUNT;
    engine.causticIntensity = CAUSTIC_INTENSITY;

    engine.gridMin = GRID_MIN;
    engine.gridMax = GRID_MAX;
    engine.gridRes = GRID_RESOLUTION;
    engine.gatherRadius = GATHER_RADIUS;

    if (!USE_LEGACY_PIPELINE)
    {
        // Tessellate object primitives into the BVH so RC probes can see them.
        // Quads (walls/ceiling) and planes (floor) stay analytical, their large
        // triangles bloat the BVH root AABB and make shadow traversal O(N).
        for (const auto& sphere : spheres) {
            tessellateSphere(sphere, sphere.materialIndex, triangles);
        }

        for (const auto& cube : cubes) {
            tessellateCube(cube, cube.materialIndex, triangles);
        }

        // Clear only the primitives now in the BVH. Quads and planes remain
        // in their SSBOs as fast analytical shadow occluders.
        spheres.clear();
        cubes.clear();

        // Rebuild BVH from unified triangle pool
        ModelLoader::buildBVH(triangles, bvhNodes);
    }

    engine.loadScene(materials, spheres, triangles, lights, planes, quads, cubes, bvhNodes);
    engine.loadTextures(texturePaths);

    try {
        engine.run();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}