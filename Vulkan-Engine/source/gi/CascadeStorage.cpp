#include "gi/CascadeStorage.hpp"

//Probe budget per level. tableSize = maxSlots * 2 (50% load factor keeps collision rare).
static constexpr uint32_t kMaxSlots[] = { 16384, 8192, 4096, 512, 64 };

void CascadeStorage::initialize(VulkanContext& ctx, const CascadeConfig& cfg)
{
	config = cfg;
	int N = cfg.numCascades;

	levels.resize(N);
	hashTableSize.resize(N);
	maxActiveSlots.resize(N);

	for (int i = 0; i < N; i++)
	{
		uint32_t maxSlots = (i < 5) ? kMaxSlots[i] : 32u;
		uint32_t tableSize = maxSlots * 2;	// 50% load factor
		maxActiveSlots[i] = maxSlots;
		hashTableSize[i] = tableSize;
		levels[i].create(ctx, tableSize, maxSlots, cfg.octRes(i));
	}

	// Print memory budget
	size_t totalBytes = 0;
	printf("Radiance Cascade memory budget:\n");
	for (int i = 0; i < N; i++)
	{
		auto& level = levels[i];
		size_t dataB = level.dataBufferSize();
		size_t hashB = size_t(level.tableSize) * 2 * sizeof(uint32_t);	// key + value
		size_t slotB = size_t(level.maxActiveSlots) * sizeof(uint32_t);	// slot keys
		size_t levelTotal = dataB + hashB + slotB + sizeof(uint32_t);	// + counter
		totalBytes += levelTotal;
		printf("  Cascade %d: data=%.1f MB  hash+slot=%.0f KB\n", i, dataB / 1e6f, (hashB + slotB) / 1e3f);
	}
	printf("  Grand total: %.1f MB\n\n", totalBytes / 1e6f);
}

void CascadeStorage::destroy()
{
	for (auto& level : levels)
		level.destroy();
	levels.clear();
}
