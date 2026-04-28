#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

class kanban_team_page: public Wt::WContainerWidget
{
public:
	kanban_team_page(kanban_db& db, const session_data& session);

private:
	kanban_db&            m_db;
	long long             m_team_id{0};
	Wt::WContainerWidget* m_members_container{nullptr};
	Wt::WLineEdit*        m_member_input{nullptr};
	Wt::WText*            m_status_msg{nullptr};

	struct member_row
	{
		Wt::WContainerWidget* container{nullptr};
		std::string           username;
	};
	std::vector<member_row> m_member_rows;

	void add_member_row(const std::string& username);
	void remove_member_row(Wt::WContainerWidget* c);
};
