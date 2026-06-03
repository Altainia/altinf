#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/WDate.h>
#include <unistd.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "org/kanban_db.hpp"
#include "seed_users.hpp"

TEST_CASE("kanban_db - v2 migration backfills user/actor/author id columns")
{
	const auto path = (std::filesystem::temp_directory_path() /
	                   ("altinf_kanban_mig_" + std::to_string(::getpid()) + ".db"))
	                    .string();
	std::filesystem::remove(path);

	// Hand-build a pre-v2 kanban schema (text user columns, no id columns) plus a
	// minimal user table, stamp kanban at the baseline.
	{
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
		Wt::Dbo::Transaction t{s};
		s.execute(
		  "CREATE TABLE \"user\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"username\" text not null, "
		  "\"display_name\" text not null, \"password_hash\" text not null, "
		  "\"permissions\" bigint not null, \"deleted_at\" text not null default '')");
		s.execute(
		  "INSERT INTO \"user\" (version, username, display_name, password_hash, "
		  "permissions, deleted_at) VALUES (0, 'alice', 'Alice', '', 0, '')");
		s.execute(
		  "CREATE TABLE \"team_member\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"team_id\" bigint not null, "
		  "\"username\" text not null, \"is_lead\" integer not null)");
		s.execute(
		  "CREATE TABLE \"task_event\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"task_id\" bigint not null, "
		  "\"actor\" text not null, \"occurred_at\" text not null, \"event_type\" text not null)");
		s.execute(
		  "CREATE TABLE \"task_comment\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"task_id\" bigint not null, \"author\" text not null, "
		  "\"body\" text not null, \"created_at\" text not null, \"is_deleted\" integer not null)");
		// The other baseline tables the v2 migration also alters (a real v1 DB has
		// them); created empty here so the ALTERs succeed.
		s.execute(
		  "CREATE TABLE \"task_assignee\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"task_id\" bigint not null, \"username\" text not null)");
		s.execute(
		  "CREATE TABLE \"task_comment_event\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"comment_id\" bigint not null, \"actor\" text not null, "
		  "\"occurred_at\" text not null, \"event_type\" text not null, \"body_snapshot\" text not null)");
		s.execute(
		  "CREATE TABLE \"team_settings_event\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"org_id\" bigint not null, \"team_id\" bigint not null, "
		  "\"actor\" text not null, \"occurred_at\" text not null, \"field_name\" text not null, "
		  "\"old_value\" text not null, \"new_value\" text not null)");
		s.execute(
		  "INSERT INTO \"team_member\" (version, team_id, username, is_lead) "
		  "VALUES (0, 1, 'alice', 1)");
		s.execute(
		  "INSERT INTO \"task_event\" (version, task_id, actor, occurred_at, event_type) "
		  "VALUES (0, 1, 'alice', '2026-01-01T00:00:00Z', 'created')");
		s.execute(
		  "INSERT INTO \"task_comment\" (version, task_id, author, body, created_at, is_deleted) "
		  "VALUES (0, 1, 'alice', 'hi', '2026-01-01T00:00:00Z', 0)");
		s.execute("CREATE TABLE schema_version (domain text primary key, version integer not null)");
		s.execute("INSERT INTO schema_version (domain, version) VALUES ('kanban', 1)");
		t.commit();
	}

	// Constructing kanban_db runs the v2 migration (creates the remaining baseline
	// tables it maps, and backfills the id columns on the pre-existing rows).
	{
		kanban_db db{path};
	}

	{
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
		Wt::Dbo::Transaction t{s};
		// the text columns were dropped by the v3 contract migration; rows are
		// keyed by id now.
		const auto uid = s.query<long long>("select id from \"user\" where username='alice'").resultValue();
		CHECK(s.query<long long>("select user_id from \"team_member\" where team_id=1").resultValue() == uid);
		CHECK(s.query<long long>("select actor_id from \"task_event\" where task_id=1").resultValue() == uid);
		CHECK(s.query<long long>("select author_id from \"task_comment\" where task_id=1").resultValue() == uid);
	}

	std::filesystem::remove(path);
}

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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("Engineering", 1);
	CHECK(tid > 0);
	const auto t = db.find_team(tid);
	REQUIRE(t.has_value());
	CHECK(t->name == "Engineering");
	CHECK(t->org_id == 1);
}

TEST_CASE("kanban_db - teams_for_org")
{
	kanban_db db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("Old", 1);
	db.rename_team(tid, "New");
	CHECK(db.find_team(tid)->name == "New");
}

TEST_CASE("kanban_db - set_team_org")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.set_team_org(tid, 99);
	CHECK(db.find_team(tid)->org_id == 99);
}

TEST_CASE("kanban_db - find_team missing returns nullopt")
{
	kanban_db db{seeded_db_path()};
	CHECK(!db.find_team(9999).has_value());
}

TEST_CASE("kanban_db - archive_team hides team and archives its tasks")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	CHECK(db.members_for_team(tid).size() == 2);
}

TEST_CASE("kanban_db - add_member is idempotent")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "alice");
	CHECK(db.members_for_team(tid).size() == 1);
}

TEST_CASE("kanban_db - remove member")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	CHECK(db.is_member(tid, "alice"));
	CHECK(!db.is_member(tid, "stranger"));
}

TEST_CASE("kanban_db - team lead role")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "My Task"), "test");
	const auto      opt = db.find_task(id);
	REQUIRE(opt.has_value());
	CHECK(opt->title == "My Task");
	CHECK(opt->type_id == 0);
}

TEST_CASE("kanban_db - find_task missing returns nullopt")
{
	kanban_db db{seeded_db_path()};
	CHECK(!db.find_task(9999).has_value());
}

TEST_CASE("kanban_db - update task")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	CHECK(db.tasks_for_team(tid).empty());
}

// ---- multi-assignee ----

TEST_CASE("kanban_db - add_assignee: success on unassigned task")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");

	CHECK(db.add_assignee(id, "alice", "alice"));
	const auto a = db.assignees_for_task(id);
	REQUIRE(a.size() == 1);
	CHECK(a[0] == "alice");
}

TEST_CASE("kanban_db - add_assignee: two users")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");

	CHECK(db.add_assignee(id, "alice", "alice"));
	CHECK(db.add_assignee(id, "bob", "alice"));
	const auto a = db.assignees_for_task(id);
	CHECK(a.size() == 2);
}

TEST_CASE("kanban_db - add_assignee: idempotent")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");

	db.add_assignee(id, "alice", "alice");
	CHECK(!db.add_assignee(id, "alice", "alice")); // already assigned → false
	CHECK(db.assignees_for_task(id).size() == 1);
}

TEST_CASE("kanban_db - add_assignee: rejects done task")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work", "done"), "alice");

	CHECK(!db.add_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - remove_assignee: success")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");
	db.add_assignee(id, "bob", "alice");

	CHECK(db.remove_assignee(id, "bob", "alice"));
	const auto a = db.assignees_for_task(id);
	REQUIRE(a.size() == 1);
	CHECK(a[0] == "alice");
}

TEST_CASE("kanban_db - remove_assignee: false when not assigned")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "Work"), "alice");

	CHECK(!db.remove_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - tasks_for_team populates assignees")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");
	db.add_assignee(id, "bob", "alice");

	const auto tasks = db.tasks_for_team(tid);
	REQUIRE(!tasks.empty());
	CHECK(tasks[0].assignees.size() == 2);
}

// Build a task with an explicit end date (make_task hard-codes one).
static kanban_task_entry dated_task(long long          team_id,
                                    const std::string& title,
                                    const Wt::WDate&   end_date)
{
	kanban_task_entry e;
	e.team_id    = team_id;
	e.title      = title;
	e.status     = "todo";
	e.start_date = Wt::WDate{2024, 1, 1};
	e.end_date   = end_date;
	return e;
}

TEST_CASE("kanban_db - assigned_tasks_for_user_in_org: end date order, no end date last")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");

	// Add out of order; one task has no end date.
	const long long late  = db.add_task(dated_task(tid, "Late", Wt::WDate{2024, 3, 1}), "alice");
	const long long none  = db.add_task(dated_task(tid, "NoEnd", Wt::WDate{}), "alice");
	const long long early = db.add_task(dated_task(tid, "Early", Wt::WDate{2024, 1, 15}), "alice");
	for(const long long id: {late, none, early})
	{
		db.add_assignee(id, "alice", "alice");
	}

	const auto tasks = db.assigned_tasks_for_user_in_org("alice", 1);
	REQUIRE(tasks.size() == 3);
	CHECK(tasks[0].title == "Early");
	CHECK(tasks[1].title == "Late");
	CHECK(tasks[2].title == "NoEnd"); // no end date sorts last
}

TEST_CASE("kanban_db - assigned_tasks_for_user_in_org: equal end dates tie-break by creation order")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");

	const Wt::WDate same{2024, 2, 1};
	const long long first  = db.add_task(dated_task(tid, "First", same), "alice");
	const long long second = db.add_task(dated_task(tid, "Second", same), "alice");
	db.add_assignee(second, "alice", "alice");
	db.add_assignee(first, "alice", "alice");

	const auto tasks = db.assigned_tasks_for_user_in_org("alice", 1);
	REQUIRE(tasks.size() == 2);
	CHECK(tasks[0].title == "First"); // lower id (created earlier) first
	CHECK(tasks[1].title == "Second");
}

TEST_CASE("kanban_db - assigned_tasks_for_user_in_org: scoped to the org's teams")
{
	kanban_db       db{seeded_db_path()};
	const long long t_a = db.create_team("A", 10);
	const long long t_b = db.create_team("B", 20);
	db.add_member(t_a, "alice");
	db.add_member(t_b, "alice");
	const long long a = db.add_task(make_task(t_a, "InOrg10"), "alice");
	const long long b = db.add_task(make_task(t_b, "InOrg20"), "alice");
	db.add_assignee(a, "alice", "alice");
	db.add_assignee(b, "alice", "alice");

	const auto tasks = db.assigned_tasks_for_user_in_org("alice", 10);
	REQUIRE(tasks.size() == 1);
	CHECK(tasks[0].title == "InOrg10");
	CHECK(tasks[0].team_id == t_a);
}

TEST_CASE("kanban_db - assigned_tasks_for_user_in_org: excludes archived, done and others' tasks")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");

	const long long active = db.add_task(make_task(tid, "Active"), "alice");
	db.add_assignee(active, "alice", "alice");

	const long long archived = db.add_task(make_task(tid, "Archived"), "alice");
	db.add_assignee(archived, "alice", "alice");
	db.archive_task(archived, "alice");

	// Assigned while "todo", then moved to "done" — which clears assignees.
	const long long done = db.add_task(make_task(tid, "Done"), "alice");
	db.add_assignee(done, "alice", "alice");
	db.update_task_status(done, "done", 0, "alice");

	const long long bobs = db.add_task(make_task(tid, "Bobs"), "alice");
	db.add_assignee(bobs, "bob", "bob");

	const auto tasks = db.assigned_tasks_for_user_in_org("alice", 1);
	REQUIRE(tasks.size() == 1);
	CHECK(tasks[0].title == "Active");
}

TEST_CASE("kanban_db - assigned_tasks_for_user_in_org: empty for unknown user or no assignments")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");

	CHECK(db.assigned_tasks_for_user_in_org("nobody", 1).empty());  // unknown user
	CHECK(db.assigned_tasks_for_user_in_org("bob", 1).empty());     // no assignments
	CHECK(db.assigned_tasks_for_user_in_org("alice", 999).empty()); // org with no teams
}

TEST_CASE("kanban_db - find_task populates assignees")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");

	const auto t = db.find_task(id);
	REQUIRE(t.has_value());
	REQUIRE(t->assignees.size() == 1);
	CHECK(t->assignees[0] == "alice");
}

TEST_CASE("kanban_db - add_assignee records history event")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");

	const auto hist = db.history_for_task(id);
	const bool has_event =
	  std::any_of(hist.begin(), hist.end(), [](const task_event_entry& e) {
		  return e.event_type == "updated" &&
		         std::any_of(e.changes.begin(), e.changes.end(), [](const task_field_change_entry& c) {
			         return c.field_name == "assignees";
		         });
	  });
	CHECK(has_event);
}

TEST_CASE("kanban_db - add_assignee: fails on archived task")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.archive_task(id, "alice");

	CHECK(!db.add_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - add_assignee: fails for non-member")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	// "alice" is NOT added as a member
	const long long id = db.add_task(make_task(tid, "Work"), "alice");

	CHECK(!db.add_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - archived_tasks_for_team populates assignees")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");
	db.archive_task(id, "alice");

	const auto tasks = db.archived_tasks_for_team(tid);
	REQUIRE(!tasks.empty());
	CHECK(tasks[0].assignees.size() == 1);
	CHECK(tasks[0].assignees[0] == "alice");
}

// ---- remove_member_from_org_teams ----

TEST_CASE("kanban_db - remove_member_from_org_teams removes from all org teams")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");

	db.remove_member_from_all_teams("nobody");

	CHECK(db.is_member(tid, "alice"));
}

// ---- task types ----

TEST_CASE("kanban_db - create and retrieve task type")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db db{seeded_db_path()};
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
	kanban_db db{seeded_db_path()};
	CHECK(db.types_for_org(99).empty());
}

TEST_CASE("kanban_db - update_task_type changes name and color")
{
	kanban_db       db{seeded_db_path()};
	const long long id = db.create_task_type(1, "Old", "#aaaaaa");
	db.update_task_type(id, "New", "#bbbbbb");
	const auto t = db.find_task_type(id);
	REQUIRE(t.has_value());
	CHECK(t->name == "New");
	CHECK(t->color == "#bbbbbb");
}

TEST_CASE("kanban_db - delete_task_type zeroes out affected tasks")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db db{seeded_db_path()};
	CHECK(!db.find_task_type(9999).has_value());
}

TEST_CASE("kanban_db - task type_id persists through add and find")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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

TEST_CASE("kanban_db - update_task_status with same status writes no event")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	const long long id  = db.add_task(make_task(tid, "T", "todo"), "alice");
	db.update_task_status(id, "todo", 0, "alice"); // same status
	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 1); // only the created event
	CHECK(history[0].event_type == "created");
}

TEST_CASE("kanban_db - archive_task records archived event")
{
	kanban_db       db{seeded_db_path()};
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
	kanban_db       db{seeded_db_path()};
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

TEST_CASE("kanban_db - update_task records type change by name")
{
	kanban_db       db{seeded_db_path()};
	const long long tid    = db.create_team("T", 1);
	const long long bug_id = db.create_task_type(1, "Bug", "#ff0000");
	const long long fix_id = db.create_task_type(1, "Fix", "#00ff00");

	auto task          = make_task(tid, "Work");
	task.type_id       = bug_id;
	const long long id = db.add_task(task, "alice");

	auto updated    = *db.find_task(id);
	updated.type_id = fix_id;
	db.update_task(updated, "alice");

	const auto history = db.history_for_task(id);
	REQUIRE(history.size() == 2);
	CHECK(history[0].event_type == "updated");
	REQUIRE(history[0].changes.size() == 1);
	CHECK(history[0].changes[0].field_name == "type");
	CHECK(history[0].changes[0].old_value == "Bug");
	CHECK(history[0].changes[0].new_value == "Fix");
}

TEST_CASE("kanban_db - archive_team cascades archived event to each task")
{
	kanban_db       db{seeded_db_path()};
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

// ---- comments ----

TEST_CASE("kanban_db - add_comment returns id and is retrievable")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "Hello **world**");
	CHECK(cid > 0);
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].id == cid);
	CHECK(comments[0].author == "alice");
	CHECK(comments[0].body == "Hello **world**");
	CHECK(!comments[0].is_deleted);
	CHECK(comments[0].last_edited_by.empty());
	CHECK(comments[0].deleted_by.empty());
	CHECK(!comments[0].created_at.empty());
}

TEST_CASE("kanban_db - comments_for_task returns in chronological order")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	db.add_comment(task, "alice", "First");
	db.add_comment(task, "bob", "Second");
	db.add_comment(task, "carol", "Third");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 3);
	CHECK(comments[0].body == "First");
	CHECK(comments[1].body == "Second");
	CHECK(comments[2].body == "Third");
}

TEST_CASE("kanban_db - edit_comment updates body and records edited event")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "Original");
	db.edit_comment(cid, "alice", "Revised");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].body == "Revised");
	CHECK(comments[0].last_edited_by == "alice");
	CHECK(!comments[0].last_edited_at.empty());
}

TEST_CASE("kanban_db - edit_comment reflects most recent editor when edited twice")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "v1");
	db.edit_comment(cid, "alice", "v2");
	db.edit_comment(cid, "bob", "v3");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].body == "v3");
	CHECK(comments[0].last_edited_by == "bob");
}

TEST_CASE("kanban_db - edit_comment is a no-op on deleted comment")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "Original");
	db.delete_comment(cid, "alice");
	db.edit_comment(cid, "alice", "Should not apply");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].body.empty());           // still deleted, body cleared
	CHECK(comments[0].last_edited_by.empty()); // no edit event recorded
}

TEST_CASE("kanban_db - delete_comment sets is_deleted and clears body in entry")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "Goodbye");
	db.delete_comment(cid, "bob");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].is_deleted);
	CHECK(comments[0].body.empty());
	CHECK(comments[0].deleted_by == "bob");
	CHECK(!comments[0].deleted_at.empty());
}

TEST_CASE("kanban_db - delete_comment is a no-op if already deleted")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	const long long cid  = db.add_comment(task, "alice", "Hello");
	db.delete_comment(cid, "alice");
	db.delete_comment(cid, "bob"); // second delete is a no-op
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 1);
	CHECK(comments[0].deleted_by == "alice"); // original deletor preserved
}

TEST_CASE("kanban_db - comments_for_task includes deleted comments as placeholders")
{
	kanban_db       db{seeded_db_path()};
	const long long tid  = db.create_team("T", 1);
	const long long task = db.add_task(make_task(tid, "Work"), "creator");
	db.add_comment(task, "alice", "Keep me");
	const long long cid2 = db.add_comment(task, "bob", "Delete me");
	db.delete_comment(cid2, "bob");
	const auto comments = db.comments_for_task(task);
	REQUIRE(comments.size() == 2);
	CHECK(!comments[0].is_deleted);
	CHECK(comments[0].body == "Keep me");
	CHECK(comments[1].is_deleted);
	CHECK(comments[1].body.empty());
}

// ---- done-column assignee clearing ----

TEST_CASE("kanban_db - update_task_status to done clears assignees")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	db.add_member(tid, "bob");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");
	db.add_assignee(id, "bob", "alice");

	db.update_task_status(id, "done", 0, "alice");
	CHECK(db.assignees_for_task(id).empty());
}

TEST_CASE("kanban_db - update_task_status to done records assignee-cleared event")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");

	db.update_task_status(id, "done", 0, "alice");

	const auto hist = db.history_for_task(id);
	const bool has_clear =
	  std::any_of(hist.begin(), hist.end(), [](const task_event_entry& e) {
		  return std::any_of(e.changes.begin(), e.changes.end(), [](const task_field_change_entry& c) {
			  return c.field_name == "assignees" &&
			         c.new_value.find("done") != std::string::npos;
		  });
	  });
	CHECK(has_clear);
}

TEST_CASE("kanban_db - update_task_status non-done does not clear assignees")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work"), "alice");
	db.add_assignee(id, "alice", "alice");

	db.update_task_status(id, "in_progress", 0, "alice");
	CHECK(db.assignees_for_task(id).size() == 1);
}

TEST_CASE("kanban_db - add_assignee rejects task already in done status")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 1);
	db.add_member(tid, "alice");
	const long long id = db.add_task(make_task(tid, "Work", "done"), "alice");

	CHECK(!db.add_assignee(id, "alice", "alice"));
}

// ---- team settings ----

TEST_CASE("kanban_db - settings_for_team returns all-true defaults when no row exists")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 42);

	const auto s = db.settings_for_team(tid);
	CHECK(s.allow_member_move_columns);
	CHECK(s.allow_self_assign_unassigned);
	CHECK(s.allow_self_assign_assigned);
	CHECK(s.allow_abandon);
}

TEST_CASE("kanban_db - set_team_settings persists team-specific override")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 42);

	team_settings_entry s;
	s.org_id                       = 42;
	s.team_id                      = tid;
	s.allow_member_move_columns    = false;
	s.allow_self_assign_unassigned = true;
	s.allow_self_assign_assigned   = true;
	s.allow_abandon                = true;
	db.set_team_settings(s, "lead");

	const auto back = db.settings_for_team(tid);
	CHECK(!back.allow_member_move_columns);
	CHECK(back.allow_self_assign_unassigned);
}

TEST_CASE("kanban_db - org-wide default applies when no team row")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 42);

	team_settings_entry org_default;
	org_default.org_id                       = 42;
	org_default.team_id                      = 0; // 0 = org-wide
	org_default.allow_member_move_columns    = false;
	org_default.allow_self_assign_unassigned = true;
	org_default.allow_self_assign_assigned   = true;
	org_default.allow_abandon                = true;
	db.set_team_settings(org_default, "lead");

	const auto s = db.settings_for_team(tid); // no team-specific row
	CHECK(!s.allow_member_move_columns);
}

TEST_CASE("kanban_db - team-specific row overrides org default")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 42);

	team_settings_entry org_default;
	org_default.org_id                       = 42;
	org_default.team_id                      = 0;
	org_default.allow_member_move_columns    = false;
	org_default.allow_self_assign_unassigned = false;
	org_default.allow_self_assign_assigned   = false;
	org_default.allow_abandon                = false;
	db.set_team_settings(org_default, "lead");

	team_settings_entry team_override;
	team_override.org_id                       = 42;
	team_override.team_id                      = tid;
	team_override.allow_member_move_columns    = true;
	team_override.allow_self_assign_unassigned = true;
	team_override.allow_self_assign_assigned   = true;
	team_override.allow_abandon                = true;
	db.set_team_settings(team_override, "lead");

	const auto s = db.settings_for_team(tid);
	CHECK(s.allow_member_move_columns);
	CHECK(s.allow_abandon);
}

TEST_CASE("kanban_db - set_team_settings records audit event")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("T", 42);

	team_settings_entry s;
	s.org_id                       = 42;
	s.team_id                      = tid;
	s.allow_member_move_columns    = false;
	s.allow_self_assign_unassigned = true;
	s.allow_self_assign_assigned   = true;
	s.allow_abandon                = true;
	db.set_team_settings(s, "lead");

	const auto events = db.settings_events_for_team(tid);
	REQUIRE(!events.empty());
	CHECK(events[0].actor == "lead");
	const bool found_move =
	  std::any_of(events.begin(), events.end(), [](const team_settings_event_entry& e) {
		  return e.field_name == "allow_member_move_columns" && e.new_value == "0";
	  });
	CHECK(found_move);
}

TEST_CASE("kanban_db - allow_member_edit_details round-trips and is audited")
{
	kanban_db       db{seeded_db_path()};
	const long long tid = db.create_team("Engineering", 1);

	// Defaults to true (members allowed) when no row exists.
	CHECK(db.settings_for_team(tid).allow_member_edit_details);

	// Turn it off and persist.
	auto s                      = db.settings_for_team(tid);
	s.org_id                    = 1;
	s.team_id                   = tid;
	s.allow_member_edit_details = false;
	db.set_team_settings(s, "lead");

	CHECK_FALSE(db.settings_for_team(tid).allow_member_edit_details);

	// The change is recorded in the audit log.
	const auto events  = db.settings_events_for_team(tid);
	const bool audited = std::ranges::any_of(events, [](const auto& e) {
		return e.field_name == "allow_member_edit_details" && e.new_value == "0";
	});
	CHECK(audited);
}
