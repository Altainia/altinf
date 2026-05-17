#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

#include <functional>
#include <string>
#include <vector>

class kanban_task_editor_page : public Wt::WContainerWidget
{
public:
    // members and types are accepted but unused (form fetches from DB directly).
    kanban_task_editor_page(kanban_db&                          db,
                            org_db&                             odb,
                            long long                           team_id,
                            const session_data&                 session,
                            team_cap::flags                     caps,
                            const kanban_task_entry*            existing,
                            const std::vector<std::string>&     members,
                            const std::vector<task_type_entry>& types,
                            std::function<void()>               on_save);
};
