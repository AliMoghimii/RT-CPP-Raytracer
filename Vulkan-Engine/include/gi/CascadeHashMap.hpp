#pragma once

#include "resources/Buffer.hpp"
#include "core/VulkanContext.hpp"

struct CascadeHashMap
{
	Buffer hashKeys;	        // uint[tableSize] - packed cell keys, 0xFFFFFFFF = empty
	Buffer hashValues;	        // uint[tableSize] - slot index for each key
	Buffer slotToKey;	        // uint[maxActiveSlots] - reverse map: slot -> key
	Buffer slotCounter;	        // uint[1] - atomic counter for active slots
	Buffer cascadeData;	        // uvec2[maxActiveSlots x octRes^2]
	Buffer shCoeffs;	        // vec4[maxActiveSlots x 4] - L0+L1 SH irradiance coefficients

	uint32_t tableSize = 0;
	uint32_t maxActiveSlots = 0;
	int octRes = 0;

	void create(VulkanContext& ctx, uint32_t tableSize, uint32_t maxActiveSlots, int octRes);
	void destroy();

	size_t dataBufferSize() const
	{
		// uvec2 = 2 x utin32 = 8 bytes per direction per slot
		return size_t(maxActiveSlots) * size_t(octRes) * size_t(octRes) * sizeof(uint64_t);
	}

	size_t shBufferSize() const
	{
		// 4 x vec4 (16 bytes) per slot: L0+L1 SH coefficients (c0, cx, cy, cz)
		return size_t(maxActiveSlots) * 4 * sizeof(float) * 4;
	}
};

