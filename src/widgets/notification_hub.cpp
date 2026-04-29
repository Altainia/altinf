#include "notification_hub.hpp"

#include <Wt/WServer.h>

notification_hub& notification_hub::instance()
{
	static notification_hub hub;
	return hub;
}

void notification_hub::reset(post_fn_t post_fn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sessions.clear();
	m_post_fn = std::move(post_fn);
}

void notification_hub::register_session(const std::string&    username,
                                        const std::string&    session_id,
                                        std::function<void()> update_fn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sessions[username].push_back({session_id, std::move(update_fn)});
}

void notification_hub::deregister_session(const std::string& username,
                                          const std::string& session_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto                        it = m_sessions.find(username);
	if(it == m_sessions.end())
	{
		return;
	}
	auto& v = it->second;
	v.erase(std::remove_if(v.begin(),
	                       v.end(),
	                       [&](const entry& e) { return e.session_id == session_id; }),
	        v.end());
	if(v.empty())
	{
		m_sessions.erase(it);
	}
}

void notification_hub::notify_user(const std::string& username)
{
	std::vector<entry> entries;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto                        it = m_sessions.find(username);
		if(it == m_sessions.end())
		{
			return;
		}
		entries = it->second;
	}
	for(const auto& e: entries)
	{
		if(m_post_fn)
		{
			m_post_fn(e.session_id, e.update_fn);
		}
		else
		{
			Wt::WServer::instance()->post(e.session_id, e.update_fn);
		}
	}
}
