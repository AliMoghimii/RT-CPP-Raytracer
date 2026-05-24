#include "resources/Image.hpp"
#include <stdexcept>
#include <cassert>

Image::Image(VmaAllocator alloc, uint32_t width, uint32_t height, VkFormat fmt,
             VkImageUsageFlags usage, VmaMemoryUsage memUsage)
    : format(fmt), extent({ width, height }) {
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = fmt;
    ici.extent = { width, height, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo aci{};
    aci.usage = memUsage;

    if (vmaCreateImage(alloc, &ici, &aci, &handle, &allocation, nullptr) != VK_SUCCESS)
        throw std::runtime_error("Image: vmaCreateImage failed.");

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D16_UNORM ||
        fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

    VmaAllocatorInfo allocInfo{};
    vmaGetAllocatorInfo(alloc, &allocInfo);

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = handle;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = aspect;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(allocInfo.device, &vci, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Image: image view creation failed.");
}

void Image::destroy(VmaAllocator alloc) {
    if (handle == VK_NULL_HANDLE) return;

    VmaAllocatorInfo allocInfo{};
    vmaGetAllocatorInfo(alloc, &allocInfo);

    vkDestroyImageView(allocInfo.device, view, nullptr);
    vmaDestroyImage(alloc, handle, allocation);
    handle = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;
}

Image::Image(Image&& other) noexcept
    : handle(other.handle), view(other.view), allocation(other.allocation),
      format(other.format), extent(other.extent) {
    other.handle = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        // Cannot auto-destroy: no allocator stored. Caller must call destroy() first.
        assert(handle == VK_NULL_HANDLE && "Image move-assigned over a live image — call destroy() first.");
        handle = other.handle;
        view = other.view;
        allocation = other.allocation;
        format = other.format;
        extent = other.extent;
        other.handle = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
    }
    return *this;
}
