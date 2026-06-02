#include "user_lookup.hpp"

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>

#include <tuple>

namespace user_lookup
{
	info resolve(Wt::Dbo::Session& session, const std::string& username)
	{
		Wt::Dbo::Transaction t{session};
		const auto           rows =
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
}
