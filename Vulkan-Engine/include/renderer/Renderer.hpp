#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "core/VulkanContext.hpp"
#include "core/Swapchain.hpp"
#include "core/CommandManager.hpp"
#include "passes/GBuffer.hpp"
#include "passes/LegacyPass.hpp"
#include "resources/Buffer.hpp"
#include "resources/Image.hpp"
#include "scene/GPUData.hpp"
#include "gi/CascadeConfig.hpp"
#include "gi/CascadeStorage.hpp"
#include "debug/FrameStats.hpp"
#include "debug/DebugUI.hpp"

struct CameraPushConstants {
    glm::vec4 camPos;
    glm::vec4 camForward;
    glm::vec4 camRight;
    glm::vec4 camUp;
    int sphereCount;
    int triangleCount;
    int planeCount;
    int quadCount;
    int cubeCount;
    int lightCount;
    int bvhCount;
    int maxDepth;
    int shadowRays;
    int primaryRaysPerPixel;
    float focalDistance;
    float lensRadius;
    glm::vec3 fogColor;
    int enableFog;
    glm::vec3 skyBottomColor;
    int enableSkybox;
    glm::vec3 skyTopColor;
    int enableTextures;
};

class Renderer {
public:
    // --- Render settings (wired into push constants each frame) ---
    int maxDepth = 5;
    int shadowRays = 4;
    bool useLegacyPipeline = false;
    int primaryRaysPerPixel = 1;
    float focalDistance = 1.0f;
    float lensRadius = 0.0f;
    int enableFog = 0;
    glm::vec3 fogColor = glm::vec3(0.05f, 0.05f, 0.05f);

    int enableSkybox = 0;
    glm::vec3 skyBottomColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 skyTopColor = glm::vec3(0.1f, 0.3f, 0.7f);
    int enableTextures = 1;

    // --- Debug / visualization (controlled by DebugUI) ---
    int probeSoftShadowSamples = 1; // 1=hard shadow (original), >1=area-light jitter per probe BVH shadow ray
    int debugMode = 0;              // 0=Final 1=Albedo 2=Normal 3=Depth 4=Emissive 5=IndirectOnly 6=Probes
    float exposure = 1.0f;          // tonemap exposure
    int tonemapMode = 0;            // 0=ACES Film  1=Reinhard  2=Linear
    int enableDirect = 1;
    int enableIndirect = 1;
    float kIndirectScale = 0.01f;   // GI irradiance -> color scale (slider in Scene Controls); halved after K0 fix (was 2x too small)
    float fogDensity = 0.04f;       // exponential fog decay coefficient
    bool fogBlendWithSky = true;    // true = fog samples sky gradient; false = no fog (RC passes)

    // --- Probe debug visualization (mode 6) ---
    int   probeVizLevel   = 0;      // which cascade level to visualize
    float probeVizRadius  = 0.09f;  // octahedron body half-extent (world units)
    float probeVizRayLen  = 0.40f;  // ray stub end distance from probe center
    float probeVizRayRad  = 0.012f; // ray stub capsule radius

    // --- Cascade configuration (public so DebugUI can live-tune it) ---
    CascadeConfig rcConfig;

    void recreateCascades();
    FrameStats lastStats() const { return lastFrameStats; }

    void run();
    void loadScene(
        const std::vector<GPUMaterial>& mats,
        const std::vector<GPUSphere>& sphs,
        const std::vector<GPUTriangle>& tris,
        const std::vector<GPULight>& lghts,
        const std::vector<GPUPlane>& plns,
        const std::vector<GPUQuad>& quds,
        const std::vector<GPUCube>& cbs,
        const std::vector<GPUBVHNode>& bvh
    );
    void loadTextures(const std::vector<std::string>& paths);

private:
    GLFWwindow* window = nullptr;
    VulkanContext ctx;
    Swapchain swapchain;
    CommandManager cmdManager;
    GBuffer gbuffer;
    Image hdrImage;
    Image ldrImage; // R8G8B8A8_UNORM, written by tonemap.comp, blitted to swapchain

    // --- Radiance Cascade state ---
    CascadeStorage rcStorage;   // populated by rcStorage.initialize(ctx, rcConfig) in initVulkan()
    std::vector<uint32_t> prevFrameSlots;   // actual probe counts read back after fence; 1-frame latency

    Buffer materialBuffer;
    Buffer sphereBuffer;
    Buffer triangleBuffer;
    Buffer lightBuffer;
    Buffer planeBuffer;
    Buffer quadBuffer;
    Buffer cubeBuffer;
    Buffer bvhBuffer;

    VkDescriptorSetLayout sceneDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout gbufDescSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> rcHashDescSets;                        // numCascades instances, one per cascade level
    std::vector<VkDescriptorSet> rcParentHashDescSets;                  // numCascades instances (used as "set 1" in alloc k>0 and merge)
    VkDescriptorSet tonemapDescSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout rcHashDescSetLayout = VK_NULL_HANDLE;         // 6 SSBOs: hash map + data + parent data
    VkDescriptorSetLayout rcParentHashDescSetLayout = VK_NULL_HANDLE;   // 4 SSBOs: parent keys, values, slotToKey, counter
    VkDescriptorSetLayout tonemapDescSetLayout = VK_NULL_HANDLE;        // 2 storage images: inHDR, outLDR
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet sceneDescSet = VK_NULL_HANDLE;
    VkDescriptorSet gbufDescSet = VK_NULL_HANDLE;

    LegacyPass legacyPass;

    VkPipelineLayout twoPassPipelineLayout = VK_NULL_HANDLE;
    VkPipeline primaryPassPipeline = VK_NULL_HANDLE;
    VkPipeline compositePassPipeline = VK_NULL_HANDLE;

    VkPipeline rcAllocPipeline = VK_NULL_HANDLE;
    VkPipeline rcTracePipeline = VK_NULL_HANDLE;
    VkPipeline rcMergePipeline = VK_NULL_HANDLE;
    VkPipeline rcSHPipeline = VK_NULL_HANDLE;
    VkPipeline rcGatherPipeline = VK_NULL_HANDLE;
    VkPipeline rcReflectionPipeline = VK_NULL_HANDLE;
    VkPipeline rcTransparentPipeline = VK_NULL_HANDLE;
    VkPipeline rcProbeDebugPipeline = VK_NULL_HANDLE;
    VkPipeline tonemapPipeline = VK_NULL_HANDLE;

    VkPipelineLayout rcAllocPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcTracePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcMergePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcSHPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcGatherPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcTransparentPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout rcProbeDebugPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout tonemapPipelineLayout = VK_NULL_HANDLE;

    VkSampler textureSampler = VK_NULL_HANDLE;
    std::vector<VkImage> textureImages;
    std::vector<VmaAllocation> textureImageAllocs;
    std::vector<VkImageView> textureImageViews;
    std::vector<std::string> pendingTexturePaths;

    std::vector<GPUMaterial> sceneMaterials;
    std::vector<GPUSphere> sceneSpheres;
    std::vector<GPUTriangle> sceneTriangles;
    std::vector<GPULight> sceneLights;
    std::vector<GPUPlane> scenePlanes;
    std::vector<GPUQuad> sceneQuads;
    std::vector<GPUCube> sceneCubes;
    std::vector<GPUBVHNode> sceneBVH;

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.3f, -1.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = 90.0f;
    float pitch = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // --- GPU timestamp queries (per-pass timing) ---
    VkQueryPool timestampPool = VK_NULL_HANDLE;
    float timestampPeriod = 0.0f;
    bool timestampsSupported = false;

    // --- Stats and debug ---
    FrameStats lastFrameStats{};
    DebugUI    debugUI;

    void initWindow();
    void initVulkan();
    void createSceneBuffers();
    void uploadSceneData();
    void createTextureSampler();
    void createTextureResources();
    void createSceneDescriptorSetLayout();
    void createGBufferDescriptorSetLayout();
    void createRCDescriptorSetLayouts();
    void createPipelines();
    void createRCPipelineLayouts();
    void createDescriptorPool();
    void createSceneDescriptorSet();
    void createGBufferDescriptorSet();
    void createRCDescriptorSets();
    void createTimestampQueryPool();
    void readbackTimestamps();
    void destroyTimestampQueryPool();
    void mainLoop();
    void processInput();
    void updateDynamicData();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanup();

    VkPipeline createComputePipelineFromSpv(const std::string& path, VkPipelineLayout layout);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void transitionImageLayout(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
    static std::vector<char> readFile(const std::string& filename);
    void emitComputeBarrier(VkCommandBuffer cmd);
    void emitTransferToComputeBarrier(VkCommandBuffer cmd);
};
