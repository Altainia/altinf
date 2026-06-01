#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <stdexcept>

#include "db/migrator.hpp"

namespace
{
	// Minimal mapped record used to exercise the engine against a real schema.
	struct widget_record
	{
		int count = 0;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, count, "count");
		}
	};

	// A session over a fresh in-memory SQLite DB with widget_record mapped but no
	// tables created yet — mirrors how the real db wrappers hand off to the engine.
	std::unique_ptr<Wt::Dbo::Session> make_session()
	{
		auto session = std::make_unique<Wt::Dbo::Session>();
		session->setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(":memory:"));
		session->mapClass<widget_record>("widget");
		return session;
	}
} // namespace

TEST_CASE("migrator - fresh DB with no migrations stamps the baseline version")
{
	auto session = make_session();

	db::migrator::run(*session, "test", {});

	CHECK(db::migrator::current_version(*session, "test") == db::migrator::baseline_version);

	// createTables() ran, so the mapped table is usable.
	Wt::Dbo::Transaction t{*session};
	CHECK_NOTHROW(session->execute("INSERT INTO widget (version, count) VALUES (0, 1)"));
}

TEST_CASE("migrator - fresh DB stamps latest and skips deltas already in the model")
{
	auto session = make_session();

	// On a brand-new file createTables() builds the current schema directly, so
	// listed deltas must NOT run — this one throws if it is ever applied.
	db::migrator::migration boom{
	  2, "must not run", [](Wt::Dbo::Session&) { throw std::runtime_error{"delta ran on fresh DB"}; }};

	CHECK_NOTHROW(db::migrator::run(*session, "test", {boom}));
	CHECK(db::migrator::current_version(*session, "test") == 2);
}

TEST_CASE("migrator - delta migration applies to an existing tracked DB")
{
	auto session = make_session();

	// Establish a baseline DB first.
	db::migrator::run(*session, "test", {});
	REQUIRE(db::migrator::current_version(*session, "test") == db::migrator::baseline_version);

	const auto add_col =
	  db::migrator::sql_migration(2, "add extra", "ALTER TABLE widget ADD COLUMN extra integer not null default 0");

	db::migrator::run(*session, "test", {add_col});

	CHECK(db::migrator::current_version(*session, "test") == 2);

	// The new column exists and is writable.
	Wt::Dbo::Transaction t{*session};
	CHECK_NOTHROW(session->execute("INSERT INTO widget (version, count, extra) VALUES (0, 1, 5)"));
}

TEST_CASE("migrator - re-running applied migrations is a no-op")
{
	auto session = make_session();

	const auto add_col =
	  db::migrator::sql_migration(2, "add extra", "ALTER TABLE widget ADD COLUMN extra integer not null default 0");

	db::migrator::run(*session, "test", {});
	db::migrator::run(*session, "test", {add_col});
	REQUIRE(db::migrator::current_version(*session, "test") == 2);

	// Second application must not re-run the ALTER (which would throw: duplicate column).
	CHECK_NOTHROW(db::migrator::run(*session, "test", {add_col}));
	CHECK(db::migrator::current_version(*session, "test") == 2);
}

TEST_CASE("migrator - a failing migration propagates and leaves the version unchanged")
{
	auto session = make_session();

	db::migrator::run(*session, "test", {});
	REQUIRE(db::migrator::current_version(*session, "test") == db::migrator::baseline_version);

	const auto broken =
	  db::migrator::sql_migration(2, "broken", "ALTER TABLE does_not_exist ADD COLUMN x integer");

	CHECK_THROWS(db::migrator::run(*session, "test", {broken}));
	CHECK(db::migrator::current_version(*session, "test") == db::migrator::baseline_version);
}
