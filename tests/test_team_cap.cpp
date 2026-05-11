#include <catch2/catch_test_macros.hpp>

#include "kanban/team_cap.hpp"

TEST_CASE("team_cap - empty flags grants nothing")
{
	const team_cap::flags none{};
	CHECK(!none.has_any(team_cap::view_board));
	CHECK(!none.has_any(team_cap::edit_task_fields));
	CHECK(!none.has_any(team_cap::comment));
	CHECK(none.empty());
}

TEST_CASE("team_cap - org_viewer_caps has only view_board")
{
	CHECK(team_cap::org_viewer_caps.has_any(team_cap::view_board));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::edit_task_fields));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::self_assign));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::comment));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::view_archived));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::create_task));
}

TEST_CASE("team_cap - team_member_caps has edit and comment but not lead caps")
{
	CHECK(team_cap::team_member_caps.has_any(team_cap::view_board));
	CHECK(team_cap::team_member_caps.has_any(team_cap::edit_task_fields));
	CHECK(team_cap::team_member_caps.has_any(team_cap::self_assign));
	CHECK(team_cap::team_member_caps.has_any(team_cap::comment));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::view_archived));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::reassign_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::create_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::archive_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::manage_team));
}

TEST_CASE("team_cap - team_lead_caps is a superset of team_member_caps")
{
	CHECK(team_cap::team_lead_caps.has_all(team_cap::team_member_caps));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::view_archived));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::reassign_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::create_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::archive_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::manage_team));
}

TEST_CASE("team_cap - org_lead_caps equals team_lead_caps")
{
	CHECK(team_cap::org_lead_caps == team_cap::team_lead_caps);
}

TEST_CASE("team_cap - admin_caps has all defined capabilities")
{
	CHECK(team_cap::admin_caps.has_any(team_cap::view_board));
	CHECK(team_cap::admin_caps.has_any(team_cap::view_archived));
	CHECK(team_cap::admin_caps.has_any(team_cap::edit_task_fields));
	CHECK(team_cap::admin_caps.has_any(team_cap::self_assign));
	CHECK(team_cap::admin_caps.has_any(team_cap::reassign_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::create_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::archive_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::manage_team));
	CHECK(team_cap::admin_caps.has_any(team_cap::comment));
}
