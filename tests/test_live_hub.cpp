#include <catch2/catch_test_macros.hpp>

#include "widgets/live_hub.hpp"

static live_hub::post_fn_t sync_post()
{
	return [](const std::string&, std::function<void()> fn) { fn(); };
}

TEST_CASE("live_hub - broadcast fires a subscribed callback")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.subscribe("org:5", "sid1", [&count] { ++count; });
	hub.broadcast("org:5");

	CHECK(count == 1);
}

TEST_CASE("live_hub - broadcast is a no-op for unknown channel")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	CHECK_NOTHROW(hub.broadcast("org:999"));
}

TEST_CASE("live_hub - broadcast fires all sessions on the same channel")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int a = 0, b = 0;
	hub.subscribe("org:5", "sid1", [&a] { ++a; });
	hub.subscribe("org:5", "sid2", [&b] { ++b; });
	hub.broadcast("org:5");

	CHECK(a == 1);
	CHECK(b == 1);
}

TEST_CASE("live_hub - broadcast does not fire other channels")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int org5 = 0, org6 = 0;
	hub.subscribe("org:5", "sid1", [&org5] { ++org5; });
	hub.subscribe("org:6", "sid2", [&org6] { ++org6; });
	hub.broadcast("org:5");

	CHECK(org5 == 1);
	CHECK(org6 == 0);
}

TEST_CASE("live_hub - broadcast does not fire different channel types")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int user_count = 0, org_count = 0;
	hub.subscribe("user:alice", "sid1", [&user_count] { ++user_count; });
	hub.subscribe("org:5", "sid2", [&org_count] { ++org_count; });
	hub.broadcast("org:5");

	CHECK(user_count == 0);
	CHECK(org_count == 1);
}

TEST_CASE("live_hub - unsubscribe stops that session from firing")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.subscribe("org:5", "sid1", [&count] { ++count; });
	hub.unsubscribe("org:5", "sid1");
	hub.broadcast("org:5");

	CHECK(count == 0);
}

TEST_CASE("live_hub - unsubscribe removes only the named session")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int a = 0, b = 0;
	hub.subscribe("org:5", "sid1", [&a] { ++a; });
	hub.subscribe("org:5", "sid2", [&b] { ++b; });
	hub.unsubscribe("org:5", "sid1");
	hub.broadcast("org:5");

	CHECK(a == 0);
	CHECK(b == 1);
}

TEST_CASE("live_hub - unsubscribe on unknown session is a no-op")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	CHECK_NOTHROW(hub.unsubscribe("org:999", "ghost"));
}

TEST_CASE("live_hub - broadcast can be called multiple times")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.subscribe("org:5", "sid1", [&count] { ++count; });
	hub.broadcast("org:5");
	hub.broadcast("org:5");

	CHECK(count == 2);
}

TEST_CASE("live_hub - duplicate subscribe fires callback twice per broadcast")
{
	auto& hub = live_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.subscribe("org:5", "sid1", [&count] { ++count; });
	hub.subscribe("org:5", "sid1", [&count] { ++count; });
	hub.broadcast("org:5");

	CHECK(count == 2);
}
