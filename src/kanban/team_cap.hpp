#pragma once

#include <alt/flags.hpp>
#include <cstdint>

namespace team_cap
{
	enum class bit : uint32_t {};
	using flags = alt::flags<bit>;

	inline constexpr flags view_board       = flags::from_value(1u << 0);
	inline constexpr flags view_archived    = flags::from_value(1u << 1);
	inline constexpr flags edit_task_fields = flags::from_value(1u << 2);
	inline constexpr flags self_assign      = flags::from_value(1u << 3);
	inline constexpr flags reassign_task    = flags::from_value(1u << 4);
	inline constexpr flags create_task      = flags::from_value(1u << 5);
	inline constexpr flags archive_task     = flags::from_value(1u << 6);
	inline constexpr flags manage_team      = flags::from_value(1u << 7);
	inline constexpr flags comment          = flags::from_value(1u << 8);

	inline constexpr flags org_viewer_caps  = view_board;
	inline constexpr flags team_member_caps = view_board | edit_task_fields | self_assign | comment;
	inline constexpr flags team_lead_caps   = team_member_caps | view_archived | reassign_task
	                                        | create_task | archive_task | manage_team;
	inline constexpr flags org_lead_caps    = team_lead_caps;
	inline constexpr flags admin_caps       = ~flags{};
}
