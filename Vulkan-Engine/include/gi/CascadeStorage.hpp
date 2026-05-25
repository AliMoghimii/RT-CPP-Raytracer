#pragma once

#include "gi/CascadeConfig.hpp"
#include "gi/CascadeHashMap.hpp"

#include <vector>

class CascadeStorage
{

public:
	CascadeConfig config;
	std::vector<CascadeHashMap> levels;	// one per cascade level
	std::vector<uint32_t> hashTableSize;
	std::vector<uint32_t> maxActiveSlots;
	

	void initialize(VulkanContext& ctx, const CascadeConfig& cfg);
	void destroy();
};