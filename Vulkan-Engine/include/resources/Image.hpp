#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>
#include <cstdint>

class Image {
public:
    VkImage handle  = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {};

    Image() = default;
    Image(VmaAllocator alloc, uint32_t width, uint32_t height, VkFormat fmt,
          VkImageUsageFlags usage, VmaMemoryUsage memUsage);

    void destroy(VmaAllocator alloc);

    Image(Image&&) noexcept;
    Image& operator=(Image&&) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
};
