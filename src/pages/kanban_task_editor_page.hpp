#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"
#include "org/org_db.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTabWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class kanban_task_editor_page: public Wt::WContainerWidget
{
public:
    kanban_task_editor_page(kanban_db&                          db,
                            org_db&                             odb,
                            long long                           team_id,
                            const session_data&                 session,
                            team_cap::flags                     caps,
                            const kanban_task_entry*            existing,
                            const std::vector<std::string>&     members,
                            const std::vector<task_type_entry>& types,
                            std::function<void()>               on_save);

    ~kanban_task_editor_page() override;

private:
    kanban_db&               m_db;
    org_db&                  m_odb;
    long long                m_team_id;
    std::string              m_username;
    team_cap::flags          m_caps;
    const kanban_task_entry* m_existing;
    std::function<void()>    m_on_save;

    long long             m_task_id{0};
    std::string           m_session_id;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    Wt::WLineEdit*    m_title{nullptr};
    Wt::WTextArea*    m_description{nullptr};
    Wt::WComboBox*    m_status{nullptr};
    Wt::WComboBox*    m_assigned_to{nullptr};
    Wt::WDateEdit*    m_start_date{nullptr};
    Wt::WDateEdit*    m_end_date{nullptr};
    long long                          m_type_id{0};
    std::vector<Wt::WContainerWidget*> m_type_chips;
    Wt::WText*                         m_status_msg{nullptr};
    Wt::WPushButton*                   m_save_btn{nullptr};
    Wt::WPushButton*                   m_del_btn{nullptr};
    Wt::WContainerWidget*              m_history_panel{nullptr};
    Wt::WContainerWidget*              m_comment_list{nullptr};
    Wt::WContainerWidget*              m_comment_compose{nullptr};

    std::vector<std::string> m_assignee_values;

    static const std::vector<std::string> k_status_vals;
    static const std::vector<std::string> k_status_labels;

    void save();
    void mark_stale();
    void rebuild_history();
    void rebuild_comments();
};
