#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Thread-safe registry that lets one Wt session trigger a live UI update in
// another session belonging to the same user.
//
// Each logged-in session calls register_session() with a callback that refreshes
// its bell and (if open) its notifications page. When a notification is pushed
// for a user, the caller calls notify_user() which posts that callback into every
// live session for that user via Wt::WServer::post().
class notification_hub
{
public:
	// How a callback is dispatched into a session's event loop.
	// Default: Wt::WServer::instance()->post(session_id, fn).
	// Tests inject a synchronous version: [](auto&, auto fn){ fn(); }
	using post_fn_t =
	  std::function<void(const std::string& session_id, std::function<void()> fn)>;

	static notification_hub& instance();

	// Replace the dispatch function and clear all registered sessions.
	// Intended for test setup; pass {} to restore the default Wt::WServer::post.
	void reset(post_fn_t post_fn = {});

	// Must be called from within the target session's event loop.
	void register_session(const std::string&    username,
	                      const std::string&    session_id,
	                      std::function<void()> update_fn);

	// Must be called from within the target session's event loop (logout/dtor).
	void deregister_session(const std::string& username,
	                        const std::string& session_id);

	// Posts update_fn into every live session for username. Thread-safe.
	void notify_user(const std::string& username);

private:
	notification_hub() = default;

	struct entry
	{
		std::string           session_id;
		std::function<void()> update_fn;
	};

	std::mutex                                m_mutex;
	std::map<std::string, std::vector<entry>> m_sessions;
	post_fn_t                                 m_post_fn; // null = use WServer::post
};
