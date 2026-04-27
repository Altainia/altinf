#include <catch2/catch_test_macros.hpp>

#include "auth/permission.hpp"

TEST_CASE("has_permission - zero mask has no permissions")
{
	CHECK(!has_permission(0, permission::admin));
	CHECK(!has_permission(0, permission::post_write));
	CHECK(!has_permission(0, permission::gantt_write));
	CHECK(!has_permission(0, permission::manage_users));
}

TEST_CASE("has_permission - single bit set")
{
	const uint64_t mask = static_cast<uint64_t>(permission::admin);
	CHECK(has_permission(mask, permission::admin));
	CHECK(!has_permission(mask, permission::post_write));
	CHECK(!has_permission(mask, permission::gantt_write));
	CHECK(!has_permission(mask, permission::manage_users));
}

TEST_CASE("has_permission - multiple bits set")
{
	uint64_t mask = static_cast<uint64_t>(permission::post_write) |
	                static_cast<uint64_t>(permission::gantt_write);
	CHECK(!has_permission(mask, permission::admin));
	CHECK(has_permission(mask, permission::post_write));
	CHECK(has_permission(mask, permission::gantt_write));
	CHECK(!has_permission(mask, permission::manage_users));
}

TEST_CASE("grant - accumulates bits")
{
	uint64_t mask = 0;
	mask          = grant(mask, permission::admin);
	CHECK(has_permission(mask, permission::admin));
	CHECK(!has_permission(mask, permission::post_write));

	mask = grant(mask, permission::post_write);
	CHECK(has_permission(mask, permission::admin));
	CHECK(has_permission(mask, permission::post_write));
	CHECK(!has_permission(mask, permission::gantt_write));
}

TEST_CASE("grant - idempotent on already-set bit")
{
	uint64_t mask = grant(0, permission::admin);
	uint64_t same = grant(mask, permission::admin);
	CHECK(mask == same);
}
