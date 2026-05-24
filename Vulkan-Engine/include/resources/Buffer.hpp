#pragma once
#include <vk_mem_alloc.h>
#include <cstddef>

class Buffer {
public:
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void* mapped = nullptr;
    size_t size = 0;

    Buffer() = default;
    Buffer(VmaAllocator allocator, size_t bufSize, VkBufferUsageFlags usage, VmaMemoryUsage memUsage);
    ~Buffer();

    void uploadStaged(VkCommandBuffer cmd, const void* data, size_t uploadSize, Buffer& staging);

    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

private:
    VmaAllocator alloc = VK_NULL_HANDLE;
};
