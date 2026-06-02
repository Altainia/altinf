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
	std::string name;
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

	// Active users by default; pass true to also include soft-deleted users.
	std::vector<user_entry> list_users(bool include_deleted = false);

	// True if a user row with this username exists, including soft-deleted users
	// (so usernames are never reused).
	bool username_exists(const std::string& username);

	// True if the user has a usable password set (a non-empty hash).
	bool has_password(const std::string& username);

	// Check a plaintext password against the stored hash, without touching the
	// session. False for unknown users or users with no password.
	bool verify_password(const std::string& username, const std::string& password);

	// The user's surrogate id, or nullopt if no such user (including soft-deleted
	// users, whose row is retained).
	std::optional<long long> user_id_for(const std::string& username);

	// Full audit history for a user, newest first.
	std::vector<user_event_entry> history_for_user(long long user_id);

	// Soft-delete: the row is retained (so audit/history references resolve and
	// the username stays taken), but login is disabled and tokens/Google links
	// are dropped. Records a "deleted" event.
	void delete_user(const std::string& username, long long actor_id = 0);

	void update_user(const std::string& username,
	                 const std::string& display_name,
	                 permission::flags  permissions,
	                 long long          actor_id = 0);

	void set_display_name(const std::string& username,
	                      const std::string& display_name,
	                      long long          actor_id = 0);

	void set_password(const std::string& username,
	                  const std::string& new_password,
	                  long long          actor_id = 0);

	// Remove the user's password. Refused (returns false) unless the user has a
	// Google link, so they are never left with no way to sign in.
	bool unset_password(const std::string& username, long long actor_id = 0);

	std::vector<api_token_entry> list_tokens(const std::string& username);

	void delete_token(long long token_id, long long actor_id = 0);

	// Returns the raw token to present once; stores only its SHA-256 hash. `name`
	// is a user-chosen label shown in the token list.
	std::string create_api_token(const std::string& username,
	                             const std::string& name,
	                             long long          actor_id = 0);

	void rename_api_token(long long token_id, const std::string& new_name, long long actor_id = 0);

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
	                 const std::string& email,
	                 long long          actor_id = 0);

	// Remove the user's Google link. Refused (returns false) unless the user has a
	// password, so they are never left with no way to sign in.
	bool unlink_google(const std::string& username, long long actor_id = 0);

	// The linked Google email for a user, if any (for display).
	std::optional<std::string> google_email_for(const std::string& username);

	// The username linked to a Google subject id, if any (sign-in lookup).
	std::optional<std::string> username_for_google_sub(const std::string& google_sub);

private:
	// user_id lookup that assumes the caller already holds a transaction.
	std::optional<long long> user_id_for_locked(const std::string& username);

	// Remove any Google identity row for the user, unconditionally (no guard, no
	// audit). Assumes the caller holds a transaction. Used by delete_user.
	void clear_google_link_locked(const std::string& username);

	// Append one audit event (plus its field changes) for a user. Must be called
	// inside an open transaction; mirrors kanban_db::record_event.
	void record_user_event(long long                                   user_id,
	                       long long                                   actor_id,
	                       const std::string&                          event_type,
	                       const std::vector<user_field_change_entry>& changes = {});

	Wt::Dbo::Session m_dbo;
};