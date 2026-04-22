#pragma once

#include "session_data.h"
#include "user.h"

#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include <cstdint>
#include <string>

class user_db
{
public:
	explicit user_db(const std::string& db_path);

	bool authenticate(const std::string& username,
	                  const std::string& password,
	                  session_data&      out_session);

	void create_user(const std::string& username,
	                 const std::string& password,
	                 uint64_t           permissions);

	bool has_users();

private:
	Wt::Dbo::backend::Sqlite3 m_sqlite;
	Wt::Dbo::Session          m_dbo;
};
