#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"

TEST_CASE("permission::flags - empty flags has no permissions")
{
	const auto none = permission::flags{};
	CHECK(!none.has_any(permission::admin));
	CHECK(!none.has_any(permission::post_write));
	CHECK(!none.has_any(permission::gantt_write));
	CHECK(!none.has_any(permission::manage_users));
	CHECK(none.empty());
}

TEST_CASE("permission::flags - single flag set")
{
	const auto mask = permission::admin;
	CHECK(mask.has_any(permission::admin));
	CHECK(!mask.has_any(permission::post_write));
	CHECK(!mask.has_any(permission::gantt_write));
	CHECK(!mask.has_any(permission::manage_users));
}

TEST_CASE("permission::flags - multiple flags set via operator|")
{
	const auto mask = permission::post_write | permission::gantt_write;
	CHECK(!mask.has_any(permission::admin));
	CHECK(mask.has_any(permission::post_write));
	CHECK(mask.has_any(permission::gantt_write));
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
	CHECK(!mask.has_any(permission::gantt_write));
}

TEST_CASE("permission::flags - operator|= is idempotent on already-set bit")
{
	permission::flags mask = permission::admin;
	permission::flags same = mask;
	same |= permission::admin;
	CHECK(mask == same);
}
