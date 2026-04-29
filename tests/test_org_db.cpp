#include <catch2/catch_test_macros.hpp>

#include "org/org_db.hpp"

// ---- organisations ----

TEST_CASE("org_db - create_org makes creator an active lead")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("Acme", "alice");
	CHECK(oid > 0);
	CHECK(db.is_org_member(oid, "alice"));
	CHECK(db.is_org_lead(oid, "alice"));
}

TEST_CASE("org_db - find_org")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("Widgets Inc", "alice");
	const auto      org = db.find_org(oid);
	REQUIRE(org.has_value());
	CHECK(org->name == "Widgets Inc");
}

TEST_CASE("org_db - find_org missing returns nullopt")
{
	org_db db{":memory:"};
	CHECK(!db.find_org(9999).has_value());
}

TEST_CASE("org_db - all_orgs")
{
	org_db db{":memory:"};
	db.create_org("A", "alice");
	db.create_org("B", "bob");
	CHECK(db.all_orgs().size() == 2);
}

// ---- membership / invite ----

TEST_CASE("org_db - invite and accept")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	CHECK(!db.is_org_member(oid, "bob")); // still pending
	db.accept_invite(oid, "bob");
	CHECK(db.is_org_member(oid, "bob"));
	CHECK(!db.is_org_lead(oid, "bob"));
}

TEST_CASE("org_db - invite as lead, then accept")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", true);
	db.accept_invite(oid, "bob");
	CHECK(db.is_org_lead(oid, "bob"));
}

TEST_CASE("org_db - decline invite")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.decline_invite(oid, "bob");
	CHECK(!db.is_org_member(oid, "bob"));
}

TEST_CASE("org_db - re-invite after decline resets to pending")
{
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "alice", false); // alice is already active
	CHECK(db.is_org_lead(oid, "alice"));   // status unchanged
}

TEST_CASE("org_db - orgs_for_user returns only active memberships")
{
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	const bool      ok  = db.remove_org_member(oid, "alice");
	CHECK(!ok);
	CHECK(db.is_org_member(oid, "alice")); // still there
}

TEST_CASE("org_db - can remove lead when another exists")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", true);
	db.accept_invite(oid, "bob");
	CHECK(db.remove_org_member(oid, "alice"));
	CHECK(!db.is_org_member(oid, "alice"));
	CHECK(db.is_org_lead(oid, "bob")); // bob still a lead
}

TEST_CASE("org_db - cannot demote last lead")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	const bool      ok  = db.set_org_lead(oid, "alice", false);
	CHECK(!ok);
	CHECK(db.is_org_lead(oid, "alice"));
}

TEST_CASE("org_db - promote then demote lead")
{
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false); // pending
	const auto members = db.org_members(oid);
	REQUIRE(members.size() == 1);
	CHECK(members[0].username == "alice");
}

TEST_CASE("org_db - org_pending returns only pending")
{
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	db.invite_to_org(oid, "bob", false);
	db.invite_to_org(oid, "carol", true);
	const auto pending = db.org_pending(oid);
	CHECK(pending.size() == 2);
}

// ---- notifications ----

TEST_CASE("org_db - push and count unread notifications")
{
	org_db db{":memory:"};
	db.push_notification("alice", "org_invite", "{\"org_id\":1,\"org_name\":\"X\"}");
	db.push_notification("alice", "task_assigned", "{}");
	CHECK(db.unread_count("alice") == 2);
	CHECK(db.unread_count("bob") == 0);
}

TEST_CASE("org_db - mark_read decrements unread count")
{
	org_db db{":memory:"};
	db.push_notification("alice", "org_invite", "{}");
	const auto notifs = db.notifications_for_user("alice");
	REQUIRE(notifs.size() == 1);
	db.mark_read(notifs[0].id);
	CHECK(db.unread_count("alice") == 0);
}

TEST_CASE("org_db - notifications_for_user ordered newest first")
{
	org_db db{":memory:"};
	db.push_notification("alice", "org_invite", "{\"n\":1}");
	db.push_notification("alice", "org_invite", "{\"n\":2}");
	const auto notifs = db.notifications_for_user("alice");
	REQUIRE(notifs.size() == 2);
	// Newest first: id of second push is larger.
	CHECK(notifs[0].id > notifs[1].id);
}

TEST_CASE("org_db - invite_to_org creates a notification for the invitee")
{
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
	const long long oid = db.create_org("X", "alice");
	CHECK(!db.get_last_org("alice").has_value());
	db.set_last_org("alice", oid);
	const auto last = db.get_last_org("alice");
	REQUIRE(last.has_value());
	CHECK(*last == oid);
}

TEST_CASE("org_db - set_last_org overwrites previous value")
{
	org_db          db{":memory:"};
	const long long oid1 = db.create_org("X", "alice");
	const long long oid2 = db.create_org("Y", "alice");
	db.set_last_org("alice", oid1);
	db.set_last_org("alice", oid2);
	CHECK(*db.get_last_org("alice") == oid2);
}

// ---- remove_user_from_all_orgs ----

TEST_CASE("org_db - remove_user_from_all_orgs clears all memberships")
{
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
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
	org_db          db{":memory:"};
	const long long oid = db.create_org("A", "alice");
	db.invite_to_org(oid, "bob", false);
	// bob has not accepted; still pending
	REQUIRE(db.org_pending(oid).size() == 1);

	db.remove_user_from_all_orgs("bob");

	CHECK(db.org_pending(oid).empty());
}
