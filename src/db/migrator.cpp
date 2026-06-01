#include "db/migrator.hpp"

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>

namespace db::migrator
{
	namespace
	{
		void ensure_version_table(Wt::Dbo::Session& session)
		{
			Wt::Dbo::Transaction t{session};
			session.execute(
			  "CREATE TABLE IF NOT EXISTS schema_version ("
			  "domain text primary key, version integer not null)");
			t.commit();
		}

		int read_version(Wt::Dbo::Session& session, const std::string& domain)
		{
			Wt::Dbo::Transaction t{session};
			const auto           rows =
			  session.query<int>("select version from schema_version where domain = ?").bind(domain).resultList();
			return rows.empty() ? 0 : *rows.begin();
		}

		void stamp(Wt::Dbo::Session& session, const std::string& domain, int version)
		{
			Wt::Dbo::Transaction t{session};
			session
			  .execute(
			    "INSERT INTO schema_version (domain, version) VALUES (?, ?) "
			    "ON CONFLICT(domain) DO UPDATE SET version = ?")
			  .bind(domain)
			  .bind(version)
			  .bind(version);
			t.commit();
		}
	} // namespace

	migration sql_migration(int version, std::string description, std::string sql)
	{
		return migration{
		  version, std::move(description), [sql = std::move(sql)](Wt::Dbo::Session& session) { session.execute(sql); }};
	}

	void run(Wt::Dbo::Session& session, const std::string& domain, const std::vector<migration>& migrations)
	{
		ensure_version_table(session);

		const int latest  = migrations.empty() ? baseline_version : migrations.back().version;
		int       current = read_version(session, domain);

		if(current == 0)
		{
			// First contact with this domain.
			try
			{
				// Brand-new database: createTables() builds the full current schema
				// directly from the mapped structs, so every listed delta is already
				// present. Stamp to latest and skip the deltas.
				session.createTables();
				stamp(session, domain, latest);
				return;
			}
			catch(const Wt::Dbo::Exception&)
			{
				// Tables exist but were never tracked (e.g. a pre-baseline dev DB):
				// assume the schema sits at the baseline, then apply deltas above it.
				stamp(session, domain, baseline_version);
				current = baseline_version;
			}
		}

		for(const migration& m: migrations)
		{
			if(m.version > current)
			{
				Wt::Dbo::Transaction t{session};
				m.apply(session);
				t.commit();
				stamp(session, domain, m.version);
				current = m.version;
			}
		}
	}

	int current_version(Wt::Dbo::Session& session, const std::string& domain)
	{
		ensure_version_table(session);
		return read_version(session, domain);
	}
} // namespace db::migrator
