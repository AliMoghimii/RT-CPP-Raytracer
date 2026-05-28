#include "gi/CascadeHashMap.hpp"

void CascadeHashMap::create(VulkanContext& ctx, uint32_t tableSize, uint32_t maxActiveSlots, int octRes)
{
	this->tableSize = tableSize;
	this->maxActiveSlots = maxActiveSlots;
	this->octRes = octRes;

	// TRANSFER_DST_BIT is required for vkCmdFillBuffer (used to clear keys + counter each frame).
	// TRANSFER_SRC_BIT is required for vkCmdCopyBuffer (used for debug readback of slotCounter).
	// GPU_ONLY: these buffers are never read by the CPU, only by compute shaders.
	constexpr VkBufferUsageFlags flags =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	hashKeys = Buffer(ctx.allocator, sizeof(uint32_t) * tableSize, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	hashValues = Buffer(ctx.allocator, sizeof(uint32_t) * tableSize, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	slotToKey = Buffer(ctx.allocator, sizeof(uint32_t) * maxActiveSlots, flags, VMA_MEMORY_USAGE_GPU_ONLY);
	slotCounter = Buffer(ctx.allocator, sizeof(uint32_t), flags, VMA_MEMORY_USAGE_GPU_ONLY);
	cascadeData = Buffer(ctx.allocator, dataBufferSize(), flags, VMA_MEMORY_USAGE_GPU_ONLY);
}

void CascadeHashMap::destroy()
{
	hashKeys = Buffer();
	hashValues = Buffer();
	slotToKey = Buffer();
	slotCounter = Buffer();
	cascadeData = Buffer();
}
