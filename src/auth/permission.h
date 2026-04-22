#pragma once

#include <cstdint>

enum class permission : uint64_t
{
	none       = 0,
	admin      = 1ULL << 0,
	post_write = 1ULL << 1,
};

inline bool has_permission(uint64_t mask, permission p)
{
	return (mask & static_cast<uint64_t>(p)) != 0;
}

inline uint64_t grant(uint64_t mask, permission p)
{
	return mask | static_cast<uint64_t>(p);
}
