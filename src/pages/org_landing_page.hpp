#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

#include <string>

class org_landing_page: public Wt::WContainerWidget
{
public:
    org_landing_page(org_db&             odb,
                     kanban_db&          kdb,
                     long long           org_id,
                     const session_data& session,
                     bool                is_org_lead);

    ~org_landing_page() override;

private:
    org_db&             m_odb;
    kanban_db&          m_kdb;
    long long           m_org_id;
    const session_data& m_session;
    bool                m_is_org_lead;
    std::string         m_session_id;

    void render();
    void refresh();
};
