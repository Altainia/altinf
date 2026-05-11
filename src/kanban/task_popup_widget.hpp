#pragma once

#include "kanban.hpp"
#include "kanban_db.hpp"
#include "team_cap.hpp"
#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WDialog.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class task_popup_widget : public Wt::WDialog
{
public:
    task_popup_widget(kanban_db&                              db,
                      org_db&                                 odb,
                      long long                               task_id,
                      const session_data&                     session,
                      team_cap::flags                         caps,
                      const std::map<long long, std::string>& type_colors,
                      long long                               team_id);

    ~task_popup_widget() override;

private:
    kanban_db&                       m_db;
    org_db&                          m_odb;
    long long                        m_task_id{0};
    long long                        m_team_id{0};
    long long                        m_org_id{0};
    std::string                      m_username;
    team_cap::flags                  m_caps;
    std::map<long long, std::string> m_type_colors;
    std::string                      m_session_id;
    std::shared_ptr<bool>            m_alive{std::make_shared<bool>(true)};

    kanban_task_entry m_original;

    Wt::WLineEdit*        m_title_edit{nullptr};
    Wt::WText*            m_title_display{nullptr};
    Wt::WContainerWidget* m_title_field{nullptr};

    Wt::WTextArea*        m_desc_edit{nullptr};
    Wt::WText*            m_desc_display{nullptr};
    Wt::WContainerWidget* m_desc_field{nullptr};

    Wt::WComboBox*        m_status_edit{nullptr};
    Wt::WText*            m_status_display{nullptr};
    Wt::WContainerWidget* m_status_field{nullptr};

    Wt::WComboBox*        m_assignee_edit{nullptr};
    Wt::WText*            m_assignee_display{nullptr};
    Wt::WContainerWidget* m_assignee_field{nullptr};

    Wt::WDateEdit*        m_start_date_edit{nullptr};
    Wt::WText*            m_start_date_display{nullptr};
    Wt::WContainerWidget* m_start_field{nullptr};

    Wt::WDateEdit*        m_end_date_edit{nullptr};
    Wt::WText*            m_end_date_display{nullptr};
    Wt::WContainerWidget* m_end_field{nullptr};

    long long                          m_type_id{0};
    std::vector<Wt::WContainerWidget*> m_type_chips;
    std::vector<std::string>           m_assignee_values;

    Wt::WContainerWidget* m_stale_banner{nullptr};
    bool                  m_stale{false};
    Wt::WPushButton*      m_save_btn{nullptr};
    Wt::WContainerWidget* m_comment_list{nullptr};
    Wt::WContainerWidget* m_comment_compose{nullptr};

    std::set<std::string> m_dirty_fields;

    static const std::vector<std::string> k_status_vals;
    static const std::vector<std::string> k_status_labels;

    void save();
    void mark_stale();
    void mark_field_dirty(const std::string& field, Wt::WContainerWidget* container);
    void enter_edit_mode(Wt::WText* display, Wt::WWidget* edit);
    void exit_edit_mode(Wt::WText* display, Wt::WWidget* edit,
                        const std::string& new_display_text);
    void rebuild_comments();
};
