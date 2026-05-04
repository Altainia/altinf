#include "live_hub.hpp"

#include <Wt/WServer.h>

live_hub& live_hub::instance()
{
	static live_hub hub;
	return hub;
}

void live_hub::reset(post_fn_t post_fn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sessions.clear();
	m_post_fn = std::move(post_fn);
}

void live_hub::subscribe(const std::string&    channel,
                         const std::string&    session_id,
                         std::function<void()> update_fn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sessions[channel].push_back({session_id, std::move(update_fn)});
}

void live_hub::unsubscribe(const std::string& channel,
                           const std::string& session_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto                        it = m_sessions.find(channel);
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

void live_hub::broadcast(const std::string& channel)
{
	std::vector<entry> entries;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto                        it = m_sessions.find(channel);
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
