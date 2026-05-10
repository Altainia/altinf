#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"

#include <Wt/WContainerWidget.h>

#include <string>

class kanban_archive_page: public Wt::WContainerWidget
{
public:
    kanban_archive_page(kanban_db&          db,
                        const session_data& session,
                        long long           team_id);
};
