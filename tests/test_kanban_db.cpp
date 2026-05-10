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
	e.sort_order = sort;
	return e;
}

// ---- teams ----

TEST_CASE("kanban_db - create team with org_id")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("Engineering", 1);
	CHECK(tid > 0);
	const auto t = db.find_team(tid);
	REQUIRE(t.has_value());
	CHECK(t->name == "Engineering");
	CHECK(t->org_id == 1);
}

TEST_CASE("kanban_db - teams_for_org")
{
	kanban_db db{":memory:"};
	db.create_team("Alpha", 10);
	db.create_team("Beta", 10);
	db.create_team("Gamma", 20);
	const auto org10 = db.teams_for_org(10);
	REQUIRE(org10.size() == 2);
	const auto org20 = db.teams_for_org(20);
	REQUIRE(org20.size() == 1);
}

TEST_CASE("kanban_db - rename team")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("Old", 1);
	db.rename_team(tid, "New");
	CHECK(db.find_team(tid)->name == "New");
}

TEST_CASE("kanban_db - set_team_org")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.set_team_org(tid, 99);
	CHECK(db.find_team(tid)->org_id == 99);
}

TEST_CASE("kanban_db - find_team missing returns nullopt")
{
	kanban_db db{":memory:"};
	CHECK(!db.find_team(9999).has_value());
}

TEST_CASE("kanban_db - archive_team hides team and archives its tasks")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_task(make_task(tid, "Chore"), "alice");
	db.archive_team(tid, "alice");
	CHECK(db.find_team(tid).has_value());            // row still exists
	CHECK(db.find_team(tid)->is_archived);           // but archived
	CHECK(db.teams_for_org(1).empty());              // not visible in active list
	CHECK(db.tasks_for_team(tid).empty());           // tasks hidden
	CHECK(!db.archived_tasks_for_team(tid).empty()); // but visible in archive
	CHECK(!db.members_for_team(tid).empty());        // members preserved
}

// ---- members ----

TEST_CASE("kanban_db - add and list members")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	CHECK(db.members_for_team(tid).size() == 2);
}

TEST_CASE("kanban_db - add_member is idempotent")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "alice");
	CHECK(db.members_for_team(tid).size() == 1);
}

TEST_CASE("kanban_db - remove member")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	db.remove_member(tid, "alice");
	const auto m = db.members_for_team(tid);
	REQUIRE(m.size() == 1);
	CHECK(m[0] == "bob");
}

TEST_CASE("kanban_db - is_member")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	CHECK(db.is_member(tid, "alice"));
	CHECK(!db.is_member(tid, "stranger"));
}

TEST_CASE("kanban_db - team lead role")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	CHECK(!db.is_team_lead(tid, "alice"));
	db.set_team_lead(tid, "alice", true);
	CHECK(db.is_team_lead(tid, "alice"));
	db.set_team_lead(tid, "alice", false);
	CHECK(!db.is_team_lead(tid, "alice"));
}

TEST_CASE("kanban_db - team_member_entries includes is_lead flag")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	db.set_team_lead(tid, "alice", true);
	const auto entries = db.team_member_entries(tid);
	REQUIRE(entries.size() == 2);
	const auto alice_it = std::find_if(
	  entries.begin(), entries.end(), [](const team_member_entry& e) { return e.username == "alice"; });
	REQUIRE(alice_it != entries.end());
	CHECK(alice_it->is_lead);
	const auto bob_it = std::find_if(
	  entries.begin(), entries.end(), [](const team_member_entry& e) { return e.username == "bob"; });
	REQUIRE(bob_it != entries.end());
	CHECK(!bob_it->is_lead);
}

// ---- tasks ----

TEST_CASE("kanban_db - add task and retrieve for team")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_task(make_task(tid, "Alpha", "todo", 1), "test");
	db.add_task(make_task(tid, "Beta", "todo", 0), "test");
	const auto tasks = db.tasks_for_team(tid);
	REQUIRE(tasks.size() == 2);
	CHECK(tasks[0].title == "Beta");
	CHECK(tasks[1].title == "Alpha");
}

TEST_CASE("kanban_db - find_task")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "My Task"), "test");
	const auto      opt = db.find_task(id);
	REQUIRE(opt.has_value());
	CHECK(opt->title == "My Task");
	CHECK(opt->type_id == 0);
}

TEST_CASE("kanban_db - find_task missing returns nullopt")
{
	kanban_db db{":memory:"};
	CHECK(!db.find_task(9999).has_value());
}

TEST_CASE("kanban_db - update task")
{
	kanban_db       db{":memory:"};
	const long long tid  = db.create_team("T", 1);
	const long long id   = db.add_task(make_task(tid, "Original"), "test");
	auto            task = *db.find_task(id);
	task.title           = "Updated";
	db.update_task(task, "test");
	const auto updated = db.find_task(id);
	CHECK(updated->title == "Updated");
}

TEST_CASE("kanban_db - update_task_status")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "T", "todo"), "test");
	db.update_task_status(id, "done", 5, "test");
	const auto opt = db.find_task(id);
	REQUIRE(opt.has_value());
	CHECK(opt->status == "done");
	CHECK(opt->sort_order == 5);
}

TEST_CASE("kanban_db - archive_task hides task from tasks_for_team")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "To Archive"), "alice");
	db.add_task(make_task(tid, "To Keep"), "alice");
	db.archive_task(id, "alice");
	const auto tasks = db.tasks_for_team(tid);
	REQUIRE(tasks.size() == 1);
	CHECK(tasks[0].title == "To Keep");
	const auto archived = db.archived_tasks_for_team(tid);
	REQUIRE(archived.size() == 1);
	CHECK(archived[0].title == "To Archive");
}

TEST_CASE("kanban_db - tasks_for_team empty when no tasks")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	CHECK(db.tasks_for_team(tid).empty());
}

TEST_CASE("kanban_db - self_assign succeeds on unassigned task")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "test");
	CHECK(db.self_assign(id, "alice"));
	CHECK(db.find_task(id)->assigned_to == "alice");
}

TEST_CASE("kanban_db - self_assign fails when already assigned")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	auto t             = make_task(tid, "Work");
	t.assigned_to      = "alice";
	const long long id = db.add_task(t, "test");
	CHECK(!db.self_assign(id, "bob"));
}

TEST_CASE("kanban_db - self_assign fails for non-member")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "Work"), "test");
	CHECK(!db.self_assign(id, "stranger"));
}

// ---- permissions ----

TEST_CASE("kanban_db - can_view_board: admin bypasses membership")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	CHECK(db.can_view_board(tid, "anyone", permission::admin));
}

TEST_CASE("kanban_db - can_view_board: org_lead bypasses membership")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	CHECK(db.can_view_board(tid, "lead", permission::none, /*is_org_lead=*/true));
}

TEST_CASE("kanban_db - can_view_board: member can view, non-member cannot")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	CHECK(db.can_view_board(tid, "alice", permission::none));
	CHECK(!db.can_view_board(tid, "stranger", permission::none));
}

TEST_CASE("kanban_db - can_edit_board: org_lead has edit rights")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	CHECK(db.can_edit_board(tid, "lead", permission::none,
	                        /*is_org_lead=*/true,
	                        /*is_team_lead=*/false));
}

TEST_CASE("kanban_db - can_edit_board: team_lead has edit rights")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.set_team_lead(tid, "alice", true);
	CHECK(db.can_edit_board(tid, "alice", permission::none,
	                        /*is_org_lead=*/false,
	                        /*is_team_lead=*/true));
}

TEST_CASE("kanban_db - can_edit_board: plain member cannot edit")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	CHECK(!db.can_edit_board(tid, "alice", permission::none));
}

TEST_CASE("kanban_db - can_edit_board: admin always can edit")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	CHECK(db.can_edit_board(tid, "anyone", permission::admin));
}

// ---- remove_member_from_org_teams ----

TEST_CASE("kanban_db - remove_member_from_org_teams removes from all org teams")
{
	kanban_db       db{":memory:"};
	const long long tid1 = db.create_team("T1", 1);
	const long long tid2 = db.create_team("T2", 1);
	db.add_member(tid1, "bob");
	db.add_member(tid2, "bob");
	db.add_member(tid1, "alice");

	db.remove_member_from_org_teams(1, "bob");

	CHECK(!db.is_member(tid1, "bob"));
	CHECK(!db.is_member(tid2, "bob"));
	CHECK(db.is_member(tid1, "alice")); // unaffected
}

TEST_CASE("kanban_db - remove_member_from_org_teams does not touch other orgs")
{
	kanban_db       db{":memory:"};
	const long long tid_org1 = db.create_team("T1", 1);
	const long long tid_org2 = db.create_team("T2", 2);
	db.add_member(tid_org1, "bob");
	db.add_member(tid_org2, "bob");

	db.remove_member_from_org_teams(1, "bob");

	CHECK(!db.is_member(tid_org1, "bob"));
	CHECK(db.is_member(tid_org2, "bob")); // org 2 unaffected
}

// ---- remove_member_from_all_teams ----

TEST_CASE("kanban_db - remove_member_from_all_teams removes from every team")
{
	kanban_db       db{":memory:"};
	const long long tid1 = db.create_team("T1", 1);
	const long long tid2 = db.create_team("T2", 2);
	db.add_member(tid1, "bob");
	db.add_member(tid2, "bob");
	db.add_member(tid1, "alice");

	db.remove_member_from_all_teams("bob");

	CHECK(!db.is_member(tid1, "bob"));
	CHECK(!db.is_member(tid2, "bob"));
	CHECK(db.is_member(tid1, "alice")); // unaffected
}

TEST_CASE("kanban_db - remove_member_from_all_teams on non-member is a no-op")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");

	db.remove_member_from_all_teams("nobody");

	CHECK(db.is_member(tid, "alice"));
}

// ---- task types ----

TEST_CASE("kanban_db - create and retrieve task type")
{
	kanban_db       db{":memory:"};
	const long long id = db.create_task_type(1, "Bug Fix", "#e07b54");
	CHECK(id > 0);
	const auto t = db.find_task_type(id);
	REQUIRE(t.has_value());
	CHECK(t->name == "Bug Fix");
	CHECK(t->color == "#e07b54");
	CHECK(t->org_id == 1);
}

TEST_CASE("kanban_db - types_for_org returns only org types")
{
	kanban_db db{":memory:"};
	db.create_task_type(1, "Bug Fix", "#e07b54");
	db.create_task_type(1, "Feature", "#5a9e6f");
	db.create_task_type(2, "Other", "#7aa2d4");
	const auto org1 = db.types_for_org(1);
	REQUIRE(org1.size() == 2);
	const auto org2 = db.types_for_org(2);
	REQUIRE(org2.size() == 1);
}

TEST_CASE("kanban_db - types_for_org empty when no types")
{
	kanban_db db{":memory:"};
	CHECK(db.types_for_org(99).empty());
}

TEST_CASE("kanban_db - update_task_type changes name and color")
{
	kanban_db       db{":memory:"};
	const long long id = db.create_task_type(1, "Old", "#aaaaaa");
	db.update_task_type(id, "New", "#bbbbbb");
	const auto t = db.find_task_type(id);
	REQUIRE(t.has_value());
	CHECK(t->name == "New");
	CHECK(t->color == "#bbbbbb");
}

TEST_CASE("kanban_db - delete_task_type zeroes out affected tasks")
{
	kanban_db       db{":memory:"};
	const long long team_id = db.create_team("T", 1);
	const long long type_id = db.create_task_type(1, "Bug Fix", "#e07b54");

	kanban_task_entry e;
	e.team_id           = team_id;
	e.title             = "A task";
	e.type_id           = type_id;
	const long long tid = db.add_task(e, "test");

	db.delete_task_type(type_id);

	CHECK(!db.find_task_type(type_id).has_value());
	CHECK(db.find_task(tid)->type_id == 0);
}

TEST_CASE("kanban_db - find_task_type missing returns nullopt")
{
	kanban_db db{":memory:"};
	CHECK(!db.find_task_type(9999).has_value());
}

TEST_CASE("kanban_db - task type_id persists through add and find")
{
	kanban_db       db{":memory:"};
	const long long team_id = db.create_team("T", 1);
	const long long type_id = db.create_task_type(1, "Feature", "#5a9e6f");

	kanban_task_entry e;
	e.team_id          = team_id;
	e.title            = "My task";
	e.type_id          = type_id;
	const long long id = db.add_task(e, "test");

	const auto found = db.find_task(id);
	REQUIRE(found.has_value());
	CHECK(found->type_id == type_id);
}

TEST_CASE("kanban_db - update_task preserves type_id change")
{
	kanban_db       db{":memory:"};
	const long long team_id = db.create_team("T", 1);
	const long long type_id = db.create_task_type(1, "Feature", "#5a9e6f");

	kanban_task_entry e;
	e.team_id          = team_id;
	e.title            = "My task";
	e.type_id          = 0;
	const long long id = db.add_task(e, "test");

	auto task    = *db.find_task(id);
	task.type_id = type_id;
	db.update_task(task, "test");

	CHECK(db.find_task(id)->type_id == type_id);
}

TEST_CASE("kanban_db - add_task records created event")
{
	kanban_db       db{":memory:"};
	const long long tid     = db.create_team("T", 1);
	const long long id      = db.add_task(make_task(tid, "My Task"), "alice");
	const auto      history = db.history_for_task(id);
	REQUIRE(history.size() == 1);
	CHECK(history[0].event_type == "created");
	CHECK(history[0].actor == "alice");
	CHECK(history[0].changes.empty());
}

TEST_CASE("kanban_db - update_task records field changes")
{
	kanban_db       db{":memory:"};
	const long long tid  = db.create_team("T", 1);
	const long long id   = db.add_task(make_task(tid, "Original"), "alice");
	auto            task = *db.find_task(id);
	task.title           = "Updated";
	db.update_task(task, "bob");
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 2); // created + updated
	CHECK(history[0].event_type == "updated");
	REQUIRE(history[0].changes.size() == 1);
	CHECK(history[0].changes[0].field_name == "title");
	CHECK(history[0].changes[0].old_value == "Original");
	CHECK(history[0].changes[0].new_value == "Updated");
}

TEST_CASE("kanban_db - update_task with no changes writes no event")
{
	kanban_db       db{":memory:"};
	const long long tid  = db.create_team("T", 1);
	const long long id   = db.add_task(make_task(tid, "Same"), "alice");
	auto            task = *db.find_task(id);
	db.update_task(task, "alice"); // nothing changed
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 1); // only the created event
	CHECK(history[0].event_type == "created");
}

TEST_CASE("kanban_db - update_task_status records status change")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "T", "todo"), "alice");
	db.update_task_status(id, "done", 5, "alice");
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 2);
	CHECK(history[0].event_type == "updated");
	REQUIRE(history[0].changes.size() == 1);
	CHECK(history[0].changes[0].field_name == "status");
	CHECK(history[0].changes[0].old_value == "todo");
	CHECK(history[0].changes[0].new_value == "done");
}

TEST_CASE("kanban_db - archive_task records archived event")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "Work"), "alice");
	db.archive_task(id, "bob");
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 2);
	CHECK(history[0].event_type == "archived");
	CHECK(history[0].actor == "bob");
}

TEST_CASE("kanban_db - unarchive_task records unarchived event")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "Work"), "alice");
	db.archive_task(id, "alice");
	db.unarchive_task(id, "carol");
	const auto active = db.tasks_for_team(tid);
	REQUIRE(active.size() == 1);
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 3);
	CHECK(history[0].event_type == "unarchived");
	CHECK(history[0].actor == "carol");
}

TEST_CASE("kanban_db - archive_team cascades archived event to each task")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("T", 1);
	db.add_task(make_task(tid, "A"), "alice");
	db.add_task(make_task(tid, "B"), "alice");
	db.archive_team(tid, "lead");
	const auto archived = db.archived_tasks_for_team(tid);
	REQUIRE(archived.size() == 2);
	for(const auto& t: archived)
	{
		const auto history = db.history_for_task(t.id);
		REQUIRE(!history.empty());
		CHECK(history[0].event_type == "archived");
		CHECK(history[0].actor == "lead");
	}
}
