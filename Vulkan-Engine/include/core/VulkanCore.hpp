#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <string>

#include "scene/GPUData.hpp"
#include "scene/ModelLoader.hpp"
#include "scene/ImageLoader.hpp"

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

class VulkanCore {
public:
    int maxDepth = 5;
    int shadowRays = 4;
    int primaryRaysPerPixel = 1;
    float focalDistance = 1.0f;
    float lensRadius = 0.0f;

    int enableFog = 0;
    glm::vec3 fogColor = glm::vec3(0.05f, 0.05f, 0.05f);

    int enableSkybox = 0;
    glm::vec3 skyBottomColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 skyTopColor = glm::vec3(0.1f, 0.3f, 0.7f);

    int enableTextures = 1;

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
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue computeQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    VkPipeline computePipeline;
    VkPipeline legacyComputePipeline;
    VkPipelineLayout pipelineLayout;

    // Two-pass G-buffer pipelines
    VkPipeline primaryPassPipeline;
    VkPipeline compositePassPipeline;
    VkPipelineLayout twoPassPipelineLayout;

    bool useLegacyRenderer = false;
    bool useTwoPassRenderer = true;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // G-buffer descriptor set (set = 1 for pass images)
    VkDescriptorSetLayout gbufDescSetLayout;
    VkDescriptorSet gbufDescSet;

    VkImage computeImage;
    VkDeviceMemory computeImageMemory;
    VkImageView computeImageView;

    // G-buffer images (written by primary pass, read by composite pass)
    VkImage gbufPosition,  gbufNormal,  gbufAlbedo,  gbufEmissive,  gbufLinearDepth;
    VkImageView gbufPositionV, gbufNormalV, gbufAlbedoV, gbufEmissiveV, gbufLinearDepthV;
    VkDeviceMemory gbufPositionM, gbufNormalM, gbufAlbedoM, gbufEmissiveM, gbufLinearDepthM;

    // HDR output image (written by composite pass, blitted to swapchain)
    VkImage hdrImage;
    VkImageView hdrImageView;
    VkDeviceMemory hdrMemory;

    VkBuffer materialBuffer;
    VkDeviceMemory materialMemory;

    VkBuffer sphereBuffer;
    VkDeviceMemory sphereMemory;

    VkBuffer triangleBuffer;
    VkDeviceMemory triangleMemory;

    VkBuffer lightBuffer;
    VkDeviceMemory lightMemory;

    VkBuffer planeBuffer;
    VkDeviceMemory planeMemory;

    VkBuffer quadBuffer;
    VkDeviceMemory quadMemory;

    VkBuffer cubeBuffer;
    VkDeviceMemory cubeMemory;

    VkBuffer bvhBuffer;
    VkDeviceMemory bvhMemory;

    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImageMemories;
    std::vector<VkImageView> textureImageViews;
    VkSampler textureSampler;
    std::vector<std::string> pendingTexturePaths;

    std::vector<GPUMaterial> sceneMaterials;
    std::vector<GPUSphere> sceneSpheres;
    std::vector<GPUTriangle> sceneTriangles;
    std::vector<GPULight> sceneLights;
    std::vector<GPUPlane> scenePlanes;
    std::vector<GPUQuad> sceneQuads;
    std::vector<GPUCube> sceneCubes;
    std::vector<GPUBVHNode> sceneBVH;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.3f, -1.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = 90.0f;
    float pitch = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    void initWindow();
    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    int ratePhysicalDevice(VkPhysicalDevice dev);
    void createLogicalDevice();
    void createCommandPool();
    void createSwapchain();
    void createSwapchainImageViews();
    void createComputeImage();

    void createTextureSampler();
    void createTextureResources();

    void createSceneBuffers();
    void createDescriptorSetLayout();
    void createComputePipeline();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void mainLoop();
    void processInput();
    void updateDynamicData();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanup();

    void createGBufferImages();
    void createTwoPassPipelines();
    void createGBufferDescriptorSet();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createStorageImage(VkFormat format, VkImage& image, VkImageView& view, VkDeviceMemory& memory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkPipeline createComputePipelineFromSpv(const std::string& path, VkPipelineLayout layout);
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
};