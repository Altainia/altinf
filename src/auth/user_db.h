#pragma once

#include "session_data.h"
#include "user.h"

#include <Wt/Dbo/Session.h>

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

	// Returns the raw token to present once; stores only its SHA-256 hash.
	std::string create_api_token(const std::string& username);

	bool verify_api_token(const std::string& raw_token, session_data& out_session);

private:
	Wt::Dbo::Session m_dbo;
};
