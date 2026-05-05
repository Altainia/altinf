#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_board_widget.hpp"
#include "kanban/gantt_view_widget.hpp"

#include <Wt/WContainerWidget.h>

#include <map>
#include <memory>
#include <string>

class kanban_board_page: public Wt::WContainerWidget
{
public:
    kanban_board_page(kanban_db&          db,
                      const session_data& session,
                      long long           team_id,
                      bool                is_lead,
                      bool                show_gantt);

    ~kanban_board_page() override;

private:
    kanban_db&                       m_db;
    long long                        m_team_id{0};
    long long                        m_org_id{0};
    bool                             m_is_lead{false};
    bool                             m_show_gantt{false};
    std::string                      m_session_id;
    std::shared_ptr<bool>            m_alive{std::make_shared<bool>(true)};
    std::map<long long, std::string> m_type_colors;
    kanban_board_widget*             m_board_widget{nullptr};
    gantt_view_widget*               m_gantt_widget{nullptr};

    void refresh();
};
