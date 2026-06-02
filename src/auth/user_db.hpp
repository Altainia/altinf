#pragma once

#include <Wt/Dbo/Session.h>

#include <optional>
#include <string>
#include <vector>

#include "permission.hpp"
#include "session_data.hpp"
#include "user.hpp"
#include "user_event.hpp"

struct user_entry
{
	std::string       username;
	std::string       display_name;
	permission::flags permissions;
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

	// Populate a session for an already-authenticated user (e.g. after a Google
	// sign-in), bypassing the password check. Returns false if the user is gone.
	bool load_session(const std::string& username, session_data& out_session);

	void create_user(const std::string& username,
	                 const std::string& password,
	                 permission::flags  permissions,
	                 const std::string& display_name = "",
	                 long long          actor_id     = 0);

	bool has_users();

	std::vector<user_entry> list_users();

	bool username_exists(const std::string& username);

	// The user's surrogate id, or nullopt if no such user (including soft-deleted
	// users, whose row is retained).
	std::optional<long long> user_id_for(const std::string& username);

	// Full audit history for a user, newest first.
	std::vector<user_event_entry> history_for_user(long long user_id);

	void delete_user(const std::string& username);

	void update_user(const std::string& username,
	                 const std::string& display_name,
	                 permission::flags  permissions);

	void set_password(const std::string& username, const std::string& new_password);

	std::vector<api_token_entry> list_tokens(const std::string& username);

	void delete_token(long long token_id);

	// Returns the raw token to present once; stores only its SHA-256 hash.
	std::string create_api_token(const std::string& username);

	bool verify_api_token(const std::string& raw_token, session_data& out_session);

	// Returns the raw token to present once; stores only its SHA-256 hash.
	std::string create_session_token(const std::string& username);

	bool verify_session_token(const std::string& raw_token, session_data& out_session);

	void delete_session_token(const std::string& raw_token);

	// ── Google account linkage ────────────────────────────────────────────────

	// Link (or re-link) a user's account to a Google identity. Replaces any
	// existing link for this user.
	void link_google(const std::string& username,
	                 const std::string& google_sub,
	                 const std::string& email);

	void unlink_google(const std::string& username);

	// The linked Google email for a user, if any (for display).
	std::optional<std::string> google_email_for(const std::string& username);

	// The username linked to a Google subject id, if any (sign-in lookup).
	std::optional<std::string> username_for_google_sub(const std::string& google_sub);

private:
	// Append one audit event (plus its field changes) for a user. Must be called
	// inside an open transaction; mirrors kanban_db::record_event.
	void record_user_event(long long                                   user_id,
	                       long long                                   actor_id,
	                       const std::string&                          event_type,
	                       const std::vector<user_field_change_entry>& changes = {});

	Wt::Dbo::Session m_dbo;
};