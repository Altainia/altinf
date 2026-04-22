#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct link_entry
{
	long long   id{0};
	std::string url;
	std::string title;
	std::string description;
	std::string section;
	int         sort_order{0};
	std::string added_date;
};

struct link_record
{
	std::string url;
	std::string title;
	std::string description;
	std::string section;
	int         sort_order{0};
	std::string added_date;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, url, "url");
		Wt::Dbo::field(a, title, "title");
		Wt::Dbo::field(a, description, "description");
		Wt::Dbo::field(a, section, "section");
		Wt::Dbo::field(a, sort_order, "sort_order");
		Wt::Dbo::field(a, added_date, "added_date");
	}
};
