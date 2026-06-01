#pragma once

#include <glm/glm.hpp>
#include <algorithm>

struct CascadeConfig
{
	int numCascades = 5;
	int branchingFactor = 2;	// a in the interval formula -> d_end(k) = spacing0 x 2^(a � k) = 0.5 x 4^k

	glm::vec3 worldOrigin = glm::vec3(-16.0f, -4.0f, -16.0f);
	glm::ivec3 gridSize0 = glm::ivec3(64, 60, 92);	// covers full room: X(-16..16) Y(-4..26) Z(-16..30)
	float spacing0 = 0.5f;
	int octRes0 = 4;

	glm::ivec3 gridSize(int level) const
	{
		return glm::ivec3(
			std::max(1, gridSize0.x >> level),
			std::max(1, gridSize0.y >> level),
			std::max(1, gridSize0.z >> level)
		);
	}
	float spacing(int level) const { return spacing0 * float(1 << level); }
	int octRes(int level) const { return octRes0 << level; }
	float intervalStart(int level) const
	{
		return (level == 0) ? 0.0f : intervalEnd(level - 1);
	}
	float intervalEnd(int level) const
	{
		return spacing0 * float(1 << (branchingFactor * level));
	}
};