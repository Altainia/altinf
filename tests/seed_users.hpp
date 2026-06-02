#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <unistd.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Test helper for the org/kanban db layers after the user-id contract phase.
//
// Those layers store user references by user_id and resolve username <-> id
// through the auth domain's `user` table, which in the live app shares the same
// SQLite file. The standalone unit DBs therefore need that table present and
// seeded. This returns a fresh temp-file DB path with a `user` table containing
// the given usernames (each gets a distinct id). org_db/kanban_db constructed on
// the path create their own tables alongside it and resolve names against it.
inline std::string seeded_db_path(
  const std::vector<std::string>& usernames = {"alice", "bob", "carol", "creator", "lead"})
{
	static int n    = 0;
	const auto path = (std::filesystem::temp_directory_path() /
	                   ("altinf_seed_" + std::to_string(::getpid()) + "_" + std::to_string(++n) +
	                    ".db"))
	                    .string();
	std::filesystem::remove(path);

	Wt::Dbo::Session s;
	s.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(path));
	Wt::Dbo::Transaction t{s};
	s.execute(
	  "CREATE TABLE \"user\" ("
	  "\"id\" integer primary key autoincrement, \"version\" integer not null, "
	  "\"username\" text not null, \"display_name\" text not null, "
	  "\"password_hash\" text not null, \"permissions\" bigint not null, "
	  "\"deleted_at\" text not null default '')");
	for(const auto& u: usernames)
	{
		s.execute(
		   "INSERT INTO \"user\" "
		   "(version, username, display_name, password_hash, permissions, deleted_at) "
		   "VALUES (0, ?, '', '', 0, '')")
		  .bind(u);
	}
	t.commit();
	return path;
}
