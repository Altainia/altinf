#pragma once

#include <Wt/Dbo/Session.h>

#include <functional>
#include <string>
#include <vector>

// Schema migration engine.
//
// Fresh databases get their full current schema from Wt::Dbo::createTables(),
// which is built directly from the mapped persist() structs. Numbered
// migrations carry only the deltas needed to evolve an already-existing
// database forward. Per-domain progress is tracked in a shared schema_version
// table, so each db wrapper owns its own ordered migration list.
namespace db::migrator
{
	// The schema version representing the model as of the migration system's
	// introduction. Post-baseline deltas are numbered 2, 3, ...
	inline constexpr int baseline_version = 1;

	struct migration
	{
		int                                    version;     // strictly increasing, > baseline_version
		std::string                            description; // human-readable summary
		std::function<void(Wt::Dbo::Session&)> apply;       // runs the delta (SQL or a rebuild)
	};

	// Convenience for the common single-statement migration.
	migration sql_migration(int version, std::string description, std::string sql);

	// Bring `domain`'s schema up to date on `session`'s connection. The caller has
	// already mapped its classes. Applies pending migrations in version order,
	// each in its own transaction; errors propagate (the transaction rolls back).
	void run(Wt::Dbo::Session& session, const std::string& domain, const std::vector<migration>& migrations);

	// Current tracked version for `domain`, or 0 if it has never been stamped.
	int current_version(Wt::Dbo::Session& session, const std::string& domain);
} // namespace db::migrator
