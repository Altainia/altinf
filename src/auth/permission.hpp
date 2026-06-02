#pragma once

#include <alt/flags.hpp>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

namespace permission
{
	enum class bit : uint64_t
	{
	};
	using flags = alt::flags<bit>;

	inline constexpr flags none              = {};
	inline constexpr flags admin             = flags::from_value(1ULL << 0);
	inline constexpr flags post_write        = flags::from_value(1ULL << 1);
	inline constexpr flags org_create        = flags::from_value(1ULL << 2);
	inline constexpr flags manage_users      = flags::from_value(1ULL << 3);
	inline constexpr flags api_token         = flags::from_value(1ULL << 4);
	inline constexpr flags view_user_history = flags::from_value(1ULL << 5);

	// Human-readable, comma-separated list of the permissions set in `perms`, or
	// "None". Shared by the admin UI and the audit trail.
	inline std::string label(flags perms)
	{
		std::vector<std::string> parts;
		if(perms.has_any(admin))
		{
			parts.emplace_back("Admin");
		}
		if(perms.has_any(manage_users))
		{
			parts.emplace_back("Manage Users");
		}
		if(perms.has_any(post_write))
		{
			parts.emplace_back("Write Posts");
		}
		if(perms.has_any(org_create))
		{
			parts.emplace_back("Create Orgs");
		}
		if(perms.has_any(api_token))
		{
			parts.emplace_back("API Tokens");
		}
		if(perms.has_any(view_user_history))
		{
			parts.emplace_back("View History");
		}
		if(parts.empty())
		{
			return "None";
		}
		return parts | std::views::join_with(std::string(", ")) | std::ranges::to<std::string>();
	}
}