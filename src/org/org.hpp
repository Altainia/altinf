#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

// ---- Dbo records ----

struct org_record
{
	std::string name;
	int         is_archived{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, name,        "name");
		Wt::Dbo::field(a, is_archived, "is_archived");
	}
};

struct org_member_record
{
	long long   org_id{0};
	std::string username;
	int         is_lead{0};
	std::string status; // "pending" | "active" | "declined"

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, org_id,   "org_id");
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, is_lead,  "is_lead");
		Wt::Dbo::field(a, status,   "status");
	}
};

struct notification_record
{
	std::string username;
	std::string type;       // "org_invite" | "task_assigned"
	std::string payload;    // JSON string
	int         is_read{0};
	std::string created_at; // ISO-8601 text e.g. "2026-04-28 14:05:00"

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username,   "username");
		Wt::Dbo::field(a, type,       "type");
		Wt::Dbo::field(a, payload,    "payload");
		Wt::Dbo::field(a, is_read,    "is_read");
		Wt::Dbo::field(a, created_at, "created_at");
	}
};

struct user_pref_record
{
	std::string username;
	long long   last_org_id{0}; // 0 = not set

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username,    "username");
		Wt::Dbo::field(a, last_org_id, "last_org_id");
	}
};

struct user_org_pref_record
{
	std::string username;
	long long   org_id{0};
	int         notify_task_available{1};
	int         notify_task_unassigned{1};
	int         notify_coassignee_changed{1};
	int         notify_task_abandoned{1};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username,                  "username");
		Wt::Dbo::field(a, org_id,                    "org_id");
		Wt::Dbo::field(a, notify_task_available,     "notify_task_available");
		Wt::Dbo::field(a, notify_task_unassigned,    "notify_task_unassigned");
		Wt::Dbo::field(a, notify_coassignee_changed, "notify_coassignee_changed");
		Wt::Dbo::field(a, notify_task_abandoned,     "notify_task_abandoned");
	}
};

struct user_org_pref_entry
{
	std::string username;
	long long   org_id{0};
	bool        notify_task_available{true};
	bool        notify_task_unassigned{true};
	bool        notify_coassignee_changed{true};
	bool        notify_task_abandoned{true};
};

// ---- Entry structs (plain data, no Dbo) ----

struct org_entry
{
	long long   id{0};
	std::string name;
	bool        is_archived{false};
};

struct org_member_entry
{
	long long   id{0};
	long long   org_id{0};
	std::string username;
	bool        is_lead{false};
	std::string status; // "pending" | "active" | "declined"
};

struct notification_entry
{
	long long   id{0};
	std::string username;
	std::string type;
	std::string payload;
	bool        is_read{false};
	std::string created_at;
};

// ---- JSON payload helpers ----

inline std::string make_org_invite_payload(long long org_id, const std::string& org_name)
{
	return "{\"org_id\":" + std::to_string(org_id) +
	       ",\"org_name\":\"" + org_name + "\"}";
}

inline std::string make_org_invite_rescinded_payload(long long org_id, const std::string& org_name)
{
	return "{\"org_id\":" + std::to_string(org_id) +
	       ",\"org_name\":\"" + org_name + "\"" +
	       ",\"rescinded\":1}";
}

// Shared payload for org_removed, org_lead_promoted, org_lead_demoted.
inline std::string make_org_event_payload(long long org_id, const std::string& org_name)
{
	return "{\"org_id\":" + std::to_string(org_id) +
	       ",\"org_name\":\"" + org_name + "\"}";
}

// Shared payload for team_added, team_removed.
inline std::string make_team_event_payload(long long          team_id,
                                           const std::string& team_name,
                                           long long          org_id,
                                           const std::string& org_name)
{
	return "{\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"" +
	       ",\"org_id\":" + std::to_string(org_id) +
	       ",\"org_name\":\"" + org_name + "\"}";
}

// Shared payload for team_lead_promoted, team_lead_demoted.
inline std::string make_team_lead_payload(long long team_id, const std::string& team_name)
{
	return "{\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_assigned_payload(long long          task_id,
                                               const std::string& task_title,
                                               long long          team_id,
                                               const std::string& team_name)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_available_payload(long long          task_id,
                                               const std::string& task_title,
                                               long long          team_id,
                                               const std::string& team_name)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_unassigned_payload(long long          task_id,
                                                const std::string& task_title,
                                                long long          team_id,
                                                const std::string& team_name)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_abandoned_payload(long long          task_id,
                                               const std::string& task_title,
                                               long long          team_id,
                                               const std::string& team_name,
                                               const std::string& abandoned_by)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"" +
	       ",\"abandoned_by\":\"" + abandoned_by + "\"}";
}

inline std::string make_task_coassignee_changed_payload(long long          task_id,
                                                        const std::string& task_title,
                                                        long long          team_id,
                                                        const std::string& team_name,
                                                        const std::string& changed_user,
                                                        const std::string& action)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"" +
	       ",\"changed_user\":\"" + changed_user + "\"" +
	       ",\"action\":\"" + action + "\"}";
}

// Minimal JSON field extractors — safe only for our controlled payloads.
inline long long json_long(const std::string& json, const std::string& key)
{
	const auto kpos = json.find("\"" + key + "\":");
	if(kpos == std::string::npos)
		return 0;
	const auto vpos = json.find_first_of("-0123456789", kpos + key.size() + 3);
	if(vpos == std::string::npos)
		return 0;
	try { return std::stoll(json.substr(vpos)); } catch(...) { return 0; }
}

inline std::string json_str(const std::string& json, const std::string& key)
{
	const std::string needle = "\"" + key + "\":\"";
	const auto        kpos   = json.find(needle);
	if(kpos == std::string::npos)
		return {};
	const auto vpos = kpos + needle.size();
	const auto epos = json.find('"', vpos);
	if(epos == std::string::npos)
		return {};
	return json.substr(vpos, epos - vpos);
}
