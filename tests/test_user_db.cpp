#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <unistd.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "auth/permission.hpp"
#include "auth/user_db.hpp"
#include "auth/user_lookup.hpp"

namespace
{
	// A unique temp file path for a file-backed (non-:memory:) SQLite DB.
	std::string temp_db_path()
	{
		static int n = 0;
		return (std::filesystem::temp_directory_path() /
		        ("altinf_userdb_test_" + std::to_string(::getpid()) + "_" +
		         std::to_string(++n) + ".db"))
		  .string();
	}

	// Build a frozen "v2 auth" schema by hand — the state of the auth domain
	// before any of this feature's deltas — so opening user_db on it exercises the
	// real migration path (not the createTables() shortcut). DDL is the historical
	// v1 baseline plus the v2 google_identity table.
	void make_v2_auth_db(const std::string& path)
	{
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
		Wt::Dbo::Transaction t{s};
		s.execute(
		  "CREATE TABLE \"user\" ("
		  "\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, "
		  "\"username\" text not null, "
		  "\"display_name\" text not null, "
		  "\"password_hash\" text not null, "
		  "\"permissions\" bigint not null)");
		s.execute(
		  "CREATE TABLE \"api_token\" ("
		  "\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, "
		  "\"token_hash\" text not null, "
		  "\"username\" text not null)");
		s.execute(
		  "CREATE TABLE \"session_token\" ("
		  "\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, "
		  "\"token_hash\" text not null, "
		  "\"username\" text not null)");
		s.execute(
		  "CREATE TABLE \"google_identity\" ("
		  "\"id\" integer primary key autoincrement, "
		  "\"version\" integer not null, "
		  "\"username\" text not null, "
		  "\"google_sub\" text not null, "
		  "\"email\" text not null)");
		s.execute(
		  "CREATE TABLE schema_version ("
		  "domain text primary key, version integer not null)");
		s.execute("INSERT INTO schema_version (domain, version) VALUES ('auth', 2)");
		s.execute(
		  "INSERT INTO \"user\" "
		  "(version, username, display_name, password_hash, permissions) "
		  "VALUES (0, 'legacy', 'Legacy User', '', 8)");
		s.execute(
		  "INSERT INTO \"api_token\" (version, token_hash, username) "
		  "VALUES (0, 'deadbeef', 'legacy')");
		t.commit();
	}
} // namespace

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

TEST_CASE("user_db - session_data carries a stable user_id")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none, "Alice");
	db.create_user("bob", "pw", permission::none, "Bob");

	session_data alice_auth;
	REQUIRE(db.authenticate("alice", "pw", alice_auth));
	CHECK(alice_auth.user_id != 0);

	session_data alice_load;
	REQUIRE(db.load_session("alice", alice_load));
	CHECK(alice_load.user_id == alice_auth.user_id);

	session_data bob_load;
	REQUIRE(db.load_session("bob", bob_load));
	CHECK(bob_load.user_id != alice_auth.user_id);
}

TEST_CASE("user_db - user_id_for returns the user's id or nullopt")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto id = db.user_id_for("alice");
	REQUIRE(id);
	CHECK(*id != 0);
	CHECK(!db.user_id_for("nobody"));
}

TEST_CASE("user_db - create_user records a created event")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none, "Alice", /*actor_id=*/0);

	const auto id = db.user_id_for("alice");
	REQUIRE(id);

	const auto hist = db.history_for_user(*id);
	REQUIRE(hist.size() == 1);
	CHECK(hist[0].user_id == *id);
	CHECK(hist[0].event_type == "created");
	CHECK(hist[0].actor_id == 0);
}

TEST_CASE("user_db - display_name_for_id resolves names, system, and unknown")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none, "Alice Smith");
	db.create_user("bob", "pw", permission::none, ""); // no display name
	const auto alice_id = db.user_id_for("alice");
	const auto bob_id   = db.user_id_for("bob");
	REQUIRE(alice_id);
	REQUIRE(bob_id);

	CHECK(db.display_name_for_id(*alice_id) == "Alice Smith");
	CHECK(db.display_name_for_id(*bob_id) == "bob"); // falls back to username
	CHECK(db.display_name_for_id(0) == "system");
	CHECK(db.display_name_for_id(999999) == "(unknown)");
}

TEST_CASE("user_db - history_for_user is empty for a user with no events")
{
	user_db db{":memory:"};
	CHECK(db.history_for_user(424242).empty());
}

TEST_CASE("user_db - upgrades a v2 auth database and gains the audit tables")
{
	const auto path = temp_db_path();
	make_v2_auth_db(path);

	{
		user_db db{path}; // runs v3+ migrations on the existing DB

		// The legacy user survives the upgrade.
		REQUIRE(db.username_exists("legacy"));
		const auto legacy_id = db.user_id_for("legacy");
		REQUIRE(legacy_id);

		// The legacy token survives and gains a backfilled name.
		const auto legacy_tokens = db.list_tokens("legacy");
		REQUIRE(legacy_tokens.size() == 1);
		CHECK(legacy_tokens[0].name == "token-" + std::to_string(legacy_tokens[0].id));

		// The audit tables exist and are usable: legacy has no events yet.
		CHECK(db.history_for_user(*legacy_id).empty());

		// The v6 user_id column exists on the migrated DB: a new token created for
		// the legacy user round-trips through verification (which loads the user by
		// its id, not username).
		const auto   raw = db.create_api_token("legacy", "post-upgrade", *legacy_id);
		session_data tok_session;
		REQUIRE(db.verify_api_token(raw, tok_session));
		CHECK(tok_session.user_id == *legacy_id);
		CHECK(tok_session.username == "legacy");

		db.create_user("fresh", "pw", permission::none, "Fresh", *legacy_id);
		const auto fresh_id = db.user_id_for("fresh");
		REQUIRE(fresh_id);
		const auto hist = db.history_for_user(*fresh_id);
		REQUIRE(hist.size() == 1);
		CHECK(hist[0].event_type == "created");
		CHECK(hist[0].actor_id == *legacy_id);

		// v5 deleted_at column is present: list_users (which filters on it) works
		// and soft-delete hides the row while keeping the username taken.
		REQUIRE(db.list_users().size() == 2); // legacy + fresh
		db.delete_user("fresh", *legacy_id);
		CHECK(db.username_exists("fresh"));
		CHECK(db.list_users().size() == 1);
	}

	std::filesystem::remove(path);
}

TEST_CASE("user_lookup - resolve reads display name and deleted flag across sessions")
{
	const auto path = temp_db_path();
	{
		user_db db{path};
		db.create_user("alice", "pw", permission::none, "Alice Smith");
		db.create_user("bob", "pw", permission::none, ""); // blank display name
		db.delete_user("bob");
	}
	{
		// A fresh session over the same file, with no classes mapped — exactly how
		// the org/kanban domains see the user table.
		Wt::Dbo::Session s;
		s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));

		const auto alice = user_lookup::resolve(s, "alice");
		CHECK(alice.display_name == "Alice Smith");
		CHECK(!alice.deleted);

		const auto bob = user_lookup::resolve(s, "bob");
		CHECK(bob.display_name == "bob"); // blank display name falls back to username
		CHECK(bob.deleted);

		const auto none = user_lookup::resolve(s, "nobody");
		CHECK(none.display_name == "nobody");
		CHECK(!none.deleted);
	}
	std::filesystem::remove(path);
}

TEST_CASE("user_db - delete_user soft-deletes: row kept, login blocked, hidden from list")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none, "Alice");
	const auto id = db.user_id_for("alice");
	REQUIRE(id);

	db.delete_user("alice", /*actor_id=*/*id);

	// The row is retained so its id keeps resolving and the username stays taken.
	CHECK(db.username_exists("alice"));
	CHECK(db.user_id_for("alice") == id);

	// But the user can no longer authenticate.
	session_data out;
	CHECK(!db.authenticate("alice", "pw", out));

	// Hidden from the default list, visible when explicitly including deleted.
	CHECK(db.list_users().empty());
	REQUIRE(db.list_users(/*include_deleted=*/true).size() == 1);

	// The deletion is audited.
	const auto hist = db.history_for_user(*id);
	CHECK(std::ranges::any_of(
	  hist, [](const auto& e) { return e.event_type == "deleted"; }));
}

TEST_CASE("user_db - delete unknown user is a no-op")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.delete_user("nobody");
	CHECK(db.username_exists("alice"));
	session_data out;
	CHECK(db.authenticate("alice", "pw", out));
}

TEST_CASE("user_db - has_password and verify_password")
{
	user_db db{":memory:"};
	db.create_user("alice", "secret", permission::none);
	CHECK(db.has_password("alice"));
	CHECK(db.verify_password("alice", "secret"));
	CHECK(!db.verify_password("alice", "wrong"));
	CHECK(!db.verify_password("nobody", "secret"));
}

TEST_CASE("user_db - set_display_name updates and audits")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none, "Old");
	const auto id = db.user_id_for("alice");
	REQUIRE(id);

	db.set_display_name("alice", "New", *id);
	CHECK(db.list_users().at(0).display_name == "New");

	const auto hist = db.history_for_user(*id);
	REQUIRE(!hist.empty());
	// newest first
	CHECK(hist.front().event_type == "display_name_changed");
	REQUIRE(hist.front().changes.size() == 1);
	CHECK(hist.front().changes[0].old_value == "Old");
	CHECK(hist.front().changes[0].new_value == "New");
}

TEST_CASE("user_db - unset_password only when a Google link exists")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto id = db.user_id_for("alice");
	REQUIRE(id);

	// No Google link: refused, password kept.
	CHECK(!db.unset_password("alice", *id));
	CHECK(db.has_password("alice"));

	db.link_google("alice", "sub-1", "alice@gmail.com");
	CHECK(db.unset_password("alice", *id));
	CHECK(!db.has_password("alice"));

	const auto hist = db.history_for_user(*id);
	CHECK(std::ranges::any_of(
	  hist, [](const auto& e) { return e.event_type == "password_unset"; }));
}

TEST_CASE("user_db - unlink_google only when a password exists")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto id = db.user_id_for("alice");
	REQUIRE(id);
	db.link_google("alice", "sub-1", "alice@gmail.com");

	// Drop the password (allowed, Google remains as a login method).
	REQUIRE(db.unset_password("alice", *id));
	CHECK(!db.has_password("alice"));

	// Now unlinking Google would leave no login method: refused.
	CHECK(!db.unlink_google("alice", *id));
	CHECK(db.google_email_for("alice"));
}

TEST_CASE("user_db - set_password audits a change")
{
	user_db db{":memory:"};
	db.create_user("alice", "old", permission::none);
	const auto id = db.user_id_for("alice");
	REQUIRE(id);
	db.set_password("alice", "new", *id);

	const auto hist = db.history_for_user(*id);
	CHECK(std::ranges::any_of(
	  hist, [](const auto& e) { return e.event_type == "password_changed"; }));
	// The new value is never recorded.
	for(const auto& ev: hist)
	{
		for(const auto& ch: ev.changes)
		{
			CHECK(ch.new_value.find("new") == std::string::npos);
		}
	}
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

	auto token = db.create_api_token("alice", "laptop");
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
	db.create_api_token("alice", "one");
	db.create_api_token("alice", "two");
	CHECK(db.list_tokens("alice").size() == 2);
}

TEST_CASE("user_db - API token stores and lists its name")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.create_api_token("alice", "ci-bot");
	auto tokens = db.list_tokens("alice");
	REQUIRE(tokens.size() == 1);
	CHECK(tokens[0].name == "ci-bot");
}

TEST_CASE("user_db - rename_api_token changes the name")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.create_api_token("alice", "old-name");
	auto tokens = db.list_tokens("alice");
	REQUIRE(tokens.size() == 1);
	db.rename_api_token(tokens[0].id, "new-name");
	auto renamed = db.list_tokens("alice");
	REQUIRE(renamed.size() == 1);
	CHECK(renamed[0].name == "new-name");
}

TEST_CASE("user_db - token create/rename/revoke are audited on the owner")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto alice_id = db.user_id_for("alice");
	REQUIRE(alice_id);

	db.create_api_token("alice", "first", *alice_id);
	const auto tok_id = db.list_tokens("alice").at(0).id;
	db.rename_api_token(tok_id, "renamed", *alice_id);
	db.delete_token(tok_id, *alice_id);

	const auto hist = db.history_for_user(*alice_id);
	// created(user) + token_created + token_renamed + token_revoked
	auto count = [&](const std::string& type) {
		return std::ranges::count(hist, type, &user_event_entry::event_type);
	};
	CHECK(count("token_created") == 1);
	CHECK(count("token_renamed") == 1);
	CHECK(count("token_revoked") == 1);
}

TEST_CASE("user_db - delete_token removes it")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	auto token  = db.create_api_token("alice", "laptop");
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
	CHECK_THROWS(db.create_api_token("nobody", "x"));
}

TEST_CASE("user_db - create_session_token returns 64-char hex token")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto tok = db.create_session_token("alice");
	CHECK(tok.size() == 64);
}

TEST_CASE("user_db - verify_session_token populates session_data")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::admin, "Alice");
	const auto   raw = db.create_session_token("alice");
	session_data out;
	REQUIRE(db.verify_session_token(raw, out));
	CHECK(out.logged_in);
	CHECK(out.username == "alice");
	CHECK(out.display_name == "Alice");
	CHECK(out.permissions == permission::admin);
}

TEST_CASE("user_db - verify_session_token bad token returns false")
{
	user_db      db{":memory:"};
	session_data out;
	CHECK(!db.verify_session_token(
	  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef", out));
	CHECK(!out.logged_in);
}

TEST_CASE("user_db - delete_session_token invalidates it")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto   raw = db.create_session_token("alice");
	session_data out;
	REQUIRE(db.verify_session_token(raw, out));
	db.delete_session_token(raw);
	CHECK(!db.verify_session_token(raw, out));
}

TEST_CASE("user_db - delete_session_token unknown token is a no-op")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.create_session_token("alice");
	db.delete_session_token(
	  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
}

TEST_CASE("user_db - create_session_token unknown user throws")
{
	user_db db{":memory:"};
	CHECK_THROWS(db.create_session_token("nobody"));
}

TEST_CASE("user_db - load_session populates without password")
{
	user_db        db{":memory:"};
	constexpr auto perms = permission::admin | permission::post_write;
	db.create_user("alice", "secret", perms, "Alice");
	session_data out;
	REQUIRE(db.load_session("alice", out));
	CHECK(out.logged_in);
	CHECK(out.username == "alice");
	CHECK(out.display_name == "Alice");
	CHECK(out.permissions == perms);
}

TEST_CASE("user_db - load_session unknown user fails")
{
	user_db      db{":memory:"};
	session_data out;
	CHECK(!db.load_session("nobody", out));
	CHECK(!out.logged_in);
}

TEST_CASE("user_db - link_google then look up by sub and email")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	CHECK(!db.google_email_for("alice"));

	db.link_google("alice", "google-sub-123", "alice@gmail.com");
	REQUIRE(db.google_email_for("alice"));
	CHECK(*db.google_email_for("alice") == "alice@gmail.com");

	REQUIRE(db.username_for_google_sub("google-sub-123"));
	CHECK(*db.username_for_google_sub("google-sub-123") == "alice");
	CHECK(!db.username_for_google_sub("google-sub-999"));
}

TEST_CASE("user_db - link_google replaces an existing link for the user")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.link_google("alice", "sub-old", "old@gmail.com");
	db.link_google("alice", "sub-new", "new@gmail.com");

	CHECK(*db.google_email_for("alice") == "new@gmail.com");
	CHECK(!db.username_for_google_sub("sub-old"));
	CHECK(*db.username_for_google_sub("sub-new") == "alice");
}

TEST_CASE("user_db - unlink_google removes the link")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.link_google("alice", "sub-1", "alice@gmail.com");
	db.unlink_google("alice");
	CHECK(!db.google_email_for("alice"));
	CHECK(!db.username_for_google_sub("sub-1"));
}

TEST_CASE("user_db - delete_user clears any google link")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.link_google("alice", "sub-1", "alice@gmail.com");
	db.delete_user("alice");
	CHECK(!db.username_for_google_sub("sub-1"));
}
