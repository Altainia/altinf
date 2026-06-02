#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>
#include <vector>

// Audit trail for user accounts, mirroring the task audit (task_event_record /
// task_field_change_record). One user_event_record per change, with detailed
// field-level deltas in user_field_change_record. Password hashes/values are
// never recorded — only the fact, the actor, and the time.

struct user_event_record
{
	long long   user_id{0};
	long long   actor_id{0}; // acting user's id; 0 = system (e.g. first-run bootstrap)
	std::string occurred_at; // ISO-8601 UTC
	std::string event_type;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, user_id, "user_id");
		Wt::Dbo::field(a, actor_id, "actor_id");
		Wt::Dbo::field(a, occurred_at, "occurred_at");
		Wt::Dbo::field(a, event_type, "event_type");
	}
};

struct user_field_change_record
{
	long long   event_id{0};
	std::string field_name;
	std::string old_value;
	std::string new_value;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, event_id, "event_id");
		Wt::Dbo::field(a, field_name, "field_name");
		Wt::Dbo::field(a, old_value, "old_value");
		Wt::Dbo::field(a, new_value, "new_value");
	}
};

// ── In-memory views returned by user_db::history_for_user ────────────────────

struct user_field_change_entry
{
	std::string field_name;
	std::string old_value;
	std::string new_value;
};

struct user_event_entry
{
	long long                            id{0};
	long long                            user_id{0};
	long long                            actor_id{0};
	std::string                          occurred_at;
	std::string                          event_type;
	std::vector<user_field_change_entry> changes;
};
