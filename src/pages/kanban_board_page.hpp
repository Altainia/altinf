#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_board_widget.hpp"

#include <Wt/WContainerWidget.h>

class kanban_board_page: public Wt::WContainerWidget
{
public:
	kanban_board_page(kanban_db&          db,
	                  const session_data& session,
	                  long long           team_id,
	                  bool                is_lead,
	                  bool                show_gantt);

private:
	kanban_db&           m_db;
	long long            m_team_id{0};
	bool                 m_is_lead{false};
	kanban_board_widget* m_board_widget{nullptr};
};
