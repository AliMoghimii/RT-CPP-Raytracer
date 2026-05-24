#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class VulkanContext;

class CommandManager {
public:
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer buffer = VK_NULL_HANDLE;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    VkFence inFlightFence = VK_NULL_HANDLE;

    void create(VulkanContext& ctx, uint32_t swapchainImageCount);
    void destroy(VulkanContext& ctx);

    VkCommandBuffer beginOneTime(VulkanContext& ctx);
    void submitOneTime(VulkanContext& ctx, VkCommandBuffer cmd);
};
