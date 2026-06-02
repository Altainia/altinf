#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "org/org_db.hpp"
#include "seed_users.hpp"

// ---- organizations ----

TEST_CASE("org_db - v2 migration backfills user_id from username")
{
	const auto path = (std::filesystem::temp_directory_path() /
	                   ("altinf_orgdb_mig_" + std::to_string(::getpid()) + ".db"))
	                    .string();
	std::filesystem::remove(path);

	// Hand-build a pre-v2 org schema (org_member has username, no user_id) plus a
	// minimal user table (owned by the auth domain in the live app) so the
	// backfill subquery can resolve. Stamp org at the baseline.
	{
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
		Wt::Dbo::Transaction t{s};
		s.execute(
		  "CREATE TABLE \"user\" ("
		  "\"id\" integer primary key autoincrement, \"version\" integer not null, "
		  "\"username\" text not null, \"display_name\" text not null, "
		  "\"password_hash\" text not null, \"permissions\" bigint not null, "
		  "\"deleted_at\" text not null default '')");
		s.execute(
		  "INSERT INTO \"user\" (version, username, display_name, password_hash, "
		  "permissions, deleted_at) VALUES (0, 'alice', 'Alice', '', 0, '')");
		s.execute(
		  "CREATE TABLE \"organization\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"name\" text not null, \"is_archived\" integer not null)");
		s.execute(
		  "CREATE TABLE \"org_member\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"org_id\" bigint not null, \"username\" text not null, "
		  "\"is_lead\" integer not null, \"status\" text not null)");
		s.execute(
		  "CREATE TABLE \"notification\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"username\" text not null, \"type\" text not null, "
		  "\"payload\" text not null, \"is_read\" integer not null, \"created_at\" text not null)");
		s.execute(
		  "CREATE TABLE \"user_pref\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"username\" text not null, \"last_org_id\" bigint not null)");
		s.execute(
		  "CREATE TABLE \"user_org_pref\" (\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, \"username\" text not null, \"org_id\" bigint not null, "
		  "\"notify_task_available\" integer not null, \"notify_task_unassigned\" integer not null, "
		  "\"notify_coassignee_changed\" integer not null, \"notify_task_abandoned\" integer not null)");
		s.execute("CREATE TABLE schema_version (domain text primary key, version integer not null)");
		s.execute("INSERT INTO schema_version (domain, version) VALUES ('org', 1)");
		s.execute(
		  "INSERT INTO \"org_member\" (version, org_id, username, is_lead, status) "
		  "VALUES (0, 1, 'alice', 1, 'active')");
		t.commit();
	}

	// Constructing org_db runs the org v2 migration (ALTER + backfill).
	{
		org_db db{path};
	}

	// The org_member row now carries alice's user id.
	{
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
		Wt::Dbo::Transaction t{s};
		const auto           user_id = s.query<long long>("select id from \"user\" where username='alice'")
		                       .resultValue();
		// username was dropped by the v3 contract migration; the row is keyed by id.
		const auto member_user_id =
		  s.query<long long>("select user_id from \"org_member\" where org_id=1").resultValue();
		CHECK(member_user_id == user_id);
	}

	std::filesystem::remove(path);
}

TEST_CASE("org_db - create_org makes creator an active lead")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Acme", "alice");
	CHECK(oid > 0);
	CHECK(db.is_org_member(oid, "alice"));
	CHECK(db.is_org_lead(oid, "alice"));
}

TEST_CASE("org_db - find_org")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Widgets Inc", "alice");
	const auto      org = db.find_org(oid);
	REQUIRE(org.has_value());
	CHECK(org->name == "Widgets Inc");
}

TEST_CASE("org_db - find_org missing returns nullopt")
{
	org_db db{seeded_db_path()};
	CHECK(!db.find_org(9999).has_value());
}

TEST_CASE("org_db - all_orgs")
{
	org_db db{seeded_db_path()};
	db.create_org("A", "alice");
	db.create_org("B", "bob");
	CHECK(db.all_orgs().size() == 2);
}

// ---- membership / invite ----

TEST_CASE("org_db - invite and accept")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	CHECK(!db.is_org_member(oid, "bob")); // still pending
	db.accept_invite(oid, "bob");
	CHECK(db.is_org_member(oid, "bob"));
	CHECK(!db.is_org_lead(oid, "bob"));
}

TEST_CASE("org_db - invite as lead, then accept")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", true);
	db.accept_invite(oid, "bob");
	CHECK(db.is_org_lead(oid, "bob"));
}

TEST_CASE("org_db - decline invite")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.decline_invite(oid, "bob");
	CHECK(!db.is_org_member(oid, "bob"));
}

TEST_CASE("org_db - re-invite after decline resets to pending")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.decline_invite(oid, "bob");
	db.invite_to_org(oid, "bob", false);  // re-invite
	CHECK(!db.is_org_member(oid, "bob")); // still pending, not yet accepted
	db.accept_invite(oid, "bob");
	CHECK(db.is_org_member(oid, "bob"));
}

TEST_CASE("org_db - invite already-active member is no-op")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "alice", false); // alice is already active
	CHECK(db.is_org_lead(oid, "alice"));   // status unchanged
}

TEST_CASE("org_db - orgs_for_user returns only active memberships")
{
	org_db          db{seeded_db_path()};
	const long long oid1 = db.create_org("Org1", "alice");
	const long long oid2 = db.create_org("Org2", "alice");
	db.invite_to_org(oid2, "bob", false); // pending — should not appear
	(void)oid1;
	const auto orgs = db.orgs_for_user("alice");
	CHECK(orgs.size() == 2);
	CHECK(db.orgs_for_user("bob").empty());
}

// ---- lead invariant ----

TEST_CASE("org_db - cannot remove last lead")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	const bool      ok  = db.remove_org_member(oid, "alice");
	CHECK(!ok);
	CHECK(db.is_org_member(oid, "alice")); // still there
}

TEST_CASE("org_db - can remove lead when another exists")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", true);
	db.accept_invite(oid, "bob");
	CHECK(db.remove_org_member(oid, "alice"));
	CHECK(!db.is_org_member(oid, "alice"));
	CHECK(db.is_org_lead(oid, "bob")); // bob still a lead
}

TEST_CASE("org_db - cannot demote last lead")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	const bool      ok  = db.set_org_lead(oid, "alice", false);
	CHECK(!ok);
	CHECK(db.is_org_lead(oid, "alice"));
}

TEST_CASE("org_db - promote then demote lead")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.accept_invite(oid, "bob");
	CHECK(db.set_org_lead(oid, "bob", true));
	CHECK(db.is_org_lead(oid, "bob"));
	CHECK(db.set_org_lead(oid, "bob", false)); // alice still a lead
	CHECK(!db.is_org_lead(oid, "bob"));
}

// ---- org_members / org_pending ----

TEST_CASE("org_db - org_members returns only active")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false); // pending
	const auto members = db.org_members(oid);
	REQUIRE(members.size() == 1);
	CHECK(members[0].username == "alice");
}

TEST_CASE("org_db - org_pending returns only pending")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.invite_to_org(oid, "carol", true);
	const auto pending = db.org_pending(oid);
	CHECK(pending.size() == 2);
}

// ---- notifications ----

TEST_CASE("org_db - push and count unread notifications")
{
	org_db db{seeded_db_path()};
	db.push_notification("alice", "org_invite", "{\"org_id\":1,\"org_name\":\"X\"}");
	db.push_notification("alice", "task_assigned", "{}");
	CHECK(db.unread_count("alice") == 2);
	CHECK(db.unread_count("bob") == 0);
}

TEST_CASE("org_db - mark_read decrements unread count")
{
	org_db db{seeded_db_path()};
	db.push_notification("alice", "org_invite", "{}");
	const auto notifs = db.notifications_for_user("alice");
	REQUIRE(notifs.size() == 1);
	db.mark_read(notifs[0].id);
	CHECK(db.unread_count("alice") == 0);
}

TEST_CASE("org_db - notifications_for_user ordered newest first")
{
	org_db db{seeded_db_path()};
	db.push_notification("alice", "org_invite", "{\"n\":1}");
	db.push_notification("alice", "org_invite", "{\"n\":2}");
	const auto notifs = db.notifications_for_user("alice");
	REQUIRE(notifs.size() == 2);
	// Newest first: id of second push is larger.
	CHECK(notifs[0].id > notifs[1].id);
}

TEST_CASE("org_db - invite_to_org creates a notification for the invitee")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Acme", "alice");
	CHECK(db.unread_count("bob") == 0);
	db.invite_to_org(oid, "bob", false);
	CHECK(db.unread_count("bob") == 1);
	const auto notifs = db.notifications_for_user("bob");
	REQUIRE(notifs.size() == 1);
	CHECK(notifs[0].type == "org_invite");
}

// ---- user preferences ----

TEST_CASE("org_db - set and get last_org")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	CHECK(!db.get_last_org("alice").has_value());
	db.set_last_org("alice", oid);
	const auto last = db.get_last_org("alice");
	REQUIRE(last.has_value());
	CHECK(*last == oid);
}

TEST_CASE("org_db - set_last_org overwrites previous value")
{
	org_db          db{seeded_db_path()};
	const long long oid1 = db.create_org("X", "alice");
	const long long oid2 = db.create_org("Y", "alice");
	db.set_last_org("alice", oid1);
	db.set_last_org("alice", oid2);
	CHECK(*db.get_last_org("alice") == oid2);
}

// ---- remove_user_from_all_orgs ----

TEST_CASE("org_db - remove_user_from_all_orgs clears all memberships")
{
	org_db          db{seeded_db_path()};
	const long long oid1 = db.create_org("A", "alice");
	const long long oid2 = db.create_org("B", "alice");
	db.invite_to_org(oid1, "bob", false);
	db.accept_invite(oid1, "bob");
	db.invite_to_org(oid2, "bob", false);
	db.accept_invite(oid2, "bob");
	REQUIRE(db.orgs_for_user("bob").size() == 2);

	db.remove_user_from_all_orgs("bob");

	CHECK(db.orgs_for_user("bob").empty());
	CHECK(!db.is_org_member(oid1, "bob"));
	CHECK(!db.is_org_member(oid2, "bob"));
}

TEST_CASE("org_db - remove_user_from_all_orgs clears notifications")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("A", "alice");
	db.invite_to_org(oid, "bob", false); // creates one notification
	db.push_notification("bob", "task_assigned", "{}");
	REQUIRE(db.unread_count("bob") == 2);

	db.remove_user_from_all_orgs("bob");

	CHECK(db.unread_count("bob") == 0);
	CHECK(db.notifications_for_user("bob").empty());
}

TEST_CASE("org_db - remove_user_from_all_orgs does not affect other users")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("A", "alice");
	db.invite_to_org(oid, "bob", false);
	db.accept_invite(oid, "bob");
	db.push_notification("alice", "task_assigned", "{}");

	db.remove_user_from_all_orgs("bob");

	// alice's membership and notification survive
	CHECK(db.is_org_member(oid, "alice"));
	CHECK(db.unread_count("alice") == 1);
}

TEST_CASE("org_db - remove_user_from_all_orgs also removes pending invites")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("A", "alice");
	db.invite_to_org(oid, "bob", false);
	// bob has not accepted; still pending
	REQUIRE(db.org_pending(oid).size() == 1);

	db.remove_user_from_all_orgs("bob");

	CHECK(db.org_pending(oid).empty());
}

// ---- rescind_invite_notification ----

TEST_CASE("org_db - rescind_invite_notification updates payload and keeps unread")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Acme", "alice");
	db.invite_to_org(oid, "bob", false);
	REQUIRE(db.unread_count("bob") == 1);

	db.rescind_invite_notification(oid, "bob");

	const auto notifs = db.notifications_for_user("bob");
	REQUIRE(notifs.size() == 1);
	CHECK(!notifs[0].is_read);
	CHECK(json_long(notifs[0].payload, "rescinded") == 1);
	CHECK(db.unread_count("bob") == 1); // still counts as unread
}

TEST_CASE("org_db - rescind_invite_notification is no-op if notification already read")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Acme", "alice");
	db.invite_to_org(oid, "bob", false);
	const auto before = db.notifications_for_user("bob");
	REQUIRE(before.size() == 1);
	db.mark_read(before[0].id);

	db.rescind_invite_notification(oid, "bob");

	// Payload must not have been rewritten since notification was already read.
	const auto after = db.notifications_for_user("bob");
	CHECK(json_long(after[0].payload, "rescinded") == 0);
}

TEST_CASE("org_db - rescind_invite_notification does not affect other users")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("Acme", "alice");
	db.invite_to_org(oid, "bob", false);
	db.invite_to_org(oid, "carol", false);

	db.rescind_invite_notification(oid, "bob");

	const auto carol_notifs = db.notifications_for_user("carol");
	REQUIRE(carol_notifs.size() == 1);
	CHECK(json_long(carol_notifs[0].payload, "rescinded") == 0);
}

TEST_CASE("org_db - rescind_invite_notification on nonexistent notification is a no-op")
{
	org_db          db{seeded_db_path()};
	const long long oid = db.create_org("X", "alice");
	// No invite sent to bob — should not throw.
	CHECK_NOTHROW(db.rescind_invite_notification(oid, "bob"));
}

// ---- archive ----

TEST_CASE("org_db - archive_org hides org from all_orgs")
{
	org_db     db{seeded_db_path()};
	const auto id = db.create_org("Acme", "alice");
	db.archive_org(id, "alice");
	CHECK(db.all_orgs().empty());
	const auto archived = db.archived_orgs();
	REQUIRE(archived.size() == 1);
	CHECK(archived[0].name == "Acme");
	CHECK(archived[0].is_archived);
}

TEST_CASE("org_db - archive_org hides org from orgs_for_user")
{
	org_db     db{seeded_db_path()};
	const auto id = db.create_org("Acme", "alice");
	db.accept_invite(id, "alice");
	db.archive_org(id, "alice");
	CHECK(db.orgs_for_user("alice").empty());
}

// ---- user_org_pref ----

TEST_CASE("org_db - get_user_org_pref returns defaults when no row")
{
	org_db     db{seeded_db_path()};
	const auto p = db.get_user_org_pref("alice", 1);
	CHECK(p.notify_task_available);
	CHECK(p.notify_task_unassigned);
	CHECK(p.notify_coassignee_changed);
	CHECK(p.notify_task_abandoned);
}

TEST_CASE("org_db - set_user_org_pref upserts and round-trips")
{
	org_db              db{seeded_db_path()};
	user_org_pref_entry p;
	p.username                  = "alice";
	p.org_id                    = 1;
	p.notify_task_available     = false;
	p.notify_task_unassigned    = true;
	p.notify_coassignee_changed = true;
	p.notify_task_abandoned     = false;
	db.set_user_org_pref(p);

	const auto back = db.get_user_org_pref("alice", 1);
	CHECK(!back.notify_task_available);
	CHECK(back.notify_task_unassigned);
	CHECK(!back.notify_task_abandoned);
}

TEST_CASE("org_db - set_user_org_pref updates existing row")
{
	org_db              db{seeded_db_path()};
	user_org_pref_entry p;
	p.username                  = "alice";
	p.org_id                    = 1;
	p.notify_task_available     = false;
	p.notify_task_unassigned    = true;
	p.notify_coassignee_changed = true;
	p.notify_task_abandoned     = true;
	db.set_user_org_pref(p);

	p.notify_task_available = true; // change it
	db.set_user_org_pref(p);

	CHECK(db.get_user_org_pref("alice", 1).notify_task_available);
}
