#pragma once

#include "session_data.hpp"
#include "user.hpp"

#include <Wt/Dbo/Session.h>

#include <cstdint>
#include <string>
#include <vector>

struct user_entry
{
	std::string username;
	std::string display_name;
	uint64_t    permissions{0};
};

struct api_token_entry
{
	long long   id{0};
	std::string token_hash;
};

class user_db
{
public:
	explicit user_db(const std::string& db_path);

	bool authenticate(const std::string& username,
	                  const std::string& password,
	                  session_data&      out_session);

	void create_user(const std::string& username,
	                 const std::string& password,
	                 uint64_t           permissions,
	                 const std::string& display_name = "");

	bool has_users();

	std::vector<user_entry> list_users();

	bool username_exists(const std::string& username);

	void delete_user(const std::string& username);

	void update_user(const std::string& username,
	                 const std::string& display_name,
	                 uint64_t           permissions);

	void set_password(const std::string& username, const std::string& new_password);

	std::vector<api_token_entry> list_tokens(const std::string& username);

	void delete_token(long long token_id);

	// Returns the raw token to present once; stores only its SHA-256 hash.
	std::string create_api_token(const std::string& username);

	bool verify_api_token(const std::string& raw_token, session_data& out_session);

private:
	Wt::Dbo::Session m_dbo;
};
