#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"

TEST_CASE("permission::flags - empty flags has no permissions")
{
	const auto none = permission::flags{};
	CHECK(!none.has_any(permission::admin));
	CHECK(!none.has_any(permission::post_write));
	CHECK(!none.has_any(permission::org_create));
	CHECK(!none.has_any(permission::manage_users));
	CHECK(none.empty());
}

TEST_CASE("permission::flags - single flag set")
{
	const auto mask = permission::admin;
	CHECK(mask.has_any(permission::admin));
	CHECK(!mask.has_any(permission::post_write));
	CHECK(!mask.has_any(permission::org_create));
	CHECK(!mask.has_any(permission::manage_users));
}

TEST_CASE("permission::flags - multiple flags set via operator|")
{
	const auto mask = permission::post_write | permission::org_create;
	CHECK(!mask.has_any(permission::admin));
	CHECK(mask.has_any(permission::post_write));
	CHECK(mask.has_any(permission::org_create));
	CHECK(!mask.has_any(permission::manage_users));
}

TEST_CASE("permission::flags - operator|= accumulates bits")
{
	permission::flags mask;
	mask |= permission::admin;
	CHECK(mask.has_any(permission::admin));
	CHECK(!mask.has_any(permission::post_write));

	mask |= permission::post_write;
	CHECK(mask.has_any(permission::admin));
	CHECK(mask.has_any(permission::post_write));
	CHECK(!mask.has_any(permission::org_create));
}

TEST_CASE("permission::flags - operator|= is idempotent on already-set bit")
{
	permission::flags mask = permission::admin;
	permission::flags same = mask;
	same |= permission::admin;
	CHECK(mask == same);
}

TEST_CASE("permission::flags - api_token and view_user_history are distinct bits")
{
	const auto mask = permission::api_token | permission::view_user_history;
	CHECK(mask.has_any(permission::api_token));
	CHECK(mask.has_any(permission::view_user_history));
	CHECK(!mask.has_any(permission::admin));
	CHECK(!mask.has_any(permission::manage_users));

	// Each new permission is its own bit, distinct from the existing four.
	CHECK(!permission::api_token.has_any(permission::view_user_history));
	CHECK(!permission::view_user_history.has_any(permission::api_token));
	for(const auto existing: {permission::admin, permission::post_write, permission::org_create, permission::manage_users})
	{
		CHECK(!permission::api_token.has_any(existing));
		CHECK(!permission::view_user_history.has_any(existing));
	}
}
