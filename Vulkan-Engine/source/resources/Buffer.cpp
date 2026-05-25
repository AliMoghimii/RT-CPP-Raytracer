#include "resources/Buffer.hpp"
#include <stdexcept>
#include <cstring>

Buffer::Buffer(VmaAllocator allocator, size_t bufSize, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
    : size(bufSize), alloc(allocator) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bufSize;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = memUsage;
    if (memUsage == VMA_MEMORY_USAGE_CPU_TO_GPU || memUsage == VMA_MEMORY_USAGE_CPU_ONLY
        || memUsage == VMA_MEMORY_USAGE_GPU_TO_CPU)
        aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(alloc, &bci, &aci, &handle, &allocation, &info) != VK_SUCCESS)
        throw std::runtime_error("Buffer: vmaCreateBuffer failed.");
    if (aci.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
        mapped = info.pMappedData;
}

Buffer::~Buffer() {
    if (handle != VK_NULL_HANDLE)
        vmaDestroyBuffer(alloc, handle, allocation);
}

void Buffer::uploadStaged(VkCommandBuffer cmd, const void* data, size_t uploadSize, Buffer& staging) {
    std::memcpy(staging.mapped, data, uploadSize);

    VkBufferCopy copy{};
    copy.size = uploadSize;
    vkCmdCopyBuffer(cmd, staging.handle, handle, 1, &copy);
}

Buffer::Buffer(Buffer&& other) noexcept
    : handle(other.handle), allocation(other.allocation), mapped(other.mapped),
      size(other.size), alloc(other.alloc) {
    other.handle = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.mapped = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (handle != VK_NULL_HANDLE)
            vmaDestroyBuffer(alloc, handle, allocation);
        handle = other.handle;
        allocation = other.allocation;
        mapped = other.mapped;
        size = other.size;
        alloc = other.alloc;
        other.handle = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }
    return *this;
}
