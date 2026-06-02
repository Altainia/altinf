#pragma once

#include <Wt/Dbo/Session.h>

#include <string>

// Cross-domain user display resolution. The org and kanban domains store user
// references by username but live in their own Wt::Dbo sessions, which do not
// map the `user` class. Because every domain shares one SQLite file, this helper
// reads the user table by raw SQL so member/assignee/author lists can show a
// display name and flag soft-deleted users without depending on user_db.
namespace user_lookup
{
	struct info
	{
		std::string display_name; // falls back to the username when blank/missing
		bool        deleted{false};
	};

	// Resolve display info for a username. Opens its own (possibly nested)
	// transaction; safe to call from within another transaction.
	info resolve(Wt::Dbo::Session& session, const std::string& username);
}
