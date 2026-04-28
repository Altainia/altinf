#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"
#include "auth/user_db.hpp"

// bcrypt cost 12 (~300 ms/call) — each TEST_CASE uses its own DB to stay isolated,
// but the total call count is kept low to avoid a painfully slow suite.

TEST_CASE("user_db - has_users on empty db")
{
	user_db db{":memory:"};
	CHECK(!db.has_users());
}

TEST_CASE("user_db - create user and basic queries")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw1", permission::none, "Alice Smith");
	db.create_user("bob", "pw2", permission::admin | permission::post_write, "");

	CHECK(db.has_users());
	CHECK(db.username_exists("alice"));
	CHECK(db.username_exists("bob"));
	CHECK(!db.username_exists("carol"));

	auto users = db.list_users();
	REQUIRE(users.size() == 2);
}

TEST_CASE("user_db - list_users reflects display_name and permissions")
{
	user_db        db{":memory:"};
	constexpr auto perms = permission::admin | permission::post_write | permission::org_create;
	db.create_user("alice", "pw", perms, "Alice");
	auto users = db.list_users();
	REQUIRE(users.size() == 1);
	CHECK(users[0].username == "alice");
	CHECK(users[0].display_name == "Alice");
	CHECK(users[0].permissions == perms);
}

TEST_CASE("user_db - authenticate correct password")
{
	user_db db{":memory:"};
	db.create_user("alice", "secret", permission::none);
	session_data out;
	REQUIRE(db.authenticate("alice", "secret", out));
	CHECK(out.logged_in);
	CHECK(out.username == "alice");
}

TEST_CASE("user_db - authenticate wrong password")
{
	user_db db{":memory:"};
	db.create_user("alice", "secret", permission::none);
	session_data out;
	CHECK(!db.authenticate("alice", "wrong", out));
	CHECK(!out.logged_in);
}

TEST_CASE("user_db - authenticate unknown user")
{
	user_db      db{":memory:"};
	session_data out;
	CHECK(!db.authenticate("nobody", "pw", out));
	CHECK(!out.logged_in);
}

TEST_CASE("user_db - authenticate populates session_data")
{
	user_db        db{":memory:"};
	constexpr auto perms = permission::admin | permission::org_create;
	db.create_user("alice", "pw", perms, "Alice Smith");
	session_data out;
	REQUIRE(db.authenticate("alice", "pw", out));
	CHECK(out.username == "alice");
	CHECK(out.display_name == "Alice Smith");
	CHECK(out.permissions == perms);
}

TEST_CASE("user_db - delete user")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.delete_user("alice");
	CHECK(!db.username_exists("alice"));
	CHECK(!db.has_users());
}

TEST_CASE("user_db - delete unknown user is a no-op")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.delete_user("nobody");
	CHECK(db.username_exists("alice"));
}

TEST_CASE("user_db - update_user changes display_name and permissions")
{
	user_db        db{":memory:"};
	constexpr auto perms = permission::admin | permission::post_write | permission::org_create;
	db.create_user("alice", "pw", permission::none, "Old Name");
	db.update_user("alice", "New Name", perms);
	auto users = db.list_users();
	REQUIRE(users.size() == 1);
	CHECK(users[0].display_name == "New Name");
	CHECK(users[0].permissions == perms);
}

TEST_CASE("user_db - set_password invalidates old password")
{
	user_db db{":memory:"};
	db.create_user("alice", "old", permission::none);
	db.set_password("alice", "new");
	session_data out;
	CHECK(!db.authenticate("alice", "old", out));
	CHECK(db.authenticate("alice", "new", out));
}

TEST_CASE("user_db - API token create and verify")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);

	auto token = db.create_api_token("alice");
	CHECK(!token.empty());
	CHECK(token.size() == 64); // 32 bytes as hex

	session_data out;
	REQUIRE(db.verify_api_token(token, out));
	CHECK(out.logged_in);
	CHECK(out.username == "alice");
}

TEST_CASE("user_db - API token appears in list_tokens")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.create_api_token("alice");
	db.create_api_token("alice");
	CHECK(db.list_tokens("alice").size() == 2);
}

TEST_CASE("user_db - delete_token removes it")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	auto token  = db.create_api_token("alice");
	auto tokens = db.list_tokens("alice");
	REQUIRE(tokens.size() == 1);
	db.delete_token(tokens[0].id);
	CHECK(db.list_tokens("alice").empty());
	session_data out;
	CHECK(!db.verify_api_token(token, out));
}

TEST_CASE("user_db - verify_api_token bad token returns false")
{
	user_db      db{":memory:"};
	session_data out;
	CHECK(!db.verify_api_token("deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
	                           out));
}

TEST_CASE("user_db - create_api_token unknown user throws")
{
	user_db db{":memory:"};
	CHECK_THROWS(db.create_api_token("nobody"));
}
