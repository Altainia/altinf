#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/WtSqlTraits.h>
#include <Wt/WDate.h>

#include <string>

// ---- Dbo records ----

struct team_record
{
	std::string name;
	long long   org_id{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, name,   "name");
		Wt::Dbo::field(a, org_id, "org_id");
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

struct kanban_task_record
{
	long long   team_id{0};
	std::string status; // "todo" | "in_progress" | "review" | "done"
	std::string title;
	std::string description;
	std::string assigned_to;
	Wt::WDate   start_date;
	Wt::WDate   end_date;
	std::string color;
	int         sort_order{0};

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
		Wt::Dbo::field(a, color,       "color");
		Wt::Dbo::field(a, sort_order,  "sort_order");
	}
};

// ---- Entry structs (plain data, no Dbo) ----

struct team_entry
{
	long long   id{0};
	std::string name;
	long long   org_id{0};
};

struct team_member_entry
{
	std::string username;
	bool        is_lead{false};
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
	std::string color;
	int         sort_order{0};
};
