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
#include "resources/Buffer.hpp"
#include "resources/Image.hpp"
#include "scene/GPUData.hpp"

struct CameraPushConstants {
    glm::vec4 camPos;
    glm::vec4 camForward;
    glm::vec4 camRight;
    glm::vec4 camUp;
    int  sphereCount;
    int  triangleCount;
    int  planeCount;
    int  quadCount;
    int  cubeCount;
    int  lightCount;
    int  bvhCount;
    int  maxDepth;
    int  shadowRays;
    int  primaryRaysPerPixel;
    float focalDistance;
    float lensRadius;
    glm::vec3 fogColor;
    int  enableFog;
    glm::vec3 skyBottomColor;
    int  enableSkybox;
    glm::vec3 skyTopColor;
    int  enableTextures;
};

class Renderer {
public:
    int   maxDepth            = 5;
    int   shadowRays          = 4;
    int   primaryRaysPerPixel = 1;
    float focalDistance       = 1.0f;
    float lensRadius          = 0.0f;

    int       enableFog      = 0;
    glm::vec3 fogColor       = glm::vec3(0.05f, 0.05f, 0.05f);

    int       enableSkybox   = 0;
    glm::vec3 skyBottomColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 skyTopColor    = glm::vec3(0.1f, 0.3f, 0.7f);

    int enableTextures = 1;

    void run();
    void loadScene(
        const std::vector<GPUMaterial>& mats,
        const std::vector<GPUSphere>&   sphs,
        const std::vector<GPUTriangle>& tris,
        const std::vector<GPULight>&    lghts,
        const std::vector<GPUPlane>&    plns,
        const std::vector<GPUQuad>&     quds,
        const std::vector<GPUCube>&     cbs,
        const std::vector<GPUBVHNode>&  bvh
    );
    void loadTextures(const std::vector<std::string>& paths);

private:
    GLFWwindow*    window = nullptr;
    VulkanContext  ctx;
    Swapchain      swapchain;
    CommandManager cmdManager;
    GBuffer        gbuffer;
    Image          hdrImage;

    Buffer materialBuffer;
    Buffer sphereBuffer;
    Buffer triangleBuffer;
    Buffer lightBuffer;
    Buffer planeBuffer;
    Buffer quadBuffer;
    Buffer cubeBuffer;
    Buffer bvhBuffer;

    VkDescriptorSetLayout sceneDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout gbufDescSetLayout  = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool     = VK_NULL_HANDLE;
    VkDescriptorSet       sceneDescSet       = VK_NULL_HANDLE;
    VkDescriptorSet       gbufDescSet        = VK_NULL_HANDLE;

    VkPipelineLayout twoPassPipelineLayout = VK_NULL_HANDLE;
    VkPipeline       primaryPassPipeline   = VK_NULL_HANDLE;
    VkPipeline       compositePassPipeline = VK_NULL_HANDLE;

    VkSampler                  textureSampler = VK_NULL_HANDLE;
    std::vector<VkImage>       textureImages;
    std::vector<VmaAllocation> textureImageAllocs;
    std::vector<VkImageView>   textureImageViews;
    std::vector<std::string>   pendingTexturePaths;

    std::vector<GPUMaterial> sceneMaterials;
    std::vector<GPUSphere>   sceneSpheres;
    std::vector<GPUTriangle> sceneTriangles;
    std::vector<GPULight>    sceneLights;
    std::vector<GPUPlane>    scenePlanes;
    std::vector<GPUQuad>     sceneQuads;
    std::vector<GPUCube>     sceneCubes;
    std::vector<GPUBVHNode>  sceneBVH;

    glm::vec3 cameraPos   = glm::vec3(0.0f, 0.3f, -1.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw       = 90.0f;
    float pitch     = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    void initWindow();
    void initVulkan();
    void createSceneBuffers();
    void uploadSceneData();
    void createTextureSampler();
    void createTextureResources();
    void createSceneDescriptorSetLayout();
    void createGBufferDescriptorSetLayout();
    void createPipelines();
    void createDescriptorPool();
    void createSceneDescriptorSet();
    void createGBufferDescriptorSet();
    void mainLoop();
    void processInput();
    void updateDynamicData();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanup();

    VkPipeline     createComputePipelineFromSpv(const std::string& path, VkPipelineLayout layout);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void           transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                         VkImageLayout oldLayout, VkImageLayout newLayout);
    static std::vector<char> readFile(const std::string& filename);
};
