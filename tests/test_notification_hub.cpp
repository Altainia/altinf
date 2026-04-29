#include <catch2/catch_test_macros.hpp>

#include "widgets/notification_hub.hpp"

// Inject a synchronous dispatcher so tests don't need a live Wt::WServer.
// Each TEST_CASE calls reset() with this lambda to both install it and clear
// any sessions left over from a previous test.
static notification_hub::post_fn_t sync_post()
{
	return [](const std::string&, std::function<void()> fn) { fn(); };
}

TEST_CASE("notification_hub - notify_user fires a registered callback")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.register_session("alice", "sid1", [&count] { ++count; });
	hub.notify_user("alice");

	CHECK(count == 1);
}

TEST_CASE("notification_hub - notify_user is a no-op for unknown username")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	CHECK_NOTHROW(hub.notify_user("nobody"));
}

TEST_CASE("notification_hub - notify_user fires all sessions for the same user")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int a = 0, b = 0;
	hub.register_session("alice", "sid1", [&a] { ++a; });
	hub.register_session("alice", "sid2", [&b] { ++b; });
	hub.notify_user("alice");

	CHECK(a == 1);
	CHECK(b == 1);
}

TEST_CASE("notification_hub - notify_user does not fire other users' callbacks")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int alice_count = 0, bob_count = 0;
	hub.register_session("alice", "sid1", [&alice_count] { ++alice_count; });
	hub.register_session("bob", "sid2", [&bob_count] { ++bob_count; });
	hub.notify_user("alice");

	CHECK(alice_count == 1);
	CHECK(bob_count == 0);
}

TEST_CASE("notification_hub - deregister_session stops that session from firing")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.register_session("alice", "sid1", [&count] { ++count; });
	hub.deregister_session("alice", "sid1");
	hub.notify_user("alice");

	CHECK(count == 0);
}

TEST_CASE("notification_hub - deregister_session removes only the named session")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int a = 0, b = 0;
	hub.register_session("alice", "sid1", [&a] { ++a; });
	hub.register_session("alice", "sid2", [&b] { ++b; });
	hub.deregister_session("alice", "sid1");
	hub.notify_user("alice");

	CHECK(a == 0);
	CHECK(b == 1);
}

TEST_CASE("notification_hub - deregister_session on unknown session is a no-op")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	CHECK_NOTHROW(hub.deregister_session("nobody", "ghost"));
}

TEST_CASE("notification_hub - notify_user can be called multiple times")
{
	auto& hub = notification_hub::instance();
	hub.reset(sync_post());

	int count = 0;
	hub.register_session("alice", "sid1", [&count] { ++count; });
	hub.notify_user("alice");
	hub.notify_user("alice");

	CHECK(count == 2);
}
