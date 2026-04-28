#include <Wt/WDate.h>

#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"
#include "kanban/kanban_db.hpp"

static kanban_task_entry make_task(long long          team_id,
                                   const std::string& title,
                                   const std::string& status = "todo",
                                   int                sort   = 0)
{
	kanban_task_entry e;
	e.team_id    = team_id;
	e.title      = title;
	e.status     = status;
	e.start_date = Wt::WDate{2024, 1, 1};
	e.end_date   = Wt::WDate{2024, 1, 31};
	e.color      = "#7aa2d4";
	e.sort_order = sort;
	return e;
}

// ---- teams ----

TEST_CASE("kanban_db - default team created on construction")
{
	kanban_db db{":memory:"};
	auto      teams = db.all_teams();
	REQUIRE(teams.size() == 1);
	CHECK(teams[0].name == "Team");
	CHECK(teams[0].id > 0);
}

TEST_CASE("kanban_db - rename team")
{
	kanban_db  db{":memory:"};
	const auto t = db.all_teams()[0];
	db.rename_team(t.id, "Engineering");
	CHECK(db.find_team(t.id)->name == "Engineering");
}

TEST_CASE("kanban_db - find_team missing returns nullopt")
{
	kanban_db db{":memory:"};
	CHECK(!db.find_team(9999).has_value());
}

// ---- members ----

TEST_CASE("kanban_db - add and list members")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	auto members = db.members_for_team(tid);
	REQUIRE(members.size() == 2);
}

TEST_CASE("kanban_db - add_member is idempotent")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	db.add_member(tid, "alice");
	CHECK(db.members_for_team(tid).size() == 1);
}

TEST_CASE("kanban_db - remove member")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	db.remove_member(tid, "alice");
	auto members = db.members_for_team(tid);
	REQUIRE(members.size() == 1);
	CHECK(members[0] == "bob");
}

TEST_CASE("kanban_db - is_member")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	CHECK(db.is_member(tid, "alice"));
	CHECK(!db.is_member(tid, "stranger"));
}

// ---- tasks ----

TEST_CASE("kanban_db - add task and retrieve for team")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_task(make_task(tid, "Alpha", "todo", 1));
	db.add_task(make_task(tid, "Beta", "todo", 0));
	auto tasks = db.tasks_for_team(tid);
	REQUIRE(tasks.size() == 2);
	CHECK(tasks[0].title == "Beta"); // sort_order 0 first
	CHECK(tasks[1].title == "Alpha");
}

TEST_CASE("kanban_db - find_task")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	long long       id  = db.add_task(make_task(tid, "My Task"));
	auto            opt = db.find_task(id);
	REQUIRE(opt.has_value());
	CHECK(opt->title == "My Task");
}

TEST_CASE("kanban_db - find_task missing returns nullopt")
{
	kanban_db db{":memory:"};
	CHECK(!db.find_task(9999).has_value());
}

TEST_CASE("kanban_db - update task")
{
	kanban_db       db{":memory:"};
	const long long tid  = db.all_teams()[0].id;
	long long       id   = db.add_task(make_task(tid, "Original"));
	auto            task = *db.find_task(id);
	task.title           = "Updated";
	task.color           = "#ff0000";
	db.update_task(task);
	auto updated = db.find_task(id);
	CHECK(updated->title == "Updated");
	CHECK(updated->color == "#ff0000");
}

TEST_CASE("kanban_db - update_task_status")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	long long       id  = db.add_task(make_task(tid, "T", "todo"));
	db.update_task_status(id, "done", 5);
	auto opt = db.find_task(id);
	REQUIRE(opt.has_value());
	CHECK(opt->status == "done");
	CHECK(opt->sort_order == 5);
}

TEST_CASE("kanban_db - delete task")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	long long       id  = db.add_task(make_task(tid, "To Delete"));
	db.add_task(make_task(tid, "To Keep"));
	db.delete_task(id);
	auto tasks = db.tasks_for_team(tid);
	REQUIRE(tasks.size() == 1);
	CHECK(tasks[0].title == "To Keep");
}

TEST_CASE("kanban_db - tasks_for_team empty when no tasks")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	CHECK(db.tasks_for_team(tid).empty());
}

// ---- permissions ----

TEST_CASE("kanban_db - can_view_board: admin bypasses membership")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	CHECK(db.can_view_board(tid, "anyone", permission::admin));
}

TEST_CASE("kanban_db - can_view_board: member can view, non-member cannot")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	CHECK(db.can_view_board(tid, "alice", permission::none));
	CHECK(!db.can_view_board(tid, "stranger", permission::none));
}

TEST_CASE("kanban_db - can_edit_board: requires gantt_write + membership")
{
	kanban_db       db{":memory:"};
	const long long tid = db.all_teams()[0].id;
	db.add_member(tid, "alice");
	CHECK(db.can_edit_board(tid, "alice", permission::gantt_write));
	CHECK(!db.can_edit_board(tid, "alice", permission::none));
	CHECK(!db.can_edit_board(tid, "bob", permission::gantt_write));
	CHECK(db.can_edit_board(tid, "anyone", permission::admin));
}
