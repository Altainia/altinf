#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "org/kanban_db.hpp"
#include "org/kanban_notifications.hpp"
#include "org/org.hpp"
#include "org/org_db.hpp"
#include "seed_users.hpp"

static kanban_task_entry make_task(long long team_id, const std::string& title)
{
	kanban_task_entry e;
	e.team_id    = team_id;
	e.title      = title;
	e.status     = "todo";
	e.sort_order = 0;
	return e;
}

// ---- notify_assignee_added ----

TEST_CASE("notify_assignee_added: task_assigned fires when lead adds member")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "bob", "lead");

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "bob", "lead");

	const auto notifs = odb.notifications_for_user("bob");
	REQUIRE(notifs.size() == 1);
	CHECK(notifs[0].type == "task_assigned");
}

TEST_CASE("notify_assignee_added: no task_assigned when self-assigning")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "alice");

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "alice", "alice");

	CHECK(odb.notifications_for_user("alice").empty());
}

TEST_CASE("notify_assignee_added: coassignee_changed fires to other current assignees")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	kdb.add_member(team_id, "carol");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");
	kdb.add_assignee(task_id, "carol", "lead");

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "carol", "lead");

	const auto alice_notifs = odb.notifications_for_user("alice");
	CHECK(!alice_notifs.empty());
	CHECK(alice_notifs[0].type == "task_coassignee_changed");

	const auto bob_notifs = odb.notifications_for_user("bob");
	CHECK(!bob_notifs.empty());
	CHECK(bob_notifs[0].type == "task_coassignee_changed");

	const auto carol_notifs   = odb.notifications_for_user("carol");
	const bool got_coassignee = std::any_of(carol_notifs.begin(), carol_notifs.end(), [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(!got_coassignee);
}

TEST_CASE("notify_assignee_added: pref disabled suppresses coassignee_changed")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");

	user_org_pref_entry pref;
	pref.username                  = "alice";
	pref.org_id                    = org_id;
	pref.notify_task_available     = true;
	pref.notify_task_unassigned    = true;
	pref.notify_coassignee_changed = false;
	pref.notify_task_abandoned     = true;
	odb.set_user_org_pref(pref);

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "bob", "lead");

	const auto alice_notifs   = odb.notifications_for_user("alice");
	const bool got_coassignee = std::any_of(alice_notifs.begin(), alice_notifs.end(), [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(!got_coassignee);
}

// ---- notify_assignee_removed ----

TEST_CASE("notify_assignee_removed: task_unassigned fires when lead removes member")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto notifs = odb.notifications_for_user("bob");
	REQUIRE(!notifs.empty());
	CHECK(notifs[0].type == "task_unassigned");
}

TEST_CASE("notify_assignee_removed: no task_unassigned on self-remove")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");
	kdb.remove_assignee(task_id, "bob", "bob");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "bob", remaining);

	const auto notifs         = odb.notifications_for_user("bob");
	const bool got_unassigned = std::any_of(notifs.begin(), notifs.end(), [](const notification_entry& n) { return n.type == "task_unassigned"; });
	CHECK(!got_unassigned);
}

TEST_CASE("notify_assignee_removed: task_unassigned pref disabled suppresses it")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	user_org_pref_entry pref;
	pref.username                  = "bob";
	pref.org_id                    = org_id;
	pref.notify_task_available     = true;
	pref.notify_task_unassigned    = false;
	pref.notify_coassignee_changed = true;
	pref.notify_task_abandoned     = true;
	odb.set_user_org_pref(pref);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto notifs         = odb.notifications_for_user("bob");
	const bool got_unassigned = std::any_of(notifs.begin(), notifs.end(), [](const notification_entry& n) { return n.type == "task_unassigned"; });
	CHECK(!got_unassigned);
}

TEST_CASE("notify_assignee_removed: task_available fires when last assignee removed by lead")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs    = odb.notifications_for_user("bob");
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(), [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(got_available);
}

TEST_CASE("notify_assignee_removed: task_abandoned + task_available on self-remove as sole assignee")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "alice");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "alice", remaining);

	const auto bob_notifs    = odb.notifications_for_user("bob");
	const bool got_abandoned = std::any_of(bob_notifs.begin(), bob_notifs.end(), [](const notification_entry& n) { return n.type == "task_abandoned"; });
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(), [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(got_abandoned);
	CHECK(got_available);
}

TEST_CASE("notify_assignee_removed: no task_abandoned when lead removes sole assignee")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs    = odb.notifications_for_user("bob");
	const bool got_abandoned = std::any_of(bob_notifs.begin(), bob_notifs.end(), [](const notification_entry& n) { return n.type == "task_abandoned"; });
	CHECK(!got_abandoned);
}

TEST_CASE("notify_assignee_removed: task_available pref disabled suppresses it")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	user_org_pref_entry pref;
	pref.username                  = "bob";
	pref.org_id                    = org_id;
	pref.notify_task_available     = false;
	pref.notify_task_unassigned    = true;
	pref.notify_coassignee_changed = true;
	pref.notify_task_abandoned     = true;
	odb.set_user_org_pref(pref);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs    = odb.notifications_for_user("bob");
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(), [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(!got_available);
}

TEST_CASE("notify_assignee_removed: coassignee_changed fires to remaining assignees")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob", "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto alice_notifs   = odb.notifications_for_user("alice");
	const bool got_coassignee = std::any_of(alice_notifs.begin(), alice_notifs.end(), [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(got_coassignee);
}

TEST_CASE("notify_assignee_removed: excluded from self-notification")
{
	const auto      seed = seeded_db_path();
	kanban_db       kdb{seed};
	org_db          odb{seed};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto alice_notifs  = odb.notifications_for_user("alice");
	const bool got_available = std::any_of(alice_notifs.begin(), alice_notifs.end(), [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(!got_available);
}
