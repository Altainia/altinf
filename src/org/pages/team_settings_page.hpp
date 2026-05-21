#pragma once

#include "auth/session_data.hpp"
#include "org/kanban_db.hpp"

#include <Wt/WContainerWidget.h>

class team_settings_page: public Wt::WContainerWidget
{
public:
	team_settings_page(kanban_db&          db,
	                   const session_data& session,
	                   long long           team_id);
};
