#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <string>

#include "GPUData.hpp"
#include "ModelLoader.hpp"

class VulkanCore {
public:
    void run();
    void loadScene(const std::vector<GPUMaterial>& mats,
        const std::vector<GPUSphere>& sphs,
        const std::vector<GPUTriangle>& tris);

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

    std::vector<GPUMaterial> sceneMaterials;
    std::vector<GPUSphere> sceneSpheres;
    std::vector<GPUTriangle> sceneTriangles;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

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
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanup();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
};