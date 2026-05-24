#pragma once

#include <vulkan/vulkan_core.h>

#include "core/VulkanContext.hpp"
#include "resources/Image.hpp"

class GBuffer {
public:
    // Position (xyz) + linear depth (w)
    Image position;    // VK_FORMAT_R32G32B32A32_SFLOAT

    // Normal (xyz) + roughness (w)
    Image normal;      // VK_FORMAT_R16G16B16A16_SFLOAT

    // Albedo (rgb) + metallic (a)
    Image albedo;      // VK_FORMAT_R8G8B8A8_UNORM

    // Emissive (rgb) + materialIndex (a)
    Image emissive;    // VK_FORMAT_R16G16B16A16_SFLOAT

    // Linear Z, separate for the cascade allocation pass
    Image linearDepth; // VK_FORMAT_R32_SFLOAT

    void create(VulkanContext& ctx, VkExtent2D size);
    void destroy(VulkanContext& ctx);
    void transitionForWrite(VkCommandBuffer cmd);
    void transitionForRead(VkCommandBuffer cmd);
};
