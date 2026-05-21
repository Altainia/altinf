#pragma once

#include "auth/session_data.hpp"
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/widgets/kanban_board_widget.hpp"
#include "org/widgets/gantt_view_widget.hpp"
#include "org/team_cap.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

#include <map>
#include <memory>
#include <string>

class team_kanban_page: public Wt::WContainerWidget
{
public:
    team_kanban_page(kanban_db&          db,
                      org_db&             odb,
                      const session_data& session,
                      long long           team_id,
                      team_cap::flags     caps,
                      team_settings_entry settings,
                      bool                show_gantt);

    ~team_kanban_page() override;

private:
    kanban_db&                       m_db;
    org_db&                          m_odb;
    session_data                     m_session;
    long long                        m_team_id{0};
    long long                        m_org_id{0};
    team_cap::flags                  m_caps;
    team_settings_entry              m_settings;
    bool                             m_show_gantt{false};
    std::string                      m_username;
    std::string                      m_session_id;
    std::shared_ptr<bool>            m_alive{std::make_shared<bool>(true)};
    std::map<long long, std::string> m_type_colors;
    kanban_board_widget*             m_board_widget{nullptr};
    gantt_view_widget*               m_gantt_widget{nullptr};

    void refresh();
};
