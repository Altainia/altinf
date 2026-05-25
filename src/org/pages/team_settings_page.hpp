#pragma once

#include <Wt/WContainerWidget.h>

#include "auth/session_data.hpp"
#include "org/kanban_db.hpp"

class team_settings_page: public Wt::WContainerWidget
{
public:
	team_settings_page(kanban_db&          db,
	                   const session_data& session,
	                   long long           team_id);
};