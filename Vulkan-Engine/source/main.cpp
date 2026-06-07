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

    bool USE_LEGACY_PIPELINE = false; // true = raytacing using legacy shader, false = ray tracing + RC

    int GLOBAL_SHADING_MODEL = 0; // 0 = Blinn-Phong, 1 = PBR

    int ENABLE_SOFT_SHADOWS = 0; // 0 = Off, 1 = On
    int MAX_DEPTH = 5; // recursion depth for ray tracing
    int MAX_SHADOW_RAYS = 4; // 4, 8, 16, 32, 64, 128

    int ENABLE_DOF = 0; // 0 = Off, 1 = On 
    float FOCAL_DISTANCE = 1.6f; // 0.5f, 1.0f, 2.0f
    float LENS_RADIUS = 0.02f; // 0.01f, 0.05f, 0.1f

    int ENABLE_FOG = 0; // 0 = Off, 1 = On
    glm::vec3 FOG_COLOR = glm::vec3(1.0, 1.0, 1.0);

    int ENABLE_SKYBOX = 0; // 0 = Off, 1 = On
    glm::vec3 SKY_BOTTOM_COLOR = glm::vec3(1.0f, 1.0f, 1.0f); // White at the horizon
    glm::vec3 SKY_TOP_COLOR = glm::vec3(0.1f, 0.3f, 0.7f); // Deep blue at the top

    int ENABLE_TEXTURES = 1; // 0 = Off, 1 = On

    int PRIMARY_RAYS_PER_PIXEL = 1; //1 (Off), 2, 4, 8, 16, 32

    int ENABLE_CAUSTICS = 0; // 0 = Off, 1 = On
    int PHOTON_COUNT = 1000000; // Max 5,000,000
    float CAUSTIC_INTENSITY = 15.0f; // 5.0f, 10.0f, 15.0f, 30.0f

    glm::vec3 GRID_MIN = glm::vec3(-20.0f);
    glm::vec3 GRID_MAX = glm::vec3(20.0f);
    int GRID_RESOLUTION = 256;
    float GATHER_RADIUS = 0.4f;

    int pavingColor = ImageLoader::load("assets/Textures/PavingStone/Color.jpg", texturePaths);
    int pavingNormal = ImageLoader::load("assets/Textures/PavingStone/Normal.jpg", texturePaths);
    int pavingRoughness = ImageLoader::load("assets/Textures/PavingStone/Roughness.jpg", texturePaths);
    int pavingAO = ImageLoader::load("assets/Textures/PavingStone/AmbientOcclusion.jpg", texturePaths);
    int pavingDisp = ImageLoader::load("assets/Textures/PavingStone/Displacement.jpg", texturePaths);

    int iceColor = ImageLoader::load("assets/Textures/Ice/Color.jpg", texturePaths);
    int iceNormal = ImageLoader::load("assets/Textures/Ice/Normal.jpg", texturePaths);
    int iceRoughness = ImageLoader::load("assets/Textures/Ice/Roughness.jpg", texturePaths);
    int iceDisp = ImageLoader::load("assets/Textures/Ice/Displacement.jpg", texturePaths);

    int tilesColor = ImageLoader::load("assets/Textures/Tiles/Color.jpg", texturePaths);
    int tilesNormal = ImageLoader::load("assets/Textures/Tiles/Normal.jpg", texturePaths);
    int tilesRoughness = ImageLoader::load("assets/Textures/Tiles/Roughness.jpg", texturePaths);
    int tilesAO = ImageLoader::load("assets/Textures/Tiles/AmbientOcclusion.jpg", texturePaths);
    int tilesDisp = ImageLoader::load("assets/Textures/Tiles/Displacement.jpg", texturePaths);

    int woodColor = ImageLoader::load("assets/Textures/Wood/Color.jpg", texturePaths);
    int woodNormal = ImageLoader::load("assets/Textures/Wood/Normal.jpg", texturePaths);
    int woodRoughness = ImageLoader::load("assets/Textures/Wood/Roughness.jpg", texturePaths);
    int woodAO = ImageLoader::load("assets/Textures/Wood/AmbientOcclusion.jpg", texturePaths);
    int woodDisp = ImageLoader::load("assets/Textures/Wood/Displacement.jpg", texturePaths);

    int groundColor = ImageLoader::load("assets/Textures/Ground/Color.jpg", texturePaths);
    int groundNormal = ImageLoader::load("assets/Textures/Ground/Normal.jpg", texturePaths);
    int groundRoughness = ImageLoader::load("assets/Textures/Ground/Roughness.jpg", texturePaths);
    int groundAO = ImageLoader::load("assets/Textures/Ground/AmbientOcclusion.jpg", texturePaths);
    int groundDisp = ImageLoader::load("assets/Textures/Ground/Displacement.jpg", texturePaths);

    int sandColor = ImageLoader::load("assets/Textures/Sand/Color.jpg", texturePaths);
    int sandNormal = ImageLoader::load("assets/Textures/Sand/Normal.jpg", texturePaths);
    int sandRoughness = ImageLoader::load("assets/Textures/Sand/Roughness.jpg", texturePaths);
    int sandAO = ImageLoader::load("assets/Textures/Sand/AmbientOcclusion.jpg", texturePaths);
    int sandDisp = ImageLoader::load("assets/Textures/Sand/Displacement.jpg", texturePaths);

    materials.push_back({ glm::vec3(0.067f, 0.067f, 0.067f), 0.2f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 0: Dark Mirror
    materials.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.2f, 0.1f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 1: Red Mirror
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.01f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 2: White Shiny
    materials.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.01f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 3: Black Shiny
    materials.push_back({ glm::vec3(1.0f, 1.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.5f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 4: Yellow Mirror
    materials.push_back({ glm::vec3(0.0f, 1.0f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 5: Green Mirror
    materials.push_back({ glm::vec3(0.0f, 0.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 1, 0.0f, 0.0f, 1, 1, -1, -1, -1, -1, -1, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 6: Checkered Blue and White
    materials.push_back({ glm::vec3(0.0f, 1.0f, 1.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 7: Cyan Mirror
    materials.push_back({ glm::vec3(1.0f, 0.647f, 0.0f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 8: Orange Mirror
    materials.push_back({ glm::vec3(0.5f, 0.0f, 0.5f), 0.05f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 9: Purple Mirror
    materials.push_back({ glm::vec3(0.54f, 0.17f, 0.886f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.2f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.3f, 0.8f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 10: Purple Mirror
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.1f, 0.75f, 1.5f, GLOBAL_SHADING_MODEL, 0, 0.0f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 11: Solid Glass
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 12: Matte White
    materials.push_back({ glm::vec3(1.0f, 0.1f, 0.1f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 13: Matte Red
    materials.push_back({ glm::vec3(0.1f, 1.0f, 0.1f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.95f, 0.0f, 1, 0, -1, -1, -1, -1, -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 14: Matte Green
    // useTexture = 2 selects triplanar world-space projection (no UVs needed -- the teapot OBJ has none);
    // proceduralScale doubles as the world-space tiling frequency for sampleTriplanar(). Albedo only --
    // normalMapIndex stays -1 since triplanar normal-map blending needs a 3-way tangent-space whiteout blend.
    materials.push_back({ glm::vec3(1.0f, 1.0f, 1.0f), 0.1f, glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.5f, 0.0f, 0.0f, 1.0f, GLOBAL_SHADING_MODEL, 0, 0.5f, 0.0f, 1, 2, tilesColor, -1, -1, -1, -1, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); // 15: Tiles Triplanar PBR

    planes.push_back({ glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f });

    quads.push_back({ glm::vec3(-15.0f, 25.0f, -15.0f), 0.0f, glm::vec3(30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 45.0f), 0.0f, glm::vec3(0.0f, -1.0f, 0.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Ceiling (Matte White)
    quads.push_back({ glm::vec3(-15.0f, -0.5f, -15.0f), 0.0f, glm::vec3(0.0f, 0.0f, 45.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(1.0f, 0.0f, 0.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Left Wall (Matte Red)
    quads.push_back({ glm::vec3(15.0f, -0.5f, 30.0f), 0.0f, glm::vec3(0.0f, 0.0f, -45.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(-1.0f, 0.0f, 0.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Right Wall (Matte Green)
    quads.push_back({ glm::vec3(15.0f, -0.5f, 30.0f), 0.0f, glm::vec3(-30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, -1.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Back Wall (Matte White)
    quads.push_back({ glm::vec3(-15.0f, -0.5f, -15.0f), 0.0f, glm::vec3(30.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 25.5f, 0.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f), 12, 0.0f, 0.0f, 0.0f, 0.0f }); // Front Wall (Matte White)

    //cubes.push_back({
    //    glm::vec3(-3.0f, -3.0f, -3.0f), 0.0f,
    //    glm::vec3(3.0f, 3.0f, 3.0f), 0.0f,
    //    glm::vec3(0.0f, 2.0f, 7.0f), 0.0f,
    //    glm::vec3(0.0f, 45.0f, 0.0f),
    //    14, 0.0f, 0.0f, 0.0f, 0.0f
    //    });

    //spheres.push_back({
    //    glm::vec3(-15.0f, 2.0f, 2.0f), 2.0f,
    //    13, 0.0f, 0.0f, 0.0f
    //    });

    //auto pushTri = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int matIdx) {
    //    glm::vec3 flatN = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    //    triangles.push_back({ v0, 0.0f, v1, 0.0f, v2, 0.0f, flatN, 0.0f, flatN, 0.0f, flatN, 0, matIdx, 0.0f, 0.0f, 0.0f });
    //    };

    //const int numSegments = 64;
    //const float radius = 3.0f;
    //const float height = 5.0f;
    //const glm::vec3 baseCenter(5.0f, 2.0f, 0.0f);
    //const glm::vec3 tip(5.0f, 2.0f + height, 0.0f);
    //const int glassMatIdx = 11;

    //const float PI = 3.14159265359f;
    //for (int i = 0; i < numSegments; i++) {
    //    float theta1 = ((float)i / numSegments) * 2.0f * PI;
    //    float theta2 = ((float)(i + 1) / numSegments) * 2.0f * PI;

    //    glm::vec3 p1 = baseCenter + glm::vec3(cos(theta1) * radius, 0.0f, sin(theta1) * radius);
    //    glm::vec3 p2 = baseCenter + glm::vec3(cos(theta2) * radius, 0.0f, sin(theta2) * radius);

    //    pushTri(p1, p2, tip, glassMatIdx);
    //    pushTri(p2, p1, baseCenter, glassMatIdx);
    //}

    // --- LIGHTS ---
    float lightRadius = (ENABLE_SOFT_SHADOWS == 1) ? 5.0f : 0.0f;
    float lightIntensity = (GLOBAL_SHADING_MODEL == 1) ? 200.0f : 50.0f;

    lights.push_back({ glm::vec3(0.0f, 18.0f, 0.0f), lightRadius, glm::vec3(1.0f, 1.0f, 1.0f) * lightIntensity, 0.0f });
    lights.push_back({ glm::vec3(1.0f, 0.0f, 0.0f), lightRadius, glm::vec3(0.0f, 1.0f, 1.0f) * glm::vec3(0.5f, 0.5f, 0.5f), 0.0f });

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
        for (const auto& sphere : spheres) {
            tessellateSphere(sphere, sphere.materialIndex, triangles);
        }
        for (const auto& cube : cubes) {
            tessellateCube(cube, cube.materialIndex, triangles);
        }

        spheres.clear();
        cubes.clear();

        ModelLoader::buildBVH(triangles, bvhNodes);
    }

    engine.loadScene(materials, spheres, triangles, lights, planes, quads, cubes, bvhNodes);
    engine.loadTextures(texturePaths);

    // --- TLAS/BLAS showcase: a doughnut floating in the middle of the room ---
    // model_doughnut.obj is ~0.15 units across (real-world-scale Blender export, has its
    // own vn/vt so no triplanar mapping is needed); scale it up to ~4.5 units and give it
    // a simple flat material override. Bobs up/down sinusoidally and spins continuously --
    // both driven purely by the per-frame transform refresh, no BVH rebuild.
    const int tilesTriplanarMatIdx = 15;
    const int solidGlassMatIdx = 11;
    const int matteRedMatIdx = 13;

    MeshInstance doughnut;
    doughnut.meshIndex = 0;
    doughnut.basePosition = glm::vec3(0.0f, 3.0f, 7.0f);
    doughnut.baseRotationDeg = glm::vec3(90.0f, 0.0f, 0.0f);
    doughnut.scale = 30.0f;
    doughnut.spinDegPerSec = 30.0f;
    doughnut.bobAmplitude = 1.0f;
    doughnut.bobSpeed = 0.25f;
    doughnut.materialOverride = matteRedMatIdx;

    /*engine.loadDynamicMeshes(
        { "assets/models/model_doughnut.obj" },
        { matteRedMatIdx },
        { doughnut }
    );*/

    MeshInstance doughnutGlass;
    doughnutGlass.meshIndex = 0;
    doughnutGlass.basePosition = glm::vec3(10.0f, 3.0f, 3.0f);
    doughnutGlass.baseRotationDeg = glm::vec3(90.0f, 0.0f, 0.0f);
    doughnutGlass.scale = 30.0f;
    doughnutGlass.spinDegPerSec = 30.0f;
    doughnutGlass.bobAmplitude = 1.0f;
    doughnutGlass.bobSpeed = 0.25f;
    doughnutGlass.materialOverride = solidGlassMatIdx;
    engine.loadDynamicMeshes(
        { "assets/models/model_doughnut.obj" },
        { matteRedMatIdx, solidGlassMatIdx },
        { doughnut, doughnutGlass }
    );

    //MeshInstance teapotTiles;
    //teapotTiles.meshIndex = 0;
    //teapotTiles.basePosition = glm::vec3(-4.0f, 3.0f, 7.0f);
    //teapotTiles.baseRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
    //teapotTiles.scale = 0.05f;
    //teapotTiles.spinDegPerSec = 30.0f;
    //teapotTiles.materialOverride = matteRedMatIdx;

    //MeshInstance teapotGlass;
    //teapotGlass.meshIndex = 0;
    //teapotGlass.basePosition = glm::vec3(4.0f, 3.0f, 7.0f);
    //teapotGlass.baseRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
    //teapotGlass.scale = 0.05f;
    //teapotGlass.spinDegPerSec = -30.0f;
    //teapotGlass.materialOverride = solidGlassMatIdx;

    //engine.loadDynamicMeshes(
    //    { "assets/models/model_teapot.obj" },
    //    { tilesTriplanarMatIdx },
    //    { teapotTiles }
    //);

    try {
        engine.run();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}