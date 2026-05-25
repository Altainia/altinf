#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>

#include <string>

#include "auth/session_data.hpp"
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/org_db.hpp"
#include "org/team_cap.hpp"

class task_edit_page: public Wt::WContainerWidget
{
public:
	task_edit_page(kanban_db&               db,
	               org_db&                  odb,
	               long long                team_id,
	               const session_data&      session,
	               team_cap::flags          caps,
	               team_settings_entry      settings,
	               const kanban_task_entry* existing);

	Wt::Signal<> saved;
};
