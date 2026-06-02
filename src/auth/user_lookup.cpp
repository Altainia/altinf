#include "user_lookup.hpp"

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>

#include <tuple>
#include <unordered_set>

namespace
{
	// The user table lives in the auth domain. In the live app every domain shares
	// one SQLite file, so it is visible to the org/kanban sessions; but the
	// standalone db unit tests open a session over an isolated :memory: database
	// with no user table. Guard reads so they degrade gracefully there.
	bool user_table_exists(Wt::Dbo::Session& session)
	{
		const auto rows =
		  session
		    .query<int>(
		      "select count(*) from sqlite_master "
		      "where type = 'table' and name = 'user'")
		    .resultList();
		return !rows.empty() && *rows.begin() > 0;
	}
} // namespace

namespace user_lookup
{
	info resolve(Wt::Dbo::Session& session, const std::string& username)
	{
		Wt::Dbo::Transaction t{session};
		if(!user_table_exists(session))
		{
			return {.display_name = username, .deleted = false};
		}
		const auto rows =
		  session
		    .query<std::tuple<std::string, std::string>>(
		      "select display_name, deleted_at from \"user\" where username = ?")
		    .bind(username)
		    .resultList();

		if(rows.empty())
		{
			return {.display_name = username, .deleted = false};
		}

		const auto& [display_name, deleted_at] = *rows.begin();
		return {.display_name = display_name.empty() ? username : display_name,
		        .deleted      = !deleted_at.empty()};
	}

	long long user_id_for(Wt::Dbo::Session& session, const std::string& username)
	{
		Wt::Dbo::Transaction t{session};
		if(!user_table_exists(session))
		{
			return 0;
		}
		const auto rows =
		  session.query<long long>("select id from \"user\" where username = ?")
		    .bind(username)
		    .resultList();
		return rows.empty() ? 0 : *rows.begin();
	}

	std::string username_for_id(Wt::Dbo::Session& session, long long user_id)
	{
		if(user_id == 0)
		{
			return {};
		}
		Wt::Dbo::Transaction t{session};
		if(!user_table_exists(session))
		{
			return {};
		}
		const auto rows = session.query<std::string>("select username from \"user\" where id = ?")
		                    .bind(user_id)
		                    .resultList();
		return rows.empty() ? std::string{} : *rows.begin();
	}

	std::unordered_map<long long, std::string>
	  usernames_for_ids(Wt::Dbo::Session& session, const std::vector<long long>& user_ids)
	{
		std::unordered_map<long long, std::string> out;

		// Build a distinct, positive id list and resolve it in a single query.
		// ids are integers, so inlining them is injection-safe.
		std::string                   in_list;
		std::unordered_set<long long> seen;
		for(const auto id: user_ids)
		{
			if(id <= 0 || !seen.insert(id).second)
			{
				continue;
			}
			if(!in_list.empty())
			{
				in_list += ',';
			}
			in_list += std::to_string(id);
		}
		if(in_list.empty())
		{
			return out;
		}

		Wt::Dbo::Transaction t{session};
		if(!user_table_exists(session))
		{
			return out;
		}
		const auto rows = session
		                    .query<std::tuple<long long, std::string>>(
		                      "select id, username from \"user\" where id in (" + in_list + ")")
		                    .resultList();
		for(const auto& [id, name]: rows)
		{
			out.emplace(id, name);
		}
		return out;
	}
}
