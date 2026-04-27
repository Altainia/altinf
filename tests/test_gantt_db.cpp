#include <Wt/WDate.h>

#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"
#include "gantt/gantt_db.hpp"

static gantt_project_entry make_project(const std::string& title, const std::string& owner)
{
	gantt_project_entry e;
	e.title          = title;
	e.description    = "";
	e.owner_username = owner;
	e.created_date   = Wt::WDate{2024, 1, 1};
	return e;
}

static gantt_task_entry make_task(long long project_id, const std::string& title, int sort = 0)
{
	gantt_task_entry e;
	e.project_id  = project_id;
	e.title       = title;
	e.assigned_to = "";
	e.start_date  = Wt::WDate{2024, 1, 1};
	e.end_date    = Wt::WDate{2024, 1, 31};
	e.color       = "blue";
	e.sort_order  = sort;
	return e;
}

// ---- projects ----

TEST_CASE("gantt_db - create and find project")
{
	gantt_db  db{":memory:"};
	long long id = db.create_project(make_project("My Plan", "alice"));
	CHECK(id > 0);
	auto opt = db.find_project(id);
	REQUIRE(opt.has_value());
	CHECK(opt->title == "My Plan");
	CHECK(opt->owner_username == "alice");
}

TEST_CASE("gantt_db - find missing project returns nullopt")
{
	gantt_db db{":memory:"};
	CHECK(!db.find_project(9999).has_value());
}

TEST_CASE("gantt_db - update project")
{
	gantt_db  db{":memory:"};
	long long id   = db.create_project(make_project("Old Title", "alice"));
	auto      proj = *db.find_project(id);
	proj.title     = "New Title";
	db.update_project(proj);
	CHECK(db.find_project(id)->title == "New Title");
}

TEST_CASE("gantt_db - delete project removes tasks and viewers")
{
	gantt_db  db{":memory:"};
	long long id = db.create_project(make_project("X", "alice"));
	db.add_task(make_task(id, "Task 1"));
	db.add_viewer(id, "bob");
	db.delete_project(id);
	CHECK(!db.find_project(id).has_value());
	CHECK(db.tasks_for_project(id).empty());
	CHECK(db.viewers_for_project(id).empty());
}

// ---- tasks ----

TEST_CASE("gantt_db - add task and retrieve for project")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_task(make_task(pid, "Alpha", 1));
	db.add_task(make_task(pid, "Beta", 0));
	auto tasks = db.tasks_for_project(pid);
	REQUIRE(tasks.size() == 2);
	CHECK(tasks[0].title == "Beta"); // sort_order 0 comes first
	CHECK(tasks[1].title == "Alpha");
}

TEST_CASE("gantt_db - tasks_for_project empty when no tasks")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	CHECK(db.tasks_for_project(pid).empty());
}

TEST_CASE("gantt_db - update task")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_task(make_task(pid, "Original"));
	auto task  = db.tasks_for_project(pid)[0];
	task.title = "Updated";
	task.color = "red";
	db.update_task(task);
	auto updated = db.tasks_for_project(pid)[0];
	CHECK(updated.title == "Updated");
	CHECK(updated.color == "red");
}

TEST_CASE("gantt_db - delete task")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	long long tid = db.add_task(make_task(pid, "To Delete"));
	db.add_task(make_task(pid, "To Keep"));
	db.delete_task(tid);
	auto tasks = db.tasks_for_project(pid);
	REQUIRE(tasks.size() == 1);
	CHECK(tasks[0].title == "To Keep");
}

TEST_CASE("gantt_db - delete_tasks_for_project")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_task(make_task(pid, "T1"));
	db.add_task(make_task(pid, "T2"));
	db.delete_tasks_for_project(pid);
	CHECK(db.tasks_for_project(pid).empty());
}

// ---- viewers ----

TEST_CASE("gantt_db - add and list viewers")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_viewer(pid, "bob");
	db.add_viewer(pid, "carol");
	auto viewers = db.viewers_for_project(pid);
	CHECK(viewers.size() == 2);
}

TEST_CASE("gantt_db - remove viewer")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_viewer(pid, "bob");
	db.add_viewer(pid, "carol");
	db.remove_viewer(pid, "bob");
	auto viewers = db.viewers_for_project(pid);
	REQUIRE(viewers.size() == 1);
	CHECK(viewers[0] == "carol");
}

TEST_CASE("gantt_db - delete_viewers_for_project")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_viewer(pid, "bob");
	db.add_viewer(pid, "carol");
	db.delete_viewers_for_project(pid);
	CHECK(db.viewers_for_project(pid).empty());
}

// ---- visibility ----

TEST_CASE("gantt_db - projects_visible_to admin sees all")
{
	gantt_db db{":memory:"};
	db.create_project(make_project("Alice's", "alice"));
	db.create_project(make_project("Bob's", "bob"));
	CHECK(db.projects_visible_to("anyone", permission::admin).size() == 2);
}

TEST_CASE("gantt_db - projects_visible_to non-admin sees own and shared")
{
	gantt_db  db{":memory:"};
	long long id1 = db.create_project(make_project("Alice's", "alice"));
	long long id2 = db.create_project(make_project("Bob's", "bob"));
	db.create_project(make_project("Carol's", "carol"));
	db.add_viewer(id2, "alice");

	auto visible = db.projects_visible_to("alice", permission::none);
	REQUIRE(visible.size() == 2);

	bool saw_id1 = false, saw_id2 = false;
	for(const auto& p: visible)
	{
		if(p.id == id1)
			saw_id1 = true;
		if(p.id == id2)
			saw_id2 = true;
	}
	CHECK(saw_id1);
	CHECK(saw_id2);
}

TEST_CASE("gantt_db - projects_visible_to stranger sees nothing")
{
	gantt_db db{":memory:"};
	db.create_project(make_project("Alice's", "alice"));
	CHECK(db.projects_visible_to("stranger", permission::none).empty());
}

// ---- can_view ----

TEST_CASE("gantt_db - can_view")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));
	db.add_viewer(pid, "bob");

	CHECK(db.can_view(pid, "anyone", permission::admin)); // admin bypasses all checks
	CHECK(db.can_view(pid, "alice", permission::none));   // owner
	CHECK(db.can_view(pid, "bob", permission::none));     // explicit viewer
	CHECK(!db.can_view(pid, "carol", permission::none));  // stranger
	CHECK(!db.can_view(9999, "alice", permission::none)); // non-existent project
}

// ---- can_edit ----

TEST_CASE("gantt_db - can_edit")
{
	gantt_db  db{":memory:"};
	long long pid = db.create_project(make_project("P", "alice"));

	CHECK(db.can_edit(pid, "anyone", permission::admin));        // admin
	CHECK(db.can_edit(pid, "alice", permission::gantt_write));   // owner with gantt_write
	CHECK(!db.can_edit(pid, "alice", permission::none));         // owner without gantt_write
	CHECK(!db.can_edit(pid, "bob", permission::gantt_write));    // non-owner even with gantt_write
	CHECK(!db.can_edit(9999, "alice", permission::gantt_write)); // non-existent project
}
