#pragma once

#include <alt/flags.hpp>
#include <cstdint>

namespace permission
{
	enum class bit : uint64_t {};
	using flags = alt::flags<bit>;

	inline constexpr flags none         = {};
	inline constexpr flags admin        = flags::from_value(1ULL << 0);
	inline constexpr flags post_write   = flags::from_value(1ULL << 1);
	inline constexpr flags gantt_write  = flags::from_value(1ULL << 2);
	inline constexpr flags manage_users = flags::from_value(1ULL << 3);
}
