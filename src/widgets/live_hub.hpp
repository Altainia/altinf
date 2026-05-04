#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Thread-safe pub/sub hub keyed on channel strings.
// Channels follow the convention "type:id", e.g. "org:5", "user:alice".
// Subscribe from within the target session's event loop.
// Broadcast is thread-safe and posts each callback via WServer::post().
class live_hub
{
public:
    using post_fn_t =
      std::function<void(const std::string& session_id, std::function<void()> fn)>;

    static live_hub& instance();

    // Replace dispatch function and clear all subscriptions. For test setup only.
    void reset(post_fn_t post_fn = {});

    // Subscribe a session callback to a channel.
    void subscribe(const std::string&    channel,
                   const std::string&    session_id,
                   std::function<void()> fn);

    // Remove a session's subscription from a channel.
    void unsubscribe(const std::string& channel,
                     const std::string& session_id);

    // Post fn into every session subscribed to channel. Thread-safe.
    void broadcast(const std::string& channel);

private:
    live_hub() = default;

    struct entry
    {
        std::string           session_id;
        std::function<void()> update_fn;
    };

    std::mutex                                m_mutex;
    std::map<std::string, std::vector<entry>> m_sessions; // channel → entries
    post_fn_t                                 m_post_fn;  // null = use WServer::post
};
