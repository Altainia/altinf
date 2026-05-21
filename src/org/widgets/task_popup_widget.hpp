#pragma once

#include "org/kanban_db.hpp"
#include "org/kanban.hpp"
#include "org/team_cap.hpp"
#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WDialog.h>
#include <Wt/WLineEdit.h>

#include <memory>
#include <string>

class task_editor_form_widget;

class task_popup_widget : public Wt::WDialog
{
public:
    task_popup_widget(kanban_db&          db,
                      org_db&             odb,
                      long long           task_id,
                      const session_data& session,
                      team_cap::flags     caps,
                      team_settings_entry settings,
                      long long           team_id);

    ~task_popup_widget() override;

private:
    task_editor_form_widget* m_form{nullptr};
    Wt::WLineEdit*           m_close_cb{nullptr};

    void try_close();
};
