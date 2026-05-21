#include <catch2/catch_test_macros.hpp>

#include "paths.hpp"

TEST_CASE("take_segment - reads up to slash")
{
	std::string_view sv = "view/42/kanban";
	CHECK(paths::take_segment(sv) == "view");
	CHECK(sv == "42/kanban");
}

TEST_CASE("take_segment - last segment consumes all")
{
	std::string_view sv = "kanban";
	CHECK(paths::take_segment(sv) == "kanban");
	CHECK(sv.empty());
}

TEST_CASE("take_segment - empty input returns empty")
{
	std::string_view sv = "";
	CHECK(paths::take_segment(sv).empty());
	CHECK(sv.empty());
}

TEST_CASE("take_segment - strips leading slash first")
{
	std::string_view sv = "/view/42";
	CHECK(paths::take_segment(sv) == "view");
	CHECK(sv == "42");
}

TEST_CASE("take_id - valid numeric segment")
{
	std::string_view sv = "42/kanban";
	const auto       id = paths::take_id(sv);
	REQUIRE(id.has_value());
	CHECK(*id == 42);
	CHECK(sv == "kanban");
}

TEST_CASE("take_id - non-numeric returns nullopt")
{
	std::string_view sv = "abc";
	CHECK_FALSE(paths::take_id(sv).has_value());
}

TEST_CASE("take_id - empty returns nullopt")
{
	std::string_view sv = "";
	CHECK_FALSE(paths::take_id(sv).has_value());
}

TEST_CASE("named path builders")
{
	CHECK(paths::blog_list() == "/blog/list");
	CHECK(paths::blog_view("hello") == "/blog/view/hello");
	CHECK(paths::blog_edit("hello") == "/blog/edit/hello");
	CHECK(paths::blog_new() == "/blog/new");
	CHECK(paths::link_list() == "/link/list");
	CHECK(paths::link_edit(5) == "/link/edit/5");
	CHECK(paths::link_new() == "/link/new");
	CHECK(paths::account_list() == "/admin/account/list");
	CHECK(paths::account_new() == "/admin/account/new");
	CHECK(paths::account_edit("alice") == "/admin/account/edit/alice");
	CHECK(paths::admin_org_list() == "/admin/org/list");
	CHECK(paths::org_view(3) == "/org/view/3");
	CHECK(paths::org_board(3) == "/org/view/3/board");
	CHECK(paths::org_edit(3) == "/org/edit/3");
	CHECK(paths::org_types(3) == "/org/view/3/types");
	CHECK(paths::team_kanban(7) == "/team/view/7/kanban");
	CHECK(paths::team_gantt(7) == "/team/view/7/gantt");
	CHECK(paths::team_task_new(7) == "/team/view/7/task/new");
	CHECK(paths::team_archive(7) == "/team/view/7/archive");
	CHECK(paths::team_edit_members(7) == "/team/edit/7/members");
	CHECK(paths::team_edit_settings(7) == "/team/edit/7/settings");
	CHECK(paths::task_edit(99) == "/task/edit/99");
}
