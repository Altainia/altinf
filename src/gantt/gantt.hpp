#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct gantt_project_record
{
	std::string title;
	std::string description;
	std::string owner_username;
	std::string created_date;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, title, "title");
		Wt::Dbo::field(a, description, "description");
		Wt::Dbo::field(a, owner_username, "owner_username");
		Wt::Dbo::field(a, created_date, "created_date");
	}
};

struct gantt_task_record
{
	long long   project_id{0};
	std::string title;
	std::string assigned_to;
	std::string start_date;
	std::string end_date;
	std::string color;
	int         sort_order{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, project_id, "project_id");
		Wt::Dbo::field(a, title, "title");
		Wt::Dbo::field(a, assigned_to, "assigned_to");
		Wt::Dbo::field(a, start_date, "start_date");
		Wt::Dbo::field(a, end_date, "end_date");
		Wt::Dbo::field(a, color, "color");
		Wt::Dbo::field(a, sort_order, "sort_order");
	}
};

struct gantt_viewer_record
{
	long long   project_id{0};
	std::string username;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, project_id, "project_id");
		Wt::Dbo::field(a, username, "username");
	}
};

struct gantt_project_entry
{
	long long   id{0};
	std::string title;
	std::string description;
	std::string owner_username;
	std::string created_date;
};

struct gantt_task_entry
{
	long long   id{0};
	long long   project_id{0};
	std::string title;
	std::string assigned_to;
	std::string start_date;
	std::string end_date;
	std::string color;
	int         sort_order{0};
};
