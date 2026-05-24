#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class VulkanContext;

class Swapchain {
public:
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {};

    void create(VulkanContext& ctx, VkSurfaceKHR surface, VkExtent2D size);
    void destroy(VulkanContext& ctx);
};
