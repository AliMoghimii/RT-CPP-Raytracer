#include "gi/CascadeHashMap.hpp"

void CascadeHashMap::create(VulkanContext& ctx, uint32_t tableSize, uint32_t maxActiveSlots, int octRes)
{
	this->tableSize = tableSize;
	this->maxActiveSlots = maxActiveSlots;
	this->octRes = octRes;

	constexpr VkBufferUsageFlags flags =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	hashKeys = Buffer(ctx.allocator, sizeof(uint32_t) * tableSize, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	hashValues = Buffer(ctx.allocator, sizeof(uint32_t) * tableSize, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	slotToKey = Buffer(ctx.allocator, sizeof(uint32_t) * maxActiveSlots, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	// GPU_TO_CPU: persistently mapped so CPU can read the previous frame's actual slot count
	// after vkWaitForFences, enabling frame-latency indirect dispatch sizing.
	slotCounter = Buffer(ctx.allocator, sizeof(uint32_t), flags, VMA_MEMORY_USAGE_GPU_TO_CPU);
	cascadeData = Buffer(ctx.allocator, dataBufferSize(), flags, VMA_MEMORY_USAGE_GPU_ONLY);
	shCoeffs    = Buffer(ctx.allocator, shBufferSize(),  flags, VMA_MEMORY_USAGE_GPU_ONLY);
}

void CascadeHashMap::destroy()
{
	hashKeys = Buffer();
	hashValues = Buffer();
	slotToKey = Buffer();
	slotCounter = Buffer();
	cascadeData = Buffer();
	shCoeffs    = Buffer();
}
