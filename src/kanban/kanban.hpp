#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/WtSqlTraits.h>
#include <Wt/WDate.h>

#include <string>
#include <vector>

// ---- Dbo records ----

struct team_record
{
	std::string name;
	long long   org_id{0};
	int         is_archived{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, name,        "name");
		Wt::Dbo::field(a, org_id,      "org_id");
		Wt::Dbo::field(a, is_archived, "is_archived");
	}
};

struct team_member_record
{
	long long   team_id{0};
	std::string username;
	int         is_lead{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, team_id,  "team_id");
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, is_lead,  "is_lead");
	}
};

struct task_type_record
{
	long long   org_id{0};
	std::string name;
	std::string color;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, org_id, "org_id");
		Wt::Dbo::field(a, name,   "name");
		Wt::Dbo::field(a, color,  "color");
	}
};

struct kanban_task_record
{
	long long   team_id{0};
	std::string status; // "todo" | "in_progress" | "review" | "done"
	std::string title;
	std::string description;
	std::string assigned_to;
	Wt::WDate   start_date;
	Wt::WDate   end_date;
	long long   type_id{0};
	int         sort_order{0};
	int         is_archived{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, team_id,     "team_id");
		Wt::Dbo::field(a, status,      "status");
		Wt::Dbo::field(a, title,       "title");
		Wt::Dbo::field(a, description, "description");
		Wt::Dbo::field(a, assigned_to, "assigned_to");
		Wt::Dbo::field(a, start_date,  "start_date");
		Wt::Dbo::field(a, end_date,    "end_date");
		Wt::Dbo::field(a, type_id,     "type_id");
		Wt::Dbo::field(a, sort_order,  "sort_order");
		Wt::Dbo::field(a, is_archived, "is_archived");
	}
};

struct task_event_record
{
	long long   task_id{0};
	std::string actor;
	std::string occurred_at;
	std::string event_type;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, task_id,     "task_id");
		Wt::Dbo::field(a, actor,       "actor");
		Wt::Dbo::field(a, occurred_at, "occurred_at");
		Wt::Dbo::field(a, event_type,  "event_type");
	}
};

struct task_field_change_record
{
	long long   event_id{0};
	std::string field_name;
	std::string old_value;
	std::string new_value;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, event_id,   "event_id");
		Wt::Dbo::field(a, field_name, "field_name");
		Wt::Dbo::field(a, old_value,  "old_value");
		Wt::Dbo::field(a, new_value,  "new_value");
	}
};

// ---- Entry structs (plain data, no Dbo) ----

struct team_entry
{
	long long   id{0};
	std::string name;
	long long   org_id{0};
	bool        is_archived{false};
};

struct team_member_entry
{
	std::string username;
	bool        is_lead{false};
};

struct task_type_entry
{
	long long   id{0};
	long long   org_id{0};
	std::string name;
	std::string color;
};

struct kanban_task_entry
{
	long long   id{0};
	long long   team_id{0};
	std::string status;
	std::string title;
	std::string description;
	std::string assigned_to;
	Wt::WDate   start_date;
	Wt::WDate   end_date;
	long long   type_id{0};
	int         sort_order{0};
	bool        is_archived{false};
};

struct task_field_change_entry
{
	std::string field_name;
	std::string old_value;
	std::string new_value;
};

struct task_event_entry
{
	long long   id{0};
	long long   task_id{0};
	std::string actor;
	std::string occurred_at;
	std::string event_type;
	std::vector<task_field_change_entry> changes;
};
