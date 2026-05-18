#pragma once

#include <Wt/WContainerWidget.h>

#include <functional>
#include <string>
#include <vector>

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"
#include "org/org_db.hpp"

class kanban_task_editor_page: public Wt::WContainerWidget
{
public:
	kanban_task_editor_page(kanban_db&               db,
	                        org_db&                  odb,
	                        long long                team_id,
	                        const session_data&      session,
	                        team_cap::flags          caps,
	                        team_settings_entry      settings,
	                        const kanban_task_entry* existing,
	                        std::function<void()>    on_save);
};
