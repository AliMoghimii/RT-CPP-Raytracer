#include "core/Swapchain.hpp"
#include "core/VulkanContext.hpp"
#include <stdexcept>

void Swapchain::create(VulkanContext& ctx, VkSurfaceKHR surf, VkExtent2D size) {
    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surf;
    ci.minImageCount = 3;
    ci.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent = size;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(ctx.device, &ci, nullptr, &handle) != VK_SUCCESS)
        throw std::runtime_error("Swapchain: creation failed.");

    format = ci.imageFormat;
    extent = size;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(ctx.device, handle, &count, nullptr);
    images.resize(count);
    vkGetSwapchainImagesKHR(ctx.device, handle, &count, images.data());

    imageViews.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount = 1;

        if (vkCreateImageView(ctx.device, &vi, nullptr, &imageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Swapchain: image view creation failed.");
    }
}

void Swapchain::destroy(VulkanContext& ctx) {
    for (auto v : imageViews)
        vkDestroyImageView(ctx.device, v, nullptr);
    imageViews.clear();
    images.clear();
    vkDestroySwapchainKHR(ctx.device, handle, nullptr);
    handle = VK_NULL_HANDLE;
}
