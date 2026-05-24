#include "passes/GBuffer.hpp"
#include <array>

void GBuffer::create(VulkanContext& ctx, VkExtent2D size) {
    auto make = [&](VkFormat fmt) -> Image {
        return Image(ctx.allocator, size.width, size.height, fmt,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    };

    position = make(VK_FORMAT_R32G32B32A32_SFLOAT);
    normal = make(VK_FORMAT_R16G16B16A16_SFLOAT);
    albedo = make(VK_FORMAT_R8G8B8A8_UNORM);
    emissive = make(VK_FORMAT_R16G16B16A16_SFLOAT);
    linearDepth = make(VK_FORMAT_R32_SFLOAT);
}

void GBuffer::destroy(VulkanContext& ctx) {
    position.destroy(ctx.allocator);
    normal.destroy(ctx.allocator);
    albedo.destroy(ctx.allocator);
    emissive.destroy(ctx.allocator);
    linearDepth.destroy(ctx.allocator);
}

void GBuffer::transitionForWrite(VkCommandBuffer cmd) {
    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    std::array<VkImageMemoryBarrier2, 5> barriers{};
    VkImage images[] = {
        position.handle, normal.handle, albedo.handle,
        emissive.handle, linearDepth.handle
    };

    for (int i = 0; i < 5; ++i) {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barriers[i].srcAccessMask = VK_ACCESS_2_NONE;
        barriers[i].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].image = images[i];
        barriers[i].subresourceRange = range;
    }

    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    di.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(cmd, &di);
}

void GBuffer::transitionForRead(VkCommandBuffer cmd) {
    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    std::array<VkImageMemoryBarrier2, 5> barriers{};
    VkImage images[] = {
        position.handle, normal.handle, albedo.handle,
        emissive.handle, linearDepth.handle
    };

    for (int i = 0; i < 5; ++i) {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[i].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barriers[i].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        barriers[i].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        barriers[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].image = images[i];
        barriers[i].subresourceRange = range;
    }

    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    di.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(cmd, &di);
}
