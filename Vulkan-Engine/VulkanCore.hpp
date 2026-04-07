#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <string>

#include "GPUData.hpp"
#include "ModelLoader.hpp"

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
    float pad1;
};

class VulkanCore {
public:
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
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    VkImage computeImage;
    VkDeviceMemory computeImageMemory;
    VkImageView computeImageView;

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
    void createLogicalDevice();
    void createCommandPool();
    void createSwapchain();
    void createSwapchainImageViews();
    void createComputeImage();
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

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
};