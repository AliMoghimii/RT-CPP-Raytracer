#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <cstdint>

class VulkanContext {
public:
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    uint32_t computeQueueFamily = 0;
    VmaAllocator allocator = VK_NULL_HANDLE;
    void initialize(GLFWwindow* window);
    void shutdown();

private:
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};
