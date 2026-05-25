#pragma once

#include <Wt/WContainerWidget.h>

#include <string>

#include "auth/session_data.hpp"
#include "org/kanban_db.hpp"

class team_archive_page: public Wt::WContainerWidget
{
public:
	team_archive_page(kanban_db&          db,
	                  const session_data& session,
	                  long long           team_id);
};